#pragma once

namespace Zibra::OpenVDBSupport
{
    struct EncodeMetadata
    {
        int32_t offsetX = 0;
        int32_t offsetY = 0;
        int32_t offsetZ = 0;
    };

    struct DecompressedFrameData
    {
        CE::Decompression::SpatialBlock* spatialBlocks;
        CE::Decompression::ChannelBlock* channelBlocks;
    };

    class OpenVDBEncoder
    {
    public:
        static openvdb::GridPtrVec CreateGrids(const CE::Decompression::FrameInfo& frameInfo,
                                               const EncodeMetadata& encodeMetadata) noexcept;
        static void EncodeFrame(openvdb::GridPtrVec& grids, const CE::Decompression::DecompressFrameDesc& desc,
                                const CE::Decompression::DecompressedFrameFeedback& feedback, const DecompressedFrameData& frameData,
                                const EncodeMetadata& encodeMetadata) noexcept;

    private:
        static bool IsTransformEmpty(const Math3D::Transform& gridTransform);
        static openvdb::math::Transform::Ptr OpenVDBTransformFromMatrix(const Math3D::Transform& gridTransform);
        static void OffsetTransform(openvdb::math::Transform::Ptr& gridTransform, const EncodeMetadata& encodeMetadata);
    };
} // namespace Zibra::OpenVDBSupport