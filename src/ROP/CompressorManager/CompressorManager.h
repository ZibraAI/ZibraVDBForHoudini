#pragma once

#include <Zibra/CE/Compression.h>
#include "Globals.h"

namespace Zibra::CE::Compression
{
    class CompressorManager
    {
    public:
        ReturnCode Initialize(FrameMappingDecs frameMappingDesc, float defaultQuality,
                              std::vector<std::pair<UT_String, float>> perChannelCompressionSettings) noexcept;
        ReturnCode StartSequence(const UT_String& filename) noexcept;
        ReturnCode CompressFrame(const CompressFrameDesc& compressFrameDesc, FrameManager** frameManager) noexcept;
        ReturnCode FinishSequence() noexcept;
        void Release() noexcept;

    private:
        CompressorFactory* m_CompressorFactory = nullptr;
        Compressor* m_Compressor = nullptr;
        RHI::RHIRuntime* m_RHIRuntime = nullptr;
        std::ofstream m_Ofstream;

    };
} // namespace Zibra