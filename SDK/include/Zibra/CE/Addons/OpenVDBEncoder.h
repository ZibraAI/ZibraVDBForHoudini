#pragma once

#include <Zibra/CE/Common.h>
#include <Zibra/CE/Decompression.h>
#include <algorithm>
#include <cstdint>
#include <execution>
#include <openvdb/tools/Dense.h>

namespace Zibra::CE::Addons::OpenVDBUtils
{
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

        static openvdb::GridPtrVec CreateGrids(const Decompression::FrameInfo& frameInfo) noexcept
        {
            using namespace Decompression;

            if (frameInfo.channelsCount == 0 || frameInfo.spatialBlockCount == 0 || frameInfo.channelBlockCount == 0)
            {
                return {};
            }

            // TODO (VDB-978): use per frame transform.
            openvdb::math::Transform::Ptr transform =
                openvdb::math::Transform::createLinearTransform(openvdb::Mat4d{frameInfo.channels[0].gridTransform.raw});

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
                grid->setTransform(transform);
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