#pragma once

#include "bridge/CompressionEngine.h"

namespace Zibra::OpenVDBSupport
{
    class OpenVDBEncoder
    {
    public:
        static openvdb::GridPtrVec EncodeFrame(const CompressionEngine::ZCE_SparseFrameData& frame) noexcept;
    };
} // namespace Zibra::OpenVDBSupport