#pragma once

#include <Zibra/CE/Addons/OpenVDBFrameLoader.h>
#include <Zibra/CE/Compression.h>

#include "DiskFrameCacheManager.h"

namespace Zibra::ZibraVDBCompressor
{
    class CompressorManager
    {
    public:
        CE::ReturnCode Initialize(CE::PlaybackInfo playbackInfo, float defaultQuality,
                                  const std::vector<std::pair<UT_String, float>>& perChannelCompressionSettings) noexcept;
        CE::ReturnCode StartSequence(const UT_String& filename) noexcept;
        CE::ReturnCode CompressFrame(const CE::Compression::CompressFrameDesc& compressFrameDesc,
                                     CE::Compression::FrameManager** frameManager) noexcept;
        CE::ReturnCode FinishSequence(std::string& warning) noexcept;
        void Release() noexcept;

    private:
        CE::Compression::Compressor* m_Compressor = nullptr;
        RHI::RHIRuntime* m_RHIRuntime = nullptr;
        std::ofstream m_Ofstream;
        bool m_IsSequenceEmpty = true;
    };
} // namespace Zibra::ZibraVDBCompressor