#include "PrecompiledHeader.h"

#include "DecompressorManager.h"

#include <Zibra/CE/Literals.h>

#include "bridge/LibraryUtils.h"
#include "utils/Helpers.h"

namespace Zibra::Helpers
{
    CE::ReturnCode DecompressorManager::Initialize() noexcept
    {
        if (m_IsInitialized)
        {
            return CE::ZCE_SUCCESS;
        }

        if (!Zibra::LibraryUtils::IsSDKLibraryLoaded())
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
                bufferDesc.buffer = nullptr;
            }

            // Do not create empty buffer
            if (newSizeInBytes == 0)
            {
                return CE::ZCE_SUCCESS;
            }

            auto RHIstatus = m_RHIRuntime->CreateBuffer(
                newSizeInBytes, RHI::ResourceHeapType::Default,
                RHI::ResourceUsage::UnorderedAccess | RHI::ResourceUsage::ShaderResource | RHI::ResourceUsage::CopySource,
                static_cast<uint32_t>(newStride), "decompressionPerChannelBlockData", &bufferDesc.buffer);
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
        
        if (m_DecompressionPerChannelBlockDataBuffer.buffer != nullptr && m_DecompressionPerChannelBlockInfoBuffer.buffer != nullptr &&
            m_DecompressionPerSpatialBlockInfoBuffer.buffer != nullptr)
        {
            CE::Decompression::DecompressorResources decompressorResources{};
            decompressorResources.decompressionPerChannelBlockData = m_DecompressionPerChannelBlockDataBuffer.buffer;
            decompressorResources.decompressionPerChannelBlockInfo = m_DecompressionPerChannelBlockInfoBuffer.buffer;
            decompressorResources.decompressionPerSpatialBlockInfo = m_DecompressionPerSpatialBlockInfoBuffer.buffer;
            status = m_Decompressor->RegisterResources(decompressorResources);
            if (status != CE::ZCE_SUCCESS)
            {
                return status;
            }
        }

