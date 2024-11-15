#pragma once

#include "bridge/CompressionEngine.h"

namespace Zibra::OpenVDBSupport
{
    class OpenVDBEncoder
    {
    public:
        static openvdb::GridPtrVec EncodeFrame(const CompressionEngine::ZCE_FrameInfo& frameInfo,
                                               const CompressionEngine::ZCE_DecompressedFrameData& frameData) noexcept;

    private:
        static bool IsTransformEmpty(const CompressionEngine::ZCE_Transform& gridTransform);
        static openvdb::math::Transform::Ptr OpenVDBTransformFromMatrix(const CompressionEngine::ZCE_Transform& gridTransform);
    };
} // namespace Zibra::OpenVDBSupport