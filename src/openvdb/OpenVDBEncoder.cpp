#include "PrecompiledHeader.h"

#include "OpenVDBEncoder.h"

#include "MathHelpers.h"

namespace Zibra::OpenVDBSupport
{

    openvdb::GridPtrVec OpenVDBEncoder::EncodeFrame(const CompressionEngine::ZCE_FrameInfo& frameInfo,
                                                    const CompressionEngine::ZCE_DecompressedFrameData& frameData) noexcept
    {
        if (frameInfo.channelCount == 0 || frameInfo.spatialBlockCount == 0 || frameInfo.channelBlockCount == 0)
        {
            return {};
        }

        openvdb::math::Transform::Ptr transform =
            openvdb::math::Transform::createLinearTransform(openvdb::Mat4d{frameInfo.perChannelInfo[0].gridTransform.raw});

        const uint32_t gridsCount = frameInfo.channelCount;

        // Create grids.
        openvdb::GridPtrVec grids{};
        grids.reserve(gridsCount);
        for (uint32_t i = 0; i < frameInfo.channelCount; ++i)
        {
            openvdb::FloatGrid::Ptr grid = openvdb::FloatGrid::create(0.f);
            grid->setName(frameInfo.channelNames[i]);
            grid->setTransform(transform);
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

            const auto sparseBlockSizeLog2 = ZIB_BLOCK_SIZE_LOG_2;
            const auto totalSparseBlockSize = ZIB_BLOCK_ELEMENT_COUNT;
            const auto totalSparseBlockSizeLog2 = 3 * sparseBlockSizeLog2;

            std::transform(std::execution::par_unseq, frameData.spatialBlocks, frameData.spatialBlocks + frameInfo.spatialBlockCount,
                           leafs.begin(), [&](const CompressionEngine::ZCE_SpatialBlock& blockInfo) -> LeafT* {
                               const int activeChannelOffset = countActiveChannelOffset(blockInfo.channelMask);
                               if (activeChannelOffset == -1)
                               {
                                   return nullptr;
                               }

                               const CompressionEngine::ZCE_ChannelBlock& block =
                                   frameData.channelBlocks[blockInfo.channelBlocksOffset + activeChannelOffset];

                               const auto x = blockInfo.coords[0];
                               const auto y = blockInfo.coords[1];
                               const auto z = blockInfo.coords[2];

                               const openvdb::Coord blockMin = {x * ZIB_BLOCK_SIZE, y * ZIB_BLOCK_SIZE, z * ZIB_BLOCK_SIZE};

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
        });

        return grids;
    }

} // namespace Zibra::OpenVDBSupport
