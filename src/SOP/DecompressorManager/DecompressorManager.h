#pragma once

#include <Zibra/CE/Decompression.h>
#include <Zibra/CE/Addons/OpenVDBEncoder.h>
#include <Zibra/CE/Literals.h>

#include "Globals.h"

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
        CE::ReturnCode Initialize() noexcept;
        CE::ReturnCode RegisterDecompressor(const UT_String& filename) noexcept;
        CE::ReturnCode DecompressFrame(CE::Decompression::CompressedFrameContainer* frameContainer, openvdb::GridPtrVec* vdbGrids) noexcept;
        CE::Decompression::CompressedFrameContainer* FetchFrame(const exint& frameIndex) const noexcept;
        CE::Decompression::FrameRange GetFrameRange() const noexcept;
        void Release() noexcept;

    private:
        CE::ReturnCode GetDecompressedFrameData(const CE::Decompression::FrameInfo& frameInfo, CE::Addons::OpenVDBUtils::OpenVDBEncoder::FrameData* outDecompressedFrameData) const noexcept;
        CE::ReturnCode AllocateExternalBuffer(BufferDesc& bufferDesc, size_t newSizeInBytes, size_t newStride) noexcept;
        CE::ReturnCode FreeExternalBuffers() noexcept;

    private:
        CE::Decompression::DecompressorFactory* m_DecompressorFactory = nullptr;
        CE::ZibraVDB::FileDecoder* m_Decoder = nullptr;
        CE::Decompression::Decompressor* m_Decompressor = nullptr;
        CE::Decompression::FormatMapper* m_FormatMapper = nullptr;
        RHI::RHIRuntime* m_RHIRuntime = nullptr;

        BufferDesc m_DecompressionPerChannelBlockDataBuffer;
        BufferDesc m_DecompressionPerChannelBlockInfoBuffer;
        BufferDesc m_DecompressionPerSpatialBlockInfoBuffer;
        BufferDesc m_DecompressionSpatialToChannelIndexLookupBuffer;
    };
} // namespace Zibra
