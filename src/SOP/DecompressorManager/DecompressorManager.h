#pragma once

#include <Zibra/CE/Decompression.h>
#include <Zibra/CE/Literals.h>
#include "openvdb/OpenVDBEncoder.h"
#include "Globals.h"

namespace Zibra::CE::Decompression
{
    class DecompressorManager
    {
        struct BufferDesc
        {
            RHI::Buffer* buffer = nullptr;
            size_t sizeInBytes = 0;
            size_t stride = 0;
        };

    public:
        ReturnCode Initialize() noexcept;
        ReturnCode RegisterDecompressor(const UT_String& filename) noexcept;
        ReturnCode DecompressFrame(CE::Decompression::CompressedFrameContainer* frameContainer) noexcept;
        ReturnCode GetDecompressedFrameData(OpenVDBSupport::DecompressedFrameData& outDecompressedFrameData,
                                            const CE::Decompression::FrameInfo& frameInfo) const noexcept;
        CompressedFrameContainer* FetchFrame(const exint& frameIndex) const noexcept;
        FrameRange GetFrameRange() const noexcept;
        void Release() noexcept;

    private:
        ReturnCode AllocateExternalBuffer(BufferDesc& bufferDesc, size_t newSizeInBytes, size_t newStride) noexcept;
        ReturnCode FreeExternalBuffers() noexcept;

    private:
        DecompressorFactory* m_DecompressorFactory = nullptr;
        Zibra::CE::ZibraVDB::FileDecoder* m_Decoder = nullptr;
        Decompressor* m_Decompressor = nullptr;
        CAPI::FormatMapperCAPI* m_FormatMapper = nullptr;
        RHI::RHIRuntime* m_RHIRuntime = nullptr;

        BufferDesc m_DecompressionPerChannelBlockDataBuffer;
        BufferDesc m_DecompressionPerChannelBlockInfoBuffer;
        BufferDesc m_DecompressionPerSpatialBlockInfoBuffer;
        BufferDesc m_DecompressionSpatialToChannelIndexLookupBuffer;
    };
} // namespace Zibra