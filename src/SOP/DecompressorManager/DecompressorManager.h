#pragma once

#include <Zibra/CE/Addons/OpenVDBFrameEncoder.h>
#include <Zibra/CE/Decompression.h>

namespace Zibra::Helpers
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
        Result Initialize() noexcept;
        Result RegisterDecompressor(const UT_String& filename) noexcept;
        Result DecompressFrame(CE::Decompression::CompressedFrameContainer* frameContainer,
                               std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> gridShuffle, openvdb::GridPtrVec* vdbGrids) noexcept;
        CE::Decompression::CompressedFrameContainer* FetchFrame(const exint& frameIndex) const noexcept;
        CE::Decompression::FrameRange GetFrameRange() const noexcept;
        void Release() noexcept;

    private:
        Result GetDecompressedFrameData(uint16_t* perChannelBlockData, size_t channelBlocksCount,
                                        CE::Decompression::Shaders::PackedSpatialBlockInfo* perSpatialBlockInfo,
                                        size_t spatialBlocksCount) const noexcept;
        Result AllocateExternalBuffer(BufferDesc& bufferDesc, size_t newSizeInBytes, size_t newStride) noexcept;
        Result FreeExternalBuffers() noexcept;

    private:
        std::optional<std::pair<std::ifstream*, IStream*>> m_FileStream = std::nullopt;
        CE::Decompression::Decompressor* m_Decompressor = nullptr;
        CE::Decompression::FormatMapper* m_FormatMapper = nullptr;
        RHI::RHIRuntime* m_RHIRuntime = nullptr;
        bool m_IsInitialized = false;

        BufferDesc m_DecompressionPerChannelBlockDataBuffer;
        BufferDesc m_DecompressionPerChannelBlockInfoBuffer;
        BufferDesc m_DecompressionPerSpatialBlockInfoBuffer;
    };
} // namespace Zibra::Helpers
