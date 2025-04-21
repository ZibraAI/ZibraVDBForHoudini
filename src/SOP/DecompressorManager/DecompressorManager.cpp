#include "PrecompiledHeader.h"

#include "DecompressorManager.h"

#include <Zibra/CE/Literals.h>

#include "bridge/LibraryUtils.h"

namespace Zibra::Helpers
{
    CE::ReturnCode DecompressorManager::Initialize() noexcept
    {
        if (m_IsInitialized)
        {
            return CE::ZCE_SUCCESS;
        }

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
        RHIFactory->SetGFXAPI(RHI::GFXAPI::Auto);

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

        using namespace Zibra::CE::Literals::Memory;
        m_DecompressorFactory->SetMemoryLimitPerResource(128_MiB);

        status = m_DecompressorFactory->UseRHI(m_RHIRuntime);
        if (status != CE::ZCE_SUCCESS)
        {
            m_DecompressorFactory->Release();
            m_DecompressorFactory = nullptr;
            return status;
        }

        m_IsInitialized = true;

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

    CE::ReturnCode DecompressorManager::DecompressFrame(CE::Decompression::CompressedFrameContainer* frameContainer,
                                                        openvdb::GridPtrVec* vdbGrids) noexcept
    {
        if (!m_RHIRuntime || !m_Decompressor)
        {
            return CE::ZCE_ERROR;
        }
        auto RHIStatus = m_RHIRuntime->StartRecording();
        if (RHIStatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        const auto frameInfo = frameContainer->GetInfo();

        const CE::Decompression::MaxDimensionsPerSubmit maxDimensionsPerSubmit = m_Decompressor->GetMaxDimensionsPerSubmit();
        const uint32_t maxChunkSize = maxDimensionsPerSubmit.maxSpatialBlocks;
        const uint32_t chunksCount = (frameInfo.spatialBlockCount + maxChunkSize - 1) / maxChunkSize;

        std::vector<ZCEDecompressionPackedSpatialBlock> readbackDecompressionPerSpatialBlockInfo{};
        readbackDecompressionPerSpatialBlockInfo.resize(frameInfo.spatialBlockCount);
        std::vector<uint16_t> readbackDecompressionPerChannelBlockData{};
        readbackDecompressionPerChannelBlockData.resize(frameInfo.channelBlockCount * CE::SPARSE_BLOCK_VOXEL_COUNT);

        size_t readbackDecompressionPerSpatialBlockInfoOffset = 0;
        size_t readbackDecompressionPerChannelBlockDataOffset = 0;
        for (int chunkIdx = 0; chunkIdx < chunksCount; ++chunkIdx)
        {
            CE::Decompression::DecompressFrameDesc decompressDesc{};
            decompressDesc.frameContainer = frameContainer;
            decompressDesc.firstSpatialBlockIndex = maxChunkSize * chunkIdx;
            decompressDesc.spatialBlocksCount = std::min(maxChunkSize, frameInfo.spatialBlockCount - maxChunkSize * chunkIdx);
            decompressDesc.decompressionPerChannelBlockDataOffset = 0;
            decompressDesc.decompressionPerChannelBlockInfoOffset = 0;
            decompressDesc.decompressionPerSpatialBlockInfoOffset = 0;

            CE::Decompression::DecompressedFrameFeedback fFeedback{};

            CE::Decompression::ReturnCode status = m_Decompressor->DecompressFrame(decompressDesc, &fFeedback);
            if (status != CE::ZCE_SUCCESS)
            {
                return status;
            }

            auto* chBlocksReadback = readbackDecompressionPerChannelBlockData.data() + readbackDecompressionPerSpatialBlockInfoOffset;
            auto* spBlocksReadback = readbackDecompressionPerSpatialBlockInfo.data() + readbackDecompressionPerSpatialBlockInfoOffset;
            GetDecompressedFrameData(chBlocksReadback, fFeedback.channelBlocksCount, spBlocksReadback, decompressDesc.spatialBlocksCount);

            readbackDecompressionPerSpatialBlockInfoOffset += decompressDesc.spatialBlocksCount;
            readbackDecompressionPerChannelBlockDataOffset += fFeedback.channelBlocksCount * CE::SPARSE_BLOCK_VOXEL_COUNT;
            m_RHIRuntime->GarbageCollect();
        }
        RHIStatus = m_RHIRuntime->StopRecording();


        CE::Addons::OpenVDBUtils::VDBGridDesc gridDesc{};
        gridDesc.voxelType = CE::Addons::OpenVDBUtils::GridVoxelType::Float3;
        gridDesc.gridName = "MyComposedGrid";
        gridDesc.chSource[0] = "density.x";
        gridDesc.chSource[1] = "density.y";
        gridDesc.chSource[2] = "density.z";

        CE::Addons::OpenVDBUtils::FrameEncoder encoder{};
        CE::Addons::OpenVDBUtils::FrameData fData{};
        fData.decompressionPerChannelBlockData = readbackDecompressionPerChannelBlockData.data();
        fData.decompressionPerSpatialBlockInfo = readbackDecompressionPerSpatialBlockInfo.data();
        // TODO VDB-1291: Implement read-back circular buffer, to optimize GPU stalls.
        //                Implement cpu circular buffer to optimize RAM allocation for DecompressedFrameData.
        //                Move EncodeFrame into separate thread to overlay CPU and CPU work.
        *vdbGrids = encoder.EncodeFrame(&gridDesc, 1, fData, frameInfo);
        if (RHIStatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        return CE::ZCE_SUCCESS;
    }

    CE::ReturnCode DecompressorManager::GetDecompressedFrameData(uint16_t* perChannelBlockData, size_t channelBlocksCount,
                                                                 ZCEDecompressionPackedSpatialBlock* perSpatialBlockInfo,
                                                                 size_t spatialBlocksCount) const noexcept
    {
        if (!m_RHIRuntime)
        {
            return CE::ZCE_ERROR;
        }

        auto RHIstatus = m_RHIRuntime->GetBufferDataImmediately(m_DecompressionPerSpatialBlockInfoBuffer.buffer, perSpatialBlockInfo,
                                                                spatialBlocksCount * sizeof(perSpatialBlockInfo[0]), 0);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }
        const size_t channelBlockDataElementCount = channelBlocksCount * CE::SPARSE_BLOCK_VOXEL_COUNT;
        RHIstatus = m_RHIRuntime->GetBufferDataImmediately(m_DecompressionPerChannelBlockDataBuffer.buffer, perChannelBlockData,
                                                           channelBlockDataElementCount * sizeof(perChannelBlockData[0]), 0);
        if (RHIstatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

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
            m_DecompressionPerChannelBlockDataBuffer = BufferDesc{};
        }
        if (m_DecompressionPerChannelBlockInfoBuffer.buffer)
        {
            RHIstatus = m_RHIRuntime->ReleaseBuffer(m_DecompressionPerChannelBlockInfoBuffer.buffer);
            if (RHIstatus != RHI::ZRHI_SUCCESS)
            {
                return CE::ZCE_ERROR;
            }
            m_DecompressionPerChannelBlockInfoBuffer = BufferDesc{};
        }
        if (m_DecompressionPerSpatialBlockInfoBuffer.buffer)
        {
            RHIstatus = m_RHIRuntime->ReleaseBuffer(m_DecompressionPerSpatialBlockInfoBuffer.buffer);
            if (RHIstatus != RHI::ZRHI_SUCCESS)
            {
                return CE::ZCE_ERROR;
            }
            m_DecompressionPerSpatialBlockInfoBuffer = BufferDesc{};
        }
        return CE::ZCE_SUCCESS;
    }

    void DecompressorManager::Release() noexcept
    {
        if (!m_IsInitialized)
        {
            return;
        }

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

        m_IsInitialized = false;
    }

} // namespace Zibra::Helpers
