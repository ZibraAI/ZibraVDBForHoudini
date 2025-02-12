#include "RHIWrapper.h"

#include "Zibra/CE/Common.h"

namespace Zibra
{
    void RHIWrapper::Initialize() noexcept
    {
        RHI::CAPI::CreateRHIFactory(&m_RHIFactory);
        m_RHIFactory->Create(&m_RHIRuntime);
    }

    void RHIWrapper::Release() noexcept
    {
        m_RHIRuntime->Release();
        m_RHIFactory->Release();
    }

    void RHIWrapper::AllocateExternalBuffers(CE::Decompression::DecompressorResourcesRequirements requirements) noexcept
    {
        m_decompressorResources.decompressionPerChannelBlockData = m_RHIRuntime->CreateBuffer(
            requirements.decompressionPerChannelBlockDataSizeInBytes, RHI::ResourceHeapType::Default,
            RHI::ResourceUsage::UnorderedAccess | RHI::ResourceUsage::ShaderResource | RHI::ResourceUsage::CopySource,
            requirements.decompressionPerChannelBlockDataStride, "decompressionPerChannelBlockData");
        m_decompressorResources.decompressionPerChannelBlockInfo = m_RHIRuntime->CreateBuffer(
            requirements.decompressionPerChannelBlockInfoSizeInBytes, RHI::ResourceHeapType::Default,
            RHI::ResourceUsage::UnorderedAccess | RHI::ResourceUsage::ShaderResource | RHI::ResourceUsage::CopySource,
            requirements.decompressionPerChannelBlockInfoStride, "decompressionPerChannelBlockInfo");
        m_decompressorResources.decompressionPerSpatialBlockInfo = m_RHIRuntime->CreateBuffer(
            requirements.decompressionPerSpatialBlockInfoSizeInBytes, RHI::ResourceHeapType::Default,
            RHI::ResourceUsage::UnorderedAccess | RHI::ResourceUsage::ShaderResource | RHI::ResourceUsage::CopySource,
            requirements.decompressionPerSpatialBlockInfoStride, "decompressionPerSpatialBlockInfo");
    }

    void RHIWrapper::FreeExternalBuffers() noexcept
    {
        m_RHIRuntime->ReleaseBuffer(m_decompressorResources.decompressionPerChannelBlockData);
        m_RHIRuntime->ReleaseBuffer(m_decompressorResources.decompressionPerChannelBlockInfo);
        m_RHIRuntime->ReleaseBuffer(m_decompressorResources.decompressionPerSpatialBlockInfo);
    }

    void RHIWrapper::StartRecording() const noexcept
    {
        m_RHIRuntime->StartRecording();
    }

    void RHIWrapper::StopRecording() const noexcept
    {
        m_RHIRuntime->StopRecording();
    }

    void RHIWrapper::GarbageCollect() noexcept
    {
        m_RHIRuntime->GarbageCollect();
    }

    RHI::RHIRuntime* const RHIWrapper::GetRHIRuntime() const noexcept
    {
        return m_RHIRuntime;
    }

    const CE::Decompression::DecompressorResources& RHIWrapper::GetDecompressorResources() const noexcept
    {
        return m_decompressorResources;
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

    static void UnpackBlocks(OpenVDBSupport::DecompressedFrameData& decompressedBlockData,
                             const std::vector<uint32_t>& scratchBufferSpatialBlockData,
                             const std::vector<uint16_t>& scratchBufferChannelBlockData) noexcept
    {
        const auto channelBlockCount = scratchBufferChannelBlockData.size() / CE::SPARSE_BLOCK_VOXEL_COUNT;
        std::for_each(std::execution::par_unseq, decompressedBlockData.channelBlocks,
                      decompressedBlockData.channelBlocks + channelBlockCount, [&](CE::Decompression::ChannelBlock& block) noexcept {
                          const auto blockIdx = &block - decompressedBlockData.channelBlocks;

                          for (size_t i = 0; i < Zibra::CE::SPARSE_BLOCK_VOXEL_COUNT; ++i)
                          {
                              const size_t bufIdx = blockIdx * Zibra::CE::SPARSE_BLOCK_VOXEL_COUNT + i;
                              block.voxels[i] = Float16ToFloat32(scratchBufferChannelBlockData[bufIdx]);
                          }
                      });

        const auto spatialBlockCount = scratchBufferSpatialBlockData.size() / 3;
        for (size_t i = 0; i < spatialBlockCount; ++i)
        {
            uint32_t packedCoords = scratchBufferSpatialBlockData[i * 3];
            decompressedBlockData.spatialBlocks[i].coords[0] = int32_t((packedCoords >> 0u) & 1023u);
            decompressedBlockData.spatialBlocks[i].coords[1] = int32_t((packedCoords >> 10u) & 1023u);
            decompressedBlockData.spatialBlocks[i].coords[2] = int32_t((packedCoords >> 20u) & 1023u);
            decompressedBlockData.spatialBlocks[i].channelBlocksOffset = scratchBufferSpatialBlockData[i * 3 + 1];
            decompressedBlockData.spatialBlocks[i].channelMask = scratchBufferSpatialBlockData[i * 3 + 2];
        }
    }

    OpenVDBSupport::DecompressedFrameData RHIWrapper::GetDecompressedFrameData(
        const CE::Decompression::DecompressFrameDesc& desc, const CE::Decompression::DecompressedFrameFeedback& feedback) const noexcept
    {
        m_RHIRuntime->StartRecording();

        OpenVDBSupport::DecompressedFrameData decompressedFrameData{};

        decompressedFrameData.channelBlocks = new CE::Decompression::ChannelBlock[feedback.channelBlocksCount];
        decompressedFrameData.spatialBlocks = new CE::Decompression::SpatialBlock[desc.spatialBlocksCount];

        const size_t spatialBlockInfoElementCount = desc.spatialBlocksCount * 3;
        std::vector<uint32_t> scratchBufferSpatialBlockData(spatialBlockInfoElementCount);
        m_RHIRuntime->GetBufferDataImmediately(m_decompressorResources.decompressionPerSpatialBlockInfo,
                                               scratchBufferSpatialBlockData.data(), spatialBlockInfoElementCount * sizeof(uint32_t),
                                               desc.decompressionPerSpatialBlockInfoOffset);
        const size_t channelBlockDataElementCount = feedback.channelBlocksCount * CE::SPARSE_BLOCK_VOXEL_COUNT;
        std::vector<uint16_t> scratchBufferChannelBlockData(channelBlockDataElementCount);
        m_RHIRuntime->GetBufferDataImmediately(m_decompressorResources.decompressionPerChannelBlockData,
                                               scratchBufferChannelBlockData.data(), channelBlockDataElementCount * sizeof(uint16_t),
                                               desc.decompressionPerChannelBlockDataOffset);

        m_RHIRuntime->StopRecording();

        UnpackBlocks(decompressedFrameData, scratchBufferSpatialBlockData, scratchBufferChannelBlockData);

        return decompressedFrameData;
    }
    void RHIWrapper::ReleaseDecompressedFrameData(OpenVDBSupport::DecompressedFrameData& decompressedFrameData) noexcept
    {
        delete[] decompressedFrameData.channelBlocks;
        delete[] decompressedFrameData.spatialBlocks;
    }

} // namespace Zibra