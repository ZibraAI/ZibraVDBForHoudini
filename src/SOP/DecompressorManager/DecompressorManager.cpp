#include "PrecompiledHeader.h"

#include "DecompressorManager.h"

#include <Zibra/CE/Literals.h>

#include "bridge/LibraryUtils.h"
#include "utils/Helpers.h"

namespace Zibra::Helpers
{
    Result DecompressorManager::Initialize() noexcept
    {
        if (m_IsInitialized)
        {
            return RESULT_SUCCESS;
        }

        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            assert(0);
            return RESULT_UNEXPECTED_ERROR;
        }

        RHI::RHIFactory* RHIFactory = nullptr;
        Result res = RHI::CreateRHIFactory(&RHIFactory);
        if (ZIB_FAILED(res))
        {
            return res;
        }

        res = RHIFactory->SetGFXAPI(Helpers::SelectGFXAPI());
        if (ZIB_FAILED(res))
        {
            return res;
        }

        if (Helpers::NeedForceSoftwareDevice())
        {
            res = RHIFactory->ForceSoftwareDevice();
            if (ZIB_FAILED(res))
            {
                return res;
            }
        }

        res = RHIFactory->Create(&m_RHIRuntime);
        if (ZIB_FAILED(res))
        {
            return res;
        }
        RHIFactory->Release();

        res = m_RHIRuntime->Initialize();
        if (ZIB_FAILED(res))
        {
            m_RHIRuntime->Release();
            m_RHIRuntime = nullptr;
            return res;
        }

        m_IsInitialized = true;

