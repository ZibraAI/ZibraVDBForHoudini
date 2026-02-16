#pragma once

#include <Zibra/CE/Common.h>
#include <Zibra/Math.h>
#include <Zibra/RHI.h>
#include <Zibra/Stream.h>
#include <cstdint>

#include "shaders/ZCEDecompressionShaderTypes.hlsli"

#if defined(_MSC_VER)
#define ZCE_API_IMPORT extern "C" __declspec(dllimport)
#define ZCE_CALL_CONV __cdecl
#elif defined(__GNUC__)
#define ZCE_API_IMPORT extern "C"
#define ZCE_CALL_CONV
#else
#error "Unsupported compiler"
#endif

namespace Zibra::CE::Decompression
{
    constexpr Version DECOMPRESSOR_VERSION = {1, 0, 0, 0};

    struct DecompressorCreateDesc
    {
        /**
         * Size limit for GPU resources that is guaranteed to work.
         * Larger values allow decompressing more chunks in single pass
         * Must be multiple of 128MiB (CHUNK_SIZE_IN_BYTES) and can't be 0
         * Safe value is
         * D3D12 - 2GiB
         * Other APIs - 128MiB
         */
        uint64_t resourceSizeLimit = Zibra::CE::CHUNK_SIZE_IN_BYTES;
        /**
         * RHIRuntime object to use for decompression
         * Must not be null
         * User is responsible keeping this object alive for whole lifetime of decompressor
         * It is invalid to use any other RHIRuntime object with created decompressor
         */
        RHI::RHIRuntime* RHI = nullptr;
    };

    struct DecompressorVRAMRequirements
    {
        uint64_t totalMemorySize;
    };

    struct DecompressorResourcesRequirements
    {
        uint64_t decompressionPerChannelBlockDataSizeInBytes;
        uint64_t decompressionPerChannelBlockDataStride;

        uint64_t decompressionPerSpatialBlockInfoSizeInBytes;
        uint64_t decompressionPerSpatialBlockInfoStride;

        uint64_t decompressionPerChannelBlockInfoSizeInBytes;
        uint64_t decompressionPerChannelBlockInfoStride;
    };

    struct DecompressorResources
    {
        RHI::Buffer* decompressionPerChannelBlockData = nullptr;
        RHI::Buffer* decompressionPerChannelBlockInfo = nullptr;
        RHI::Buffer* decompressionPerSpatialBlockInfo = nullptr;
    };

    inline SpatialBlock UnpackPackedSpatialBlockInfo(const Shaders::PackedSpatialBlockInfo& packedBlock) noexcept
    {
        SpatialBlock result{};
        result.coords[0] = static_cast<int32_t>((packedBlock.packedCoords >> 0u) & 1023u);
        result.coords[1] = static_cast<int32_t>((packedBlock.packedCoords >> 10u) & 1023u);
        result.coords[2] = static_cast<int32_t>((packedBlock.packedCoords >> 20u) & 1023u);
        result.channelBlocksOffset = packedBlock.channelBlocksOffset;
        result.channelMask = packedBlock.channelMask;
        result.channelCount = CountBits(packedBlock.channelMask);
        return result;
    }

    struct MaxDimensionsPerPass
    {
        /// Maximum allowed 128MiB chunks decoding at once. Calculates based on max VRAM limit per resource provided by user.
        uint32_t maxChunks;
        /// Maximum allowed spatial blocks decoding at once. Calculates based on max VRAM limit per resource provided by user.
        uint32_t maxSpatialBlocks;
        /// Maximum allowed channel blocks decoding at once. Calculates based on max VRAM limit per resource provided by user.
        uint32_t maxChannelBlocks;
    };

    struct DecompressFrameDesc
    {
        Span<const char> frameMemory = {};
        /// First chunk index to decompress. Must be in range [0; frameTotalChunksCount].
        uint32_t firstChunkIndex = 0;
        /// Chunks number per batch. Must be less or equal to MaxDimensionsPerPass::maxChunks.
        uint32_t chunkCount = 0;
        /**
         * Write offset in bytes for decompressionPerChannelBlockData buffer. Must be multiple of 4 (sizeof uint).
         * @note Decompressor expects the size of registered buffer to be larger than offset + VRAM MemoryLimitPerResource.
        */
        uint32_t decompressionPerChannelBlockDataOffset = 0;
        /**
         * Write offset in bytes for decompressionPerChannelBlockInfo buffer. Must be multiple of 4 (sizeof uint).
         * @note Decompressor expects the size of registered buffer to be larger than offset + VRAM MemoryLimitPerResource.
         */
        uint32_t decompressionPerChannelBlockInfoOffset = 0;
        /**
         * Write offset in bytes for decompressionPerSpatialBlockInfo buffer. Must be multiple of 4 (sizeof uint).
         * @note Decompressor expects the size of registered buffer to be larger than offset + VRAM MemoryLimitPerResource.
         */
        uint32_t decompressionPerSpatialBlockInfoOffset = 0;
    };

