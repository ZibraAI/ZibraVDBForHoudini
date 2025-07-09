#pragma once

#include <Zibra/CE/Addons/OpenVDBFrameLoader.h>
#include <Zibra/CE/Compression.h>

namespace Zibra::CE::Compression
{
    class CompressorManager
    {
    public:
        ReturnCode Initialize(PlaybackInfo playbackInfo, float defaultQuality,
                              const std::vector<std::pair<UT_String, float>>& perChannelCompressionSettings) noexcept;
        ReturnCode StartSequence(const UT_String& filename) noexcept;
        ReturnCode CompressFrame(const CompressFrameDesc& compressFrameDesc, FrameManager** frameManager) noexcept;
        ReturnCode FinishSequence(std::string& warning) noexcept;
        void Release() noexcept;

    private:
        Compressor* m_Compressor = nullptr;
        RHI::RHIRuntime* m_RHIRuntime = nullptr;
        std::ofstream m_Ofstream;
        bool m_IsSequenceEmpty = true;
    };
} // namespace Zibra