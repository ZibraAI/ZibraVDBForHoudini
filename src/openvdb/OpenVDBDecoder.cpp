#include "PrecompiledHeader.h"

#include "OpenVDBDecoder.h"

#include "MathHelpers.h"

namespace Zibra::OpenVDBSupport
{
    float GetVoxelScale(const openvdb::GridBase::ConstPtr& grid)
    {
        using namespace MathHelpers;

        const openvdb::Vec3f voxelSize{grid->transform().voxelSize()};
        assert(IsNearlyEqual(voxelSize[0], voxelSize[1]) && IsNearlyEqual(voxelSize[0], voxelSize[2]));
        return voxelSize[0];
    }

    CompressionEngine::ZCE_Transform CastOpenVDBTransformToTransform(const openvdb::math::Transform& transform)
    {
        CompressionEngine::ZCE_Transform result{};

        const openvdb::math::Mat4 map = transform.baseMap()->getAffineMap()->getMat4();
        for (int i = 0; i < 16; ++i)
        {
            result.matrix[i] = float(map(i >> 2, i & 3));
        }

        return result;
    }

    /**
     * Selects the "origin" grid. Grid to index space of which, all other channels will be transformed.
     * Origin grid is selected by grid scale. Origin grid is one with smallest grid scale.
     * @param grids - vector of grids.
     * @return - selected "origin" grid.
     */
    openvdb::FloatGrid::ConstPtr GetOriginGrid(const std::map<std::string, openvdb::FloatGrid::ConstPtr>& grids)
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
    openvdb::math::Transform GetIndexSpaceRelativeTransform(const openvdb::GridBase::ConstPtr& baseGrid,
                                                            const openvdb::GridBase::ConstPtr& originGrid)
    {
        openvdb::math::Transform result{baseGrid->transform().baseMap()->copy()};
        result.postMult(originGrid->transform().baseMap()->getAffineMap()->getMat4().inverse());
        return result;
    }

    CompressionEngine::ZCE_AABB CalculateAABB(const openvdb::CoordBBox bbox)
    {
        using namespace MathHelpers;

        CompressionEngine::ZCE_AABB result{};

        const openvdb::math::Vec3d transformedBBoxMin = bbox.min().asVec3d();
        const openvdb::math::Vec3d transformedBBoxMax = bbox.max().asVec3d();

        result.minX = FloorToBlockSize(FloorWithEpsilon(transformedBBoxMin.x())) / ZIB_BLOCK_SIZE;
        result.minY = FloorToBlockSize(FloorWithEpsilon(transformedBBoxMin.y())) / ZIB_BLOCK_SIZE;
        result.minZ = FloorToBlockSize(FloorWithEpsilon(transformedBBoxMin.z())) / ZIB_BLOCK_SIZE;

        result.maxX = CeilToBlockSize(CeilWithEpsilon(transformedBBoxMax.x())) / ZIB_BLOCK_SIZE;
        result.maxY = CeilToBlockSize(CeilWithEpsilon(transformedBBoxMax.y())) / ZIB_BLOCK_SIZE;
        result.maxZ = CeilToBlockSize(CeilWithEpsilon(transformedBBoxMax.z())) / ZIB_BLOCK_SIZE;

        return result;
    }

