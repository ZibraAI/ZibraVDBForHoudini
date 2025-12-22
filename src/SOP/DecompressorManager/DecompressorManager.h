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
        Result DecompressFrame(Span<const char> frameMemory, CE::Decompression::FrameProxy* frameDecoder,
                               std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> gridShuffle, openvdb::GridPtrVec* vdbGrids) noexcept;
        std::pair<Span<char>, CE::Decompression::FrameProxy*> FetchFrame(const exint& frameIndex) noexcept;
        CE::Decompression::FrameRange GetFrameRange() const noexcept;
        void Release() noexcept;

    private:
        Result GetDecompressedFrameData(uint16_t* perChannelBlockData, size_t channelBlocksCount,
                                        CE::Decompression::Shaders::PackedSpatialBlockInfo* perSpatialBlockInfo,
                                        size_t spatialBlocksCount) const noexcept;
        Result AllocateExternalBuffer(BufferDesc& bufferDesc, size_t newSizeInBytes, size_t newStride) noexcept;
        Result FreeExternalBuffers() noexcept;

    private:
        std::optional<std::ifstream> m_FileStream = std::nullopt;
        CE::Decompression::Decompressor* m_Decompressor = nullptr;
        CE::Decompression::FileDecoder* m_FileDecoder = nullptr;
        RHI::RHIRuntime* m_RHIRuntime = nullptr;
        bool m_IsInitialized = false;

        BufferDesc m_DecompressionPerChannelBlockDataBuffer;
        BufferDesc m_DecompressionPerChannelBlockInfoBuffer;
        BufferDesc m_DecompressionPerSpatialBlockInfoBuffer;
    };
} // namespace Zibra::Helpers
