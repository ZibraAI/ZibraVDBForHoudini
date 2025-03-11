#pragma once

#include <Zibra/CE/Decompression.h>
#include "openvdb/OpenVDBEncoder.h"
#include "../../PrecompiledHeader.h"

namespace Zibra::CE::Decompression
{
    class DecompressorManager
    {
    public:
        void Initialize() noexcept;
        ReturnCode RegisterDecompressor(const UT_String& filename) noexcept;
        ReturnCode DecompressFrame(CE::Decompression::CompressedFrameContainer* frameContainer) noexcept;
        OpenVDBSupport::DecompressedFrameData GetDecompressedFrameData(const CE::Decompression::FrameInfo& frameInfo) const noexcept;
        CompressedFrameContainer* FetchFrame(const exint& frameIndex) const noexcept;
        FrameRange GetFrameRange() const noexcept;
        void Release() noexcept;

    private:
        void AllocateExternalBuffers();
        void FreeExternalBuffers();

    private:
        DecompressorFactory* m_Factory = nullptr;
        Zibra::CE::ZibraVDB::FileDecoder* m_Decoder = nullptr;
        Decompressor* m_Decompressor = nullptr;
        CAPI::FormatMapperCAPI* m_FormatMapper = nullptr;
        RHI::RHIFactory* m_RHIFactory = nullptr;
        RHI::RHIRuntime* m_RHIRuntime = nullptr;

        CE::Decompression::DecompressorResources m_DecompressorResources;
    };
} // namespace Zibra