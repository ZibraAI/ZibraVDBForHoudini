#pragma once

#include <Zibra/CE/Common.h>
#include <execution>
#include <map>
#include <openvdb/io/Stream.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/GridTransformer.h>
#include <unordered_set>

#include "Zibra/CE/Compression.h"

namespace Zibra::CE::Addons::OpenVDBUtils
{
    class OpenVDBReader final
    {
    public:
        struct Feedback
        {
            int32_t offsetX = 0;
            int32_t offsetY = 0;
            int32_t offsetZ = 0;
        };
    public:
        OpenVDBReader(std::ifstream& fstream, const char* const* orderedChannelNames, size_t orderedChannelNamesCount) noexcept
        {
            auto stream = openvdb::io::Stream(reinterpret_cast<std::istream&>(fstream));

            openvdb::GridPtrVecPtr gridsPtr = stream.getGrids();
            std::vector<openvdb::GridBase::Ptr>& grids = *gridsPtr;
            const size_t gridsCount = grids.size();

            assert(orderedChannelNamesCount == 0 || gridsPtr);
            assert(orderedChannelNamesCount == 0 || orderedChannelNames);

            openvdb::initialize();

            m_OrderedChannelNames.reserve(gridsCount);

            for (size_t channelNameIdx = 0; channelNameIdx < orderedChannelNamesCount; ++channelNameIdx)
            {
                const char* name = orderedChannelNames[channelNameIdx];

                for (int gridIdx = 0; gridIdx < gridsCount; ++gridIdx)
                {
                    openvdb::GridBase::Ptr grid = grids[gridIdx];
                    if (grid->getName() == name && IsGridValid(grid))
                    {
                        m_ChannelMapping[name] = 1 << m_OrderedChannelNames.size();
                        m_OrderedChannelNames.emplace_back(name);
                        m_Grids[name] = openvdb::gridConstPtrCast<openvdb::FloatGrid>(grid);
                        break;
                    }
                }
            }
        }

        OpenVDBReader(openvdb::GridBase::ConstPtr* grids, const char* const* orderedChannelNames, size_t orderedChannelNamesCount) noexcept
        {
            assert(orderedChannelNamesCount != 0);
            assert(orderedChannelNames != nullptr);
            m_OrderedChannelNames.reserve(orderedChannelNamesCount);
            for (size_t i = 0; i < orderedChannelNamesCount; ++i)
            {
                m_ChannelMapping[orderedChannelNames[i]] = 1 << i;
                m_Grids[orderedChannelNames[i]] = openvdb::gridConstPtrCast<openvdb::FloatGrid>(grids[i]);
                m_OrderedChannelNames.emplace_back(orderedChannelNames[i]);
            }
        }

