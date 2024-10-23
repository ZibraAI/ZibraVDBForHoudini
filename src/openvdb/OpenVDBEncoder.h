#pragma once

#include "bridge/CompressionEngine.h"

namespace Zibra::OpenVDBSupport
{
    class OpenVDBEncoder
    {
    public:
        static openvdb::GridPtrVec EncodeFrame(const CompressionEngine::ZCE_SparseFrameData& frame) noexcept;

    private:
        static bool IsTransformEmpty(const CompressionEngine::ZCE_Transform& gridTransform);
    };
} // namespace Zibra::OpenVDBSupport