        return RESULT_SUCCESS;
    }

    Result DecompressorManager::AllocateExternalBuffer(BufferDesc& bufferDesc, size_t newSizeInBytes, size_t newStride) noexcept
    {
        if (bufferDesc.sizeInBytes != newSizeInBytes || bufferDesc.stride != newStride)
        {
            if (bufferDesc.buffer)
            {
                Result res = m_RHIRuntime->ReleaseBuffer(bufferDesc.buffer);
                if (ZIB_FAILED(res))
                {
                    return res;
                }
                bufferDesc.buffer = nullptr;
            }

            // Do not create empty buffer
            if (newSizeInBytes == 0)
            {
                return RESULT_SUCCESS;
            }

            Result res = m_RHIRuntime->CreateBuffer(
                newSizeInBytes, RHI::ResourceHeapType::Default,
                RHI::ResourceUsage::UnorderedAccess | RHI::ResourceUsage::ShaderResource | RHI::ResourceUsage::CopySource,
                static_cast<uint32_t>(newStride), "decompressionPerChannelBlockData", &bufferDesc.buffer);
            if (ZIB_FAILED(res))
            {
                return res;
            }
            bufferDesc.sizeInBytes = newSizeInBytes;
            bufferDesc.stride = newStride;
        }
        return RESULT_SUCCESS;
    }

    Result DecompressorManager::RegisterDecompressor(const UT_String& filename) noexcept
    {
        if (m_FileStream.has_value())
        {
            delete m_FileStream->first;
            delete m_FileStream->second;
            m_FileStream = std::nullopt;
        }

        auto stream = new std::ifstream{filename.c_str(), std::ios::binary};
        if (!stream->is_open() || !stream->good())
        {
            delete stream;
            return RESULT_FILE_NOT_FOUND;
        }
        m_FileStream = {stream, new STDIStreamWrapper{*stream}};

        if (m_FormatMapper)
        {
            m_FormatMapper->Release();
            m_FormatMapper = nullptr;
        }

        auto status = CE::Decompression::CreateFormatMapper(m_FileStream->second, &m_FormatMapper);
        if (status != RESULT_SUCCESS)
        {
            return status;
        }

        if (m_Decompressor)
        {
            m_Decompressor->Release();
            m_Decompressor = nullptr;
        }
        CE::Decompression::DecompressorFactory* factory = m_FormatMapper->CreateDecompressorFactory();
        factory->UseRHI(m_RHIRuntime);
        using namespace Zibra::CE::Literals::Memory;
        factory->SetMemoryLimitPerResource(128_MiB);

        status = factory->Create(&m_Decompressor);
        factory->Release();
        if (status != RESULT_SUCCESS)
        {
            return status;
        }

        status = m_Decompressor->Initialize();
        if (status != RESULT_SUCCESS)
        {
            return status;
        }

        CE::Decompression::DecompressorResourcesRequirements newRequirements = m_Decompressor->GetResourcesRequirements();
        status =
            AllocateExternalBuffer(m_DecompressionPerChannelBlockDataBuffer, newRequirements.decompressionPerChannelBlockDataSizeInBytes,
                                   newRequirements.decompressionPerChannelBlockDataStride);
        if (status != RESULT_SUCCESS)
        {
            return status;
        }
        status =
            AllocateExternalBuffer(m_DecompressionPerChannelBlockInfoBuffer, newRequirements.decompressionPerChannelBlockInfoSizeInBytes,
                                   newRequirements.decompressionPerChannelBlockInfoStride);
        if (status != RESULT_SUCCESS)
        {
            return status;
        }
        status =
            AllocateExternalBuffer(m_DecompressionPerSpatialBlockInfoBuffer, newRequirements.decompressionPerSpatialBlockInfoSizeInBytes,
                                   newRequirements.decompressionPerSpatialBlockInfoStride);
        if (status != RESULT_SUCCESS)
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
            if (status != RESULT_SUCCESS)
            {
                return status;
            }
        }

        return RESULT_SUCCESS;
    }
    Result DecompressorManager::DecompressFrame(CE::Decompression::CompressedFrameContainer* frameContainer,
                                                        std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> gridShuffle,
                                                        openvdb::GridPtrVec* vdbGrids) noexcept
    {
        if (!m_RHIRuntime || !m_Decompressor)
        {
            assert(0);
            return RESULT_UNEXPECTED_ERROR;
        }
        auto res = m_RHIRuntime->StartRecording();
        if (ZIB_FAILED(res))
        {
            return res;
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

        CE::Addons::OpenVDBUtils::FrameEncoder encoder{gridShuffle.data(), gridShuffle.size(), frameInfo};

        const CE::Decompression::MaxDimensionsPerSubmit maxDimensionsPerSubmit = m_Decompressor->GetMaxDimensionsPerSubmit();
        const uint32_t maxChunkSize = static_cast<uint32_t>(maxDimensionsPerSubmit.maxSpatialBlocks);
        const uint32_t chunksCount = (frameInfo.spatialInfoCount + maxChunkSize - 1) / maxChunkSize;

        std::vector<CE::Decompression::Shaders::PackedSpatialBlockInfo> readbackDecompressionPerSpatialBlockInfo{};
        readbackDecompressionPerSpatialBlockInfo.reserve(maxChunkSize);
        std::vector<uint16_t> readbackDecompressionPerChannelBlockData{};
        readbackDecompressionPerChannelBlockData.reserve(maxChunkSize * CE::MAX_CHANNEL_COUNT * CE::SPARSE_BLOCK_VOXEL_COUNT);

        for (int chunkIdx = 0; chunkIdx < chunksCount; ++chunkIdx)
        {
            CE::Decompression::DecompressFrameDesc decompressDesc{};
            decompressDesc.frameContainer = frameContainer;
            decompressDesc.firstSpatialBlockIndex = maxChunkSize * chunkIdx;
            decompressDesc.spatialBlocksCount = std::min(maxChunkSize, frameInfo.spatialInfoCount - maxChunkSize * chunkIdx);
            decompressDesc.decompressionPerChannelBlockDataOffset = 0;
            decompressDesc.decompressionPerChannelBlockInfoOffset = 0;
            decompressDesc.decompressionPerSpatialBlockInfoOffset = 0;

            CE::Decompression::DecompressedFrameFeedback fFeedback{};

            Result status = m_Decompressor->DecompressFrame(decompressDesc, &fFeedback);
            if (status != RESULT_SUCCESS)
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
            encoder.EncodeChunk(fData, decompressDesc.spatialBlocksCount, fFeedback.firstChannelBlockIndex);
        }
        res = m_RHIRuntime->StopRecording();
        if (ZIB_FAILED(res))
        {
            return res;
        }

        *vdbGrids = encoder.GetGrids();
        return RESULT_SUCCESS;
    }

    Result DecompressorManager::GetDecompressedFrameData(uint16_t* perChannelBlockData, size_t channelBlocksCount,
                                                                 CE::Decompression::Shaders::PackedSpatialBlockInfo* perSpatialBlockInfo,
                                                                 size_t spatialBlocksCount) const noexcept
    {
        if (!m_RHIRuntime)
        {
            assert(0);
            return RESULT_UNEXPECTED_ERROR;
        }

        auto res = m_RHIRuntime->GetBufferDataImmediately(m_DecompressionPerSpatialBlockInfoBuffer.buffer, perSpatialBlockInfo,
                                                                spatialBlocksCount * sizeof(perSpatialBlockInfo[0]), 0);
        if (ZIB_FAILED(res))
        {
            return res;
        }
        const size_t channelBlockDataElementCount = channelBlocksCount * CE::SPARSE_BLOCK_VOXEL_COUNT;
        res = m_RHIRuntime->GetBufferDataImmediately(m_DecompressionPerChannelBlockDataBuffer.buffer, perChannelBlockData,
                                                           channelBlockDataElementCount * sizeof(perChannelBlockData[0]), 0);
        if (ZIB_FAILED(res))
        {
            return res;
        }

        return RESULT_SUCCESS;
    }

    CE::Decompression::CompressedFrameContainer* DecompressorManager::FetchFrame(const exint& frameIndex) const noexcept
    {
        if (!m_FormatMapper)
        {
            return nullptr;
        }
        CE::Decompression::CompressedFrameContainer* frameContainer = nullptr;
        auto status = m_FormatMapper->FetchFrame(frameIndex, &frameContainer);
        if (status != RESULT_SUCCESS)
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

    Result DecompressorManager::FreeExternalBuffers() noexcept
    {
        Result res;
        if (m_DecompressionPerChannelBlockDataBuffer.buffer)
        {
            res = m_RHIRuntime->ReleaseBuffer(m_DecompressionPerChannelBlockDataBuffer.buffer);
            if (ZIB_FAILED(res))
            {
                return res;
            }
            m_DecompressionPerChannelBlockDataBuffer = BufferDesc{};
        }
        if (m_DecompressionPerChannelBlockInfoBuffer.buffer)
        {
            res = m_RHIRuntime->ReleaseBuffer(m_DecompressionPerChannelBlockInfoBuffer.buffer);
            if (ZIB_FAILED(res))
            {
                return res;
            }
            m_DecompressionPerChannelBlockInfoBuffer = BufferDesc{};
        }
        if (m_DecompressionPerSpatialBlockInfoBuffer.buffer)
        {
            res = m_RHIRuntime->ReleaseBuffer(m_DecompressionPerSpatialBlockInfoBuffer.buffer);
            if (ZIB_FAILED(res))
            {
                return res;
            }
            m_DecompressionPerSpatialBlockInfoBuffer = BufferDesc{};
        }
        return RESULT_SUCCESS;
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
        if (m_RHIRuntime)
        {
            m_RHIRuntime->Release();
            m_RHIRuntime = nullptr;
        }

        m_IsInitialized = false;
    }

} // namespace Zibra::Helpers
