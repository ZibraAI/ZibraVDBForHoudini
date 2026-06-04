#pragma once

#include <Zibra/CE/Compression.h>
#include <execution>
#include <map>
#include <openvdb/openvdb.h>
#include <openvdb/tools/GridTransformer.h>

namespace Zibra::CE::Addons::OpenVDBUtils
{
    class OpenVDBToSparseFrame
    {
        struct ChannelDescriptor
        {
            std::string name;
            openvdb::GridBase::Ptr grid;
            uint8_t componentCount;
            uint8_t firstComponentIndex;
        };

        struct ComponentDescriptor
        {
            ComponentMask componentMask;
            openvdb::GridBase::Ptr grid;
            uint32_t valueSize;
            uint32_t valueStride;
            uint32_t valueOffset;
            uint8_t channelIndex;
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
            std::map<ComponentMask, ChannelBlockIntermediate> blocks{};
        };

    public:
        /**
         *
         * @param grids - grids to encode
         * @param gridsCount - number of entries in grids param
         * @param matchVoxelSize - gets one origin grid and resamples other to its voxel size. (Heavy operation)
         */
        explicit OpenVDBToSparseFrame(openvdb::GridBase::ConstPtr* grids, size_t gridsCount, bool matchVoxelSize = false) noexcept;
        ~OpenVDBToSparseFrame() noexcept = default;

        OpenVDBToSparseFrame(const OpenVDBToSparseFrame&) = delete;
        OpenVDBToSparseFrame& operator=(const OpenVDBToSparseFrame&) = delete;

        OpenVDBToSparseFrame(OpenVDBToSparseFrame&& other) = default;
        OpenVDBToSparseFrame& operator=(OpenVDBToSparseFrame&&) = delete;

        [[nodiscard]] Compression::SparseFrame* LoadFrame() const noexcept;
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
        Math::AABB ResolveBlocks(const ComponentDescriptor& ch,
                                 std::map<openvdb::Coord, SpatialBlockIntermediate>& spatialMap) const noexcept;
        static Math::Transform OpenVDBTransformToMathTransform(const openvdb::math::Transform& transform) noexcept;
        static uint32_t FirstChannelIndexFromMask(ComponentMask mask) noexcept;
        static Math::AABB CalculateAABB(const openvdb::CoordBBox& bbox) noexcept;
        static void PackFromStride(void* dst, const void* src, size_t stride, size_t offset, size_t size, size_t count) noexcept;
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
        std::vector<ComponentDescriptor> m_Components{};
    };

