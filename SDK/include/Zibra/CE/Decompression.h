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

    struct DecompressorVRAMRequirements
    {
        size_t totalMemorySize;
    };

    struct DecompressorResourcesRequirements
    {
        size_t decompressionPerChannelBlockDataSizeInBytes;
        size_t decompressionPerChannelBlockDataStride;

        size_t decompressionPerSpatialBlockInfoSizeInBytes;
        size_t decompressionPerSpatialBlockInfoStride;

        size_t decompressionPerChannelBlockInfoSizeInBytes;
        size_t decompressionPerChannelBlockInfoStride;
    };

    /**
     * Decompressor resources abstraction
     *
     * @code
     * DecompressedFormat
     * ├── positionP0  -> PerSpatialGroupInfo[0]
     * │   ├── channelC0 -> PerChannelBlockInfo[spatialToChannelLookup[0] + 0] + PerChannelBlockPayload[spatialToChannelLookup[0] + 0]
     * │   ├── channelC1 -> PerChannelBlockInfo[spatialToChannelLookup[0] + 1] + PerChannelBlockPayload[spatialToChannelLookup[0] + 1]
     * │   ├── ...
     * │   └── channelCN -> PerChannelBlockInfo[spatialToChannelLookup[0] + CN] + PerChannelBlockPayload[spatialToChannelLookup[0] + CN]
     * ├── positionP1  -> PerSpatialGroupInfo[1]
     * │   ├── channelC0 -> PerChannelBlockInfo[spatialToChannelLookup[1] + 0] + PerChannelBlockPayload[spatialToChannelLookup[1] + 0]
     * │   ├── channelC1 -> PerChannelBlockInfo[spatialToChannelLookup[1] + 1] + PerChannelBlockPayload[spatialToChannelLookup[1] + 1]
     * │   ├── ...
     * │   └── channelCN -> PerChannelBlockInfo[spatialToChannelLookup[1] + CN] + PerChannelBlockPayload[spatialToChannelLookup[1] + CN]
     * ├── ...
     * └── positionPN  -> PerSpatialGroupInfo[PN]
     *     ├── channelC0 -> PerChannelBlockInfo[spatialToChannelLookup[PN] + 0] + PerChannelBlockPayload[spatialToChannelLookup[PN] + 0]
     *    ├── ...
     *    └── channelCN -> PerChannelBlockInfo[spatialToChannelLookup[PN] + CN] + PerChannelBlockPayload[spatialToChannelLookup[PN] + CN]
     * @endcode
     */
    struct DecompressorResources
    {
        /**
         * Payload per channel block.
         * @layout float16[SPARSE_BLOCK_VOXEL_COUNT][FrameInfo::channelBlockCount]
         */
        RHI::Buffer* decompressionPerChannelBlockData = nullptr;
        /**
         * Info per channel block
         * @layout ZCEDecompressionPackedChannelBlockInfo[FrameInfo::channelBlockCount]
         */
        RHI::Buffer* decompressionPerChannelBlockInfo = nullptr;
        /**
         * Info per spatial group
         * @layout ZCEDecompressionPackedSpatialBlock[FrameInfo::spatialBlockCount]
         */
        RHI::Buffer* decompressionPerSpatialBlockInfo = nullptr;
    };

    inline SpatialBlockInfo UnpackPackedSpatialBlockInfo(const Shaders::PackedSpatialBlockInfo& packedBlock) noexcept
    {
        SpatialBlockInfo result{};
        result.coords[0] = static_cast<int32_t>((packedBlock.packedCoords >> 0u) & 1023u);
        result.coords[1] = static_cast<int32_t>((packedBlock.packedCoords >> 10u) & 1023u);
        result.coords[2] = static_cast<int32_t>((packedBlock.packedCoords >> 20u) & 1023u);
        result.channelBlocksOffset = packedBlock.channelBlocksOffset;
        result.channelMask = packedBlock.channelMask;
        result.channelCount = CountBits(packedBlock.channelMask);
        return result;
    }

    struct MaxDimensionsPerSubmit
    {
        /// Maximum allowed 128MiB chunks decoding at once. Calculates based on max VRAM limit per resource provided by user.
        size_t maxChunks;
        /// Maximum allowed spatial blocks decoding at once. Calculates based on max VRAM limit per resource provided by user.
        size_t maxSpatialBlocks;
        /// Maximum allowed channel blocks decoding at once. Calculates based on max VRAM limit per resource provided by user.
        size_t maxChannelBlocks;
    };

    struct DecompressFrameDesc
    {
        Span<const char> frameMemory = {};
        /// First chunk index to decompress. Must be in range [0; frameTotalChunksCount].
        size_t firstChunkIndex = 0;
        /// Chunks number per batch. Must be less or equal to MaxDimensionsPerSubmit::maxChunks.
        size_t chunkCount = 0;
        /**
         * Write offset in bytes for decompressionPerChannelBlockData buffer. Must be multiple of 4 (sizeof uint).
         * @note Decompressor expects the size of registered buffer to be larger than offset + VRAM MemoryLimitPerResource.
        */
        size_t decompressionPerChannelBlockDataOffset = 0;
        /**
         * Write offset in bytes for decompressionPerChannelBlockInfo buffer. Must be multiple of 4 (sizeof uint).
         * @note Decompressor expects the size of registered buffer to be larger than offset + VRAM MemoryLimitPerResource.
         */
        size_t decompressionPerChannelBlockInfoOffset = 0;
        /**
         * Write offset in bytes for decompressionPerSpatialBlockInfo buffer. Must be multiple of 4 (sizeof uint).
         * @note Decompressor expects the size of registered buffer to be larger than offset + VRAM MemoryLimitPerResource.
         */
        size_t decompressionPerSpatialBlockInfoOffset = 0;
    };

    struct DecompressedFrameFeedback
    {
        /// First index of channel block that was decompressed.
        size_t firstChannelBlockIndex = 0;
        /// Amount of channel blocks that were decompressed.
        size_t channelBlockCount = 0;
        /// First index of spatial block that was decompressed.
        size_t firstSpatialBlockIndex = 0;
        /// Amount of spatial blocks that were decompressed.
        size_t spatialBlockCount = 0;
    };

    class Decompressor
    {
    protected:
        virtual ~Decompressor() noexcept = default;

    public:
        virtual DecompressorVRAMRequirements GetVRAMRequirements() noexcept = 0;
        /**
         * Resource requirements getter.
         * @return Resource requirements desc structure. Should be used for DecompressorResources initialization.
         */
        virtual DecompressorResourcesRequirements GetResourcesRequirements() noexcept = 0;
        /**
         * Maximum allowed dispatch decompression parameters, based on max VRAM limit per resource provided by user.
         * @return MaxDimensionsPerSubmit structure. Should be used for calculating chunks sizes and count during decompression of large
         * effects.
         */
        [[nodiscard]] virtual MaxDimensionsPerSubmit GetMaxDimensionsPerSubmit() const noexcept = 0;
        /**
         * Initializes decompressor instance. Can enqueue RHI commands and submit work.
         * Must be called before any other method.
         */
        virtual size_t GetFrameChunkCount(Span<const char> frameMemory) noexcept = 0;

    public:
        virtual Result Initialize() noexcept = 0;
        /**
         * Registers external output RHI resources.
         * Must be called before calling Decompress method.
         * @param resources - Initialized resource set to be used as decompression payload output. Must be created using compatible RHI.
         */
        virtual Result RegisterResources(const DecompressorResources& resources) noexcept = 0;
        /**
         * Enqueues GPU frame decompression work for provided spatial blocks range.
         * @param desc - DecompressFrameDesc structure that contains input frame handle and decompression spatial blocks range.
         * @param outFeedback - [Output] DecompressedFrameStats structure that contains statistics of decompressed frame.
         * @return Writes data to registered resources on GPU side, and decompression channel blocks range to output DecompressedFrameStats.
         */
        virtual Result DecompressFrame(const DecompressFrameDesc& desc, DecompressedFrameFeedback* outFeedback) noexcept = 0;
        /**
         * Destructs Decompressor instance and releases it's memory.
         * After release pointer becomes invalid.
         */
        virtual void Release() noexcept = 0;
    };

    class DecompressorFactory
    {
    protected:
        virtual ~DecompressorFactory() noexcept = default;

    public:
        /**
         * Sets VRAM limit per resource.
         * @note During resource allocation minimum of user defined limit and RHI capabilities limit will be chosen.
         */
        virtual Result SetMemoryLimitPerResource(size_t limit) noexcept = 0;
        virtual Result UseRHI(RHI::RHIRuntime* rhi) noexcept = 0;
        virtual Result Create(Decompressor** outInstance) noexcept = 0;
        /**
         * Destructs DecompressorFactory instance and releases it's memory.
         * @warning After release pointer becomes invalid.
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
        size_t start;
        size_t size;
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
        virtual FrameInfo GetInfo() const noexcept = 0;
        /**
         * Finds metadata entry by key and returns payload.
         * @param key - Metadata key
         * @return String payload or nullptr if key is not present.
         * Memory managed by FrameHandle instance and present while handle exists.
         */
        virtual const char* GetMetadataByKey(const char* key) const noexcept = 0;
        virtual size_t GetMetadataCount() const noexcept = 0;
        virtual Result GetMetadataByIndex(size_t index, MetadataEntry* outEntry) const noexcept = 0;
        virtual void Release() noexcept = 0;
    };

    class FileDecoder
    {
    protected:
        virtual ~FileDecoder() noexcept = default;

    public:
        /**
         * Returns valid frame range decompression parametrization.
         * @return pair StartFrame - EndFrame
         */
        virtual FrameRange GetFrameRange() const noexcept = 0;
        virtual SequenceInfo GetSequenceInfo() const noexcept = 0;
        virtual PlaybackInfo GetPlaybackInfo() const noexcept = 0;
        /**
         * Finds metadata entry by key and returns payload.
         * @param key - Metadata key
         * @return String payload or nullptr if key is not present.
         * Memory managed by FrameHandle instance and present while handle exists.
         */
        virtual const char* GetMetadataByKey(const char* key) const noexcept = 0;
        virtual size_t GetMetadataCount() const noexcept = 0;
        virtual Result GetMetadataByIndex(size_t index, MetadataEntry* outEntry) const noexcept = 0;
        /**
         * Allocates CompressedFrameContainer object from input position params.
         * @param frame - frame index in original frame space
         * @return CompressedFrameContainer with data or fails assertion.
         */
        virtual Result GetFrameByteRange(float frame, ByteRange* outRange) noexcept = 0;
        virtual Result CreateFrameProxy(Span<const char> frameMemory, FrameProxy** outProxy) noexcept = 0;

        virtual Result CreateDecompressorFactory(DecompressorFactory** outDecompressionFactory) noexcept = 0;
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


    typedef Result(ZCE_CALL_CONV* PFN_CreateFileDecoder)(Span<const char> memory, FileDecoder** outFormatMapper);
#ifdef ZCE_STATIC_LINKING
    Result CreateFileDecoder(Span<const char> memory, FormatMapper** outFormatMapper) noexcept;
#elif defined(ZCE_DYNAMIC_IMPLICIT_LINKING)
    ZCE_API_IMPORT Result ZCE_CALL_CONV Zibra_CE_Decompression_CreateFileDecoder(Span<const char> memory, FormatMapper** outMapper) noexcept;
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
