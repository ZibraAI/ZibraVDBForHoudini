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

    Math3D::Transform CastOpenVDBTransformToTransform(const openvdb::math::Transform& transform)
    {
        Math3D::Transform result{};

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

    Math3D::AABB CalculateAABB(const openvdb::CoordBBox bbox)
    {
        using namespace MathHelpers;

        Math3D::AABB result{};

        const openvdb::math::Vec3d transformedBBoxMin = bbox.min().asVec3d();
        const openvdb::math::Vec3d transformedBBoxMax = bbox.max().asVec3d();

        result.minX = FloorToBlockSize(FloorWithEpsilon(transformedBBoxMin.x())) / CE::SPARSE_BLOCK_SIZE;
        result.minY = FloorToBlockSize(FloorWithEpsilon(transformedBBoxMin.y())) / CE::SPARSE_BLOCK_SIZE;
        result.minZ = FloorToBlockSize(FloorWithEpsilon(transformedBBoxMin.z())) / CE::SPARSE_BLOCK_SIZE;

        result.maxX = CeilToBlockSize(CeilWithEpsilon(transformedBBoxMax.x())) / CE::SPARSE_BLOCK_SIZE;
        result.maxY = CeilToBlockSize(CeilWithEpsilon(transformedBBoxMax.y())) / CE::SPARSE_BLOCK_SIZE;
        result.maxZ = CeilToBlockSize(CeilWithEpsilon(transformedBBoxMax.z())) / CE::SPARSE_BLOCK_SIZE;

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

    CE::Compression::SparseFrame* OpenVDBDecoder::DecodeFrame(DecodeMetadata& decodeMetadata) noexcept
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



        auto* sparseFrame = new CE::Compression::SparseFrame{};
        sparseFrame->orderedChannelsCount =  m_OrderedChannelNames.size();
        auto* channels = new CE::Compression::ChannelInfo[m_OrderedChannelNames.size()];
        sparseFrame->orderedChannelsCount = m_OrderedChannelNames.size();
        for (size_t i = 0; i < sparseFrame->orderedChannelsCount; ++i)
        {
            size_t stringSizeInBytes = m_OrderedChannelNames[i].size() + 1;
            auto* channelName = new char[stringSizeInBytes];
            std::strncpy(channelName, m_OrderedChannelNames[i].c_str(), stringSizeInBytes);
            channels[i].name = channelName;
        }

        Math3D::AABB totalAABB{std::numeric_limits<int>::max(), std::numeric_limits<int>::max(),
                               std::numeric_limits<int>::max(), std::numeric_limits<int>::min(),
                               std::numeric_limits<int>::min(), std::numeric_limits<int>::min()};
        std::vector<Math3D::AABB> channelAABB{static_cast<size_t>(channelCount)};

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

                const Math3D::AABB leafAABB = CalculateAABB(leafNode->getNodeBoundingBox());

                totalAABB = totalAABB | leafAABB;
                openvdb::Coord blockOrigin = openvdb::Coord(leafAABB.minX, leafAABB.minY, leafAABB.minZ);

                channelBlockData[blockOrigin].channelBlockDataMap.insert({channelIndex, LocalBlockData{leafNode->buffer().data()}});
            }
        }

        if (IsEmpty(totalAABB))
        {
            sparseFrame->aabb = {0, 0, 0, 0, 0, 0};;
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
            sparseFrame->aabb = {0, 0, 0, 0, 0, 0};;
            return sparseFrame;
        }

        struct LocalVoxelStatistics
        {
            float minValue = FLT_MAX;
            float maxValue = FLT_MIN;
            float meanPositiveValue = 0.f;
            float meanNegativeValue = 0.f;
        };

        sparseFrame->spatialInfoCount = spatialBlockCount;
        auto spatialInfo = new CE::Compression::SpatialBlockInfo[sparseFrame->spatialInfoCount];
        sparseFrame->spatialInfo = spatialInfo;
        sparseFrame->blocksCount = channelBlockCount;
        auto blocks = new CE::Compression::ChannelBlock[sparseFrame->blocksCount];
        sparseFrame->blocks = blocks;
        auto channelIndexPerBlock = new uint32_t[sparseFrame->blocksCount];
        sparseFrame->channelIndexPerBlock = channelIndexPerBlock;

        std::vector<LocalVoxelStatistics> perBlockStatistics{};
        perBlockStatistics.resize(channelBlockCount);

        std::for_each(std::execution::par_unseq, channelBlockData.cbegin(), channelBlockData.cend(),
                      [&](const std::pair<openvdb::Coord, LocalSpatialBlockData>& spatialBlockDataPair) {
                          const auto& [blockCoord, spatialBlockData] = spatialBlockDataPair;
                          auto& info = spatialInfo[spatialBlockData.spatialBlockOffset];
                          info.coords[0] = blockCoord.x() - totalAABB.minX;
                          info.coords[1] = blockCoord.y() - totalAABB.minY;
                          info.coords[2] = blockCoord.z() - totalAABB.minZ;
                          info.channelBlocksOffset = spatialBlockData.channelBlockOffset;

                          for (const auto& [channelIndex, blockData] : spatialBlockData.channelBlockDataMap)
                          {
                              const auto& [channelName, grid] = orderedChannels[channelIndex];

                              CE::Compression::ChannelBlock& blockVoxels = blocks[blockData.channelBlockOffset];
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
                              statistics.meanPositiveValue /= CE::SPARSE_BLOCK_VOXEL_COUNT;
                              statistics.meanNegativeValue /= CE::SPARSE_BLOCK_VOXEL_COUNT;


                              info.channelMask |= m_ChannelMapping[channelName];
                              ++info.channelCount;
                              channelIndexPerBlock[blockData.channelBlockOffset] = channelIndex;
                          }
                      });

        // Calculate per channel statistics.
        float minChannelValue[8] = {FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
        float maxChannelValue[8] = {FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN};
        double meanPositiveChannelValue[8] = {};
        double meanNegativeChannelValue[8] = {};
        uint32_t perChannelBlockCount[8] = {};
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

        // Calculate per channel scale
        for (int channelIndex = 0; channelIndex < channelCount; ++channelIndex)
        {
            if (perChannelBlockCount[channelIndex] == 0)
            {
                continue;
            }

            const double meanPositiveValue = meanPositiveChannelValue[channelIndex] / perChannelBlockCount[channelIndex];
            const double meanNegativeValue = meanNegativeChannelValue[channelIndex] / perChannelBlockCount[channelIndex];

            channels[channelIndex].statistics.minValue = minChannelValue[channelIndex];
            channels[channelIndex].statistics.maxValue = maxChannelValue[channelIndex];
            channels[channelIndex].statistics.meanPositiveValue = meanPositiveValue;
            channels[channelIndex].statistics.meanNegativeValue = meanNegativeValue;
            channels[channelIndex].statistics.voxelCount = perChannelBlockCount[channelIndex] * CE::SPARSE_BLOCK_VOXEL_COUNT;

            const auto& gridTransform = orderedChannels[channelIndex].second->constTransform();
            const double volume = gridTransform.voxelVolume();
            if (!MathHelpers::IsNearlyEqual(volume, 0.))
            {
                channels[channelIndex].gridTransform = CastOpenVDBTransformToTransform(GetTranslatedFrameTransform(
                    gridTransform, openvdb::math::Vec3d(totalAABB.minX, totalAABB.minY, totalAABB.minZ) * CE::SPARSE_BLOCK_SIZE));
            }
        }

        decodeMetadata = {};
        decodeMetadata.offsetX = totalAABB.minX * CE::SPARSE_BLOCK_SIZE;
        decodeMetadata.offsetY = totalAABB.minY * CE::SPARSE_BLOCK_SIZE;
        decodeMetadata.offsetZ = totalAABB.minZ * CE::SPARSE_BLOCK_SIZE;

        // Translate aabb to positive quarter of coordinate system.
        sparseFrame->aabb = Math3D::AABB{0, 0, 0, totalAABB.maxX - totalAABB.minX, totalAABB.maxY - totalAABB.minY, totalAABB.maxZ - totalAABB.minZ};

        std::thread deallocThread([orderedChannels = std::move(orderedChannels)]() mutable { orderedChannels.clear(); });
        deallocThread.detach();

        return sparseFrame;
    }

    void OpenVDBDecoder::FreeFrame(CE::Compression::SparseFrame* frame)
    {
        delete[] frame->blocks;
        delete[] frame->spatialInfo;
        delete[] frame->channelIndexPerBlock;
        for (size_t i = 0; i < frame->orderedChannelsCount; ++i)
        {
            delete[] frame->orderedChannels[i].name;
        }
        delete[] frame->orderedChannels;
    }

} // namespace Zibra::OpenVDBSupport
