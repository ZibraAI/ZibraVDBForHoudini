#pragma once

#include <Zibra/CE/Common.h>
#include <execution>
#include <map>
#include <openvdb/io/Stream.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/GridTransformer.h>

#include "Zibra/CE/Compression.h"

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
        explicit FrameLoader(openvdb::GridBase::ConstPtr* grids, size_t gridsCount) noexcept
        {
            m_Channels.reserve(gridsCount);

            ChannelMask mask = 0x1;
            for (size_t i = 0; i < gridsCount; ++i)
            {
                std::vector<ChannelDescriptor> channels;
                openvdb::GridBase::Ptr mutableCopy = grids[i]->deepCopyGrid();

                if (grids[i]->baseTree().isType<openvdb::Vec3fTree>())
                {
                    channels = ChannelsFromGrid(mutableCopy, 3, sizeof(float), mask);
                }
                else if (grids[i]->baseTree().isType<openvdb::FloatTree>())
                {
                    channels = ChannelsFromGrid(mutableCopy, 1, sizeof(float), mask);
                }
                else
                {
                    assert(0 && "Unsupported grid type. Loader supports only floating point grids.");
                    m_Channels.clear();
                    return;
                }
                mask = mask << channels.size();
                m_Channels.insert(m_Channels.end(), channels.begin(), channels.end());
            }
        }

        [[nodiscard]] Compression::SparseFrame* LoadFrame() const noexcept
        {
            auto result = new Compression::SparseFrame{};
            std::map<openvdb::Coord, SpatialBlockIntermediate> spatialBlocks{};
            Math3D::AABB totalAABB = {};

            for (size_t i = 0; i < m_Channels.size(); ++i)
            {
                const ChannelDescriptor& channel = m_Channels[i];
                if (channel.grid->baseTree().isType<openvdb::Vec3fTree>())
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

            uint32_t channelBlockAccumulator = 0;
            for (auto& [coord, spatialBlock] : spatialBlocks)
            {
                spatialBlock.destFirstChannelBlockIndex = channelBlockAccumulator;
                channelBlockAccumulator += spatialBlock.blocks.size();
            }

            result->blocksCount = channelBlockAccumulator;
            result->spatialInfoCount = spatialBlocks.size();
            result->orderedChannelsCount = m_Channels.size();

            auto* resultBlocks = new ChannelBlock[result->blocksCount];
            auto* channelIndexPerBlock = new uint32_t[result->blocksCount];
            auto* resultSpatialInfo = new SpatialBlockInfo[result->spatialInfoCount];
            auto* orderedChannels = new Compression::ChannelInfo[result->orderedChannelsCount];

            result->blocks = resultBlocks;
            result->spatialInfo = resultSpatialInfo;
            result->orderedChannels = orderedChannels;

            result->aabb = totalAABB;
            result->channelIndexPerBlock = channelIndexPerBlock;

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

            std::vector<Compression::VoxelStatistics> perBlockStatistics{};
            perBlockStatistics.resize(result->blocksCount);
            std::for_each(std::execution::par_unseq, spatialBlocks.begin(), spatialBlocks.end(), [&](const std::pair<openvdb::Coord, SpatialBlockIntermediate>& item){
                auto& [coord, spatialIntrm] = item;
                ChannelMask mask = 0x0;

                //TODO: ensure right channels order
                size_t chIdx = 0;
                for (auto& [chMask, chBlockIntrm] : spatialIntrm.blocks) {
                    uint32_t channelBlockIndex = spatialIntrm.destFirstChannelBlockIndex + chIdx;
                    ChannelBlock& outBlock = resultBlocks[channelBlockIndex];
                    mask |= chMask;
                    channelIndexPerBlock[channelBlockIndex] = FirstChannelIndexFromMask(chMask);
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
                orderedChannels[i].statistics.meanPositiveValue /= static_cast<float>(orderedChannels[i].statistics.voxelCount);
                orderedChannels[i].statistics.meanNegativeValue /= static_cast<float>(orderedChannels[i].statistics.voxelCount);
                // Previous cycle has written blocks per channel count to voxelCount field.
                orderedChannels[i].statistics.voxelCount *= SPARSE_BLOCK_VOXEL_COUNT;
            }

            return result;
        }
    private:
        template<typename T>
        Math3D::AABB ResolveBlocks(const ChannelDescriptor& ch, std::map<openvdb::Coord, SpatialBlockIntermediate>& spatialMap) const noexcept
        {
            auto grid = openvdb::gridPtrCast<T>(ch.grid);
            grid->tree().voxelizeActiveTiles();

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
                std::string chName = grid->getName();
                if (voxelComponentCount > 1)
                    chName += "." + ValueComponentIndexToLetter(chIdx);

                ChannelDescriptor chDesc{};
                chDesc.name = chName;
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

        static std::string ValueComponentIndexToLetter(uint32_t idx) noexcept
        {
            using namespace std::string_literals;
            switch (idx)
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
                return "c"s + std::to_string(idx);
            }
        }

    private:
        std::vector<ChannelDescriptor> m_Channels{};
    };









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
            auto stream = openvdb::io::Stream(fstream);

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

        Compression::SparseFrame* LoadFrame(bool resampleChannels = true, Feedback* outFeedback = nullptr) noexcept
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

                openvdb::FloatGrid::ConstPtr originGrid = nullptr;
                if (resampleChannels)
                {
                    originGrid = GetOriginGrid(m_Grids);
                }

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
                    if (resampleChannels)
                    {
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
                                      spatialInfoBlock.channelMask = 0;
                                      spatialInfoBlock.channelCount = 0;

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

                                      assert(spatialInfoBlock.channelCount >= 1 && spatialInfoBlock.channelCount <= 8);
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

                        if (resampleChannels)
                        {
                            const auto& vdbTransform = originGrid->constTransform();
                            const double volume = vdbTransform.voxelVolume();
                            if (!Math3D::IsNearlyEqual(volume, 0.))
                            {
                                channelInfo.gridTransform = CastOpenVDBTransformToTransform(GetTranslatedFrameTransform(
                                    vdbTransform, openvdb::math::Vec3d(aabb.minX, aabb.minY, aabb.minZ) * SPARSE_BLOCK_SIZE));
                            }
                        }
                        else
                        {
                            const openvdb::FloatGrid::ConstPtr grid = orderedChannels[channelIndex].second;
                            const auto& vdbTransform = grid->constTransform();
                            const double volume = vdbTransform.voxelVolume();
                            if (!Math3D::IsNearlyEqual(volume, 0.))
                            {
                                channelInfo.gridTransform = CastOpenVDBTransformToTransform(GetTranslatedFrameTransform(
                                    grid->constTransform(), openvdb::math::Vec3d(aabb.minX, aabb.minY, aabb.minZ) * SPARSE_BLOCK_SIZE));
                            }
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
            // Intentional cast to float.
            const openvdb::Vec3f voxelSize{grid->transform().voxelSize()};
            assert(Math3D::IsNearlyEqual(voxelSize[0], voxelSize[1]) && Math3D::IsNearlyEqual(voxelSize[0], voxelSize[2]));
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
} // namespace Zibra::CE::Addons::OpenVDBUtils
