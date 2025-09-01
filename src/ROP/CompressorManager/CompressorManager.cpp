#include "PrecompiledHeader.h"

#include "CompressorManager.h"

#include "bridge/LibraryUtils.h"
#include "utils/Helpers.h"

namespace Zibra::CE::Compression
{
    ReturnCode CompressorManager::Initialize(FrameMappingDecs frameMappingDesc, float defaultQuality,
                                             const std::vector<std::pair<UT_String, float>>& perChannelCompressionSettings) noexcept
    {
        LibraryUtils::LoadZibSDKLibrary();
        if (!Zibra::LibraryUtils::IsZibSDKLoaded())
        {
            return CE::ZCE_ERROR;
        }

        RHI::RHIFactory* RHIFactory = nullptr;
        auto RHIstatus = RHI::CAPI::CreateRHIFactory(&RHIFactory);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        RHIstatus = RHIFactory->SetGFXAPI(Helpers::SelectGFXAPI());
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        if (Helpers::NeedForceSoftwareDevice())
        {
            RHIstatus = RHIFactory->ForceSoftwareDevice();
            if (RHIstatus != RHI::ZRHI_SUCCESS)
            {
                return CE::ZCE_ERROR;
            }
        }

        RHIstatus = RHIFactory->Create(&m_RHIRuntime);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            RHIFactory->Release();
            return CE::ZCE_ERROR;
        }
        RHIFactory->Release();

        RHIstatus = m_RHIRuntime->Initialize();
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            m_RHIRuntime->Release();
            m_RHIRuntime = nullptr;
            return CE::ZCE_ERROR;
        }

        CompressorFactory* compressorFactory = nullptr;
        compressorFactory = CAPI::CreateCompressorFactory();
        if (compressorFactory == nullptr)
        {
            return CE::ZCE_ERROR;
        }

        auto status = compressorFactory->UseRHI(m_RHIRuntime);
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            compressorFactory->Release();
            return status;
        }
        status = compressorFactory->SetFrameMapping(frameMappingDesc);
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            compressorFactory->Release();
            return status;
        }
        status = compressorFactory->SetQuality(defaultQuality);
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            compressorFactory->Release();
            return status;
        }

        for (const auto& [channelName, quality] : perChannelCompressionSettings)
        {
            status = compressorFactory->OverrideChannelQuality(channelName.c_str(), quality);
            if (status != CE::ReturnCode::ZCE_SUCCESS)
            {
                compressorFactory->Release();
                return status;
            }
        }

        status = compressorFactory->Create(&m_Compressor);
        if (status != Zibra::CE::ReturnCode::ZCE_SUCCESS)
        {
            compressorFactory->Release();
            return status;
        }
        compressorFactory->Release();

        status = m_Compressor->Initialize();
        if (status != Zibra::CE::ReturnCode::ZCE_SUCCESS)
        {
            return status;
        }
        return CE::ZCE_SUCCESS;
    }

    ReturnCode CompressorManager::StartSequence(const UT_String& filename) noexcept
    {
        if (!m_Compressor)
        {
            return CE::ZCE_ERROR;
        }

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
        if (!m_RHIRuntime || !m_Compressor)
        {
            return CE::ZCE_ERROR;
        }

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

        if (compressFrameDesc.channelsCount != 0)
        {
            m_IsSequenceEmpty = false;
        }

        // Force GPU memory cleanup after each frame
        m_RHIRuntime->GarbageCollect();

        return CE::ZCE_SUCCESS;
    }

    ReturnCode CompressorManager::FinishSequence(std::string& warning) noexcept
    {
        if (!m_Compressor)
        {
            return CE::ZCE_ERROR;
        }

        Zibra::STDOStreamWrapper ostream(m_Ofstream);
        if (ostream.fail())
        {
            return CE::ZCE_ERROR;
        }

        if (m_IsSequenceEmpty)
        {
            warning = "Sequence is empty. No grids were compressed.";
        }

        return m_Compressor->FinishSequence(&ostream);
    }

    void CompressorManager::Release() noexcept
    {
        m_Ofstream.close();
        if (m_Compressor)
        {
            m_Compressor->Release();
            m_Compressor = nullptr;
        }
        if (m_RHIRuntime)
        {
            // Force aggressive cleanup before releasing RHI
            m_RHIRuntime->GarbageCollect();
            m_RHIRuntime->Release();
            m_RHIRuntime = nullptr;
        }
    }

} // namespace Zibra::CE::Compression
