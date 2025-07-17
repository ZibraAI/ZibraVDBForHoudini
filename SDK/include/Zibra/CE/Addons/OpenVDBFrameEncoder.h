#pragma once

#include <algorithm>
#include <cstdint>
#include <execution>
#include <Zibra/CE/Decompression.h>
#include <openvdb/tools/Dense.h>

#include "OpenVDBCommon.h"

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
        explicit FrameEncoder(const VDBGridDesc* gridsDescs, size_t gridsCount, const Decompression::FrameInfo& fInfo) noexcept
            : m_FrameInfo(fInfo)
        {
            m_GridDescs.insert(m_GridDescs.begin(), gridsDescs, gridsDescs + gridsCount);

            for (size_t i = 0; i < m_GridDescs.size(); ++i)
            {
                const auto& gridDesc = m_GridDescs[i];
                switch (gridDesc.voxelType)
                {
                case GridVoxelType::Float1:
                    ResolveChGridToGridDescItem(m_ChNameToGridDescs, gridDesc, 0);
                    break;
                case GridVoxelType::Float3:
                    ResolveChGridToGridDescItem(m_ChNameToGridDescs, gridDesc, 0);
                    ResolveChGridToGridDescItem(m_ChNameToGridDescs, gridDesc, 1);
                    ResolveChGridToGridDescItem(m_ChNameToGridDescs, gridDesc, 2);
                    break;
                default:
                    assert(0 && "Unsupported grid voxel type");
                }
            }

            for (size_t i = 0; i < m_FrameInfo.channelsCount; ++i)
            {
                m_ChNameToChInfo[m_FrameInfo.channels[i].name] = &m_FrameInfo.channels[i];
            }
        }

        void EncodeChunk(const FrameData& fData, size_t spatialBlocksCount, size_t chunkChBlocksFirstIndex) noexcept
        {
            using PackedSpatialBlockInfo = Decompression::Shaders::PackedSpatialBlockInfo;

            std::map<std::string, GridIntermediate> gridsIntermediate{};
            const auto* packedSpatialInfo = static_cast<const PackedSpatialBlockInfo*>(fData.decompressionPerSpatialBlockInfo);
            const auto* channelBlocksSrc = static_cast<const ChannelBlockF16Mem*>(fData.decompressionPerChannelBlockData);

            for (size_t spatialIdx = 0; spatialIdx < spatialBlocksCount; ++spatialIdx)
            {
                size_t localChannelBlockIdx = 0;
                for (size_t i = 0; i < MAX_CHANNEL_COUNT; ++i)
                {
                    const auto& curSpatialInfo = Decompression::UnpackPackedSpatialBlockInfo(packedSpatialInfo[spatialIdx]);
                    openvdb::Coord blockCoord{curSpatialInfo.coords[0] + m_FrameInfo.aabb.minX,
                                              curSpatialInfo.coords[1] + m_FrameInfo.aabb.minY,
                                              curSpatialInfo.coords[2] + m_FrameInfo.aabb.minZ};
                    if (curSpatialInfo.channelMask & (1 << i))
                    {
                        const char* chName = m_FrameInfo.channels[i].name;

                        auto gridRefIt = m_ChNameToGridDescs.find(chName);
                        if (gridRefIt != m_ChNameToGridDescs.end())
                        {
                            const Decompression::ChannelInfo& channelInfo = *m_ChNameToChInfo[chName];
                            for (const VDBGridDescRef& gridRef : gridRefIt->second)
                            {
                                const char* targetGridName = gridRef.desc->gridName;

                                // Find grid intermediate by name or create empty if it is absent
                                const auto gridIntermediateToCreate = GridIntermediate{gridRef.desc->voxelType, channelInfo.gridTransform, {}};
                                auto gridIntermediateIt = gridsIntermediate.find(targetGridName);
                                if (gridIntermediateIt == gridsIntermediate.end())
                                    gridIntermediateIt = gridsIntermediate.insert({targetGridName, gridIntermediateToCreate}).first;

                                // Find leaf intermediate in grid intermediate or create if it is absent
                                auto leafIntermediateMapIt = gridIntermediateIt->second.leafs.find(blockCoord);
                                if (leafIntermediateMapIt == gridIntermediateIt->second.leafs.end())
                                    leafIntermediateMapIt = gridIntermediateIt->second.leafs.insert({blockCoord, {}}).first;

                                const size_t chBlcIdx = curSpatialInfo.channelBlocksOffset + localChannelBlockIdx - chunkChBlocksFirstIndex;
                                leafIntermediateMapIt->second.chBlocks[gridRef.chIdx] = &channelBlocksSrc[chBlcIdx];
                            }
                            ++localChannelBlockIdx;
                        }
                    }
                }
            }

            std::for_each(
                #if !ZIB_TARGET_OS_MAC
                std::execution::par_unseq, 
                #endif
                gridsIntermediate.begin(), gridsIntermediate.end(), [&](auto& gridIt) {
                auto outGridIt = m_Grids.find(gridIt.first);
                if (outGridIt == m_Grids.end())
                    outGridIt = m_Grids.insert({gridIt.first, nullptr}).first;

                switch (gridIt.second.voxelType)
                {
                case GridVoxelType::Float1: {
                    ConstructGrid<openvdb::FloatGrid>(gridIt.second, &outGridIt->second);
                    break;
                }
                case GridVoxelType::Float3: {
                    ConstructGrid<openvdb::Vec3fGrid>(gridIt.second, &outGridIt->second);
                    break;
                }
                default:
                    assert(0 && "Unsupported grid voxel type");
                }
            });
        }

        openvdb::GridPtrVec GetGrids() noexcept
        {
            openvdb::GridPtrVec result{};
            result.reserve(m_Grids.size());
            for (const auto& [gridName, grid] : m_Grids)
            {
                grid->setName(gridName);
                result.emplace_back(grid);
            }
            return result;
        }
    private:
        template<typename GridT>
        void ConstructGrid(const GridIntermediate& gridIntermediate, openvdb::GridBase::Ptr* inoutGrid) noexcept
        {
            auto inoutGridTyped = *inoutGrid ? openvdb::gridPtrCast<GridT>(*inoutGrid) : GridT::create();
            if (!*inoutGrid)
            {
                inoutGridTyped->setTransform(SanitizeTransform(gridIntermediate.transform));
            }

            std::mutex gridAccessMutex{};
            const auto& leafIntermediates = gridIntermediate.leafs;
            std::for_each(
                #if !ZIB_TARGET_OS_MAC
                std::execution::par_unseq, 
                #endif
                leafIntermediates.begin(), leafIntermediates.end(), [&](auto leafIt) {
                using TreeT = typename GridT::TreeType;
                using LeafT = typename TreeT::LeafNodeType;
                LeafT* leaf = ConstructLeaf<LeafT>(leafIt.first, leafIt.second, gridIntermediate.voxelType);

                std::lock_guard guard{gridAccessMutex};
                inoutGridTyped->tree().addLeaf(leaf);
            });
            *inoutGrid = inoutGridTyped;
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
    private:
        std::vector<VDBGridDesc> m_GridDescs{};
        Decompression::FrameInfo m_FrameInfo{};
        std::map<std::string, std::vector<VDBGridDescRef>> m_ChNameToGridDescs{};
        std::map<std::string, const Decompression::ChannelInfo*> m_ChNameToChInfo{};

        std::map<std::string, openvdb::GridBase::Ptr> m_Grids{};
    };
} // namespace Zibra::CE::Addons::OpenVDBUtils
