#include "PrecompiledHeader.h"

#include "OpenVDBEncoder.h"

#include "MathHelpers.h"

namespace Zibra::OpenVDBSupport
{

    openvdb::GridPtrVec OpenVDBEncoder::EncodeFrame(const CE::Decompression::FrameInfo& frameInfo,
                                                    const DecompressedFrameData& frameData,
                                                    const EncodeMetadata& encodeMetadata) noexcept
    {
        if (frameInfo.channelsCount == 0 || frameInfo.spatialBlockCount == 0 || frameInfo.channelBlockCount == 0)
        {
            return {};
        }

        const uint32_t gridsCount = frameInfo.channelsCount;

        // Create grids.
        openvdb::GridPtrVec grids{};
        grids.reserve(gridsCount);
        for (uint32_t i = 0; i < frameInfo.channelsCount; ++i)
        {
            openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(0.f);
            grid->setName(frameInfo.channels[i].name);

            Math3D::Transform gridTransform = frameInfo.channels[i].gridTransform;
            openvdb::math::Transform::Ptr openVDBTransform = OpenVDBTransformFromMatrix(gridTransform);
            OffsetTransform(openVDBTransform, encodeMetadata);
            grid->setTransform(openVDBTransform);
            grids.push_back(grid);
        }

        // For each channel fill grids.
        std::for_each(std::execution::seq, grids.begin(), grids.end(), [&](auto& baseGrid) {
            const int channelIndex = &baseGrid - grids.data();

            const auto countActiveChannelOffset = [currentChannelMask = 1 << channelIndex](const int blockChannelMask) noexcept -> int {
                if (!(blockChannelMask & currentChannelMask))
                {
                    return -1;
                }
                return int(MathHelpers::CountBits(blockChannelMask & (currentChannelMask - 1)));
            };

            // Calculate leafs of tree.
            using TreeT = openvdb::FloatGrid::TreeType;
            using LeafT = typename TreeT::LeafNodeType;
            std::vector<LeafT*> leafs(frameInfo.spatialBlockCount);

            const auto totalSparseBlockSize = CE::SPARSE_BLOCK_VOXEL_COUNT;

            std::transform(std::execution::par_unseq, frameData.spatialBlocks, frameData.spatialBlocks + frameInfo.spatialBlockCount,
                           leafs.begin(), [&](const CE::Decompression::SpatialBlock& blockInfo) -> LeafT* {
                               const int activeChannelOffset = countActiveChannelOffset(blockInfo.channelMask);
                               if (activeChannelOffset == -1)
                               {
                                   return nullptr;
                               }

                               const CE::Decompression::ChannelBlock& block = frameData.channelBlocks[blockInfo.channelBlocksOffset + activeChannelOffset];

                               const auto x = blockInfo.coords[0];
                               const auto y = blockInfo.coords[1];
                               const auto z = blockInfo.coords[2];

                               const openvdb::Coord blockMin = {x * CE::SPARSE_BLOCK_SIZE + encodeMetadata.offsetX,
                                                                y * CE::SPARSE_BLOCK_SIZE + encodeMetadata.offsetY,
                                                                z * CE::SPARSE_BLOCK_SIZE + encodeMetadata.offsetZ};

                               auto* leaf = new LeafT{openvdb::PartialCreate{}, blockMin, 0.f, true};
                               leaf->allocate();

                               float* leafBuffer = leaf->buffer().data();
                               std::memcpy(leafBuffer, block.voxels, totalSparseBlockSize * sizeof(float));
                               return leaf;
                           });

            // Fill the grid with leafs.
            auto grid = openvdb::gridPtrCast<openvdb::FloatGrid>(baseGrid);
            openvdb::tree::ValueAccessor<TreeT> acc(grid->tree());
            for (auto& leaf : leafs)
            {
                if (leaf)
                {
                    acc.addLeaf(leaf);
                }
            }

            grid->pruneGrid();
        });

        return grids;
    }

    bool OpenVDBEncoder::IsTransformEmpty(const Math3D::Transform& gridTransform)
    {
        static_assert(sizeof(gridTransform.matrix) != sizeof(void*));

        const auto isAlmostZero = [](float v) { return std::abs(v) < std::numeric_limits<float>::epsilon(); };

        return std::all_of(std::begin(gridTransform.raw), std::end(gridTransform.raw), isAlmostZero);
    }

    openvdb::math::Transform::Ptr OpenVDBEncoder::OpenVDBTransformFromMatrix(const Math3D::Transform& gridTransform)
    {
        return openvdb::math::Transform::createLinearTransform(IsTransformEmpty(gridTransform) ? openvdb::Mat4d::identity()
                                                                                               : openvdb::Mat4d{gridTransform.raw});
    }

    void OpenVDBEncoder::OffsetTransform(openvdb::math::Transform::Ptr openVDBTransform, const EncodeMetadata& encodeMetadata)
    {
        const openvdb::math::Vec3d translationFromMetadata(-encodeMetadata.offsetX, -encodeMetadata.offsetY, -encodeMetadata.offsetZ);
        // transform3x3 will apply only 3x3 part of matrix, without translation.
        const openvdb::math::Vec3d frameTranslationInFrameCoordinateSystem =
            openVDBTransform->baseMap()->getAffineMap()->getMat4().transform3x3(translationFromMetadata);
        openVDBTransform->postTranslate(frameTranslationInFrameCoordinateSystem);
    }

} // namespace Zibra::OpenVDBSupport
