#include "PrecompiledHeader.h"

#include "DecompressorManager.h"

namespace Zibra::CE::Decompression
{

    ReturnCode DecompressorManager::Initialize() noexcept
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

        auto status = CAPI::CreateDecompressorFactory(&m_DecompressorFactory);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }
        status = m_DecompressorFactory->UseRHI(m_RHIRuntime);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }
        return CE::ZCE_SUCCESS;
    }

    ReturnCode DecompressorManager::RegisterDecompressor(const UT_String& filename) noexcept
    {
        if (m_Decoder)
        {
            CE::Decompression::CAPI::ReleaseDecoder(m_Decoder);
            m_Decoder = nullptr;
        }

        auto status = CE::Decompression::CAPI::CreateDecoder(filename.c_str(), &m_Decoder);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }

        status = m_DecompressorFactory->UseDecoder(m_Decoder);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }

        if (m_Decompressor)
        {
            status = FreeExternalBuffers();
            if (status != CE::ZCE_SUCCESS)
            {
                return status;
            }
            m_FormatMapper->Release();
            m_FormatMapper = nullptr;
            m_Decompressor->Release();
            m_Decompressor = nullptr;
        }

        status = m_DecompressorFactory->Create(&m_Decompressor);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }

        status = m_Decompressor->Initialize();
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }

        m_FormatMapper = static_cast<CE::Decompression::CAPI::FormatMapperCAPI*>(m_Decompressor->GetFormatMapper());
        if (!m_FormatMapper)
        {
            return status;
        }

        status = AllocateExternalBuffers();
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }

        status = m_Decompressor->RegisterResources(m_DecompressorResources);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }
        return CE::ZCE_SUCCESS;
    }

    ReturnCode DecompressorManager::DecompressFrame(CE::Decompression::CompressedFrameContainer* frameContainer) noexcept
    {
        m_RHIRuntime->StartRecording();
        ReturnCode status = m_Decompressor->DecompressFrame(frameContainer);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }
        m_RHIRuntime->StopRecording();
        m_RHIRuntime->GarbageCollect();
        return CE::ZCE_SUCCESS;
    }

    static float Float16ToFloat32(uint16_t float16Value) noexcept
    {
        uint32_t t1, t2, t3;

        t1 = float16Value & 0x7fffu; // Non-sign bits
        t2 = float16Value & 0x8000u; // Sign bit
        t3 = float16Value & 0x7c00u; // Exponent

        t1 <<= 13u; // Align mantissa on MSB
        t2 <<= 16u; // Shift sign bit into position

        t1 += 0x38000000; // Adjust bias

        t1 = (t3 == 0 ? 0 : t1); // Denormals-as-zero

        t1 |= t2; // Re-insert sign bit

        uint32_t float32_value = t1;

        return *(reinterpret_cast<float*>(&float32_value));
    }

    static void UnpackBlocks(OpenVDBSupport::DecompressedFrameData& decompressedBlockData, const CE::Decompression::FrameInfo& frameInfo,
                             const std::vector<uint32_t>& scratchBufferSpatialBlockData,
                             const std::vector<uint16_t>& scratchBufferChannelBlockData) noexcept
    {
        for (size_t i = 0; i < frameInfo.channelBlockCount; ++i)
        {
            for (size_t j = 0; j < CE::SPARSE_BLOCK_VOXEL_COUNT; ++j)
            {
                size_t bufIdx = i * CE::SPARSE_BLOCK_VOXEL_COUNT + j;
                decompressedBlockData.channelBlocks[i].voxels[j] = Float16ToFloat32(scratchBufferChannelBlockData[bufIdx]);
            }
        }

        for (size_t i = 0; i < frameInfo.spatialBlockCount; ++i)
        {
            uint32_t packedCoords = scratchBufferSpatialBlockData[i * 3];
            decompressedBlockData.spatialBlocks[i].coords[0] = int32_t((packedCoords >> 0u) & 1023u);
            decompressedBlockData.spatialBlocks[i].coords[1] = int32_t((packedCoords >> 10u) & 1023u);
            decompressedBlockData.spatialBlocks[i].coords[2] = int32_t((packedCoords >> 20u) & 1023u);
            decompressedBlockData.spatialBlocks[i].channelBlocksOffset = scratchBufferSpatialBlockData[i * 3 + 1];
            decompressedBlockData.spatialBlocks[i].channelMask = scratchBufferSpatialBlockData[i * 3 + 2];
        }
    }

    OpenVDBSupport::DecompressedFrameData DecompressorManager::GetDecompressedFrameData(
        const CE::Decompression::FrameInfo& frameInfo) const noexcept
    {
        m_RHIRuntime->StartRecording();

        OpenVDBSupport::DecompressedFrameData decompressedFrameData;

        decompressedFrameData.channelBlocks = new CE::Decompression::ChannelBlock[frameInfo.channelBlockCount];
        decompressedFrameData.spatialBlocks = new CE::Decompression::SpatialBlock[frameInfo.spatialBlockCount];

        const size_t spatialBlockInfoElementCount = frameInfo.spatialBlockCount * 3;
        std::vector<uint32_t> scratchBufferSpatialBlockData(spatialBlockInfoElementCount);
        m_RHIRuntime->GetBufferDataImmediately(m_DecompressorResources.decompressionPerSpatialBlockInfo,
                                               scratchBufferSpatialBlockData.data(), spatialBlockInfoElementCount * sizeof(uint32_t), 0);
        const size_t channelBlockDataElementCount = frameInfo.channelBlockCount * CE::SPARSE_BLOCK_VOXEL_COUNT;
        std::vector<uint16_t> scratchBufferChannelBlockData(channelBlockDataElementCount);
        m_RHIRuntime->GetBufferDataImmediately(m_DecompressorResources.decompressionPerChannelBlockData,
                                               scratchBufferChannelBlockData.data(), channelBlockDataElementCount * sizeof(uint16_t), 0);

        m_RHIRuntime->StopRecording();

        UnpackBlocks(decompressedFrameData, frameInfo, scratchBufferSpatialBlockData, scratchBufferChannelBlockData);

        return decompressedFrameData;
    }

    CompressedFrameContainer* DecompressorManager::FetchFrame(const exint& frameIndex) const noexcept
    {
        CompressedFrameContainer* frameContainer = nullptr;
        m_FormatMapper->FetchFrame(frameIndex, &frameContainer);
        return frameContainer;
    }

    FrameRange DecompressorManager::GetFrameRange() const noexcept
    {
        return m_FormatMapper->GetFrameRange();
    }

    ReturnCode DecompressorManager::AllocateExternalBuffers()
    {
        DecompressorResourcesRequirements requirements = m_Decompressor->GetResourcesRequirements();
        auto RHIstatus = m_RHIRuntime->CreateBuffer(
            requirements.decompressionPerChannelBlockDataSizeInBytes, RHI::ResourceHeapType::Default,
            RHI::ResourceUsage::UnorderedAccess | RHI::ResourceUsage::ShaderResource | RHI::ResourceUsage::CopySource,
            requirements.decompressionPerChannelBlockDataStride, "decompressionPerChannelBlockData",
            &m_DecompressorResources.decompressionPerChannelBlockData);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }
        RHIstatus = m_RHIRuntime->CreateBuffer(requirements.decompressionPerChannelBlockInfoSizeInBytes, RHI::ResourceHeapType::Default,
                                               RHI::ResourceUsage::UnorderedAccess | RHI::ResourceUsage::ShaderResource |
                                                   RHI::ResourceUsage::CopySource,
                                               requirements.decompressionPerChannelBlockInfoStride, "decompressionPerChannelBlockInfo",
                                               &m_DecompressorResources.decompressionPerChannelBlockInfo);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }
        RHIstatus = m_RHIRuntime->CreateBuffer(requirements.decompressionPerSpatialBlockInfoSizeInBytes, RHI::ResourceHeapType::Default,
                                               RHI::ResourceUsage::UnorderedAccess | RHI::ResourceUsage::ShaderResource |
                                                   RHI::ResourceUsage::CopySource,
                                               requirements.decompressionPerSpatialBlockInfoStride, "decompressionPerSpatialBlockInfo",
                                               &m_DecompressorResources.decompressionPerSpatialBlockInfo);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }
        RHIstatus = m_RHIRuntime->CreateBuffer(
            requirements.decompressionSpatialToChannelIndexLookupSizeInBytes, RHI::ResourceHeapType::Default,
            RHI::ResourceUsage::UnorderedAccess | RHI::ResourceUsage::ShaderResource | RHI::ResourceUsage::CopySource,
            requirements.decompressionSpatialToChannelIndexLookupStride, "decompressionSpatialToChannelIndexLookup",
            &m_DecompressorResources.decompressionSpatialToChannelIndexLookup);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }
        return CE::ZCE_SUCCESS;
    }

    ReturnCode DecompressorManager::FreeExternalBuffers()
    {
        RHI::ReturnCode RHIstatus;
        if (m_DecompressorResources.decompressionPerChannelBlockData)
        {
            RHIstatus = m_RHIRuntime->ReleaseBuffer(m_DecompressorResources.decompressionPerChannelBlockData);
            if (RHIstatus != RHI::ZRHI_SUCCESS)
            {
                return CE::ZCE_ERROR;
            }
        }
        if (m_DecompressorResources.decompressionPerChannelBlockInfo)
        {
            RHIstatus = m_RHIRuntime->ReleaseBuffer(m_DecompressorResources.decompressionPerChannelBlockInfo);
            if (RHIstatus != RHI::ZRHI_SUCCESS)
            {
                return CE::ZCE_ERROR;
            }
        }
        if (m_DecompressorResources.decompressionPerSpatialBlockInfo)
        {
            RHIstatus = m_RHIRuntime->ReleaseBuffer(m_DecompressorResources.decompressionPerSpatialBlockInfo);
            if (RHIstatus != RHI::ZRHI_SUCCESS)
            {
                return CE::ZCE_ERROR;
            }
        }
        if (m_DecompressorResources.decompressionSpatialToChannelIndexLookup)
        {
            RHIstatus = m_RHIRuntime->ReleaseBuffer(m_DecompressorResources.decompressionSpatialToChannelIndexLookup);
            if (RHIstatus != RHI::ZRHI_SUCCESS)
            {
                return CE::ZCE_ERROR;
            }
        }
        return CE::ZCE_SUCCESS;
    }

    void DecompressorManager::Release() noexcept
    {
        FreeExternalBuffers();
        if (m_Decompressor)
        {
            m_Decompressor->Release();
            m_Decompressor = nullptr;
        }
        if (m_DecompressorFactory)
        {
            m_DecompressorFactory->Release();
            m_DecompressorFactory = nullptr;
        }
        if (m_RHIRuntime)
        {
            m_RHIRuntime->Release();
            m_RHIRuntime = nullptr;
        }
    }

} // namespace Zibra::CE::Decompression
