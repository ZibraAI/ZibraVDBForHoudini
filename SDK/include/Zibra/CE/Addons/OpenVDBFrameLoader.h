#pragma once

#include <Zibra/CE/Compression.h>
#include <execution>
#include <map>
#include <openvdb/openvdb.h>
#include <openvdb/tools/GridTransformer.h>

#include "OpenVDBCommon.h"

namespace Zibra::CE::Addons::OpenVDBUtils
{
    class FrameLoader
    {
        struct ChannelDescriptor
        {
            std::string name;
            ChannelMask channelMask;
            openvdb::GridBase::Ptr grid;
            uint32_t valueSize;
            uint32_t valueStride;
            uint32_t valueOffset;
        };
        struct ChannelBlockIntermediate
        {
            const void* data;
            uint32_t valueSize;
            uint32_t valueStride;
            uint32_t valueOffset;
        };
        struct SpatialBlockIntermediate
        {
            uint32_t destSpatialBlockIndex = 0;
            uint32_t destFirstChannelBlockIndex = 0;
            std::map<ChannelMask, ChannelBlockIntermediate> blocks{};
        };

    public:
        /**
         *
         * @param grids - grids to encode
         * @param gridsCount - number of entries in grids param
         * @param matchVoxelSize - gets one origin grid and resamples other to its voxel size. (Heavy operation)
         */
        explicit FrameLoader(openvdb::GridBase::ConstPtr* grids, size_t gridsCount, bool matchVoxelSize = false) noexcept;
        ~FrameLoader() noexcept;

        [[nodiscard]] Compression::SparseFrame* LoadFrame() const noexcept;
        [[nodiscard]] const std::vector<VDBGridDesc>& GetGridsShuffleInfo() noexcept;
        static void ReleaseFrame(const Compression::SparseFrame* frame) noexcept;

    private:
        /**
         * Iterates over input channel grid leafs, resolved offsets and adds transition data (SpatialBlockIntermediate) to spatialMap.
         * @tparam T - OpenVDB::BasicGrid subtype
         * @param ch - Source channel grid descriptor
         * @param spatialMap - out map SpatialBlockIntermediate descriptor will be stored in.
         * @return Total channel AABB
         */
        template <typename T>
        Math::AABB ResolveBlocks(const ChannelDescriptor& ch,
                                 std::map<openvdb::Coord, SpatialBlockIntermediate>& spatialMap) const noexcept;
        static Math::Transform OpenVDBTransformToMathTransform(const openvdb::math::Transform& transform) noexcept;
        static uint32_t FirstChannelIndexFromMask(ChannelMask mask) noexcept;
        static std::vector<ChannelDescriptor> ChannelsFromGrid(openvdb::GridBase::Ptr grid, uint32_t voxelComponentCount,
                                                               uint32_t voxelComponentSize, ChannelMask firstChMask) noexcept;
        static Math::AABB CalculateAABB(const openvdb::CoordBBox& bbox);
        static void PackFromStride(void* dst, const void* src, size_t stride, size_t offset, size_t size, size_t count) noexcept;
        static std::string ValueComponentIndexToLetter(uint32_t valueComponentIdx) noexcept;
        static std::string SplitGridNameFromValueComponentIdx(const std::string gridName, uint32_t valueComponentIdx);
        static float GetUniformVoxelScale(const openvdb::GridBase::ConstPtr& grid);

        /** Calculate transformation to origin grid's index space.
         * T^B_O = T^B_W * T^W_O  -- B - Base, O - Origin, W - World.
         *
         * Also, for optimization reasons we assume that rotation matrix of base and origin grid within 1 frame is the same.
         * Therefore, transformation matrix calculated in this method will be ScaleTranslation matrix (or close to that due to floating
         * point errors). This assumption later allows to cut off drastic amount of multiplications.
         *
         * @param targetGrid - base grid transform will be applied to
         * @param referenceGrid - reference grid (transformation destination)
         * @return - T^B_O - transformation from targetGrid to referenceGrid index space.
         */
        static openvdb::math::Transform GetIndexSpaceRelativeTransform(const openvdb::GridBase::ConstPtr& targetGrid,
                                                                       const openvdb::GridBase::ConstPtr& referenceGrid) noexcept;