    struct DecompressedFrameFeedback
    {
        /// First index of channel block that was decompressed.
        uint64_t firstChannelBlockIndex = 0;
        /// Amount of channel blocks that were decompressed.
        uint64_t channelBlockCount = 0;
        /// First index of spatial block that was decompressed.
        uint64_t firstSpatialBlockIndex = 0;
        /// Amount of spatial blocks that were decompressed.
        uint64_t spatialBlockCount = 0;
    };

    class Decompressor
    {
    protected:
        virtual ~Decompressor() noexcept = default;

    public:
        /**
         * VRAM requirement estimation getter.
         * @return VRAM estimation desc structure.
         */
        [[nodiscard]] virtual DecompressorVRAMRequirements GetVRAMRequirements() const noexcept = 0;
        /**
         * Resource requirements getter.
         * @return Resource requirements desc structure. Should be used for DecompressorResources initialization.
         */
        [[nodiscard]] virtual DecompressorResourcesRequirements GetResourcesRequirements() const noexcept = 0;
        /**
         * Maximum allowed dispatch decompression parameters, based on max VRAM limit per resource provided by user.
         * @return MaxDimensionsPerPass structure. Should be used for calculating chunks sizes and count during decompression of large
         * effects.
         */
        [[nodiscard]] virtual MaxDimensionsPerPass GetMaxDimensionsPerPass() const noexcept = 0;
        /**
         * Initializes decompressor instance. Can enqueue RHI commands and submit work.
         * Must be called before any other method.
         */
        [[nodiscard]] virtual uint32_t GetFrameChunkCount(Span<const char> frameMemory) const noexcept = 0;

        /**
         * VRAM requirement estimation getter.
         */
        [[nodiscard]] virtual Result Initialize() noexcept = 0;
        /**
         * Registers external output RHI resources.
         * Must be called before calling Decompress method.
         * @param resources - Initialized resource set to be used as decompression payload output. Must be created using compatible RHI.
         */
        [[nodiscard]] virtual Result RegisterResources(const DecompressorResources& resources) noexcept = 0;
        /**
         * Enqueues GPU frame decompression work for provided spatial blocks range.
         * @param desc - DecompressFrameDesc structure that contains input frame handle and decompression spatial blocks range.
         * @param outFeedback - [Output] DecompressedFrameStats structure that contains statistics of decompressed frame.
         * @return Writes data to registered resources on GPU side, and decompression channel blocks range to output DecompressedFrameStats.
         */
        [[nodiscard]] virtual Result DecompressFrame(const DecompressFrameDesc& desc, DecompressedFrameFeedback* outFeedback) noexcept = 0;
        /**
         * Destructs Decompressor instance and releases it's memory.
         * After release pointer becomes invalid.
         */
        virtual void Release() noexcept = 0;
    };

    struct FrameRange
    {
        int32_t start;
        int32_t end;
    };

    struct ByteRange
    {
        uint64_t start;
        uint64_t size;
    };

    struct SequenceInfo
    {
        uint64_t fileUUID[2];
        Math::uint3 maxAABBSize;
        uint32_t maxChannelBlocksCount;
        uint32_t maxSpatialBlocksCount;
        uint64_t originalSize;
        uint8_t channelCount;
        const char* channels[MAX_CHANNEL_COUNT];
    };

    class FrameProxy
    {
    protected:
        virtual ~FrameProxy() noexcept = default;