        Compression::SparseFrame* LoadFrame(Feedback* outFeedback = nullptr) noexcept
        {
            using namespace Compression;

            openvdb::initialize();
            {
                // Sort channels by m_ChannelMapping.
                assert(m_ChannelMapping.size() <= 8);
                std::vector<std::pair<std::string, openvdb::FloatGrid::ConstPtr>> orderedInputChannels{m_ChannelMapping.size()};
                for (const auto& [name, mask] : m_ChannelMapping)
                {
                    const auto index = int(std::round(std::log2(mask)));
                    const auto gridIt = m_Grids.find(name);

                    orderedInputChannels[index] = {name, gridIt != m_Grids.cend() ? m_Grids[name] : nullptr};
                }

                const auto channelCount = int32_t(orderedInputChannels.size());

                const openvdb::FloatGrid::ConstPtr originGrid = GetOriginGrid(m_Grids);

                Math3D::AABB aabb = {};
                struct LocalBlockData
                {
                    const float* voxels = nullptr;
                    uint32_t channelBlockOffset = 0;
                };
                struct LocalSpatialBlockData
                {
                    uint32_t spatialBlockOffset = 0;
                    uint32_t channelBlockOffset = 0;
                    std::map<uint8_t, LocalBlockData> channelBlockDataMap{};
                };

                // std::map is used to keep the values sorted by key.
                std::map<openvdb::Coord, LocalSpatialBlockData> channelBlockData{};
                std::vector<std::pair<std::string, openvdb::FloatGrid::Ptr>> orderedChannels{};
                orderedChannels.resize(orderedInputChannels.size());
                SparseFrame* sparseFrame = new SparseFrame{};

                for (int channelIndex = 0; channelIndex < channelCount; channelIndex++)
                {
                    const auto& [name, grid] = orderedInputChannels[channelIndex];

                    if (!grid)
                    {
                        orderedChannels[channelIndex] = {name, nullptr};
                        continue;
                    }
                    // Copy grid to a new grid for voxelization.
                    openvdb::FloatGrid::Ptr gridCopy = grid->deepCopy();

                    // Voxelize all tiles to make sure that all active regions are in leaf nodes.
                    gridCopy->tree().voxelizeActiveTiles();

                    openvdb::FloatGrid::Ptr gridOut;

                    const openvdb::math::Transform relativeTransform = GetIndexSpaceRelativeTransform(grid, originGrid);

                    // Only apply transformation if it's not identity to not waste time on unnecessary operations.
                    if (!relativeTransform.isIdentity())
                    {
                        gridOut = OpenVDBGridTransform(gridCopy, originGrid, relativeTransform);
                    }
                    else
                    {
                        gridOut = gridCopy;
                    }

                    orderedChannels[channelIndex] = {name, gridOut};
                }

                for (int channelIndex = 0; channelIndex < channelCount; channelIndex++)
                {
                    const auto& [name, gridOut] = orderedChannels[channelIndex];
                    if (!gridOut || gridOut->empty())
                        continue;

                    // Iterate over all leaf nodes to get active blocks.
                    // openvdb::FloatGrid has ALWAYS leaf blocks of size 8x8x8.
                    for (auto leafIter = gridOut->tree().cbeginLeaf(); leafIter; ++leafIter)
                    {
                        const auto& leafNode = leafIter.getLeaf();

                        const Math3D::AABB leafAABB = CalculateAABB(leafNode->getNodeBoundingBox());

                        aabb = aabb | leafAABB;

                        openvdb::Coord blockOrigin = openvdb::Coord(leafAABB.minX, leafAABB.minY, leafAABB.minZ);

                        channelBlockData[blockOrigin].channelBlockDataMap.insert({channelIndex, LocalBlockData{leafNode->buffer().data()}});
                    }
                }

                if (IsEmpty(aabb))
                {
                    sparseFrame->aabb = {0, 0, 0, 0, 0, 0};
                    return sparseFrame;
                }

                // Precalculate block/channel offsets and their counts.
                uint32_t spatialBlockCount = 0;
                uint32_t channelBlockCount = 0;

                {
                    for (auto& [block, spatialBlockData] : channelBlockData)
                    {

                        spatialBlockData.spatialBlockOffset = spatialBlockCount++;
                        spatialBlockData.channelBlockOffset = channelBlockCount;
                        for (auto& [channelIndex, blockData] : spatialBlockData.channelBlockDataMap)
                        {
                            blockData.channelBlockOffset = channelBlockCount++;
                        }
                    }
                }

                if (spatialBlockCount == 0 || channelBlockCount == 0)
                {
                    sparseFrame->aabb = {0, 0, 0, 0, 0, 0};
                    return sparseFrame;
                }

                struct LocalVoxelStatistics
                {
                    float minValue = FLT_MAX;
                    float maxValue = FLT_MIN;
                    float meanPositiveValue = 0.f;
                    float meanNegativeValue = 0.f;
                };

                std::vector<LocalVoxelStatistics> perBlockStatistics{};

                SpatialBlockInfo* spatialInfo = nullptr;
                ChannelBlock* blocks = nullptr;
                uint32_t* channelIndexPerBlock = nullptr;

                {
                    sparseFrame->spatialInfoCount = spatialBlockCount;
                    spatialInfo = new SpatialBlockInfo[spatialBlockCount];
                    sparseFrame->spatialInfo = spatialInfo;
                    sparseFrame->blocksCount = channelBlockCount;
                    blocks = new ChannelBlock[channelBlockCount];
                    sparseFrame->blocks = blocks;
                    channelIndexPerBlock = new uint32_t[channelBlockCount];
                    sparseFrame->channelIndexPerBlock = channelIndexPerBlock;
                    perBlockStatistics.resize(channelBlockCount);
                }

                {
                    std::for_each(std::execution::par_unseq, channelBlockData.cbegin(), channelBlockData.cend(),
                                  [&](const std::pair<openvdb::Coord, LocalSpatialBlockData>& spatialBlockDataPair) {
                                      const auto& [blockCoord, spatialBlockData] = spatialBlockDataPair;
                                      SpatialBlockInfo& spatialInfoBlock = spatialInfo[spatialBlockData.spatialBlockOffset];
                                      spatialInfoBlock.coords[0] = blockCoord.x() - aabb.minX;
                                      spatialInfoBlock.coords[1] = blockCoord.y() - aabb.minY;
                                      spatialInfoBlock.coords[2] = blockCoord.z() - aabb.minZ;
                                      spatialInfoBlock.channelBlocksOffset = spatialBlockData.channelBlockOffset;

                                      for (const auto& [channelIndex, blockData] : spatialBlockData.channelBlockDataMap)
                                      {
                                          const auto& [channelName, grid] = orderedChannels[channelIndex];

                                          ChannelBlock& blockVoxels = blocks[blockData.channelBlockOffset];
                                          std::memcpy(blockVoxels.voxels, blockData.voxels, sizeof(blockVoxels.voxels));

                                          LocalVoxelStatistics& statistics = perBlockStatistics[blockData.channelBlockOffset];

                                          for (const float value : blockVoxels.voxels)
                                          {
                                              statistics.minValue = std::min(statistics.minValue, value);
                                              statistics.maxValue = std::max(statistics.maxValue, value);

                                              if (value > 0.f)
                                              {
                                                  statistics.meanPositiveValue += value;
                                              }
                                              else
                                              {
                                                  statistics.meanNegativeValue += value;
                                              }
                                          }
                                          statistics.meanPositiveValue /= SPARSE_BLOCK_VOXEL_COUNT;
                                          statistics.meanNegativeValue /= SPARSE_BLOCK_VOXEL_COUNT;

                                          spatialInfoBlock.channelMask |= m_ChannelMapping[channelName];
                                          ++spatialInfoBlock.channelCount;
                                          channelIndexPerBlock[blockData.channelBlockOffset] = channelIndex;
                                      }
                                  });
                }

                // Calculate per channel statistics.
                float minChannelValue[8] = {FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
                float maxChannelValue[8] = {FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN};
                double meanPositiveChannelValue[8] = {};
                double meanNegativeChannelValue[8] = {};
                uint32_t perChannelBlockCount[8] = {};
                {
                    for (int blockIndex = 0; blockIndex < channelBlockCount; ++blockIndex)
                    {
                        const auto channelIndex = sparseFrame->channelIndexPerBlock[blockIndex];
                        const LocalVoxelStatistics& blockStatistics = perBlockStatistics[blockIndex];

                        minChannelValue[channelIndex] = std::min(minChannelValue[channelIndex], blockStatistics.minValue);
                        maxChannelValue[channelIndex] = std::max(maxChannelValue[channelIndex], blockStatistics.maxValue);
                        meanPositiveChannelValue[channelIndex] += blockStatistics.meanPositiveValue;
                        meanNegativeChannelValue[channelIndex] += blockStatistics.meanNegativeValue;
                        perChannelBlockCount[channelIndex]++;
                    }
                }

                // Calculate per channel grid data.
                {
                    ChannelInfo* orderedChannelsArray = new ChannelInfo[channelCount];
                    sparseFrame->orderedChannelsCount = channelCount;
                    sparseFrame->orderedChannels = orderedChannelsArray;
                    for (int channelIndex = 0; channelIndex < channelCount; ++channelIndex)
                    {
                        ChannelInfo& channelInfo = orderedChannelsArray[channelIndex];
                        channelInfo.name = m_OrderedChannelNames[channelIndex];

                        if (perChannelBlockCount[channelIndex] == 0)
                        {
                            continue;
                        }

                        VoxelStatistics& statistics = channelInfo.statistics;

                        statistics.meanPositiveValue = minChannelValue[channelIndex];
                        statistics.maxValue = maxChannelValue[channelIndex];
                        statistics.meanPositiveValue =
                            static_cast<float>(meanPositiveChannelValue[channelIndex] / perChannelBlockCount[channelIndex]);
                        statistics.meanNegativeValue =
                            static_cast<float>(meanNegativeChannelValue[channelIndex] / perChannelBlockCount[channelIndex]);
                        statistics.voxelCount = perChannelBlockCount[channelIndex] * SPARSE_BLOCK_VOXEL_COUNT;

                        const double volume = originGrid->constTransform().voxelVolume();
                        if (!IsNearlyEqual(volume, 0.))
                        {
                            // TODO: calculate translated transform per channel.
                            channelInfo.gridTransform = CastOpenVDBTransformToTransform(
                                GetTranslatedFrameTransform(originGrid->constTransform(),
                                                            openvdb::math::Vec3d(aabb.minX, aabb.minY, aabb.minZ) * SPARSE_BLOCK_SIZE));
                        }
                    }

                    // Translate aabb to positive quarter of coordinate system.
                    aabb.maxX -= aabb.minX;
                    aabb.maxY -= aabb.minY;
                    aabb.maxZ -= aabb.minZ;
                    aabb.minX = 0;
                    aabb.minY = 0;
                    aabb.minZ = 0;
                    sparseFrame->aabb = aabb;
                }

                if (outFeedback)
                {
                    outFeedback->offsetX = aabb.minX * SPARSE_BLOCK_SIZE;
                    outFeedback->offsetY = aabb.minY * SPARSE_BLOCK_SIZE;
                    outFeedback->offsetZ = aabb.minZ * SPARSE_BLOCK_SIZE;
                }

                std::thread deallocThread(
                    [orderedChannels = std::move(orderedChannels), channelBlockData = std::move(channelBlockData)]() mutable {
                        channelBlockData.clear();
                        orderedChannels.clear();
                    });
                deallocThread.detach();

                return sparseFrame;
            }
        }

        static Math3D::Transform CastOpenVDBTransformToTransform(const openvdb::math::Transform& transform) noexcept
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

        static void ReleaseSparseFrame(Compression::SparseFrame* sparseFrame) noexcept
        {
            delete[] sparseFrame->spatialInfo;
            delete[] sparseFrame->blocks;
            delete[] sparseFrame->channelIndexPerBlock;

            delete sparseFrame;
        }

    private:
        static float GetVoxelScale(const openvdb::GridBase::ConstPtr& grid)
        {
            using namespace Zibra::CE;

            // Intentional cast to float.
            const openvdb::Vec3f voxelSize{grid->transform().voxelSize()};
            assert(IsNearlyEqual(voxelSize[0], voxelSize[1]) && IsNearlyEqual(voxelSize[0], voxelSize[2]));
            return voxelSize[0];
        }

        /**
         * Selects the "origin" grid. Grid to index space of which, all other channels will be transformed.
         * Origin grid is selected by grid scale. Origin grid is one with smallest grid scale.
         * @param grids - vector of grids.
         * @return - selected "origin" grid.
         */
        static openvdb::FloatGrid::ConstPtr GetOriginGrid(const std::map<std::string, openvdb::FloatGrid::ConstPtr>& grids)
        {
            float minVoxelScale = std::numeric_limits<float>::max();
            openvdb::FloatGrid::ConstPtr originGrid = nullptr;

            for (const auto& [name, grid] : grids)
            {
                if (!grid)
                {
                    continue;
                }

                const float currentScale = GetVoxelScale(grid);
                if (currentScale < minVoxelScale)
                {
                    minVoxelScale = currentScale;
                    originGrid = grid;
                }
                minVoxelScale = std::min(minVoxelScale, currentScale);
            }

            return originGrid;
        }

        /** Calculate transformation to origin grid's index space.
         * T^B_O = T^B_W * T^W_O  -- B - Base, O - Origin, W - World.
         *
         * Also, for optimization reasons we assume that rotation matrix of base and origin grid within 1 frame is the same.
         * Therefore, transformation matrix calculated in this method will be ScaleTranslation matrix (or close to that due to floating
         * point errors). This assumption later allows to cut off drastic amount of multiplications.
         *
         * @param baseGrid
         * @param originGrid
         * @return - T^B_O - transformation from base to origin grid index space.
         */
        static openvdb::math::Transform GetIndexSpaceRelativeTransform(const openvdb::GridBase::ConstPtr& baseGrid,
                                                                       const openvdb::GridBase::ConstPtr& originGrid)
        {
            openvdb::math::Transform result{baseGrid->transform().baseMap()->copy()};
            result.postMult(originGrid->transform().baseMap()->getAffineMap()->getMat4().inverse());
            return result;
        }

        static Math3D::AABB CalculateAABB(const openvdb::CoordBBox bbox)
        {
            using namespace Zibra::CE;

            Math3D::AABB result{};

            const openvdb::math::Vec3d transformedBBoxMin = bbox.min().asVec3d();
            const openvdb::math::Vec3d transformedBBoxMax = bbox.max().asVec3d();

            result.minX = FloorToBlockSize(FloorWithEpsilon(transformedBBoxMin.x())) / SPARSE_BLOCK_SIZE;
            result.minY = FloorToBlockSize(FloorWithEpsilon(transformedBBoxMin.y())) / SPARSE_BLOCK_SIZE;
            result.minZ = FloorToBlockSize(FloorWithEpsilon(transformedBBoxMin.z())) / SPARSE_BLOCK_SIZE;

            result.maxX = CeilToBlockSize(CeilWithEpsilon(transformedBBoxMax.x())) / SPARSE_BLOCK_SIZE;
            result.maxY = CeilToBlockSize(CeilWithEpsilon(transformedBBoxMax.y())) / SPARSE_BLOCK_SIZE;
            result.maxZ = CeilToBlockSize(CeilWithEpsilon(transformedBBoxMax.z())) / SPARSE_BLOCK_SIZE;

            return result;
        }

        static openvdb::math::Transform GetTranslatedFrameTransform(const openvdb::math::Transform& frameTransform,
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

        static openvdb::FloatGrid::Ptr OpenVDBGridTransform(const openvdb::FloatGrid::Ptr& grid,
                                                            const openvdb::FloatGrid::ConstPtr& originGrid,
                                                            const openvdb::math::Transform& transform)
        {
            // Create a GridTransformer with this transformation.
            openvdb::tools::GridTransformer transformer(transform.baseMap()->getAffineMap()->getMat4());

            // Create an output grid with the same transform as the base grid
            openvdb::FloatGrid::Ptr transformedGrid = openvdb::FloatGrid::create(originGrid->background());
            transformedGrid->setName(grid->getName());
            transformedGrid->setTransform(originGrid->transform().copy());

            // Transform the input grid into the output grid
            transformer.transformGrid<openvdb::tools::BoxSampler>(*grid, *transformedGrid);

            return transformedGrid;
        }

        bool IsGridValid(const openvdb::GridBase::Ptr& grid) noexcept
        {
            if (!grid)
            {
                return false;
            }

            if (grid->empty())
            {
                return false;
            }

            // evalActiveVoxelBoundingBox returns CoordBBox
            // which has operator bool that will return false for an empty bounding box
            if (!grid->evalActiveVoxelBoundingBox())
            {
                return false;
            }

            if (grid->activeVoxelCount() == 0)
            {
                return false;
            }

            if (!grid->isType<openvdb::FloatGrid>() && !grid->isType<openvdb::DoubleGrid>() && !grid->isType<openvdb::Int32Grid>() &&
                !grid->isType<openvdb::Int64Grid>())
            {
                return false;
            }

            return true;
        }

        std::map<std::string, openvdb::FloatGrid::ConstPtr> m_Grids = {};
        std::map<std::string, uint32_t> m_ChannelMapping = {};
        std::vector<const char*> m_OrderedChannelNames = {};
    };
} // namespace Zibra::CE::Addons