    private:
        std::vector<ChannelDescriptor> m_Channels{};
        std::vector<VDBGridDesc> m_GridsShuffle{};
    };

    template <typename T>
    Math::AABB FrameLoader::ResolveBlocks(const ChannelDescriptor& ch, std::map<openvdb::Coord,
                                          SpatialBlockIntermediate>& spatialMap) const noexcept
    {
        auto grid = openvdb::gridPtrCast<T>(ch.grid);

        Math::AABB totalAABB = {};
        for (auto leafIt = grid->tree().cbeginLeaf(); leafIt; ++leafIt)
        {
            const auto leaf = leafIt.getLeaf();
            const Math::AABB leafAABB = CalculateAABB(leaf->getNodeBoundingBox());
            totalAABB = totalAABB | leafAABB;
            openvdb::Coord origin = openvdb::Coord(leafAABB.minX, leafAABB.minY, leafAABB.minZ);

            auto slbIt = spatialMap.find(origin);
            SpatialBlockIntermediate newBlock{static_cast<uint32_t>(spatialMap.size()), 0, {}};
            SpatialBlockIntermediate& localSpatialBlock = slbIt == spatialMap.end() ? newBlock : slbIt->second;

            ChannelBlockIntermediate localChannelBlock{};
            localChannelBlock.data = leaf->buffer().data();
            localChannelBlock.valueSize = ch.valueSize;
            localChannelBlock.valueStride = ch.valueStride;
            localChannelBlock.valueOffset = ch.valueOffset;
            localSpatialBlock.blocks[ch.channelMask] = localChannelBlock;

            spatialMap[origin] = localSpatialBlock;
        }
        return totalAABB;
    }

    inline openvdb::math::Transform FrameLoader::GetIndexSpaceRelativeTransform(const openvdb::GridBase::ConstPtr& targetGrid,
                                                                                const openvdb::GridBase::ConstPtr& referenceGrid) noexcept
    {
        openvdb::math::Transform result{targetGrid->transform().baseMap()->copy()};
        result.postMult(referenceGrid->transform().baseMap()->getAffineMap()->getMat4().inverse());
        return result;
    }

    inline float FrameLoader::GetUniformVoxelScale(const openvdb::GridBase::ConstPtr& grid)
    {
        const openvdb::Vec3f voxelSize{grid->voxelSize()};
        assert(grid->hasUniformVoxels());
        return voxelSize.x();
    }

    inline std::string FrameLoader::SplitGridNameFromValueComponentIdx(const std::string gridName, uint32_t valueComponentIdx)
    {
        return gridName + "." + ValueComponentIndexToLetter(valueComponentIdx);
    }

    inline std::string FrameLoader::ValueComponentIndexToLetter(uint32_t valueComponentIdx) noexcept
    {
        switch (valueComponentIdx)
        {
        case 0:
            return "x";
        case 1:
            return "y";
        case 2:
            return "z";
        case 3:
            return "w";
        default:
            return "c" + std::to_string(valueComponentIdx);
        }
    }

    inline void FrameLoader::PackFromStride(void* dst, const void* src, size_t stride, size_t offset, size_t size, size_t count) noexcept
    {
        if (stride == size && offset == 0)
        {
            memcpy(dst, src, count * size);
        }
        else
        {
            auto* dstBytes = static_cast<uint8_t*>(dst);
            auto* srcBytes = static_cast<const uint8_t*>(src);
            for (size_t i = 0; i < count; ++i)
            {
                memcpy(dstBytes + size * i, srcBytes + stride * i + offset, size);
            }
        }
    }

