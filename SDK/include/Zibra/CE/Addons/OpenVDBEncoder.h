#pragma once

#include <Zibra/Math3D.h>
#include <Zibra/CE/Common.h>
#include <Zibra/CE/Decompression.h>
#include <algorithm>
#include <cstdint>
#include <execution>
#include <openvdb/tools/Dense.h>

namespace Zibra::CE::Addons::OpenVDBUtils
{
    struct FrameData
    {
        const void* decompressionPerChannelBlockData;
        const void* decompressionPerSpatialBlockInfo;
    };

    class FrameEncoder
    {
        using float16_mem = uint16_t;
        struct ChannelBlockF16Mem
        {
            float16_mem mem[SPARSE_BLOCK_VOXEL_COUNT];
        };
        struct LeafIntermediate
        {
            const ChannelBlockF16Mem* chBlocks[4] = {};
        };
        struct GridIntermediate
        {
            // GridType type
            std::map<openvdb::Coord, LeafIntermediate> leafs;
        };
    public:
        openvdb::GridPtrVec EncodeFrame(const FrameData& fData, const Decompression::FrameInfo& fInfo) noexcept
        {
            std::map<std::string, GridIntermediate> gridsIntermediate{};

            const auto* packedSpatialInfo = static_cast<const ZCEDecompressionPackedSpatialBlock*>(fData.decompressionPerSpatialBlockInfo);
            const auto* channelBlocksSrc = static_cast<const ChannelBlockF16Mem*>(fData.decompressionPerChannelBlockData);

            for (size_t spatialIdx = 0; spatialIdx < fInfo.spatialBlockCount; ++spatialIdx)
            {
                size_t localChannelBlockIdx = 0;
                for (size_t i = 0; i < MAX_CHANNEL_COUNT; ++i)
                {
                    const auto& curSpatialInfo = UnpackPackedSpatialBlock(packedSpatialInfo[spatialIdx]);
                    openvdb::Coord blockCoord{curSpatialInfo.coords[0], curSpatialInfo.coords[1], curSpatialInfo.coords[2]};
                    if (curSpatialInfo.channelMask & 1 << i)
                    {
                        const char* chName = fInfo.channels[i].name;

                        // Find grid intermediate by name or create empty if it is absent
                        auto gridIntermediateIt = gridsIntermediate.find(chName);
                        if (gridIntermediateIt == gridsIntermediate.end())
                            gridIntermediateIt = gridsIntermediate.insert({chName, {}}).first;

                        // Find leaf intermediate in grid intermediate or create if it is absent
                        auto leafIntermediateMapIt = gridIntermediateIt->second.leafs.find(blockCoord);
                        if (leafIntermediateMapIt == gridIntermediateIt->second.leafs.end())
                            leafIntermediateMapIt = gridIntermediateIt->second.leafs.insert({blockCoord, {}}).first;

                        const size_t channelBlockIdx = curSpatialInfo.channelBlocksOffset + localChannelBlockIdx;
                        leafIntermediateMapIt->second.chBlocks[0] = &channelBlocksSrc[channelBlockIdx];

                        ++localChannelBlockIdx;
                    }
                }
            }

            std::map<std::string, openvdb::GridBase::Ptr> outGrids{};
            std::for_each(std::execution::par_unseq, gridsIntermediate.begin(), gridsIntermediate.end(), [&](auto& gridIt) {
                outGrids[gridIt.first] = ConstructGrid<openvdb::FloatGrid>(gridIt.second);

            });
            openvdb::GridPtrVec result{};
            result.reserve(outGrids.size());
            for (const auto& [gridName, grid] : outGrids)
            {
                grid->setName(gridName);
                result.emplace_back(grid);
            }
            return result;
        }
    private:
        template<typename GridT>
        auto ConstructGrid(const GridIntermediate& gridIntermediate) noexcept
        {
            const auto& leafIntermediates = gridIntermediate.leafs;

            std::mutex gridAccessMutex{};
            auto result = GridT::create();

            std::for_each(std::execution::par_unseq, leafIntermediates.begin(), leafIntermediates.end(), [&](auto leafIt) {
                using TreeT = typename GridT::TreeType;
                using LeafT = typename TreeT::LeafNodeType;
                LeafT* leaf = ConstructLeaf<LeafT>(leafIt.first, leafIt.second);

                std::lock_guard guard{gridAccessMutex};
                result->tree().addLeaf(leaf);
            });
            return result;
        }

        template<typename LeafT>
        LeafT* ConstructLeaf(const openvdb::Coord& leafCoord, const LeafIntermediate& leafIntermediate) noexcept
        {
            const openvdb::Coord blockMin = {leafCoord.x() * SPARSE_BLOCK_SIZE, leafCoord.y() * SPARSE_BLOCK_SIZE,
                                             leafCoord.z() * SPARSE_BLOCK_SIZE};

            auto* leaf = new LeafT{openvdb::PartialCreate{}, blockMin, 0.0f, true};
            leaf->allocate();
            float* leafBuffer = leaf->buffer().data();

            if (leafIntermediate.chBlocks[0] != nullptr)
            {
                CopyFromStrided(leafBuffer, *leafIntermediate.chBlocks[0], 1, 0);
            }
            return leaf;
        }
    private:
        static void CopyFromStrided(float* dst, const ChannelBlockF16Mem& src, size_t componentCount, size_t componentIdx) noexcept
        {
            for (size_t i = 0; i < SPARSE_BLOCK_VOXEL_COUNT; ++i)
            {
                dst[i * componentCount + componentIdx] = Float16ToFloat32(src.mem[i]);
            }
        }
    };

















