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
    enum class GridVoxelType
    {
        Float1,
        Float3,
    };

    struct FrameData
    {
        const void* decompressionPerChannelBlockData;
        const void* decompressionPerSpatialBlockInfo;
    };

    struct VDBGridDesc
    {
        const char* gridName = "UnnamedVDB";
        GridVoxelType voxelType = GridVoxelType::Float1;
        const char* chSource[4] = {nullptr, nullptr, nullptr, nullptr};
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
            GridVoxelType voxelType;
            Math3D::Transform transform;
            std::map<openvdb::Coord, LeafIntermediate> leafs;
        };
        struct VDBGridDescRef
        {
            const VDBGridDesc* desc;
            uint32_t chIdx;
        };
    public:
        openvdb::GridPtrVec EncodeFrame(const VDBGridDesc* gridsDescs, size_t gridsCount, const FrameData& fData,
                                        const Decompression::FrameInfo& fInfo) noexcept
        {
            std::map<std::string, std::vector<VDBGridDescRef>> chNameToGridDescs{};
            for (size_t i = 0; i < gridsCount; ++i)
            {
                const auto& gridDesc = gridsDescs[i];
                switch (gridDesc.voxelType)
                {
                case GridVoxelType::Float1:
                    ResolveChGridToGridDescItem(chNameToGridDescs, gridDesc, 0);
                    break;
                case GridVoxelType::Float3:
                    ResolveChGridToGridDescItem(chNameToGridDescs, gridDesc, 0);
                    ResolveChGridToGridDescItem(chNameToGridDescs, gridDesc, 1);
                    ResolveChGridToGridDescItem(chNameToGridDescs, gridDesc, 2);
                    break;
                default:
                    assert(0 && "Unsupported grid voxel type");
                }
            }

            std::map<std::string, const Decompression::ChannelInfo*> chNameToChInfo{};
            for (size_t i = 0; i < fInfo.channelsCount; ++i)
            {
                chNameToChInfo[fInfo.channels[i].name] = &fInfo.channels[i];
            }

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
                        const Decompression::ChannelInfo& chInfo = *chNameToChInfo[chName];

                        for (const VDBGridDescRef& gridRef : chNameToGridDescs[chName])
                        {
                            const char* targetGridName = gridRef.desc->gridName;

                            // Find grid intermediate by name or create empty if it is absent
                            const auto gridIntermediateToCreate = GridIntermediate{gridRef.desc->voxelType, chInfo.gridTransform, {}};
                            auto gridIntermediateIt = gridsIntermediate.find(targetGridName);
                            if (gridIntermediateIt == gridsIntermediate.end())
                                gridIntermediateIt = gridsIntermediate.insert({targetGridName, gridIntermediateToCreate}).first;

                            // Find leaf intermediate in grid intermediate or create if it is absent
                            auto leafIntermediateMapIt = gridIntermediateIt->second.leafs.find(blockCoord);
                            if (leafIntermediateMapIt == gridIntermediateIt->second.leafs.end())
                                leafIntermediateMapIt = gridIntermediateIt->second.leafs.insert({blockCoord, {}}).first;

                            const size_t channelBlockIdx = curSpatialInfo.channelBlocksOffset + localChannelBlockIdx;
                            leafIntermediateMapIt->second.chBlocks[gridRef.chIdx] = &channelBlocksSrc[channelBlockIdx];
                        }

                        ++localChannelBlockIdx;
                    }
                }
            }

            std::map<std::string, openvdb::GridBase::Ptr> outGrids{};
            std::for_each(std::execution::par_unseq, gridsIntermediate.begin(), gridsIntermediate.end(), [&](auto& gridIt) {
                switch (gridIt.second.voxelType)
                {
                case GridVoxelType::Float1: {
                    outGrids[gridIt.first] = ConstructGrid<openvdb::FloatGrid>(gridIt.second);
                    break;
                }
                case GridVoxelType::Float3: {
                    outGrids[gridIt.first] = ConstructGrid<openvdb::Vec3fGrid>(gridIt.second);
                    break;
                }
                default:
                    assert(0 && "Unsupported grid voxel type");
                }
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
            result->setTransform(SanitizeTransform(gridIntermediate.transform));

            std::for_each(std::execution::par_unseq, leafIntermediates.begin(), leafIntermediates.end(), [&](auto leafIt) {
                using TreeT = typename GridT::TreeType;
                using LeafT = typename TreeT::LeafNodeType;
                LeafT* leaf = ConstructLeaf<LeafT>(leafIt.first, leafIt.second, gridIntermediate.voxelType);

                std::lock_guard guard{gridAccessMutex};
                result->tree().addLeaf(leaf);
            });
            return result;
        }

        template<typename LeafT>
        LeafT* ConstructLeaf(const openvdb::Coord& leafCoord, const LeafIntermediate& leafIntermediate, GridVoxelType voxelType) noexcept
        {
            const openvdb::Coord blockMin = {leafCoord.x() * SPARSE_BLOCK_SIZE, leafCoord.y() * SPARSE_BLOCK_SIZE,
                                             leafCoord.z() * SPARSE_BLOCK_SIZE};
            auto* leaf = new LeafT{openvdb::PartialCreate{}, blockMin, {}, true};
            leaf->allocate();
            float* leafBuf = reinterpret_cast<float*>(leaf->buffer().data());

            static constexpr ChannelBlockF16Mem zeroedBlock = {};

            switch (voxelType)
            {
            case GridVoxelType::Float1: {
                static constexpr size_t compCount = 1;
                CopyToStrided(leafBuf, leafIntermediate.chBlocks[0] ? *leafIntermediate.chBlocks[0] : zeroedBlock, compCount, 0);
                break;
            }
            case GridVoxelType::Float3: {
                static constexpr size_t compCount = 3;
                CopyToStrided(leafBuf, leafIntermediate.chBlocks[0] ? *leafIntermediate.chBlocks[0] : zeroedBlock, compCount, 0);
                CopyToStrided(leafBuf, leafIntermediate.chBlocks[1] ? *leafIntermediate.chBlocks[1] : zeroedBlock, compCount, 1);
                CopyToStrided(leafBuf, leafIntermediate.chBlocks[2] ? *leafIntermediate.chBlocks[2] : zeroedBlock, compCount, 2);
                break;
            }
            default:
                assert(0 && "Unsupported grid voxel type");
            }
            return leaf;
        }
        static void CopyToStrided(float* dst, const ChannelBlockF16Mem& src, size_t componentCount, size_t componentIdx) noexcept
        {
            for (size_t i = 0; i < SPARSE_BLOCK_VOXEL_COUNT; ++i)
            {
                dst[i * componentCount + componentIdx] = Float16ToFloat32(src.mem[i]);
            }
        }
        static void ResolveChGridToGridDescItem(std::map<std::string, std::vector<VDBGridDescRef>>& chNameToGridDescs,
                                                const VDBGridDesc& gridDesc, uint32_t chSrcIdx) noexcept
        {
            if (!gridDesc.chSource[chSrcIdx])
                return;
            auto chIt = chNameToGridDescs.find(gridDesc.chSource[chSrcIdx]);
            if (chIt == chNameToGridDescs.end())
                chIt = chNameToGridDescs.insert({gridDesc.chSource[chSrcIdx], {}}).first;
            chIt->second.push_back(VDBGridDescRef{&gridDesc, chSrcIdx});
        }
        static openvdb::math::Transform::Ptr SanitizeTransform(const Math3D::Transform& inTransform) noexcept
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
                return openvdb::math::Transform::createLinearTransform();

            return openvdb::math::Transform::createLinearTransform(openvdb::Mat4d{inTransform.raw});
        }
    };
} // namespace Zibra::CE::Addons::OpenVDBUtils