    inline Math::AABB FrameLoader::CalculateAABB(const openvdb::CoordBBox& bbox)
    {
        Math::AABB result{};

        const openvdb::math::Vec3d transformedBBoxMin = bbox.min().asVec3d();
        const openvdb::math::Vec3d transformedBBoxMax = bbox.max().asVec3d();

        result.minX = FloorToBlockSize(Math::FloorWithEpsilon(transformedBBoxMin.x())) / SPARSE_BLOCK_SIZE;
        result.minY = FloorToBlockSize(Math::FloorWithEpsilon(transformedBBoxMin.y())) / SPARSE_BLOCK_SIZE;
        result.minZ = FloorToBlockSize(Math::FloorWithEpsilon(transformedBBoxMin.z())) / SPARSE_BLOCK_SIZE;

        result.maxX = CeilToBlockSize(Math::CeilWithEpsilon(transformedBBoxMax.x())) / SPARSE_BLOCK_SIZE;
        result.maxY = CeilToBlockSize(Math::CeilWithEpsilon(transformedBBoxMax.y())) / SPARSE_BLOCK_SIZE;
        result.maxZ = CeilToBlockSize(Math::CeilWithEpsilon(transformedBBoxMax.z())) / SPARSE_BLOCK_SIZE;

        return result;
    }

    inline std::vector<FrameLoader::ChannelDescriptor> FrameLoader::ChannelsFromGrid(openvdb::GridBase::Ptr grid,
                                                                                     uint32_t voxelComponentCount,
                                                                                     uint32_t voxelComponentSize,
                                                                                     ChannelMask firstChMask) noexcept
    {
        std::vector<ChannelDescriptor> result{};
        for (size_t chIdx = 0; chIdx < voxelComponentCount; ++chIdx)
        {
            ChannelDescriptor chDesc{};
            chDesc.name = voxelComponentCount > 1 ? SplitGridNameFromValueComponentIdx(grid->getName(), chIdx) : grid->getName();
            chDesc.grid = grid;
            chDesc.channelMask = firstChMask << chIdx;
            chDesc.valueOffset = chIdx * voxelComponentSize;
            chDesc.valueSize = voxelComponentSize;
            chDesc.valueStride = voxelComponentSize * voxelComponentCount;
            result.emplace_back(chDesc);
        }
        return result;
    }

    inline uint32_t FrameLoader::FirstChannelIndexFromMask(ChannelMask mask) noexcept
    {
        for (size_t i = 0; i < sizeof(mask) * 8; ++i)
        {
            if (mask & (1 << i))
            {
                return i;
            }
        }
        return 0;
    }

    inline Math::Transform FrameLoader::OpenVDBTransformToMathTransform(
        const openvdb::math::Transform& transform) noexcept
    {
        Math::Transform result{};

        const openvdb::math::Mat4 map = transform.baseMap()->getAffineMap()->getMat4();
        for (int i = 0; i < 16; ++i)
        {
            // Flat indexing to 2D index conversion.
            result.raw[i] = float(map(i >> 2, i & 3));
        }

        return result;
    }

    inline void FrameLoader::ReleaseFrame(const Compression::SparseFrame* frame) noexcept
    {
        if (frame)
        {
            for (size_t i = 0; i < frame->info.channelsCount; ++i)
            {
                delete[] frame->info.channels[i].name;
            }
            delete[] frame->channelIndexPerBlock;
            delete[] frame->blocks;
            delete[] frame->spatialInfo;
            delete frame;
        }
    }

    inline const std::vector<VDBGridDesc>& FrameLoader::GetGridsShuffleInfo() noexcept
    {
        return m_GridsShuffle;
    }