        return CE::ZCE_SUCCESS;
    }

    CE::ReturnCode DecompressorManager::DecompressFrame(CE::Decompression::CompressedFrameContainer* frameContainer,
                                                        std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> gridShuffle,
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

        // Filling default mapping if metadata is empty/invalid
        if (gridShuffle.empty())
        {
            for (size_t i = 0; i < frameInfo.channelsCount; ++i)
            {
                CE::Addons::OpenVDBUtils::VDBGridDesc gridDesc{};
                gridDesc.gridName = frameInfo.channels[i].name;
                gridDesc.voxelType = CE::Addons::OpenVDBUtils::GridVoxelType::Float1;
                gridDesc.chSource[0] = frameInfo.channels[i].name;
                gridShuffle.emplace_back(gridDesc);
            }
        }

        CE::Addons::OpenVDBUtils::EncodingMetadata encodingMetadataStorage; 
        CE::Addons::OpenVDBUtils::EncodingMetadata* encodingMetadata = nullptr;
        const char* encodingMetadataStr = frameContainer->GetMetadataByKey("houdiniDecodeMetadata");
        if (encodingMetadataStr)
        {
            encodingMetadataStorage = {};
            encodingMetadata = &encodingMetadataStorage;

            std::istringstream metadataStream(encodingMetadataStr);
            metadataStream >> encodingMetadataStorage.offsetX >> encodingMetadataStorage.offsetY >> encodingMetadataStorage.offsetZ;
        }

        CE::Addons::OpenVDBUtils::FrameEncoder encoder{gridShuffle.data(), gridShuffle.size(), frameInfo, encodingMetadata};

        const CE::Decompression::MaxDimensionsPerSubmit maxDimensionsPerSubmit = m_Decompressor->GetMaxDimensionsPerSubmit();
        const uint32_t maxChunkSize = static_cast<uint32_t>(maxDimensionsPerSubmit.maxSpatialBlocks);
        const uint32_t chunksCount = (frameInfo.spatialBlockCount + maxChunkSize - 1) / maxChunkSize;

        std::vector<CE::Decompression::Shaders::PackedSpatialBlockInfo> readbackDecompressionPerSpatialBlockInfo{};
        readbackDecompressionPerSpatialBlockInfo.reserve(maxChunkSize);
        std::vector<uint16_t> readbackDecompressionPerChannelBlockData{};
        readbackDecompressionPerChannelBlockData.reserve(maxChunkSize * CE::MAX_CHANNEL_COUNT * CE::SPARSE_BLOCK_VOXEL_COUNT);

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

            readbackDecompressionPerSpatialBlockInfo.resize(decompressDesc.spatialBlocksCount);
            readbackDecompressionPerChannelBlockData.resize(fFeedback.channelBlocksCount * CE::SPARSE_BLOCK_VOXEL_COUNT);
            GetDecompressedFrameData(readbackDecompressionPerChannelBlockData.data(), fFeedback.channelBlocksCount,
                                     readbackDecompressionPerSpatialBlockInfo.data(), decompressDesc.spatialBlocksCount);
            m_RHIRuntime->GarbageCollect();

            CE::Addons::OpenVDBUtils::FrameData fData{};
            fData.decompressionPerChannelBlockData = readbackDecompressionPerChannelBlockData.data();
            fData.decompressionPerSpatialBlockInfo = readbackDecompressionPerSpatialBlockInfo.data();
            // TODO VDB-1291: Implement read-back circular buffer, to optimize GPU stalls.
            //                Implement cpu circular buffer to optimize RAM allocation for DecompressedFrameData.
            //                Move EncodeChunk into separate thread to overlay CPU and CPU work.
            encoder.EncodeChunk(fData, decompressDesc.spatialBlocksCount, fFeedback.firstChannelBlockIndex, encodingMetadata);
        }
        RHIStatus = m_RHIRuntime->StopRecording();
        if (RHIStatus != RHI::ZRHI_SUCCESS)
        {
            return CE::ZCE_ERROR;
        }

        *vdbGrids = encoder.GetGrids();
        return CE::ZCE_SUCCESS;
    }

    CE::ReturnCode DecompressorManager::GetDecompressedFrameData(uint16_t* perChannelBlockData, size_t channelBlocksCount,
                                                                 CE::Decompression::Shaders::PackedSpatialBlockInfo* perSpatialBlockInfo,
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

        if (m_Decoder)
        {
            CE::Decompression::CAPI::ReleaseDecoder(m_Decoder);
            m_Decoder = nullptr;
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
            m_RHIRuntime->GarbageCollect();
            m_RHIRuntime->Release();
            m_RHIRuntime = nullptr;
        }

        m_IsInitialized = false;
    }

    inline char* TransferStr(const std::string& src) noexcept
    {
        char* dst = new char[src.length() + 1];
        strcpy(dst, src.c_str());
        return dst;
    }

    std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> DecompressorManager::DeserializeGridShuffleInfo(
        CE::Decompression::CompressedFrameContainer* frameContainer) noexcept
    {
        static std::map<std::string, CE::Addons::OpenVDBUtils::GridVoxelType> strToVoxelType = {
            {"Float1", CE::Addons::OpenVDBUtils::GridVoxelType::Float1},
            {"Float3", CE::Addons::OpenVDBUtils::GridVoxelType::Float3}
        };

        const char* meta = frameContainer->GetMetadataByKey("chShuffle");
        if (!meta) return {};

        auto serialized = nlohmann::json::parse(meta);
        if (!serialized.is_array())
        {
            return {};
        }
        std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> result{};
        for (const auto& serializedDesc : serialized)
        {
            CE::Addons::OpenVDBUtils::VDBGridDesc gridDesc{};
            if (!serializedDesc.is_object())
            {
                continue;
            }
            gridDesc.gridName = TransferStr(serializedDesc["gridName"]);
            gridDesc.voxelType = strToVoxelType.at(serializedDesc["voxelType"]);

            for (size_t i = 0; i < std::size(gridDesc.chSource); ++i)
            {
                auto key = std::string{"chSource"} + std::to_string(i);
                if (serializedDesc.contains(key) && serializedDesc[key].is_string())
                {
                    gridDesc.chSource[i] = TransferStr(serializedDesc[key]);
                }
            }
            result.emplace_back(gridDesc);
        }
        return result;
    }

    void DecompressorManager::ReleaseGridShuffleInfo(std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc>& gridDescs) noexcept
    {
        for (const CE::Addons::OpenVDBUtils::VDBGridDesc& desc : gridDescs)
        {
            delete desc.gridName;
            for (size_t i = 0; i < std::size(desc.chSource); ++i)
            {
                delete desc.chSource[i];
            }
        }
        gridDescs.clear();
    }

    CE::Decompression::SequenceInfo DecompressorManager::GetSequenceInfo() const noexcept
    {
        if (!m_FormatMapper)
        {
            return {};
        }
        return m_FormatMapper->GetSequenceInfo();
    }

} // namespace Zibra::Helpers