    class OpenVDBEncoder final
    {
    public:
        struct FrameData
        {
            /// Payload per channel block.
            const ChannelBlock* decompressionPerChannelBlockData = nullptr;
            size_t perChannelBlocksDataCount = 0;
            /// Info per spatial group
            const SpatialBlockInfo* decompressionPerSpatialBlockInfo = nullptr;
            size_t perSpatialBlocksInfoCount = 0;
        };

        static openvdb::math::Transform::Ptr SanitizeTransform(const Math3D::Transform& inTransform)
        {
            bool isEmpty = true;
            for (float value : inTransform.raw)
            {
                if (!Math3D::IsNearlyEqual(value, 0.0f))
                {
                    isEmpty = false;
                    break;
                }
            }

            if (isEmpty)
            {
                return openvdb::math::Transform::createLinearTransform();
            }

            return openvdb::math::Transform::createLinearTransform(openvdb::Mat4d{inTransform.raw});
        }

        static openvdb::GridPtrVec CreateGrids(const Decompression::FrameInfo& frameInfo) noexcept
        {
            using namespace Decompression;

            if (frameInfo.channelsCount == 0 || frameInfo.spatialBlockCount == 0 || frameInfo.channelBlockCount == 0)
            {
                return {};
            }

            const uint32_t gridCount = frameInfo.channelsCount;

            // Create grids.
            openvdb::GridPtrVec grids{};
            grids.reserve(gridCount);
            for (int i = 0; i < gridCount; ++i)
            {
                const auto& channel = frameInfo.channels[i];
                const char* channelName = channel.name;

                openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(0.f);
                grid->setName(channelName);
                grid->setTransform(SanitizeTransform(frameInfo.channels[i].gridTransform));
                grids.push_back(grid);
            }

            return grids;
        }

        static openvdb::GridPtrVec Encode(const Decompression::DecompressFrameDesc& desc,
                                          const Decompression::DecompressedFrameFeedback& feedback,
                                          const Decompression::FrameInfo& frameInfo, const FrameData& frameData) noexcept
        {
            auto grids = CreateGrids(frameInfo);
            Encode(grids, desc, feedback, frameInfo, frameData);
            return grids;
        }
        static openvdb::GridPtrVec Encode(openvdb::GridPtrVec& grids, const Decompression::DecompressFrameDesc& desc,
                                          const Decompression::DecompressedFrameFeedback& feedback,
                                          const Decompression::FrameInfo& frameInfo, const FrameData& frameData) noexcept
        {
            using namespace Decompression;

            if (frameData.perSpatialBlocksInfoCount == 0 || frameData.perChannelBlocksDataCount == 0)
            {
                return {};
            }

            // For each channel fill grids.
            std::for_each(std::execution::seq, grids.begin(), grids.end(), [&](auto& baseGrid) {
                const int channelIndex = &baseGrid - grids.data();

                const auto countActiveChannelOffset = [currentChannelMask = 1 << channelIndex](const int blockChannelMask) noexcept -> int {
                    if (!(blockChannelMask & currentChannelMask))
                    {
                        return -1;
                    }
                    return static_cast<int>(CountBits(blockChannelMask & (currentChannelMask - 1)));
                };

                // Calculate leafs of tree.
                using TreeT = openvdb::FloatGrid::TreeType;
                using LeafT = typename TreeT::LeafNodeType;
                std::vector<LeafT*> leafs(frameData.perSpatialBlocksInfoCount);

                std::transform(std::execution::par_unseq, frameData.decompressionPerSpatialBlockInfo,
                               frameData.decompressionPerSpatialBlockInfo + frameData.perSpatialBlocksInfoCount, leafs.begin(),
                               [&](const SpatialBlockInfo& blockInfo) -> LeafT* {
                                   const int activeChannelOffset = countActiveChannelOffset(blockInfo.channelMask);
                                   if (activeChannelOffset == -1)
                                   {
                                       return nullptr;
                                   }

                                   const ChannelBlock& block =
                                       frameData.decompressionPerChannelBlockData[blockInfo.channelBlocksOffset -
                                                                                  feedback.firstChannelBlockIndex + activeChannelOffset];

                                   const auto x = blockInfo.coords[0];
                                   const auto y = blockInfo.coords[1];
                                   const auto z = blockInfo.coords[2];

                                   const openvdb::Coord blockMin = {x * SPARSE_BLOCK_SIZE, y * SPARSE_BLOCK_SIZE, z * SPARSE_BLOCK_SIZE};

                                   auto* leaf = new LeafT{openvdb::PartialCreate{}, blockMin, 0.f, true};
                                   leaf->allocate();

                                   float* leafBuffer = leaf->buffer().data();
                                   std::memcpy(leafBuffer, block.voxels, SPARSE_BLOCK_VOXEL_COUNT * sizeof(float));
                                   return leaf;
                               });

                // Fill the grid with leafs.
                auto grid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);
                openvdb::tree::ValueAccessor<TreeT> acc(grid->tree());
                for (auto& leaf : leafs)
                {
                    if (leaf)
                    {
                        acc.addLeaf(leaf);
                    }
                }
            });
            return grids;
        }
    };
} // namespace Zibra::CE::Addons::OpenVDBUtils