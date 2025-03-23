#include "PrecompiledHeader.h"

#include "DecompressorManager.h"

#include "bridge/LibraryUtils.h"

namespace Zibra::Helpers
{
    CE::ReturnCode DecompressorManager::Initialize() noexcept
    {
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            return CE::ZCE_ERROR;
        }

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
        RHIFactory->Release();

        RHIstatus = m_RHIRuntime->Initialize();
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            m_RHIRuntime->Release();
            m_RHIRuntime = nullptr;
            return CE::ZCE_ERROR;
        }

        auto status = CE::Decompression::CAPI::CreateDecompressorFactory(&m_DecompressorFactory);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }
        status = m_DecompressorFactory->UseRHI(m_RHIRuntime);
        if (status != CE::ZCE_SUCCESS)
        {
            m_DecompressorFactory->Release();
            m_DecompressorFactory = nullptr;
            return status;
        }
        return CE::ZCE_SUCCESS;
    }

    CE::ReturnCode DecompressorManager::AllocateExternalBuffer(BufferDesc& bufferDesc, size_t newSizeInBytes, size_t newStride) noexcept
    {
        if (bufferDesc.sizeInBytes != newSizeInBytes || bufferDesc.stride != newStride)
        {
            if (bufferDesc.buffer)
            {
                auto RHIstatus = m_RHIRuntime->ReleaseBuffer(bufferDesc.buffer);
                if (RHIstatus != RHI::ZRHI_SUCCESS)
                {
                    return CE::ZCE_ERROR;
                }
            }
            auto RHIstatus = m_RHIRuntime->CreateBuffer(newSizeInBytes, RHI::ResourceHeapType::Default,
                                                        RHI::ResourceUsage::UnorderedAccess | RHI::ResourceUsage::ShaderResource |
                                                            RHI::ResourceUsage::CopySource,
                                                        newStride, "decompressionPerChannelBlockData", &bufferDesc.buffer);
            if (RHIstatus != RHI::ZRHI_SUCCESS)
            {
                return CE::ZCE_ERROR;
            }
            bufferDesc.sizeInBytes = newSizeInBytes;
            bufferDesc.stride = newStride;
        }
        return CE::ZCE_SUCCESS;
    }

    CE::ReturnCode DecompressorManager::RegisterDecompressor(const UT_String& filename) noexcept
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

        if (!m_DecompressorFactory)
        {
            return CE::ZCE_ERROR;
        }

        status = m_DecompressorFactory->UseDecoder(m_Decoder);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }

        if (m_FormatMapper)
        {
            m_FormatMapper->Release();
            m_FormatMapper = nullptr;
        }

        if (m_Decompressor)
        {

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

        CE::Decompression::DecompressorResourcesRequirements newRequirements = m_Decompressor->GetResourcesRequirements();
        status =
            AllocateExternalBuffer(m_DecompressionPerChannelBlockDataBuffer, newRequirements.decompressionPerChannelBlockDataSizeInBytes,
                                   newRequirements.decompressionPerChannelBlockDataStride);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }
        status =
            AllocateExternalBuffer(m_DecompressionPerChannelBlockInfoBuffer, newRequirements.decompressionPerChannelBlockInfoSizeInBytes,
                                   newRequirements.decompressionPerChannelBlockInfoStride);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }
        status =
            AllocateExternalBuffer(m_DecompressionPerSpatialBlockInfoBuffer, newRequirements.decompressionPerSpatialBlockInfoSizeInBytes,
                                   newRequirements.decompressionPerSpatialBlockInfoStride);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }

        CE::Decompression::DecompressorResources decompressorResources{};
        decompressorResources.decompressionPerChannelBlockData = m_DecompressionPerChannelBlockDataBuffer.buffer;
        decompressorResources.decompressionPerChannelBlockInfo = m_DecompressionPerChannelBlockInfoBuffer.buffer;
        decompressorResources.decompressionPerSpatialBlockInfo = m_DecompressionPerSpatialBlockInfoBuffer.buffer;
        status = m_Decompressor->RegisterResources(decompressorResources);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }
        return CE::ZCE_SUCCESS;
    }

    CE::ReturnCode DecompressorManager::DecompressFrame(CE::Decompression::CompressedFrameContainer* frameContainer, openvdb::GridPtrVec* vdbGrids) noexcept
    {
        if (!m_RHIRuntime || !m_Decompressor)
        {
            return CE::ZCE_ERROR;
        }
        auto RHIstatus = m_RHIRuntime->StartRecording();
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }
        auto frameInfo = frameContainer->GetInfo();

        CE::Decompression::DecompressFrameDesc decompressDesc{};
        decompressDesc.firstSpatialBlockIndex = 0;
        decompressDesc.spatialBlocksCount = frameInfo.spatialBlockCount;
        decompressDesc.decompressionPerChannelBlockDataOffset = 0;
        decompressDesc.decompressionPerChannelBlockInfoOffset = 0;
        decompressDesc.decompressionPerSpatialBlockInfoOffset = 0;
        decompressDesc.frameContainer = frameContainer;

        CE::Decompression::DecompressedFrameFeedback frameFeedback;
        CE::Decompression::ReturnCode status = m_Decompressor->DecompressFrame(decompressDesc, &frameFeedback);
        if (status != CE::ZCE_SUCCESS)
        {
            return status;
        }
        RHIstatus = m_RHIRuntime->StopRecording();
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        CE::Addons::OpenVDBUtils::OpenVDBEncoder::FrameData frameData{};
        GetDecompressedFrameData(frameInfo, &frameData);

        CE::Addons::OpenVDBUtils::OpenVDBEncoder vdbEncoder{};
        openvdb::GridPtrVec girds = vdbEncoder.Encode(decompressDesc, frameFeedback, frameInfo, frameData);

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

    static void UnpackBlocks(const CE::Decompression::FrameInfo& frameInfo,
                             const std::vector<uint32_t>& scratchBufferSpatialBlockData,
                             const std::vector<uint16_t>& scratchBufferChannelBlockData,
                             CE::Addons::OpenVDBUtils::OpenVDBEncoder::FrameData* frameData) noexcept
    {
        for (size_t i = 0; i < frameInfo.channelBlockCount; ++i)
        {
            for (size_t j = 0; j < CE::SPARSE_BLOCK_VOXEL_COUNT; ++j)
            {
                size_t bufIdx = i * CE::SPARSE_BLOCK_VOXEL_COUNT + j;
                frameData->decompressionPerChannelBlockData[i].voxels[j] = Float16ToFloat32(scratchBufferChannelBlockData[bufIdx]);
            }
        }

        for (size_t i = 0; i < frameInfo.spatialBlockCount; ++i)
        {
            uint32_t packedCoords = scratchBufferSpatialBlockData[i * 3];
            frameData->decompressionPerSpatialBlockInfo[i].coords[0] = int32_t((packedCoords >> 0u) & 1023u);
            frameData->decompressionPerSpatialBlockInfo[i].coords[1] = int32_t((packedCoords >> 10u) & 1023u);
            frameData->decompressionPerSpatialBlockInfo[i].coords[2] = int32_t((packedCoords >> 20u) & 1023u);
            frameData->decompressionPerSpatialBlockInfo[i].channelBlocksOffset = scratchBufferSpatialBlockData[i * 3 + 1];
            frameData->decompressionPerSpatialBlockInfo[i].channelMask = scratchBufferSpatialBlockData[i * 3 + 2];
        }
    }

    CE::ReturnCode DecompressorManager::GetDecompressedFrameData(const CE::Decompression::FrameInfo& frameInfo, CE::Addons::OpenVDBUtils::OpenVDBEncoder::FrameData* outDecompressedFrameData) const noexcept
    {
        if (!m_RHIRuntime)
        {
            return CE::ZCE_ERROR;
        }

        auto RHIstatus = m_RHIRuntime->StartRecording();
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        *outDecompressedFrameData = CE::Addons::OpenVDBUtils::OpenVDBEncoder::FrameData{};
        outDecompressedFrameData->decompressionPerChannelBlockData.resize(frameInfo.channelBlockCount);
        outDecompressedFrameData->decompressionPerSpatialBlockInfo.resize(frameInfo.spatialBlockCount);

        const size_t spatialBlockInfoElementCount = frameInfo.spatialBlockCount * 3;
        std::vector<uint32_t> scratchBufferSpatialBlockData(spatialBlockInfoElementCount);
        RHIstatus =
            m_RHIRuntime->GetBufferDataImmediately(m_DecompressionPerSpatialBlockInfoBuffer.buffer, scratchBufferSpatialBlockData.data(),
                                                   spatialBlockInfoElementCount * sizeof(uint32_t), 0);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }
        const size_t channelBlockDataElementCount = frameInfo.channelBlockCount * CE::SPARSE_BLOCK_VOXEL_COUNT;
        std::vector<uint16_t> scratchBufferChannelBlockData(channelBlockDataElementCount);
        RHIstatus =
            m_RHIRuntime->GetBufferDataImmediately(m_DecompressionPerChannelBlockDataBuffer.buffer, scratchBufferChannelBlockData.data(),
                                                   channelBlockDataElementCount * sizeof(uint16_t), 0);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        RHIstatus = m_RHIRuntime->StopRecording();
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        UnpackBlocks(frameInfo, scratchBufferSpatialBlockData, scratchBufferChannelBlockData, outDecompressedFrameData);
        return CE::ZCE_SUCCESS;
    }

    CE::Decompression::CompressedFrameContainer* DecompressorManager::FetchFrame(const exint& frameIndex) const noexcept
    {
        if (!m_FormatMapper)
        {
            return nullptr;
        }
        CE::Decompression::CompressedFrameContainer* frameContainer = nullptr;
        auto status = m_FormatMapper->FetchFrame(frameIndex, &frameContainer);
        if (status != CE::ZCE_SUCCESS)
        {
            return nullptr;
        }
        return frameContainer;
    }

    CE::Decompression::FrameRange DecompressorManager::GetFrameRange() const noexcept
    {
        if (!m_FormatMapper)
        {
            return {};
        }
        return m_FormatMapper->GetFrameRange();
    }

    CE::ReturnCode DecompressorManager::FreeExternalBuffers() noexcept
    {
        RHI::ReturnCode RHIstatus;
        if (m_DecompressionPerChannelBlockDataBuffer.buffer)
        {
            RHIstatus = m_RHIRuntime->ReleaseBuffer(m_DecompressionPerChannelBlockDataBuffer.buffer);
            if (RHIstatus != RHI::ZRHI_SUCCESS)
            {
                return CE::ZCE_ERROR;
            }
        }
        if (m_DecompressionPerChannelBlockInfoBuffer.buffer)
        {
            RHIstatus = m_RHIRuntime->ReleaseBuffer(m_DecompressionPerChannelBlockInfoBuffer.buffer);
            if (RHIstatus != RHI::ZRHI_SUCCESS)
            {
                return CE::ZCE_ERROR;
            }
        }
        if (m_DecompressionPerSpatialBlockInfoBuffer.buffer)
        {
            RHIstatus = m_RHIRuntime->ReleaseBuffer(m_DecompressionPerSpatialBlockInfoBuffer.buffer);
            if (RHIstatus != RHI::ZRHI_SUCCESS)
            {
                return CE::ZCE_ERROR;
            }
        }
        if (m_DecompressionPerSpatialBlockInfoBuffer.buffer)
        {
            RHIstatus = m_RHIRuntime->ReleaseBuffer(m_DecompressionPerSpatialBlockInfoBuffer.buffer);
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
