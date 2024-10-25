#pragma once

#include "bridge/CompressionEngine.h"

namespace Zibra::OpenVDBSupport
{
    class OpenVDBEncoder
    {
    public:
        static openvdb::GridPtrVec EncodeFrame(const CompressionEngine::ZCE_FrameInfo& frameInfo,
                                               const CompressionEngine::ZCE_DecompressedFrameData& frameData) noexcept;
    };
} // namespace Zibra::OpenVDBSupport