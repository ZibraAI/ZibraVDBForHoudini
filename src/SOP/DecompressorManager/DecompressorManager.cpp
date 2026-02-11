#include "PrecompiledHeader.h"

#include "DecompressorManager.h"

#include <Zibra/CE/Literals.h>

#include "analytics/Analytics.h"
#include "bridge/LibraryUtils.h"
#include "utils/Helpers.h"

namespace Zibra::Helpers
{
    Result DecompressorManager::Initialize() noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&DecompressorManager::Initialize)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&DecompressorManager::AllocateExternalBuffer)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&DecompressorManager::RegisterDecompressor)>);

        if (m_FileStream.has_value())
        {
            m_FileStream->close();
            m_FileStream = std::nullopt;
        }

        m_FileStream = std::ifstream{filename.c_str(), std::ios::binary};
        if (!m_FileStream->is_open() || !m_FileStream->good())
        {
            m_FileStream = std::nullopt;
            return RESULT_FILE_NOT_FOUND;
        }

        if (m_FileDecoder)
        {
            m_FileDecoder->Release();
            m_FileDecoder = nullptr;
        }
        STDIStreamWrapper stream{m_FileStream.value()};

        CE::Decompression::ByteRange range = {};
        CE::Decompression::ReadFileDecoderInitByteRange(&stream, &range);
        std::vector<char> initFileMemory{};
        initFileMemory.resize(range.size);
        stream.seekg(range.start);
        stream.read(initFileMemory);

        Result res = CE::Decompression::CreateFileDecoder(initFileMemory, &m_FileDecoder);
        if (ZIB_FAILED(res))
        {
            return res;
        }

        if (m_Decompressor)
        {
            m_Decompressor->Release();
            m_Decompressor = nullptr;
        }
        CE::Decompression::DecompressorFactory* factory;
        res = m_FileDecoder->CreateDecompressorFactory(&factory);
        if (ZIB_FAILED(res))
        {
            return res;
        }
        factory->UseRHI(m_RHIRuntime);
        using namespace Zibra::CE::Literals::Memory;
        factory->SetMemoryLimitPerResource(128_MiB);

        res = factory->Create(&m_Decompressor);
        factory->Release();
        if (ZIB_FAILED(res))
        {
            return res;
        }

        res = m_Decompressor->Initialize();
        if (ZIB_FAILED(res))
        {
            return res;
        }

        CE::Decompression::DecompressorResourcesRequirements newRequirements = m_Decompressor->GetResourcesRequirements();
        res =
            AllocateExternalBuffer(m_DecompressionPerChannelBlockDataBuffer, newRequirements.decompressionPerChannelBlockDataSizeInBytes,
                                   newRequirements.decompressionPerChannelBlockDataStride);
        if (ZIB_FAILED(res))
        {
            return res;
        }
        res =
            AllocateExternalBuffer(m_DecompressionPerChannelBlockInfoBuffer, newRequirements.decompressionPerChannelBlockInfoSizeInBytes,
                                   newRequirements.decompressionPerChannelBlockInfoStride);
        if (ZIB_FAILED(res))
        {
            return res;
        }
        res =
            AllocateExternalBuffer(m_DecompressionPerSpatialBlockInfoBuffer, newRequirements.decompressionPerSpatialBlockInfoSizeInBytes,
                                   newRequirements.decompressionPerSpatialBlockInfoStride);
        if (ZIB_FAILED(res))
        {
            return res;
        }

        if (m_DecompressionPerChannelBlockDataBuffer.buffer != nullptr && m_DecompressionPerChannelBlockInfoBuffer.buffer != nullptr &&
            m_DecompressionPerSpatialBlockInfoBuffer.buffer != nullptr)
        {
            CE::Decompression::DecompressorResources decompressorResources{};
            decompressorResources.decompressionPerChannelBlockData = m_DecompressionPerChannelBlockDataBuffer.buffer;
            decompressorResources.decompressionPerChannelBlockInfo = m_DecompressionPerChannelBlockInfoBuffer.buffer;
            decompressorResources.decompressionPerSpatialBlockInfo = m_DecompressionPerSpatialBlockInfoBuffer.buffer;
            res = m_Decompressor->RegisterResources(decompressorResources);
            if (ZIB_FAILED(res))
            {
                return res;
            }
        }

        return RESULT_SUCCESS;
    }
    Result DecompressorManager::DecompressFrame(Span<const char> frameMemory, CE::Decompression::FrameProxy* frameDecoder,
                                                const std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc>& gridShuffle,
                                                openvdb::GridPtrVec* vdbGrids) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&DecompressorManager::DecompressFrame)>);

        Analytics::AnalyticsManager::GetInstance().SendEventUsage();

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

        const auto frameInfo = frameDecoder->GetInfo();

        const CE::Addons::OpenVDBUtils::VDBGridDesc* gridShuffleData = nullptr;
        size_t gridShuffleSize = 0;
        std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> defaultGridShuffle;

        if (!gridShuffle.empty())
        {
            gridShuffleData = gridShuffle.data();
            gridShuffleSize = gridShuffle.size();
        }
        else
        {
            defaultGridShuffle.reserve(frameInfo.channelsCount);

            for (size_t i = 0; i < frameInfo.channelsCount; ++i)
            {
                CE::Addons::OpenVDBUtils::VDBGridDesc gridDesc{};
                gridDesc.gridName = frameInfo.channels[i].name;
                gridDesc.voxelType = CE::Addons::OpenVDBUtils::GridVoxelType::Float1;
                gridDesc.chSource[0] = frameInfo.channels[i].name;
                defaultGridShuffle.emplace_back(gridDesc);
            }

            gridShuffleData = defaultGridShuffle.data();
            gridShuffleSize = defaultGridShuffle.size();
        }

        CE::Addons::OpenVDBUtils::FrameEncoder encoder{gridShuffleData, gridShuffleSize, frameInfo};

        const CE::Decompression::MaxDimensionsPerSubmit maxDimensionsPerSubmit = m_Decompressor->GetMaxDimensionsPerSubmit();
        const auto maxChunksPerSubmit = maxDimensionsPerSubmit.maxChunks;
        const auto chunksCount = m_Decompressor->GetFrameChunkCount(frameMemory);
        const auto chunkedIterations = Math::CeilToMultipleOf(chunksCount, maxChunksPerSubmit) / maxChunksPerSubmit;

        std::vector<CE::Decompression::Shaders::PackedSpatialBlockInfo> readbackDecompressionPerSpatialBlockInfo{};
        readbackDecompressionPerSpatialBlockInfo.reserve(maxDimensionsPerSubmit.maxSpatialBlocks);
        std::vector<uint16_t> readbackDecompressionPerChannelBlockData{};
        readbackDecompressionPerChannelBlockData.reserve(maxDimensionsPerSubmit.maxChannelBlocks * CE::SPARSE_BLOCK_VOXEL_COUNT);

        auto chunksToDecompress = chunksCount;
        for (int chunkIter = 0; chunkIter < chunkedIterations; ++chunkIter)
        {
            CE::Decompression::DecompressFrameDesc decompressDesc{};
            decompressDesc.frameMemory = frameMemory;
            decompressDesc.firstChunkIndex = chunksCount - chunksToDecompress;
            decompressDesc.chunkCount = std::min(chunksToDecompress, maxChunksPerSubmit);
            decompressDesc.decompressionPerChannelBlockDataOffset = 0;
            decompressDesc.decompressionPerChannelBlockInfoOffset = 0;
            decompressDesc.decompressionPerSpatialBlockInfoOffset = 0;

            chunksToDecompress -= decompressDesc.chunkCount;

            CE::Decompression::DecompressedFrameFeedback fFeedback{};
            Result res = m_Decompressor->DecompressFrame(decompressDesc, &fFeedback);
            if (ZIB_FAILED(res))
            {
                return res;
            }

            readbackDecompressionPerSpatialBlockInfo.resize(maxDimensionsPerSubmit.maxSpatialBlocks);
            readbackDecompressionPerChannelBlockData.resize(fFeedback.channelBlockCount * CE::SPARSE_BLOCK_VOXEL_COUNT);
            GetDecompressedFrameData(readbackDecompressionPerChannelBlockData.data(), fFeedback.channelBlockCount,
                                     readbackDecompressionPerSpatialBlockInfo.data(), fFeedback.spatialBlockCount);
            m_RHIRuntime->GarbageCollect();

            CE::Addons::OpenVDBUtils::FrameData fData{};
            fData.decompressionPerChannelBlockData = readbackDecompressionPerChannelBlockData.data();
            fData.decompressionPerSpatialBlockInfo = readbackDecompressionPerSpatialBlockInfo.data();
            // TODO VDB-1291: Implement read-back circular buffer, to optimize GPU stalls.
            //                Implement cpu circular buffer to optimize RAM allocation for DecompressedFrameData.
            //                Move EncodeChunk into separate thread to overlay CPU and CPU work.
            encoder.EncodeChunk(fData, fFeedback.spatialBlockCount, fFeedback.firstChannelBlockIndex);
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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&DecompressorManager::GetDecompressedFrameData)>);

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

    std::pair<Span<char>, CE::Decompression::FrameProxy*> DecompressorManager::FetchFrame(const exint& frameIndex) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&DecompressorManager::FetchFrame)>);

        if (!m_FileDecoder)
        {
            return {};
        }
        CE::Decompression::ByteRange byteRange{};

        Result result = m_FileDecoder->GetFrameByteRange(static_cast<float>(frameIndex), &byteRange);
        if (ZIB_FAILED(result))
        {
            return {};
        }
        void* frameMemoryBuf = malloc(byteRange.size);
        auto frameMemory = Span<char>{static_cast<char*>(frameMemoryBuf), byteRange.size};
        STDIStreamWrapper stream{m_FileStream.value()};
        stream.seekg(byteRange.start);
        stream.read(frameMemory);
        CE::Decompression::FrameProxy* proxy{};
        result = m_FileDecoder->CreateFrameProxy(frameMemory, &proxy);
        if (ZIB_FAILED(result))
        {
            free(frameMemory.data());
            return {};
        }
        return {frameMemory, proxy};
    }

    CE::Decompression::FrameRange DecompressorManager::GetFrameRange() const noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&DecompressorManager::GetFrameRange)>);

        if (!m_FileDecoder)
        {
            return {};
        }
        return m_FileDecoder->GetFrameRange();
    }

    Result DecompressorManager::FreeExternalBuffers() noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&DecompressorManager::FreeExternalBuffers)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&DecompressorManager::Release)>);

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