    inline Compression::SparseFrame* FrameLoader::LoadFrame() const noexcept
    {
        auto result = new Compression::SparseFrame{};
        std::map<openvdb::Coord, SpatialBlockIntermediate> spatialBlocks{};
        std::vector<Math::AABB> orderedChannelAABBs{};
        Math::AABB totalAABB = {};

        // Resolving leaf data to spatial descriptors structure for future concurrent processing.
        for (size_t i = 0; i < m_Channels.size(); ++i)
        {
            const ChannelDescriptor& channel = m_Channels[i];
            if (channel.grid->baseTree().isType<openvdb::Vec3STree>())
            {
                Math::AABB channelAABB = ResolveBlocks<openvdb::Vec3fGrid>(channel, spatialBlocks);
                orderedChannelAABBs.emplace_back(channelAABB);
                totalAABB = totalAABB | channelAABB;
            }
            else if (channel.grid->baseTree().isType<openvdb::FloatTree>())
            {
                Math::AABB channelAABB = ResolveBlocks<openvdb::FloatGrid>(channel, spatialBlocks);
                orderedChannelAABBs.emplace_back(channelAABB);
                totalAABB = totalAABB | channelAABB;
            }
            else
            {
                assert(0 && "Unsupported grid type. Loader supports only floating point grids.");
                return nullptr;
            }
        }

        // Iterating over resolved spatial descriptors and precalculating voxel data destination memory offset
        std::vector<SpatialBlockIntermediate*> orderedSpatialBlockIntermediates{};
        orderedSpatialBlockIntermediates.resize(spatialBlocks.size());
        for (auto& [coord, spatialBlock] : spatialBlocks)
        {
            orderedSpatialBlockIntermediates[spatialBlock.destSpatialBlockIndex] = &spatialBlock;
        }
        uint32_t channelBlockAccumulator = 0;
        for (auto& spatialBlock : orderedSpatialBlockIntermediates)
        {
            spatialBlock->destFirstChannelBlockIndex = channelBlockAccumulator;
            channelBlockAccumulator += spatialBlock->blocks.size();
        }

        result->info.channelBlockCount = channelBlockAccumulator;
        result->info.spatialBlockCount = spatialBlocks.size();
        result->info.channelsCount = m_Channels.size();
        result->info.aabb = totalAABB;

        // Allocating result frame buffers from precalculated data
        auto* resultBlocks = new ChannelBlock[result->info.channelBlockCount];
        auto* resultChannelIndexPerBlock = new uint32_t[result->info.channelBlockCount];
        auto* resultSpatialInfo = new SpatialBlockInfo[result->info.spatialBlockCount];
        auto* channels = result->info.channels;

        result->blocks = resultBlocks;
        result->spatialInfo = resultSpatialInfo;
        result->channelIndexPerBlock = resultChannelIndexPerBlock;

        // Preparing channel info. Filling known data and setting edge initial values for future statistics calculation.
        for (size_t i = 0; i < m_Channels.size(); ++i)
        {
            auto chName = new char[m_Channels[i].name.length() + 1];
            strcpy(chName, m_Channels[i].name.c_str());
            channels[i].name = chName;
            channels[i].aabb = orderedChannelAABBs[i];
            channels[i].gridTransform = OpenVDBTransformToMathTransform(m_Channels[i].grid->constTransform());
            channels[i].voxelStatistics.minValue = std::numeric_limits<float>::max();
            channels[i].voxelStatistics.maxValue = std::numeric_limits<float>::min();
        }

        // Concurrently moving voxel data from leafs to destination ChannelBlock array using precalculated offsets and other data
        // stored in spatial descriptors + calculating voxel statistics per block.
        std::vector<ChannelVoxelStatistics> perBlockStatistics{};
        perBlockStatistics.resize(result->info.channelBlockCount);
        std::for_each(
#if !ZIB_TARGET_OS_MAC
            std::execution::par_unseq,
#endif
            spatialBlocks.begin(), spatialBlocks.end(), [&](const std::pair<openvdb::Coord, SpatialBlockIntermediate>& item) {
                auto& [coord, spatialIntrm] = item;
                ChannelMask mask = 0x0;

                // Iterating channels in order of increasing mask.
                size_t chIdx = 0;
                for (auto& [chMask, chBlockIntrm] : spatialIntrm.blocks)
                {
                    uint32_t channelBlockIndex = spatialIntrm.destFirstChannelBlockIndex + chIdx;
                    ChannelBlock& outBlock = resultBlocks[channelBlockIndex];
                    mask |= chMask;
                    resultChannelIndexPerBlock[channelBlockIndex] = FirstChannelIndexFromMask(chMask);
                    PackFromStride(&outBlock, chBlockIntrm.data, chBlockIntrm.valueStride, chBlockIntrm.valueOffset, chBlockIntrm.valueSize,
                                   SPARSE_BLOCK_VOXEL_COUNT);

                    ChannelVoxelStatistics& dstStatistics = perBlockStatistics[channelBlockIndex];
                    dstStatistics.minValue = std::numeric_limits<float>::max();
                    dstStatistics.maxValue = std::numeric_limits<float>::min();
                    for (float voxel : outBlock.voxels)
                    {
                        dstStatistics.minValue = std::min(dstStatistics.minValue, voxel);
                        dstStatistics.maxValue = std::max(dstStatistics.maxValue, voxel);
                        dstStatistics.meanPositiveValue += voxel > 0.0f ? voxel : 0.0f;
                        dstStatistics.meanNegativeValue += voxel < 0.0f ? voxel : 0.0f;
                    }
                    dstStatistics.meanPositiveValue /= static_cast<float>(SPARSE_BLOCK_VOXEL_COUNT);
                    dstStatistics.meanNegativeValue /= static_cast<float>(SPARSE_BLOCK_VOXEL_COUNT);

                    ++chIdx;
                }

                SpatialBlockInfo spatialInfo{};
                spatialInfo.coords[0] = coord.x() - totalAABB.minX;
                spatialInfo.coords[1] = coord.y() - totalAABB.minY;
                spatialInfo.coords[2] = coord.z() - totalAABB.minZ;
                spatialInfo.channelMask = mask;
                spatialInfo.channelCount = spatialIntrm.blocks.size();
                spatialInfo.channelBlocksOffset = spatialIntrm.destFirstChannelBlockIndex;
                resultSpatialInfo[spatialIntrm.destSpatialBlockIndex] = spatialInfo;
            });

        // Resolving concurrently calculated per block voxel statistics to general frame per channel voxel statistics.
        for (size_t i = 0; i < perBlockStatistics.size(); ++i)
        {
            const auto channelIndex = result->channelIndexPerBlock[i];
            auto& dstStatistics = channels[channelIndex].voxelStatistics;
            dstStatistics.minValue = std::min(dstStatistics.minValue, perBlockStatistics[i].minValue);
            dstStatistics.maxValue = std::max(dstStatistics.maxValue, perBlockStatistics[i].maxValue);
            dstStatistics.meanPositiveValue += perBlockStatistics[i].meanPositiveValue;
            dstStatistics.meanNegativeValue += perBlockStatistics[i].meanNegativeValue;
            // Writing blocks count to voxelCount to use it in the future.
            dstStatistics.voxelCount += 1;
        }
        for (size_t i = 0; i < result->info.channelsCount; ++i)
        {
            if (channels[i].voxelStatistics.voxelCount == 0)
            {
                channels[i].voxelStatistics.minValue = 0;
                channels[i].voxelStatistics.maxValue = 0;
            }
            else
            {
                channels[i].voxelStatistics.meanPositiveValue /= static_cast<float>(channels[i].voxelStatistics.voxelCount);
                channels[i].voxelStatistics.meanNegativeValue /= static_cast<float>(channels[i].voxelStatistics.voxelCount);
                // Previous cycle has written blocks per channel count to voxelCount field.
                channels[i].voxelStatistics.voxelCount *= SPARSE_BLOCK_VOXEL_COUNT;
            }
        }

        return result;
    }

