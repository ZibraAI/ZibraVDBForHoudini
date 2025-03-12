#include "PrecompiledHeader.h"

#include "CompressorManager.h"

namespace Zibra::CE::Compression
{
    ReturnCode CompressorManager::Initialize(FrameMappingDecs frameMappingDesc, float defaultQuality,
                                             std::vector<std::pair<UT_String, float>> perChannelCompressionSettings) noexcept
    {
        RHI::RHIFactory* RHIFactory = nullptr;
        auto RHIstatus = RHI::CAPI::CreateRHIFactory(&RHIFactory);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        RHIstatus = RHIFactory->Create(&m_RHIRuntime);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        RHIstatus = m_RHIRuntime->Initialize();
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        m_CompressorFactory = CAPI::CreateCompressorFactory();
        if (m_CompressorFactory == nullptr)
        {
            return CE::ZCE_ERROR;
        }

        auto status = m_CompressorFactory->UseRHI(m_RHIRuntime);
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            return status;
        }
        status = m_CompressorFactory->SetFrameMapping(frameMappingDesc);
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            return status;
        }
        status = m_CompressorFactory->SetQuality(defaultQuality);
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            return status;
        }

        for (const auto& [channelName, quality] : perChannelCompressionSettings)
        {
            status = m_CompressorFactory->OverrideChannelQuality(channelName.c_str(), quality);
            if (status != CE::ReturnCode::ZCE_SUCCESS)
            {
                return status;
            }
        }

        status = m_CompressorFactory->Create(&m_Compressor);
        if (status != Zibra::CE::ReturnCode::ZCE_SUCCESS)
        {
            return status;
        }
        m_CompressorFactory->Release();

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
        return CE::ZCE_SUCCESS;
    }

    ReturnCode CompressorManager::CompressFrame(const CompressFrameDesc& compressFrameDesc, FrameManager** frameManager) noexcept
    {
        auto RHIStatus = m_RHIRuntime->StartRecording();
        if (RHIStatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        auto status = m_Compressor->CompressFrame(compressFrameDesc, frameManager);
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            return status;
        }

        RHIStatus = m_RHIRuntime->StopRecording();
        if (RHIStatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }
        return CE::ZCE_SUCCESS;
    }

    ReturnCode CompressorManager::FinishSequence() noexcept
    {
        if (!m_Compressor)
        {
            return CE::ZCE_ERROR;
        }

        Zibra::CE::STDOStreamWrapper ostream(m_Ofstream);
        return m_Compressor->FinishSequence(&ostream);
    }

    void CompressorManager::Release() noexcept
    {
        m_Ofstream.close();
        m_Compressor->Release();
        m_RHIRuntime->Release();
    }


} // namespace Zibra::CE::Decompression
