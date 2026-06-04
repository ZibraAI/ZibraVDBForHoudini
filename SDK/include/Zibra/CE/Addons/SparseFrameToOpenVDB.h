#pragma once

#include <Zibra/CE/Decompression.h>
#include <algorithm>
#include <cstdint>
#include <execution>
#include <mutex>
#include <optional>
#include <vector>
#include <openvdb/tools/Dense.h>

namespace Zibra::CE::Addons::OpenVDBUtils
{
    struct FrameData
    {
        const void* decompressionPerChannelBlockData;
        const void* decompressionPerSpatialBlockInfo;
    };

    class SparseFrameToOpenVDB
    {
    private:
        using float16_mem = uint16_t;
        struct ChannelBlockF16Mem
        {
            float16_mem mem[SPARSE_BLOCK_VOXEL_COUNT];
        };
        struct LeafIntermediate
        {
            const ChannelBlockF16Mem* chBlocks[MAX_COMPONENTS_PER_CHANNEL] = {};
        };
        struct GridIntermediate
        {
            uint8_t componentCount = 1;
            Math::Transform transform{};
            std::map<openvdb::Coord, LeafIntermediate> leafs{};
        };

        FrameInfo m_FrameInfo{};
        std::vector<openvdb::GridBase::Ptr> m_Grids{};
    public:
        explicit SparseFrameToOpenVDB(const FrameInfo& fInfo) noexcept
            : m_FrameInfo(fInfo)
            , m_Grids(fInfo.channelsCount)
        {
        }

        void AddChunk(const FrameData& fData, size_t spatialBlocksCount, size_t chunkChBlocksFirstIndex) noexcept
        {
            using PackedSpatialBlock = Decompression::Shaders::PackedSpatialBlock;

            std::vector<GridIntermediate> gridsIntermediate;
            gridsIntermediate.reserve(m_FrameInfo.channelsCount);
            for (uint8_t chIdx = 0; chIdx < m_FrameInfo.channelsCount; ++chIdx)
            {
                const ChannelInfo& chInfo = m_FrameInfo.channels[chIdx];
                gridsIntermediate.emplace_back(GridIntermediate{chInfo.componentCount, chInfo.gridTransform, {}});
            }

            const auto* packedSpatialInfo = static_cast<const PackedSpatialBlock*>(fData.decompressionPerSpatialBlockInfo);
            const auto* channelBlocksSrc = static_cast<const ChannelBlockF16Mem*>(fData.decompressionPerChannelBlockData);

            for (size_t spatialIdx = 0; spatialIdx < spatialBlocksCount; ++spatialIdx)
            {
                const SpatialBlock currentSpatialBlock = Decompression::UnpackSpatialBlock(packedSpatialInfo[spatialIdx]);
                openvdb::Coord blockCoord{currentSpatialBlock.coords[0] + m_FrameInfo.aabb.minX,
                                                currentSpatialBlock.coords[1] + m_FrameInfo.aabb.minY,
                                                currentSpatialBlock.coords[2] + m_FrameInfo.aabb.minZ};

                size_t localChBlockIdx = 0;
                for (uint8_t chIdx = 0; chIdx < m_FrameInfo.channelsCount; ++chIdx)
                {
                    const ChannelInfo& chInfo = m_FrameInfo.channels[chIdx];

                    for (uint8_t i = 0; i < chInfo.componentCount; ++i)
                    {
                        const uint8_t componentIdx = chInfo.firstComponentIndex + i;
                        if (!(currentSpatialBlock.componentMask & (1u << componentIdx)))
                            continue;

                        LeafIntermediate& leaf = gridsIntermediate[chIdx].leafs[blockCoord];

                        const size_t chBlcIdx = currentSpatialBlock.channelBlocksOffset + localChBlockIdx - chunkChBlocksFirstIndex;
                        leaf.chBlocks[i] = &channelBlocksSrc[chBlcIdx];
                        ++localChBlockIdx;
                    }
                }
            }

            for (uint8_t chIdx = 0; chIdx < m_FrameInfo.channelsCount; ++chIdx)
            {
                const auto& grid = gridsIntermediate[chIdx];
                if (grid.leafs.empty())
                    continue;
                auto& outGrid = m_Grids[chIdx];
                switch (grid.componentCount)
                {
                case 1:
                    ConstructGrid<openvdb::FloatGrid>(grid, &outGrid);
                    break;
                case 3:
                    ConstructGrid<openvdb::Vec3fGrid>(grid, &outGrid);
                    break;
                default:
                    assert(0 && "Unsupported grid voxel type");
                }
            }
        }

        openvdb::GridPtrVec GetGrids() noexcept
        {
            openvdb::GridPtrVec result{};
            result.reserve(m_FrameInfo.channelsCount);

            for (uint8_t i = 0; i < m_FrameInfo.channelsCount; ++i)
            {
                openvdb::GridBase::Ptr grid = m_Grids[i];
                if (!grid)
                {
                    continue;
                }
                grid->setName(m_FrameInfo.channels[i].name);
                result.emplace_back(grid);
            }
            return result;
        }

    private:
        template <typename GridT>
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
                    LeafT* leaf = ConstructLeaf<LeafT>(leafIt.first, leafIt.second, gridIntermediate.componentCount);

                    std::lock_guard guard{gridAccessMutex};
                    inoutGridTyped->tree().addLeaf(leaf);
                });
            *inoutGrid = inoutGridTyped;
        }

        template <typename LeafT>
        LeafT* ConstructLeaf(const openvdb::Coord& leafCoord, const LeafIntermediate& leafIntermediate, uint8_t componentCount) noexcept
        {
            const openvdb::Coord blockMin = {leafCoord.x() * SPARSE_BLOCK_SIZE, leafCoord.y() * SPARSE_BLOCK_SIZE,
                                             leafCoord.z() * SPARSE_BLOCK_SIZE};
            auto* leaf = new LeafT{openvdb::PartialCreate{}, blockMin, {}, true};
            leaf->allocate();
            float* leafBuf = reinterpret_cast<float*>(leaf->buffer().data());

            static constexpr ChannelBlockF16Mem zeroedBlock = {};
            for (uint8_t i = 0; i < componentCount; ++i)
            {
                CopyToStrided(leafBuf, leafIntermediate.chBlocks[i] ? *leafIntermediate.chBlocks[i] : zeroedBlock, componentCount, i);
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

        static openvdb::math::Transform::Ptr SanitizeTransform(const Math::Transform& inTransform) noexcept
        {
            bool isEmpty = true;
            for (float value : inTransform.raw)
            {
                if (!Math::IsNearlyEqual(value, 0.0f))
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