    inline FrameLoader::~FrameLoader() noexcept
    {
        for (auto& shuffleItem : m_GridsShuffle)
        {
            delete[] shuffleItem.gridName;
            for (size_t i = 0; i < std::size(shuffleItem.chSource); ++i)
            {
                delete[] shuffleItem.chSource[i];
            }
        }
    }

    inline FrameLoader::FrameLoader(openvdb::GridBase::ConstPtr* grids, size_t gridsCount,
                                                                     bool matchVoxelSize /*= false*/) noexcept
    {
        if (!gridsCount)
            return;

        m_Channels.reserve(gridsCount * 4);
        m_GridsShuffle.reserve(gridsCount);

        // Selecting orign grid for resampling
        openvdb::GridBase::ConstPtr originGrid = grids[0];
        if (matchVoxelSize)
        {
            auto minVoxelScale = std::numeric_limits<float>::max();
            for (size_t i = 0; i < gridsCount; ++i)
            {
                const float voxelScale = GetUniformVoxelScale(grids[i]);
                if (voxelScale < minVoxelScale)
                {
                    minVoxelScale = voxelScale;
                    originGrid = grids[i];
                }
            }
        }

        // Creating mutable grid copies for future voxelization. Resample if matchVoxelSize is enabled.
        std::vector<openvdb::GridBase::Ptr> processedGrids{};
        processedGrids.resize(gridsCount);
        std::transform(
#if !ZIB_TARGET_OS_MAC
            std::execution::par_unseq,
#endif
            grids, grids + gridsCount, processedGrids.begin(), [&](const auto& grid) {
                const openvdb::math::Transform relativeTransform = GetIndexSpaceRelativeTransform(grid, originGrid);
                openvdb::tools::GridTransformer transformer{relativeTransform.baseMap()->getAffineMap()->getMat4()};

                openvdb::GridBase::Ptr mutableCopy = grid->deepCopyGrid();
                if (mutableCopy->baseTree().isType<openvdb::Vec3STree>())
                {
                    const auto src = openvdb::gridConstPtrCast<openvdb::Vec3SGrid>(grid);
                    const auto dst = openvdb::gridPtrCast<openvdb::Vec3SGrid>(mutableCopy);
                    dst->tree().voxelizeActiveTiles();
                    if (matchVoxelSize && !relativeTransform.isIdentity())
                    {
                        dst->clear();
                        transformer.transformGrid<openvdb::tools::BoxSampler>(*src, *dst);
                        dst->setTransform(originGrid->transform().copy());
                    }
                }
                else if (mutableCopy->baseTree().isType<openvdb::FloatTree>())
                {
                    const auto src = openvdb::gridConstPtrCast<openvdb::FloatGrid>(grid);
                    const auto dst = openvdb::gridPtrCast<openvdb::FloatGrid>(mutableCopy);
                    dst->tree().voxelizeActiveTiles();
                    if (matchVoxelSize && !relativeTransform.isIdentity())
                    {
                        dst->clear();
                        transformer.transformGrid<openvdb::tools::BoxSampler>(*src, *dst);
                        dst->setTransform(originGrid->transform().copy());
                    }
                }
                else
                {
                    assert(0 && "Unsupported grid type. Loader supports only floating point grids.");
                }
                return mutableCopy;
            });

        // Splitting vector grids to separate scalar channels + constructing channels unshuffle structure
        ChannelMask mask = 0x1;
        for (size_t i = 0; i < gridsCount; ++i)
        {
            VDBGridDesc shuffleGridInfo{};
            std::vector<ChannelDescriptor> channels;
            if (processedGrids[i]->baseTree().isType<openvdb::Vec3STree>())
            {
                shuffleGridInfo.voxelType = GridVoxelType::Float3;
                channels = ChannelsFromGrid(processedGrids[i], 3, sizeof(float), mask);
            }
            else if (processedGrids[i]->baseTree().isType<openvdb::FloatTree>())
            {
                shuffleGridInfo.voxelType = GridVoxelType::Float1;
                channels = ChannelsFromGrid(processedGrids[i], 1, sizeof(float), mask);
            }
            else
            {
                assert(0 && "Unsupported grid type. Loader supports only floating point grids.");
                m_Channels.clear();
                return;
            }

            const auto& gridName = processedGrids[i]->getName();
            shuffleGridInfo.gridName = new char[gridName.length() + 1];
            strcpy(const_cast<char*>(shuffleGridInfo.gridName), gridName.c_str());

            for (size_t chIdx = 0; chIdx < std::min(channels.size(), std::size(shuffleGridInfo.chSource)); ++chIdx)
            {
                shuffleGridInfo.chSource[chIdx] = new char[channels[chIdx].name.length() + 1];
                strcpy(const_cast<char*>(shuffleGridInfo.chSource[chIdx]), channels[chIdx].name.c_str());
            }

            mask = mask << channels.size();
            m_Channels.insert(m_Channels.end(), channels.begin(), channels.end());
            m_GridsShuffle.push_back(shuffleGridInfo);
        }
    }

} // namespace Zibra::CE::Addons::OpenVDBUtils
