#pragma once

namespace Zibra::OpenVDBSupport
{
    struct EncodeMetadata
    {
        int32_t offsetX = 0;
        int32_t offsetY = 0;
        int32_t offsetZ = 0;
    };

    class OpenVDBEncoder
    {
    public:
        static openvdb::GridPtrVec EncodeFrame(const CompressionEngine::ZCE_FrameInfo& frameInfo,
                                               const CompressionEngine::ZCE_DecompressedFrameData& frameData,
                                               const EncodeMetadata& encodeMetadata) noexcept;

    private:
        static bool IsTransformEmpty(const CompressionEngine::ZCE_Transform& gridTransform);
        static openvdb::math::Transform::Ptr OpenVDBTransformFromMatrix(const CompressionEngine::ZCE_Transform& gridTransform);
        static void OffsetTransform(openvdb::math::Transform::Ptr gridTransform, const EncodeMetadata& encodeMetadata);
    };
} // namespace Zibra::OpenVDBSupport