    template <typename T>
    Math::AABB OpenVDBToSparseFrame::ResolveBlocks(const ComponentDescriptor& ch, std::map<openvdb::Coord,
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
            localSpatialBlock.blocks[ch.componentMask] = localChannelBlock;

            spatialMap[origin] = localSpatialBlock;
        }
        return totalAABB;
    }

    inline openvdb::math::Transform OpenVDBToSparseFrame::GetIndexSpaceRelativeTransform(const openvdb::GridBase::ConstPtr& targetGrid,
                                                                                const openvdb::GridBase::ConstPtr& referenceGrid) noexcept
    {
        openvdb::math::Transform result{targetGrid->transform().baseMap()->copy()};
        result.postMult(referenceGrid->transform().baseMap()->getAffineMap()->getMat4().inverse());
        return result;
    }

    inline float OpenVDBToSparseFrame::GetUniformVoxelScale(const openvdb::GridBase::ConstPtr& grid)
    {
        const openvdb::Vec3f voxelSize{grid->voxelSize()};
        assert(grid->hasUniformVoxels());
        return voxelSize.x();
    }

    inline void OpenVDBToSparseFrame::PackFromStride(void* dst, const void* src, size_t stride, size_t offset, size_t size, size_t count) noexcept
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

    inline Math::AABB OpenVDBToSparseFrame::CalculateAABB(const openvdb::CoordBBox& bbox) noexcept
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

    inline uint32_t OpenVDBToSparseFrame::FirstChannelIndexFromMask(ComponentMask mask) noexcept
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

    inline Math::Transform OpenVDBToSparseFrame::OpenVDBTransformToMathTransform(const openvdb::math::Transform& transform) noexcept
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

    inline void OpenVDBToSparseFrame::ReleaseFrame(const Compression::SparseFrame* frame) noexcept
    {
        if (frame)
        {
            for (size_t i = 0; i < frame->info.channelsCount; ++i)
            {
                delete[] frame->info.channels[i].name;
            }
            delete[] frame->channelIndexPerBlock;
            delete[] frame->blocks;
            delete[] frame->spatialBlock;
            delete frame;
        }
    }

    inline Compression::SparseFrame* OpenVDBToSparseFrame::LoadFrame() const noexcept
    {
        auto result = new Compression::SparseFrame{};
        std::map<openvdb::Coord, SpatialBlockIntermediate> spatialBlocks{};
        std::vector<Math::AABB> orderedChannelAABBs{};
        Math::AABB frameAABB = {};

        // ResolveBlocks requires component-level iteration to correctly fill spatialBlocks
        // As side effect, it gives component AABB
        for (size_t i = 0; i < m_Components.size(); ++i)
        {
            const ComponentDescriptor& component = m_Components[i];
            if (component.grid->baseTree().isType<openvdb::Vec3STree>())
            {
                Math::AABB componentAABB = ResolveBlocks<openvdb::Vec3fGrid>(component, spatialBlocks);
                orderedChannelAABBs.emplace_back(componentAABB);
                frameAABB = frameAABB | componentAABB;
            }
            else if (component.grid->baseTree().isType<openvdb::FloatTree>())
            {
                Math::AABB componentAABB = ResolveBlocks<openvdb::FloatGrid>(component, spatialBlocks);
                orderedChannelAABBs.emplace_back(componentAABB);
                frameAABB = frameAABB | componentAABB;
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
        result->info.componentsCount = m_Components.size();
        result->info.aabb = frameAABB;

        // Allocating result frame buffers from precalculated data
        auto* resultBlocks = new ChannelBlock[result->info.channelBlockCount];
        auto* resultChannelIndexPerBlock = new uint32_t[result->info.channelBlockCount];
        auto* resultSpatialInfo = new SpatialBlock[result->info.spatialBlockCount];
        auto* channels = result->info.channels;

        result->blocks = resultBlocks;
        result->spatialBlock = resultSpatialInfo;
        result->channelIndexPerBlock = resultChannelIndexPerBlock;

        // Preparing channel info. Filling known data and setting edge initial values for future statistics calculation.
        for (size_t i = 0; i < m_Channels.size(); ++i)
        {
            auto chName = new char[m_Channels[i].name.length() + 1];
            strcpy(chName, m_Channels[i].name.c_str());
            channels[i].name = chName;
            channels[i].componentCount = m_Channels[i].componentCount;
            channels[i].firstComponentIndex = m_Channels[i].firstComponentIndex;
            channels[i].aabb = orderedChannelAABBs[channels[i].firstComponentIndex];
            channels[i].gridTransform = OpenVDBTransformToMathTransform(m_Channels[i].grid->constTransform());
        }

        for (size_t i = 0; i < result->info.componentsCount; ++i)
        {
            result->info.components[i].minValue = std::numeric_limits<float>::max();
            result->info.components[i].maxValue = std::numeric_limits<float>::min();
        }

        // Concurrently moving voxel data from leafs to destination ChannelBlock array using precalculated offsets and other data
        // stored in spatial descriptors + calculating voxel statistics per block.
        std::vector<ComponentVoxelStatistics> perBlockStatistics{};
        perBlockStatistics.resize(result->info.channelBlockCount);
        std::for_each(
#if !ZIB_TARGET_OS_MAC
            std::execution::par_unseq,
#endif
            spatialBlocks.begin(), spatialBlocks.end(), [&](const std::pair<openvdb::Coord, SpatialBlockIntermediate>& item) {
                auto& [coord, spatialIntermediate] = item;
                ComponentMask mask = 0x0;

                // Iterating components in order of increasing mask.
                uint8_t componentIndex = 0;
                for (auto& [compMask, compBlockIntrm] : spatialIntermediate.blocks)
                {
                    uint32_t componentBlockIndex = spatialIntermediate.destFirstChannelBlockIndex + componentIndex;
                    ChannelBlock& outBlock = resultBlocks[componentBlockIndex];
                    mask |= compMask;
                    resultChannelIndexPerBlock[componentBlockIndex] = FirstChannelIndexFromMask(compMask);
                    PackFromStride(&outBlock, compBlockIntrm.data, compBlockIntrm.valueStride, compBlockIntrm.valueOffset, compBlockIntrm.valueSize,
                                   SPARSE_BLOCK_VOXEL_COUNT);

                    ComponentVoxelStatistics& dstStatistics = perBlockStatistics[componentBlockIndex];
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

                    ++componentIndex;
                }

                SpatialBlock spatialBlock{};
                spatialBlock.coords[0] = coord.x() - frameAABB.minX;
                spatialBlock.coords[1] = coord.y() - frameAABB.minY;
                spatialBlock.coords[2] = coord.z() - frameAABB.minZ;
                spatialBlock.componentMask = mask;
                spatialBlock.channelCount = spatialIntermediate.blocks.size();
                spatialBlock.channelBlocksOffset = spatialIntermediate.destFirstChannelBlockIndex;
                resultSpatialInfo[spatialIntermediate.destSpatialBlockIndex] = spatialBlock;
            });

        // Resolving concurrently calculated per block voxel statistics to general frame per component voxel statistics.
        for (size_t i = 0; i < perBlockStatistics.size(); ++i)
        {
            const auto componentIndex = result->channelIndexPerBlock[i];
            const auto& compDesc = m_Components[componentIndex];
            auto& dstStatistics = result->info.components[componentIndex];

            dstStatistics.minValue = std::min(dstStatistics.minValue, perBlockStatistics[i].minValue);
            dstStatistics.maxValue = std::max(dstStatistics.maxValue, perBlockStatistics[i].maxValue);
            dstStatistics.meanPositiveValue += perBlockStatistics[i].meanPositiveValue;
            dstStatistics.meanNegativeValue += perBlockStatistics[i].meanNegativeValue;
            // Writing blocks count to voxelCount to use it in the future.
            dstStatistics.voxelCount += 1;
        }
        
        for (size_t i = 0; i < result->info.componentsCount; ++i)
        {
            auto& statistics = result->info.components[i];
            if (statistics.voxelCount == 0)
            {
                statistics.minValue = 0;
                statistics.maxValue = 0;
            }
            else
            {
                statistics.meanPositiveValue /= static_cast<float>(statistics.voxelCount);
                statistics.meanNegativeValue /= static_cast<float>(statistics.voxelCount);
                // Previous cycle has written blocks per component count to voxelCount field.
                statistics.voxelCount *= SPARSE_BLOCK_VOXEL_COUNT;
            }
        }

        return result;
    }

    inline OpenVDBToSparseFrame::OpenVDBToSparseFrame(openvdb::GridBase::ConstPtr* grids, size_t gridsCount, bool matchVoxelSize /*= false*/) noexcept
    {
        if (!gridsCount)
            return;

        m_Channels.reserve(gridsCount);
        m_Components.reserve(gridsCount * 3);

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

        // One logical channel per grid, vec3 grid -> 3 component, scalar grid -> 1 component
        ComponentMask mask = 0x1;
        for (size_t i = 0; i < gridsCount; ++i)
        {
            uint8_t componentCount = {};
            if (processedGrids[i]->baseTree().isType<openvdb::Vec3STree>())
            {
                componentCount = 3;
            }
            else if (processedGrids[i]->baseTree().isType<openvdb::FloatTree>())
            {
                componentCount = 1;
            }
            else
            {
                assert(0 && "Unsupported grid type. Loader supports only floating point grids.");
                m_Channels.clear();
                m_Components.clear();
                return;
            }

            ChannelDescriptor chDesc{};
            chDesc.name = processedGrids[i]->getName();
            chDesc.grid = processedGrids[i];
            chDesc.componentCount = componentCount;
            chDesc.firstComponentIndex = static_cast<uint8_t>(m_Components.size());
            m_Channels.emplace_back(chDesc);

            for (uint8_t componentIndex = 0; componentIndex < componentCount; ++componentIndex)
            {
                ComponentDescriptor compDesc{};
                compDesc.componentMask = mask;
                compDesc.grid = processedGrids[i];
                compDesc.valueSize = sizeof(float);
                compDesc.valueStride = componentCount * sizeof(float);
                compDesc.valueOffset = componentIndex * sizeof(float);
                compDesc.channelIndex = static_cast<uint8_t>(m_Channels.size() - 1);
                m_Components.emplace_back(compDesc);
                mask = mask << 1;
            }
        }
    }

} // namespace Zibra::CE::Addons::OpenVDBUtils