    openvdb::math::Transform GetTranslatedFrameTransform(const openvdb::math::Transform& frameTransform,
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

    OpenVDBDecoder::OpenVDBDecoder(openvdb::GridBase::ConstPtr* grids, const char* const* orderedChannelNames,
                                   size_t orderedChannelNamesCount) noexcept
    {
        assert(orderedChannelNames);
        m_OrderedChannelNames.reserve(orderedChannelNamesCount);
        for (size_t i = 0; i < orderedChannelNamesCount; ++i)
        {
            m_ChannelMapping[orderedChannelNames[i]] = 1 << i;
            m_Grids[orderedChannelNames[i]] = openvdb::gridConstPtrCast<openvdb::FloatGrid>(grids[i]);
            m_OrderedChannelNames.emplace_back(orderedChannelNames[i]);
        }
    }

    OpenVDBDecoder::OpenVDBDecoder(openvdb::io::Stream& stream, const char* const* orderedChannelNames,
                                   size_t orderedChannelNamesCount) noexcept
    {
        assert(orderedChannelNames);
        m_OrderedChannelNames.reserve(orderedChannelNamesCount);
        for (size_t i = 0; i < orderedChannelNamesCount; ++i)
        {
            m_ChannelMapping[orderedChannelNames[i]] = 1 << i;
            // TODO: write only available channel names
            m_OrderedChannelNames.emplace_back(orderedChannelNames[i]);
        }

        openvdb::initialize();

        openvdb::GridPtrVecPtr gridVec = stream.getGrids();
        assert(!gridVec->empty());

        for (const openvdb::GridBase::Ptr& grid : *gridVec)
        {
            if (m_ChannelMapping.find(grid->getName()) != m_ChannelMapping.end())
            {
                m_Grids[grid->getName()] = openvdb::gridConstPtrCast<openvdb::FloatGrid>(grid);
            }
        }
    }

    openvdb::FloatGrid::Ptr OpenVDBGridTransform(const openvdb::FloatGrid::Ptr& grid, const openvdb::FloatGrid::ConstPtr& originGrid,
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

    CompressionEngine::ZCE_SparseFrameData OpenVDBDecoder::DecodeFrame() noexcept
    {
        using namespace MathHelpers;

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

        CompressionEngine::ZCE_SparseFrameData sparseFrame = {};
        sparseFrame.channelNames = new char*[m_OrderedChannelNames.size()];
        sparseFrame.channelCount = m_OrderedChannelNames.size();
        sparseFrame.perChannelGridInfo = new CompressionEngine::ZCE_PerChannelGridInfo[m_OrderedChannelNames.size()];
        for (size_t i = 0; i < m_OrderedChannelNames.size(); ++i)
        {
            size_t stringSizeInBytes = m_OrderedChannelNames[i].size() + 1;
            sparseFrame.channelNames[i] = new char[stringSizeInBytes];
            std::strncpy(sparseFrame.channelNames[i], m_OrderedChannelNames[i].c_str(), stringSizeInBytes);
        }

        CompressionEngine::ZCE_AABB totalAABB{std::numeric_limits<int>::max(), std::numeric_limits<int>::max(),
                                              std::numeric_limits<int>::max(), std::numeric_limits<int>::min(),
                                              std::numeric_limits<int>::min(), std::numeric_limits<int>::min()};
        std::vector<CompressionEngine::ZCE_AABB> channelAABB{size_t(channelCount)};

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

            channelAABB[channelIndex] = CalculateAABB(gridCopy->evalActiveVoxelBoundingBox());

            orderedChannels[channelIndex] = {name, gridCopy};
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

                const CompressionEngine::ZCE_AABB leafAABB = CalculateAABB(leafNode->getNodeBoundingBox());

                totalAABB = totalAABB | leafAABB;
                openvdb::Coord blockOrigin =
                    openvdb::Coord(leafAABB.minX, leafAABB.minY, leafAABB.minZ);

                channelBlockData[blockOrigin].channelBlockDataMap.insert({channelIndex, LocalBlockData{leafNode->buffer().data()}});
            }
        }

        if (IsEmpty(totalAABB))
        {
            sparseFrame.boundingBox = {0, 0, 0, 0, 0, 0};
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
            sparseFrame.boundingBox = {0, 0, 0, 0, 0, 0};
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

        {
            sparseFrame.spatialBlocks = new CompressionEngine::ZCE_SpatialBlock[spatialBlockCount];
            sparseFrame.channelBlocks = new CompressionEngine::ZCE_ChannelBlock[channelBlockCount];
            sparseFrame.channelIndexPerBlock = new int32_t[channelBlockCount];
            perBlockStatistics.resize(channelBlockCount);
            sparseFrame.spatialBlockCount = spatialBlockCount;
            sparseFrame.channelBlockCount = channelBlockCount;
        }

        {
            std::for_each(std::execution::par_unseq, channelBlockData.cbegin(), channelBlockData.cend(),
                          [&](const std::pair<openvdb::Coord, LocalSpatialBlockData>& spatialBlockDataPair) {
                              const auto& [blockCoord, spatialBlockData] = spatialBlockDataPair;
                              CompressionEngine::ZCE_SpatialBlock& spatialInfo =
                                  sparseFrame.spatialBlocks[spatialBlockData.spatialBlockOffset];
                              spatialInfo.coords[0] = blockCoord.x() - totalAABB.minX;
                              spatialInfo.coords[1] = blockCoord.y() - totalAABB.minY;
                              spatialInfo.coords[2] = blockCoord.z() - totalAABB.minZ;
                              spatialInfo.channelBlocksOffset = spatialBlockData.channelBlockOffset;

                              for (const auto& [channelIndex, blockData] : spatialBlockData.channelBlockDataMap)
                              {
                                  const auto& [channelName, grid] = orderedChannels[channelIndex];

                                  CompressionEngine::ZCE_ChannelBlock& blockVoxels =
                                      sparseFrame.channelBlocks[blockData.channelBlockOffset];
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
                                  statistics.meanPositiveValue /= ZIB_BLOCK_ELEMENT_COUNT;
                                  statistics.meanNegativeValue /= ZIB_BLOCK_ELEMENT_COUNT;

                                  spatialInfo.channelMask |= m_ChannelMapping[channelName];
                                  ++spatialInfo.channelCount;
                                  sparseFrame.channelIndexPerBlock[blockData.channelBlockOffset] = channelIndex;
                              }
                          });
        }

        // Calculate per channel statistics.
        float minChannelValue[8] = {FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
        float maxChannelValue[8] = {FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN};
        double meanPositiveChannelValue[8] = {};
        double meanNegativeChannelValue[8] = {};
        uint32_t perChannelBlockCount[8] = {};
        for (int blockIndex = 0; blockIndex < channelBlockCount; ++blockIndex)
        {
            const auto channelIndex = sparseFrame.channelIndexPerBlock[blockIndex];
            const LocalVoxelStatistics& blockStatistics = perBlockStatistics[blockIndex];

            minChannelValue[channelIndex] = std::min(minChannelValue[channelIndex], blockStatistics.minValue);
            maxChannelValue[channelIndex] = std::max(maxChannelValue[channelIndex], blockStatistics.maxValue);
            meanPositiveChannelValue[channelIndex] += blockStatistics.meanPositiveValue;
            meanNegativeChannelValue[channelIndex] += blockStatistics.meanNegativeValue;
            perChannelBlockCount[channelIndex]++;
        }

        // Calculate per channel scale
        for (int channelIndex = 0; channelIndex < channelCount; ++channelIndex)
        {
            if (perChannelBlockCount[channelIndex] == 0)
            {
                continue;
            }

            const double meanPositiveValue = meanPositiveChannelValue[channelIndex] / perChannelBlockCount[channelIndex];
            const double meanNegativeValue = meanNegativeChannelValue[channelIndex] / perChannelBlockCount[channelIndex];

            sparseFrame.perChannelGridInfo[channelIndex].statistics.minValue = minChannelValue[channelIndex];
            sparseFrame.perChannelGridInfo[channelIndex].statistics.maxValue = maxChannelValue[channelIndex];
            sparseFrame.perChannelGridInfo[channelIndex].statistics.meanPositiveValue = meanPositiveValue;
            sparseFrame.perChannelGridInfo[channelIndex].statistics.meanNegativeValue = meanNegativeValue;
            sparseFrame.perChannelGridInfo[channelIndex].statistics.voxelCount =
                perChannelBlockCount[channelIndex] * ZIB_BLOCK_ELEMENT_COUNT;

            const auto& gridTransform = orderedChannels[channelIndex].second->constTransform();
            const double volume = gridTransform.voxelVolume();
            if (!MathHelpers::IsNearlyEqual(volume, 0.))
            {
                sparseFrame.perChannelGridInfo[channelIndex].gridTransform = CastOpenVDBTransformToTransform(GetTranslatedFrameTransform(
                    gridTransform, openvdb::math::Vec3d(totalAABB.minX, totalAABB.minY, totalAABB.minZ) * ZIB_BLOCK_SIZE));
            }
        }

        // Translate aabb to positive quarter of coordinate system.
        sparseFrame.boundingBox = CompressionEngine::ZCE_AABB{
            0, 0, 0, totalAABB.maxX - totalAABB.minX, totalAABB.maxY - totalAABB.minY, totalAABB.maxZ - totalAABB.minZ};

        std::thread deallocThread([orderedChannels = std::move(orderedChannels)]() mutable { orderedChannels.clear(); });
        deallocThread.detach();

        return sparseFrame;
    }

    void OpenVDBDecoder::FreeFrame(CompressionEngine::ZCE_SparseFrameData& frame)
    {
        for (size_t i = 0; i < frame.channelCount; ++i)
        {
            delete[] frame.channelNames[i];
        }
        delete[] frame.channelNames;
        delete[] frame.spatialBlocks;
        delete[] frame.channelBlocks;
        delete[] frame.channelIndexPerBlock;
        delete[] frame.perChannelGridInfo;
    }

} // namespace Zibra::OpenVDBSupport
