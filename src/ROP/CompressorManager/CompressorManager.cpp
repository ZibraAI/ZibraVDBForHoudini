#include "CompressorManager.h"

namespace Zibra::CE::Compression
{
    ReturnCode CompressorManager::Initialize(FrameMappingDecs frameMappingDesc, float defaultQuality,
                                             std::unordered_map<const char*, float> perChannelCompressionSettings) noexcept
    {
        RHI::CAPI::CreateRHIFactory(&m_RHIFactory);
        m_RHIFactory->Create(&m_RHIRuntime);
        m_RHIRuntime->Initialize();

        m_Factory = CAPI::CreateCompressorFactory();
        if (m_Factory == nullptr)
        {
            return CE::ZCE_ERROR;
        }

        auto status = m_Factory->UseRHI(m_RHIRuntime);
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            return status;
        }
        m_Factory->SetFrameMapping(frameMappingDesc);
        m_Factory->SetQuality(defaultQuality);

        for (const auto& [channelName, quality] : perChannelCompressionSettings)
        {
            m_Factory->OverrideChannelQuality(channelName, quality);
        }

        status = m_Factory->Create(&m_Compressor);
        if (status != Zibra::CE::ReturnCode::ZCE_SUCCESS)
        {
            return status;
        }
        m_Factory->Release();

        status = m_Compressor->Initialize();
        if (status != Zibra::CE::ReturnCode::ZCE_SUCCESS)
        {
            return status;
        }
        return CE::ZCE_SUCCESS;
    }

    ReturnCode CompressorManager::StartSequence(const UT_String& filename) noexcept
    {
        auto status = m_Compressor->StartSequence();
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            return status;
        }

        m_Ofstream.open(filename, std::ios::binary);
        if (!m_Ofstream.is_open())
        {
            return CE::ZCE_ERROR;
        }
    }

    ReturnCode CompressorManager::CompressFrame(const CompressFrameDesc& compressFrameDesc, FrameManager* frameManager) noexcept
    {
        m_RHIRuntime->StartRecording();

        auto status = m_Compressor->CompressFrame(compressFrameDesc, &frameManager);
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            return status;
        }

        m_RHIRuntime->StopRecording();
    }

    ReturnCode CompressorManager::FinishSequence() noexcept
    {
        Zibra::CE::STDOStreamWrapper ostream(m_Ofstream);
        return m_Compressor->FinishSequence(&ostream);
    }

    void CompressorManager::Release() noexcept
    {
        m_Ofstream.close();
        m_Compressor->Release();
        m_RHIRuntime->Release();
        m_RHIFactory->Release();
    }


} // namespace Zibra::CE::Decompression
