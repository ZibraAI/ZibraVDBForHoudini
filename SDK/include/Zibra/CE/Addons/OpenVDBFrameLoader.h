#pragma once

#include <execution>
#include <map>
#include <Zibra/CE/Compression.h>
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
        explicit FrameLoader(openvdb::GridBase::ConstPtr* grids, size_t gridsCount, bool matchVoxelSize = false) noexcept
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

        ~FrameLoader() noexcept
        {
            for (auto& shuffleItem : m_GridsShuffle)
            {
                delete [] shuffleItem.gridName;
                for (size_t i = 0; i < std::size(shuffleItem.chSource); ++i)
                {
                    delete [] shuffleItem.chSource[i];
                }
            }
        }

        [[nodiscard]] Compression::SparseFrame* LoadFrame(EncodingMetadata* encodingMetadata = nullptr) const noexcept
        {
            auto result = new Compression::SparseFrame{};
            std::map<openvdb::Coord, SpatialBlockIntermediate> spatialBlocks{};
            Math3D::AABB totalAABB = {};

            // Resolving leaf data to spatial descriptors structure for future concurrent processing.
            for (size_t i = 0; i < m_Channels.size(); ++i)
            {
                const ChannelDescriptor& channel = m_Channels[i];
                if (channel.grid->baseTree().isType<openvdb::Vec3STree>())
                {
                    totalAABB = totalAABB | ResolveBlocks<openvdb::Vec3fGrid>(channel, spatialBlocks);
                }
                else if (channel.grid->baseTree().isType<openvdb::FloatTree>())
                {
                    totalAABB = totalAABB | ResolveBlocks<openvdb::FloatGrid>(channel, spatialBlocks);
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

            result->blocksCount = channelBlockAccumulator;
            result->spatialInfoCount = spatialBlocks.size();
            result->orderedChannelsCount = m_Channels.size();

            // Allocating result frame buffers from precalculated data
            auto* resultBlocks = new ChannelBlock[result->blocksCount];
            auto* resultChannelIndexPerBlock = new uint32_t[result->blocksCount];
            auto* resultSpatialInfo = new SpatialBlockInfo[result->spatialInfoCount];
            auto* orderedChannels = new Compression::ChannelInfo[result->orderedChannelsCount];

            result->blocks = resultBlocks;
            result->spatialInfo = resultSpatialInfo;
            result->orderedChannels = orderedChannels;
            result->channelIndexPerBlock = resultChannelIndexPerBlock;

            // Preparing channel info. Filling known data and setting edge initial values for future statistics calculation.
            for (size_t i = 0; i < m_Channels.size(); ++i)
            {
                auto chName = new char[m_Channels[i].name.length() + 1];
                strcpy(chName, m_Channels[i].name.c_str());
                orderedChannels[i].name = chName;
                orderedChannels[i].statistics.minValue = std::numeric_limits<float>::max();
                orderedChannels[i].statistics.maxValue = std::numeric_limits<float>::min();

                auto posVoxelSpaceCompensation = openvdb::math::Vec3d(totalAABB.minX, totalAABB.minY, totalAABB.minZ) * SPARSE_BLOCK_SIZE;
                auto translatedTransform = TranslateOpenVDBTransform(m_Channels[i].grid->constTransform(), posVoxelSpaceCompensation);
                orderedChannels[i].gridTransform = OpenVDBTransformToMath3DTransform(translatedTransform);
            }

            // Concurrently moving voxel data from leafs to destination ChannelBlock array using precalculated offsets and other data
            // stored in spatial descriptors + calculating voxel statistics per block.
            std::vector<Compression::VoxelStatistics> perBlockStatistics{};
            perBlockStatistics.resize(result->blocksCount);
            std::for_each(
                #if !ZIB_TARGET_OS_MAC
                std::execution::par_unseq, 
                #endif
                spatialBlocks.begin(), spatialBlocks.end(), [&](const std::pair<openvdb::Coord, SpatialBlockIntermediate>& item){
                auto& [coord, spatialIntrm] = item;
                ChannelMask mask = 0x0;

                // Channels are ordered right way due to map soring and mask bit order.
                size_t chIdx = 0;
                for (auto& [chMask, chBlockIntrm] : spatialIntrm.blocks) {
                    uint32_t channelBlockIndex = spatialIntrm.destFirstChannelBlockIndex + chIdx;
                    ChannelBlock& outBlock = resultBlocks[channelBlockIndex];
                    mask |= chMask;
                    resultChannelIndexPerBlock[channelBlockIndex] = FirstChannelIndexFromMask(chMask);
                    PackFromStride(&outBlock, chBlockIntrm.data, chBlockIntrm.valueStride, chBlockIntrm.valueOffset, chBlockIntrm.valueSize,
                                   SPARSE_BLOCK_VOXEL_COUNT);

                    Compression::VoxelStatistics& dstStatistics = perBlockStatistics[channelBlockIndex];
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
                auto& dstStatistics = orderedChannels[channelIndex].statistics;
                dstStatistics.minValue = std::min(dstStatistics.minValue, perBlockStatistics[i].minValue);
                dstStatistics.maxValue = std::max(dstStatistics.maxValue, perBlockStatistics[i].maxValue);
                dstStatistics.meanPositiveValue += perBlockStatistics[i].meanPositiveValue;
                dstStatistics.meanNegativeValue += perBlockStatistics[i].meanNegativeValue;
                // Writing blocks count to voxelCount to use it in the future.
                dstStatistics.voxelCount += 1;
            }
            for (size_t i = 0; i < result->orderedChannelsCount; ++i)
            {
                if (orderedChannels[i].statistics.voxelCount == 0)
                {
                    orderedChannels[i].statistics.minValue = 0;
                    orderedChannels[i].statistics.maxValue = 0;
                }
                else
                {
                    orderedChannels[i].statistics.meanPositiveValue /= static_cast<float>(orderedChannels[i].statistics.voxelCount);
                    orderedChannels[i].statistics.meanNegativeValue /= static_cast<float>(orderedChannels[i].statistics.voxelCount);
                    // Previous cycle has written blocks per channel count to voxelCount field.
                    orderedChannels[i].statistics.voxelCount *= SPARSE_BLOCK_VOXEL_COUNT;
                }
            }

            if (encodingMetadata != nullptr)
            {
                *encodingMetadata = {};
                encodingMetadata->offsetX = totalAABB.minX * SPARSE_BLOCK_SIZE;
                encodingMetadata->offsetY = totalAABB.minY * SPARSE_BLOCK_SIZE;
                encodingMetadata->offsetZ = totalAABB.minZ * SPARSE_BLOCK_SIZE;
            }

            totalAABB.maxX -= totalAABB.minX;
            totalAABB.maxY -= totalAABB.minY;
            totalAABB.maxZ -= totalAABB.minZ;
            totalAABB.minX = 0.0f;
            totalAABB.minY = 0.0f;
            totalAABB.minZ = 0.0f;

            result->aabb = totalAABB;

            return result;
        }

        const std::vector<VDBGridDesc>& GetGridsShuffleInfo() noexcept
        {
            return m_GridsShuffle;
        }

        void ReleaseFrame(const Compression::SparseFrame* frame) const noexcept
        {
            if (frame)
            {
                for (size_t i = 0; i < frame->orderedChannelsCount; ++i)
                {
                    delete[] frame->orderedChannels[i].name;
                }
                delete[] frame->orderedChannels;
                delete[] frame->channelIndexPerBlock;
                delete[] frame->blocks;
                delete[] frame->spatialInfo;
                delete frame;
            }
        }
    private:
        /**
         * Iterates over input channel grid leafs, resolved offsets and adds transition data (SpatialBlockIntermediate) to spatialMap.
         * @tparam T - OpenVDB::BasicGrid subtype
         * @param ch - Source channel grid descriptor
         * @param spatialMap - out map SpatialBlockIntermediate descriptor will be stored in.
         * @return Total channel AABB
         */
        template <typename T>
        Math3D::AABB ResolveBlocks(const ChannelDescriptor& ch, std::map<openvdb::Coord, SpatialBlockIntermediate>& spatialMap) const noexcept
        {
            auto grid = openvdb::gridPtrCast<T>(ch.grid);

            Math3D::AABB totalAABB = {};
            for (auto leafIt = grid->tree().cbeginLeaf(); leafIt; ++leafIt)
            {
                const auto leaf = leafIt.getLeaf();
                const Math3D::AABB leafAABB = CalculateAABB(leaf->getNodeBoundingBox());
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

        static Math3D::Transform OpenVDBTransformToMath3DTransform(const openvdb::math::Transform& transform) noexcept
        {
            Math3D::Transform result{};

            const openvdb::math::Mat4 map = transform.baseMap()->getAffineMap()->getMat4();
            for (int i = 0; i < 16; ++i)
            {
                // Flat indexing to 2D index conversion.
                result.raw[i] = float(map(i >> 2, i & 3));
            }

            return result;
        }

        static openvdb::math::Transform TranslateOpenVDBTransform(const openvdb::math::Transform& frameTransform,
                                                                  const openvdb::math::Vec3d& frameTranslation)
        {
            // Update transformation matrix to account for additional frameTranslation that was added to coords.
            openvdb::math::Transform resultTransform = frameTransform;

            // transform3x3 will apply only 3x3 part of matrix, without translation.
            const openvdb::math::Vec3d frameTranslationInFrameCoordinateSystem =
                frameTransform.baseMap()->getAffineMap()->getMat4().transform3x3(frameTranslation);
            resultTransform.postTranslate(frameTranslationInFrameCoordinateSystem);

            return resultTransform;
        }

        static uint32_t FirstChannelIndexFromMask(ChannelMask mask) noexcept
        {
            for (size_t i = 0; i < sizeof(mask) * 8; ++i)
            {
                if (mask & 1 << i)
                {
                    return i;
                }
            }
            return 0;
        }

        static std::vector<ChannelDescriptor> ChannelsFromGrid(openvdb::GridBase::Ptr grid, uint32_t voxelComponentCount,
                                                               uint32_t voxelComponentSize, ChannelMask firstChMask) noexcept
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

        static Math3D::AABB CalculateAABB(const openvdb::CoordBBox bbox)
        {
            Math3D::AABB result{};

            const openvdb::math::Vec3d transformedBBoxMin = bbox.min().asVec3d();
            const openvdb::math::Vec3d transformedBBoxMax = bbox.max().asVec3d();

            result.minX = FloorToBlockSize(Math3D::FloorWithEpsilon(transformedBBoxMin.x())) / SPARSE_BLOCK_SIZE;
            result.minY = FloorToBlockSize(Math3D::FloorWithEpsilon(transformedBBoxMin.y())) / SPARSE_BLOCK_SIZE;
            result.minZ = FloorToBlockSize(Math3D::FloorWithEpsilon(transformedBBoxMin.z())) / SPARSE_BLOCK_SIZE;

            result.maxX = CeilToBlockSize(Math3D::CeilWithEpsilon(transformedBBoxMax.x())) / SPARSE_BLOCK_SIZE;
            result.maxY = CeilToBlockSize(Math3D::CeilWithEpsilon(transformedBBoxMax.y())) / SPARSE_BLOCK_SIZE;
            result.maxZ = CeilToBlockSize(Math3D::CeilWithEpsilon(transformedBBoxMax.z())) / SPARSE_BLOCK_SIZE;

            return result;
        }

        static void PackFromStride(void* dst, const void* src, size_t stride, size_t offset, size_t size, size_t count) noexcept {
            if (stride == size && offset == 0) {
                memcpy(dst, src, count * size);
            } else {
                auto* dstBytes = static_cast<uint8_t*>(dst);
                auto* srcBytes = static_cast<const uint8_t*>(src);
                for (size_t i = 0; i < count; ++i) {
                    memcpy(dstBytes + size * i, srcBytes + stride * i + offset, size);
                }
            }
        }

        static std::string ValueComponentIndexToLetter(uint32_t valueComponentIdx) noexcept
        {
            using namespace std::string_literals;
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
                return "c"s + std::to_string(valueComponentIdx);
            }
        }

        static std::string SplitGridNameFromValueComponentIdx(const std::string gridName, uint32_t valueComponentIdx)
        {
            return gridName + "." + ValueComponentIndexToLetter(valueComponentIdx);
        }

        static float GetUniformVoxelScale(const openvdb::GridBase::ConstPtr& grid)
        {
            const openvdb::Vec3f voxelSize{grid->voxelSize()};
            assert(grid->hasUniformVoxels());
            return voxelSize.x();
        }

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
                                                                       const openvdb::GridBase::ConstPtr& referenceGrid) noexcept
        {
            openvdb::math::Transform result{targetGrid->transform().baseMap()->copy()};
            result.postMult(referenceGrid->transform().baseMap()->getAffineMap()->getMat4().inverse());
            return result;
        }

    private:
        std::vector<ChannelDescriptor> m_Channels{};
        std::vector<VDBGridDesc> m_GridsShuffle{};
    };
} // namespace Zibra::CE::Addons::OpenVDBUtils