    public:
        /**
         * Decodes frame info and returns it.
         * @deprecated
         * @param frame - frame index in original frame space
         * @return RESULT_SUCCESS in case of success or other code in case of failure.
         */
        [[nodiscard]] virtual FrameInfo GetInfo() const noexcept = 0;
        /**
         * Finds metadata entry by key and returns payload.
         * @param key - Metadata key
         * @return String payload or nullptr if key is not present.
         * Memory managed by FrameHandle instance and present while handle exists.
         */
        [[nodiscard]] virtual const char* GetMetadataByKey(const char* key) const noexcept = 0;
        [[nodiscard]] virtual uint64_t GetMetadataCount() const noexcept = 0;
        [[nodiscard]] virtual Result GetMetadataByIndex(uint64_t index, MetadataEntry* outEntry) const noexcept = 0;
        virtual void Release() noexcept = 0;
    };

    class FileDecoder
    {
    protected:
        virtual ~FileDecoder() noexcept = default;

    public:
        [[nodiscard]] virtual Version GetFileVersion() const noexcept = 0;
        /**
         * Returns valid frame range decompression parametrization.
         * @return pair StartFrame - EndFrame
         */
        [[nodiscard]] virtual FrameRange GetFrameRange() const noexcept = 0;
        [[nodiscard]] virtual SequenceInfo GetSequenceInfo() const noexcept = 0;
        [[nodiscard]] virtual PlaybackInfo GetPlaybackInfo() const noexcept = 0;
        /**
         * Finds metadata entry by key and returns payload.
         * @param key - Metadata key
         * @return String payload or nullptr if key is not present.
         * Memory managed by FrameHandle instance and present while handle exists.
         */
        [[nodiscard]] virtual const char* GetMetadataByKey(const char* key) const noexcept = 0;
        [[nodiscard]] virtual uint64_t GetMetadataCount() const noexcept = 0;
        [[nodiscard]] virtual Result GetMetadataByIndex(uint64_t index, MetadataEntry* outEntry) const noexcept = 0;
        /**
         * Allocates CompressedFrameContainer object from input position params.
         * @param frame - frame index in original frame space
         * @return CompressedFrameContainer with data or fails assertion.
         */
        [[nodiscard]] virtual Result GetFrameByteRange(float frame, ByteRange* outRange) const noexcept = 0;
        [[nodiscard]] virtual Result CreateFrameProxy(Span<const char> frameMemory, FrameProxy** outProxy) const noexcept = 0;

        [[nodiscard]] virtual Result CreateDecompressor(const DecompressorCreateDesc* createDesc, Decompressor** outInstance) const noexcept = 0;
        virtual void Release() noexcept = 0;
    };

    typedef Result(ZCE_CALL_CONV* PFN_ReadFileDecoderInitByteRange)(IStream* stream, ByteRange* outByteRange);
#ifdef ZCE_STATIC_LINKING
    Result ReadFileDecoderInitByteRange(IStream* stream, ByteRange* outByteRange) noexcept;
#elif defined(ZCE_DYNAMIC_IMPLICIT_LINKING)
    ZCE_API_IMPORT Result ZCE_CALL_CONV Zibra_CE_Decompression_ReadFileDecoderInitByteRange(IStream* stream, ByteRange* outByteRange) noexcept;
#else
    constexpr const char* ReadFileDecoderInitByteRangeExportName = "Zibra_CE_Decompression_ReadFileDecoderInitByteRange";
#endif


    typedef Result(ZCE_CALL_CONV* PFN_CreateFileDecoder)(Span<const char> memory, FileDecoder** outFileDecoder);
#ifdef ZCE_STATIC_LINKING
    Result CreateFileDecoder(Span<const char> memory, FileDecoder** outFileDecoder) noexcept;
#elif defined(ZCE_DYNAMIC_IMPLICIT_LINKING)
    ZCE_API_IMPORT Result ZCE_CALL_CONV Zibra_CE_Decompression_CreateFileDecoder(Span<const char> memory, FileDecoder** outMapper) noexcept;
#else
    constexpr const char* CreateFileDecoderExportName = "Zibra_CE_Decompression_CreateFileDecoder";
#endif

    typedef Version(ZCE_CALL_CONV* PFN_GetVersion)();
#ifdef ZCE_STATIC_LINKING
    Version GetVersion() noexcept;
#elif defined(ZCE_DYNAMIC_IMPLICIT_LINKING)
    ZCE_API_IMPORT Version ZCE_CALL_CONV Zibra_CE_Decompression_GetVersion() noexcept;
#else
    constexpr const char* GetVersionExportName = "Zibra_CE_Decompression_GetVersion";
#endif
} // namespace Zibra::CE::Decompression

#undef ZCE_API_IMPORT
#undef ZCE_CALL_CONV
