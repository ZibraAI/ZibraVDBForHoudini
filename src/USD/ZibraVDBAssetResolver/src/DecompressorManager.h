#pragma once

#include <Zibra/CE/Decompression.h>
#include <Zibra/CE/Addons/OpenVDBFrameEncoder.h>

// TODO unify with the one in parent project
namespace Zibra::Decompression
{
    using namespace Zibra;

    class DecompressorManager
    {
        struct BufferDesc
        {
            RHI::Buffer* buffer = nullptr;
            size_t sizeInBytes = 0;
            size_t stride = 0;
        };

    public:
        CE::ReturnCode Initialize() noexcept;
        CE::ReturnCode RegisterDecompressor(const UT_String& filename) noexcept;
        CE::ReturnCode DecompressFrame(CE::Decompression::CompressedFrameContainer* frameContainer,
                                       std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> gridShuffle,
                                       openvdb::GridPtrVec* vdbGrids) noexcept;
        CE::Decompression::CompressedFrameContainer* FetchFrame(const exint& frameIndex) const noexcept;
        CE::Decompression::FrameRange GetFrameRange() const noexcept;
        void Release() noexcept;
        
        std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> DeserializeGridShuffleInfo(CE::Decompression::CompressedFrameContainer* frameContainer) noexcept;
        void ReleaseGridShuffleInfo(std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc>& gridDescs) noexcept;
        
        CE::Decompression::SequenceInfo GetSequenceInfo() const noexcept;

    private:
        CE::ReturnCode GetDecompressedFrameData(uint16_t* perChannelBlockData, size_t channelBlocksCount,
                                                CE::Decompression::Shaders::PackedSpatialBlockInfo* perSpatialBlockInfo,
                                                size_t spatialBlocksCount) const noexcept;
        CE::ReturnCode AllocateExternalBuffer(BufferDesc& bufferDesc, size_t newSizeInBytes, size_t newStride) noexcept;
        CE::ReturnCode FreeExternalBuffers() noexcept;

    private:
        CE::Decompression::DecompressorFactory* m_DecompressorFactory = nullptr;
        CE::ZibraVDB::FileDecoder* m_Decoder = nullptr;
        CE::Decompression::Decompressor* m_Decompressor = nullptr;
        CE::Decompression::FormatMapper* m_FormatMapper = nullptr;
        RHI::RHIRuntime* m_RHIRuntime = nullptr;
        bool m_IsInitialized = false;

        BufferDesc m_DecompressionPerChannelBlockDataBuffer;
        BufferDesc m_DecompressionPerChannelBlockInfoBuffer;
        BufferDesc m_DecompressionPerSpatialBlockInfoBuffer;
    };
} // namespace Zibra
