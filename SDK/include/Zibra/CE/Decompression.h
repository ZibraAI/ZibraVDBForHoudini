#pragma once

#include <Zibra/CE/Common.h>
#include <Zibra/Math3D.h>
#include <Zibra/RHI.h>
#include <cassert>
#include <cstdint>

#include "shaders/ZCEDecompressionShaderTypes.hlsli"

namespace Zibra::CE::ZibraVDB
{
    class FileDecoder;
}

namespace Zibra::CE::Decompression
{
    constexpr Version ZCE_DECOMPRESSION_VERSION = {0, 9, 5, 0};

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
         * @layout ZCEDecompressionPackedChannelBlockInfo[FrameInfo::channelBlockCount / 2]
         */
        RHI::Buffer* decompressionPerChannelBlockInfo = nullptr;
        /// Info per spatial group
        /// @layout ZCEDecompressionPackedSpatialBlock[FrameInfo::spatialBlockCount]
        RHI::Buffer* decompressionPerSpatialBlockInfo = nullptr;
    };

    inline SpatialBlockInfo UnpackPackedSpatialBlockInfo(const Shaders::PackedSpatialBlockInfo& packedBlock) noexcept {
        SpatialBlockInfo result{};
        result.coords[0] = static_cast<int32_t>((packedBlock.packedCoords >> 0u) & 1023u);
        result.coords[1] = static_cast<int32_t>((packedBlock.packedCoords >> 10u) & 1023u);
        result.coords[2] = static_cast<int32_t>((packedBlock.packedCoords >> 20u) & 1023u);
        result.channelBlocksOffset = packedBlock.channelBlocksOffset;
        result.channelMask = packedBlock.channelMask;
        result.channelCount = CountBits(packedBlock.channelMask);
        return result;
    }

    struct ChannelInfo
    {
        /**
         * Channel name.
         */
        const char* name;
        /**
         * Min grid's voxel value.
         * @range [-INF; INF]
         */
        float minGridValue;

        /**
         * Max grid's voxel value.
         * @range [-INF; INF]
         */
        float maxGridValue;
        /**
         * Affine transformation matrix. Cannot be 0.
         */
        Math3D::Transform gridTransform;
    };

    struct FrameInfo
    {
        /**
         * Count of channels present in frame.
         * @range [0; MAX_CHANNEL_COUNT]
         */
        size_t channelsCount;
        /**
         * Per channel information.
         * @range Length: =channelsCount
         */
        ChannelInfo channels[MAX_CHANNEL_COUNT];
        /**
         * Spatial blocks count in DecompressorResources::decompressionBlockInfo
         * @range [1; INF]
         */
        uint32_t spatialBlockCount;
        /**
         * Channel blocks count in DecompressorResources::decompressionBlockData.
         * @range [1; INF]
         */
        uint32_t channelBlockCount;
        /**
         * AABB dimensions sizes.
         * @range [1; INF]
         */
        uint16_t AABBSize[3];
    };

    class CompressedFrameContainer
    {
    protected:
        virtual ~CompressedFrameContainer() noexcept = default;

    public:
        virtual FrameInfo GetInfo() const noexcept = 0;
        /**
         * Finds metadata entry by key and returns payload.
         * @param key - Metadata key
         * @return String payload or nullptr if key is not present.
         * Memory managed by CompressedFrameContainer instance and present while container exists.
         */
        virtual const char* GetMetadataByKey(const char* key) const noexcept = 0;
        virtual size_t GetMetadataCount() const noexcept = 0;
        virtual ReturnCode GetMetadataByIndex(size_t index, MetadataEntry* outEntry) const noexcept = 0;
        virtual void Release() noexcept = 0;
    };

    struct FrameRange
    {
        int32_t start;
        int32_t end;
    };

    struct SequenceInfo
    {
        uint64_t fileUUID[2];
        Math3D::uint3 maxAABBSize;
        uint64_t originalSize;
        uint8_t channelCount;
        const char* channels[MAX_CHANNEL_COUNT];
    };

    struct PlaybackInfo
    {
        uint32_t frameCount;
        uint32_t framerateNumerator;
        uint32_t framerateDenominator;
        int32_t sequenceStartIndex;
        uint32_t sequenceIndexIncrement;
    };

    /**
     * General file format -> specific compression implementation frames mapper.
     * @note All CompressedFrameContainers allocated by this class become not valid after its release.
     */
    class FormatMapper
    {
    protected:
        virtual ~FormatMapper() noexcept = default;

    public:
        /**
         * Allocates CompressedFrameContainer object from input position params.
         * @param frame - frame index in original frame space
         * @return CompressedFrameContainer with data or fails assertion.
         */
        virtual ReturnCode FetchFrame(float frame, CompressedFrameContainer** outFrame) noexcept = 0;
        virtual ReturnCode FetchFrameInfo(float frame, FrameInfo* outInfo) noexcept = 0;
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
         * Memory managed by CompressedFrameContainer instance and present while container exists.
         */
        virtual const char* GetMetadataByKey(const char* key) const noexcept = 0;
        virtual size_t GetMetadataCount() const noexcept = 0;
        virtual ReturnCode GetMetadataByIndex(size_t index, MetadataEntry* outEntry) const noexcept = 0;
        virtual void Release() noexcept = 0;
    };

    struct MaxDimensionsPerSubmit
    {
        /**
         * Maximum allowed spatial blocks decoding at once. Calculates based based on max VRAM limit per resource provided by user.
         */
        size_t maxSpatialBlocks;
        /**
         * Maximum allowed channel blocks decoding at once. Calculates based based on max VRAM limit per resource provided by user.
         */
        size_t maxChannelBlocks;
    };

    struct DecompressFrameDesc
    {
        /// FrameContainer object allocated by FormatMapper created by this class instance.
        /// If frameContainer is nullptr ReturnCode::ZCE_ERROR_INVALID_ARGUMENTS will be returned.
        CompressedFrameContainer* frameContainer = nullptr;

        /// First index of spatial block that need to be decompressed.
        /// If firstSpatialBlockIndex is out of bounce of frame spatial block range ReturnCode::ZCE_ERROR_INVALID_ARGUMENTS will be
        /// returned.
        size_t firstSpatialBlockIndex = 0;
        /// Amount of spatial blocks that need to be decompressed.
        /// If firstSpatialBlockIndex + spatialBlocksCount is out of bounce of frame spatial block range or
        /// larger than MaxDimensionsPerSubmit.maxSpatialBlocks ReturnCode::ZCE_ERROR_INVALID_ARGUMENTS will be returned.
        size_t spatialBlocksCount = 0;

        /// Write offset in bytes for decompressionPerChannelBlockData buffer. Must be multiple of 4 (sizeof uint).
        /// Decompressor expects the size of registered buffer to be larger than offset + VRAM MemoryLimitPerResource.
        size_t decompressionPerChannelBlockDataOffset = 0;
        /// Write offset in bytes for decompressionPerChannelBlockInfo buffer. Must be multiple of 4 (sizeof uint).
        /// Decompressor expects the size of registered buffer to be larger than offset + VRAM MemoryLimitPerResource.
        size_t decompressionPerChannelBlockInfoOffset = 0;
        /// Write offset in bytes for decompressionPerSpatialBlockInfo buffer. Must be multiple of 4 (sizeof uint).
        /// Decompressor expects the size of registered buffer to be larger than offset + VRAM MemoryLimitPerResource.
        size_t decompressionPerSpatialBlockInfoOffset = 0;
    };

    struct DecompressedFrameFeedback
    {
        /// First index of channel block that was decompressed.
        size_t firstChannelBlockIndex = 0;
        /// Amount of channel blocks that were decompressed.
        size_t channelBlocksCount = 0;
    };

    class Decompressor
    {
    protected:
        virtual ~Decompressor() noexcept = default;

    public:
        /**
         * Initializes decompressor instance. Can enqueue RHI commands and submit work.
         * Must be called before any other method.
         */
        virtual ReturnCode Initialize() noexcept = 0;
        /**
         * Destructs Decompressor instance and releases it's memory.
         * After release pointer becomes invalid.
         */
        virtual void Release() noexcept = 0;
        /**
         * Registers external output RHI resources.
         * Must be called before calling Decompress method.
         * @param resources - Initialized resource set to be used as decompression payload output. Must be created using compatible RHI.
         */
        virtual ReturnCode RegisterResources(const DecompressorResources& resources) noexcept = 0;
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
         * Format mapper getter.
         * @return FormatMapper object
         */
        virtual FormatMapper* GetFormatMapper() noexcept = 0;
        /**
         * Enqueues GPU frame decompression work for provided spatial blocks range.
         * @param desc - DecompressFrameDesc structure that contains input frame container and decompression spatial blocks range.
         * @param outStats - [Output] DecompressedFrameStats structure that contains statistics of decompressed frame.
         * @return Writes data to registered resources on GPU side, and decompression channel blocks range to output DecompressedFrameStats.
         */
        virtual ReturnCode DecompressFrame(const DecompressFrameDesc& desc, DecompressedFrameFeedback* outFeedback) noexcept = 0;
    };

    class DecompressorFactory
    {
    protected:
        virtual ~DecompressorFactory() noexcept = default;

    public:
        /// Sets VRAM limit per resource.
        /// During resource allocation minimum of user defined limit and RHI capabilities limit will be chosen.
        virtual ReturnCode SetMemoryLimitPerResource(size_t limit) noexcept = 0;
        virtual ReturnCode UseDecoder(ZibraVDB::FileDecoder* decoder) noexcept = 0;
        virtual ReturnCode UseRHI(RHI::RHIRuntime* rhi) noexcept = 0;
        virtual ReturnCode Create(Decompressor** outInstance) noexcept = 0;
        /**
         * Destructs DecompressorFactory instance and releases it's memory.
         * After release pointer becomes invalid.
         */
        virtual void Release() noexcept = 0;
    };
    ReturnCode CreateDecompressorFactory(DecompressorFactory** outFactory) noexcept;

    Version GetVersion() noexcept;

    ReturnCode CreateDecoder(const char* filepath, ZibraVDB::FileDecoder** outInstance) noexcept;
    uint64_t GetFileFormatVersion(ZibraVDB::FileDecoder* decoder) noexcept;
    void ReleaseDecoder(ZibraVDB::FileDecoder* decoder) noexcept;
} // namespace Zibra::CE::Decompression

#pragma region CAPI
#if defined(_MSC_VER)
#define ZCE_API_IMPORT extern "C" __declspec(dllimport)
#define ZCE_CALL_CONV __cdecl
#elif defined(__GNUC__)
#define ZCE_API_IMPORT extern "C"
#define ZCE_CALL_CONV
#else
#error "Unsupported compiler"
#endif

#define ZCE_NS Zibra::CE::Decompression

namespace ZCE_NS::CAPI
{
    using FileDecoderHandle = void*;
    using CompressedFrameContainerHandle = void*;
    using FormatMapperHandle = void*;
    using DecompressorHandle = void*;
    using DecompressorFactoryHandle = void*;
} // namespace ZCE_NS::CAPI

#ifndef ZCE_NO_CAPI_IMPL

namespace ZCE_NS
{
    using ReturnCode = Zibra::CE::ReturnCode;
    using MetadataEntry = Zibra::CE::MetadataEntry;
} // namespace ZCE_NS

#pragma region CompressedFrameContainer
#define ZCE_DECOMPRESSION_COMPRESSEDFRAMECONTAINER_EXPORT_FNPFX(name) Zibra_CE_Decompression_CompressedFrameContainer_##name

#define ZCE_DECOMPRESSION_COMPRESSEDFRAMECONTAINER_API_APPLY(macro)                     \
    macro(ZCE_DECOMPRESSION_COMPRESSEDFRAMECONTAINER_EXPORT_FNPFX(GetInfo));            \
    macro(ZCE_DECOMPRESSION_COMPRESSEDFRAMECONTAINER_EXPORT_FNPFX(GetMetadataByKey));   \
    macro(ZCE_DECOMPRESSION_COMPRESSEDFRAMECONTAINER_EXPORT_FNPFX(GetMetadataCount));   \
    macro(ZCE_DECOMPRESSION_COMPRESSEDFRAMECONTAINER_EXPORT_FNPFX(GetMetadataByIndex)); \
    macro(ZCE_DECOMPRESSION_COMPRESSEDFRAMECONTAINER_EXPORT_FNPFX(Release))

#define ZCE_FNPFX(name) ZCE_DECOMPRESSION_COMPRESSEDFRAMECONTAINER_EXPORT_FNPFX(name)

typedef ZCE_NS::FrameInfo (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetInfo)))(ZCE_NS::CAPI::CompressedFrameContainerHandle instance);
typedef const char* (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetMetadataByKey)))(ZCE_NS::CAPI::CompressedFrameContainerHandle instance, const char* key);
typedef size_t (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetMetadataCount)))(ZCE_NS::CAPI::CompressedFrameContainerHandle instance);
typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetMetadataByIndex)))(ZCE_NS::CAPI::CompressedFrameContainerHandle instance, size_t index,
                                                                     ZCE_NS::MetadataEntry* outEntry);
typedef void (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(Release)))(ZCE_NS::CAPI::CompressedFrameContainerHandle instance);

#ifndef ZCE_NO_STATIC_API_DECL
ZCE_API_IMPORT ZCE_NS::FrameInfo ZCE_CALL_CONV ZCE_FNPFX(GetInfo)(ZCE_NS::CAPI::CompressedFrameContainerHandle instance) noexcept;
ZCE_API_IMPORT const char* ZCE_CALL_CONV ZCE_FNPFX(GetMetadataByKey)(ZCE_NS::CAPI::CompressedFrameContainerHandle instance, const char* key) noexcept;
ZCE_API_IMPORT size_t ZCE_CALL_CONV ZCE_FNPFX(GetMetadataCount)(ZCE_NS::CAPI::CompressedFrameContainerHandle instance) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(GetMetadataByIndex)(ZCE_NS::CAPI::CompressedFrameContainerHandle instance, size_t index,
                                                                ZCE_NS::MetadataEntry* outEntry) noexcept;
ZCE_API_IMPORT void ZCE_CALL_CONV ZCE_FNPFX(Release)(ZCE_NS::CAPI::CompressedFrameContainerHandle instance) noexcept;
#else
#define ZCE_DECLARE_API_EXTERN_FUNCS(name) extern ZCE_PFN(name) name;
ZCE_DECOMPRESSION_COMPRESSEDFRAMECONTAINER_API_APPLY(ZCE_DECLARE_API_EXTERN_FUNCS);
#undef ZCE_DECLARE_API_EXTERN_FUNCS
#endif

namespace ZCE_NS::CAPI
{
    class CompressedFrameContainerCAPI final : public CompressedFrameContainer
    {
        friend class DecompressorCAPI;

    public:
        explicit CompressedFrameContainerCAPI(CompressedFrameContainerHandle handle)
            : m_NativeInstance(handle)
        {
        }

    public:
        FrameInfo GetInfo() const noexcept final
        {
            return ZCE_FNPFX(GetInfo)(m_NativeInstance);
        }
        const char* GetMetadataByKey(const char* key) const noexcept final
        {
            return ZCE_FNPFX(GetMetadataByKey)(m_NativeInstance, key);
        }
        size_t GetMetadataCount() const noexcept final
        {
            return ZCE_FNPFX(GetMetadataCount)(m_NativeInstance);
        }
        ReturnCode GetMetadataByIndex(size_t index, MetadataEntry* outEntry) const noexcept final
        {
            return ZCE_FNPFX(GetMetadataByIndex)(m_NativeInstance, index, outEntry);
        }
        void Release() noexcept final
        {
            ZCE_FNPFX(Release)(m_NativeInstance);
            delete this;
        }

    private:
        CompressedFrameContainerHandle m_NativeInstance;
    };
} // namespace ZCE_NS::CAPI

#undef ZCE_FNPFX
#pragma endregion CompressedFrameContainer

#pragma region FormatMapper
#define ZCE_DECOMPRESSION_FORMATMAPPER_EXPORT_FNPFX(name) Zibra_CE_Decompression_FormatMapper_##name

#define ZCE_DECOMPRESSION_FORMATMAPPER_API_APPLY(macro)                     \
    macro(ZCE_DECOMPRESSION_FORMATMAPPER_EXPORT_FNPFX(FetchFrame));         \
    macro(ZCE_DECOMPRESSION_FORMATMAPPER_EXPORT_FNPFX(FetchFrameInfo));     \
    macro(ZCE_DECOMPRESSION_FORMATMAPPER_EXPORT_FNPFX(GetFrameRange));      \
    macro(ZCE_DECOMPRESSION_FORMATMAPPER_EXPORT_FNPFX(GetSequenceInfo));    \
    macro(ZCE_DECOMPRESSION_FORMATMAPPER_EXPORT_FNPFX(GetPlaybackInfo));    \
    macro(ZCE_DECOMPRESSION_FORMATMAPPER_EXPORT_FNPFX(GetMetadataByKey));   \
    macro(ZCE_DECOMPRESSION_FORMATMAPPER_EXPORT_FNPFX(GetMetadataCount));   \
    macro(ZCE_DECOMPRESSION_FORMATMAPPER_EXPORT_FNPFX(GetMetadataByIndex)); \
    macro(ZCE_DECOMPRESSION_FORMATMAPPER_EXPORT_FNPFX(Release))

#define ZCE_FNPFX(name) ZCE_DECOMPRESSION_FORMATMAPPER_EXPORT_FNPFX(name)

typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(FetchFrame)))(ZCE_NS::CAPI::FormatMapperHandle instance, float frame,
                                                                           ZCE_NS::CAPI::CompressedFrameContainerHandle* outFrame);
typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(FetchFrameInfo)))(ZCE_NS::CAPI::FormatMapperHandle instance, float frame,
                                                                               ZCE_NS::FrameInfo* outInfo);
typedef ZCE_NS::FrameRange (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetFrameRange)))(ZCE_NS::CAPI::FormatMapperHandle instance);
typedef ZCE_NS::SequenceInfo (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetSequenceInfo)))(ZCE_NS::CAPI::FormatMapperHandle instance);
typedef ZCE_NS::PlaybackInfo (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetPlaybackInfo)))(ZCE_NS::CAPI::FormatMapperHandle instance);
typedef const char* (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetMetadataByKey)))(ZCE_NS::CAPI::FormatMapperHandle instance, const char* key);
typedef size_t (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetMetadataCount)))(ZCE_NS::CAPI::FormatMapperHandle instance);
typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetMetadataByIndex)))(ZCE_NS::CAPI::FormatMapperHandle instance, size_t index,
                                                                                   ZCE_NS::MetadataEntry* outEntry);
typedef void (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(Release)))(ZCE_NS::CAPI::FormatMapperHandle instance);

#ifndef ZCE_NO_STATIC_API_DECL
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(FetchFrame)(ZCE_NS::CAPI::FormatMapperHandle instance, float frame,
                                                        ZCE_NS::CAPI::CompressedFrameContainerHandle* outFrame) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(FetchFrameInfo)(ZCE_NS::CAPI::FormatMapperHandle instance, float frame,
                                                            ZCE_NS::FrameInfo* outInfo) noexcept;
ZCE_API_IMPORT ZCE_NS::FrameRange ZCE_CALL_CONV ZCE_FNPFX(GetFrameRange)(ZCE_NS::CAPI::FormatMapperHandle instance) noexcept;
ZCE_API_IMPORT ZCE_NS::SequenceInfo ZCE_CALL_CONV ZCE_FNPFX(GetSequenceInfo)(ZCE_NS::CAPI::FormatMapperHandle instance) noexcept;
ZCE_API_IMPORT ZCE_NS::PlaybackInfo ZCE_CALL_CONV ZCE_FNPFX(GetPlaybackInfo)(ZCE_NS::CAPI::FormatMapperHandle instance) noexcept;
ZCE_API_IMPORT const char* ZCE_CALL_CONV ZCE_FNPFX(GetMetadataByKey)(ZCE_NS::CAPI::FormatMapperHandle instance, const char* key) noexcept;
ZCE_API_IMPORT size_t ZCE_CALL_CONV ZCE_FNPFX(GetMetadataCount)(ZCE_NS::CAPI::FormatMapperHandle instance) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(GetMetadataByIndex)(ZCE_NS::CAPI::FormatMapperHandle instance, size_t index,
                                                                              ZCE_NS::MetadataEntry* outEntry) noexcept;
ZCE_API_IMPORT void ZCE_CALL_CONV ZCE_FNPFX(Release)(ZCE_NS::CAPI::FormatMapperHandle instance) noexcept;
#else
#define ZCE_DECLARE_API_EXTERN_FUNCS(name) extern ZCE_PFN(name) name;
ZCE_DECOMPRESSION_FORMATMAPPER_API_APPLY(ZCE_DECLARE_API_EXTERN_FUNCS);
#undef ZCE_DECLARE_API_EXTERN_FUNCS
#endif

namespace ZCE_NS::CAPI
{
    class FormatMapperCAPI final : public FormatMapper
    {
    public:
        explicit FormatMapperCAPI(FormatMapperHandle handle)
            : m_NativeInstance(handle)
        {
        }

    public:
        ReturnCode FetchFrame(float frame, CompressedFrameContainer** outFrame) noexcept final
        {
            CompressedFrameContainerHandle outHandle{};
            auto code = ZCE_FNPFX(FetchFrame)(m_NativeInstance, frame, &outHandle);
            *outFrame = new CompressedFrameContainerCAPI{outHandle};
            return code;
        }
        ReturnCode FetchFrameInfo(float frame, FrameInfo* outInfo) noexcept final
        {
            return ZCE_FNPFX(FetchFrameInfo)(m_NativeInstance, frame, outInfo);
        }
        FrameRange GetFrameRange() const noexcept final
        {
            return ZCE_FNPFX(GetFrameRange)(m_NativeInstance);
        }
        SequenceInfo GetSequenceInfo() const noexcept final
        {
            return ZCE_FNPFX(GetSequenceInfo)(m_NativeInstance);
        }
        PlaybackInfo GetPlaybackInfo() const noexcept final
        {
            return ZCE_FNPFX(GetPlaybackInfo)(m_NativeInstance);
        }
        const char* GetMetadataByKey(const char* key) const noexcept final
        {
            return ZCE_FNPFX(GetMetadataByKey)(m_NativeInstance, key);
        }
        size_t GetMetadataCount() const noexcept final
        {
            return ZCE_FNPFX(GetMetadataCount)(m_NativeInstance);
        }
        ReturnCode GetMetadataByIndex(size_t index, MetadataEntry* outEntry) const noexcept final
        {
            return ZCE_FNPFX(GetMetadataByIndex)(m_NativeInstance, index, outEntry);
        }
        void Release() noexcept final
        {
            ZCE_FNPFX(Release)(m_NativeInstance);
            delete this;
        }

    private:
        FormatMapperHandle m_NativeInstance;
    };
} // namespace ZCE_NS::CAPI

#undef ZCE_FNPFX
#pragma endregion FormatMapper

#pragma region Decompressor
#define ZCE_DECOMPRESSION_DECOMPRESSOR_EXPORT_FNPFX(name) Zibra_CE_Decompression_Decompressor_##name

#define ZCE_DECOMPRESSION_DECOMPRESSOR_API_APPLY(macro)                            \
    macro(ZCE_DECOMPRESSION_DECOMPRESSOR_EXPORT_FNPFX(Initialize));                \
    macro(ZCE_DECOMPRESSION_DECOMPRESSOR_EXPORT_FNPFX(Release));                   \
    macro(ZCE_DECOMPRESSION_DECOMPRESSOR_EXPORT_FNPFX(RegisterResources));         \
    macro(ZCE_DECOMPRESSION_DECOMPRESSOR_EXPORT_FNPFX(GetResourcesRequirements));  \
    macro(ZCE_DECOMPRESSION_DECOMPRESSOR_EXPORT_FNPFX(GetFormatMapper));           \
    macro(ZCE_DECOMPRESSION_DECOMPRESSOR_EXPORT_FNPFX(GetMaxDimensionsPerSubmit)); \
    macro(ZCE_DECOMPRESSION_DECOMPRESSOR_EXPORT_FNPFX(DecompressFrame))

#define ZCE_FNPFX(name) ZCE_DECOMPRESSION_DECOMPRESSOR_EXPORT_FNPFX(name)

typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(Initialize)))(ZCE_NS::CAPI::DecompressorHandle instance);
typedef void (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(Release)))(ZCE_NS::CAPI::DecompressorHandle instance);
typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(RegisterResources)))(ZCE_NS::CAPI::DecompressorHandle instance,
                                                                    const ZCE_NS::DecompressorResources& resources);
typedef ZCE_NS::DecompressorResourcesRequirements (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetResourcesRequirements)))(
    ZCE_NS::CAPI::DecompressorHandle instance);
typedef ZCE_NS::CAPI::FormatMapperHandle (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetFormatMapper)))(ZCE_NS::CAPI::DecompressorHandle instance);
typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(DecompressFrame)))(ZCE_NS::CAPI::DecompressorHandle instance,
                                                                               const ZCE_NS::DecompressFrameDesc& desc,
                                                                               ZCE_NS::DecompressedFrameFeedback* outFeedback);
typedef ZCE_NS::MaxDimensionsPerSubmit (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetMaxDimensionsPerSubmit)))(ZCE_NS::CAPI::DecompressorHandle instance);

#ifndef ZCE_NO_STATIC_API_DECL
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(Initialize)(ZCE_NS::CAPI::DecompressorHandle instance) noexcept;
ZCE_API_IMPORT void ZCE_CALL_CONV ZCE_FNPFX(Release)(ZCE_NS::CAPI::DecompressorHandle instance) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(RegisterResources)(ZCE_NS::CAPI::DecompressorHandle instance,
                                                               const ZCE_NS::DecompressorResources& resources) noexcept;
ZCE_API_IMPORT ZCE_NS::DecompressorResourcesRequirements ZCE_CALL_CONV ZCE_FNPFX(GetResourcesRequirements)(
    ZCE_NS::CAPI::DecompressorHandle instance) noexcept;
ZCE_API_IMPORT ZCE_NS::CAPI::FormatMapperHandle ZCE_CALL_CONV ZCE_FNPFX(GetFormatMapper)(ZCE_NS::CAPI::DecompressorHandle instance) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(DecompressFrame)(ZCE_NS::CAPI::DecompressorHandle instance,
                                                                           const ZCE_NS::DecompressFrameDesc& desc,
                                                                           ZCE_NS::DecompressedFrameFeedback* outFeedback) noexcept;
ZCE_API_IMPORT ZCE_NS::MaxDimensionsPerSubmit ZCE_CALL_CONV ZCE_FNPFX(GetMaxDimensionsPerSubmit)(ZCE_NS::CAPI::DecompressorHandle instance) noexcept;

#else
#define ZCE_DECLARE_API_EXTERN_FUNCS(name) extern ZCE_PFN(name) name;
ZCE_DECOMPRESSION_DECOMPRESSOR_API_APPLY(ZCE_DECLARE_API_EXTERN_FUNCS);
#undef ZCE_DECLARE_API_EXTERN_FUNCS
#endif

namespace ZCE_NS::CAPI
{
    class DecompressorCAPI final : public Decompressor
    {
    public:
        explicit DecompressorCAPI(DecompressorHandle handle) noexcept
            : m_NativeInstance(handle)
        {
        }

    public:
        ReturnCode Initialize() noexcept final
        {
            return ZCE_FNPFX(Initialize)(m_NativeInstance);
        }
        void Release() noexcept final
        {
            ZCE_FNPFX(Release)(m_NativeInstance);
            delete this;
        }
        ReturnCode RegisterResources(const DecompressorResources& resources) noexcept final
        {
            return ZCE_FNPFX(RegisterResources)(m_NativeInstance, resources);
        }
        DecompressorResourcesRequirements GetResourcesRequirements() noexcept final
        {
            return ZCE_FNPFX(GetResourcesRequirements)(m_NativeInstance);
        }
        FormatMapper* GetFormatMapper() noexcept final
        {
            return new FormatMapperCAPI{ZCE_FNPFX(GetFormatMapper)(m_NativeInstance)};
        }
        ReturnCode DecompressFrame(const DecompressFrameDesc& desc, DecompressedFrameFeedback* outFeedback) noexcept final
        {
            assert(dynamic_cast<CompressedFrameContainerCAPI*>(desc.frameContainer) != nullptr);
            DecompressFrameDesc patchedDesc = desc;
            auto wrapped = static_cast<CompressedFrameContainerCAPI*>(desc.frameContainer)->m_NativeInstance;
            patchedDesc.frameContainer = reinterpret_cast<CompressedFrameContainer*>(wrapped);
            return ZCE_FNPFX(DecompressFrame)(m_NativeInstance, patchedDesc, outFeedback);
        }

        [[nodiscard]] MaxDimensionsPerSubmit GetMaxDimensionsPerSubmit() const noexcept final
        {
            return ZCE_FNPFX(GetMaxDimensionsPerSubmit)(m_NativeInstance);
        }

    private:
        DecompressorHandle m_NativeInstance;
    };
} // namespace ZCE_NS::CAPI

#undef ZCE_FNPFX
#pragma endregion Decompressor

#pragma region DecompressorFactory
#define ZCE_DECOMPRESSION_DECOMPRESSORFACTORY_EXPORT_FNPFX(name) Zibra_CE_Decompression_DecompressorFactory_##name

#define ZCE_DECOMPRESSION_DECOMPRESSORFACTORY_API_APPLY(macro)                            \
    macro(ZCE_DECOMPRESSION_DECOMPRESSORFACTORY_EXPORT_FNPFX(SetMemoryLimitPerResource)); \
    macro(ZCE_DECOMPRESSION_DECOMPRESSORFACTORY_EXPORT_FNPFX(UseDecoder));                \
    macro(ZCE_DECOMPRESSION_DECOMPRESSORFACTORY_EXPORT_FNPFX(UseRHI));                    \
    macro(ZCE_DECOMPRESSION_DECOMPRESSORFACTORY_EXPORT_FNPFX(Create));                    \
    macro(ZCE_DECOMPRESSION_DECOMPRESSORFACTORY_EXPORT_FNPFX(Release))

#define ZCE_FNPFX(name) ZCE_DECOMPRESSION_DECOMPRESSORFACTORY_EXPORT_FNPFX(name)

typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(SetMemoryLimitPerResource)))(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                                            size_t limitInBytes);
typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(UseDecoder)))(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                             Zibra::CE::ZibraVDB::FileDecoder* decoder);
typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(UseRHI)))(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                         Zibra::RHI::CAPI::ConsumerBridge::RHIRuntimeVTable vt);
typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(Create)))(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                         ZCE_NS::CAPI::DecompressorHandle* outInstanceHnd);
typedef void (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(Release)))(ZCE_NS::CAPI::DecompressorFactoryHandle instance);

#ifndef ZCE_NO_STATIC_API_DECL
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(SetMemoryLimitPerResource)(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                                       size_t limitInBytes) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(UseDecoder)(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                        Zibra::CE::ZibraVDB::FileDecoder* decoder) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(UseRHI)(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                    Zibra::RHI::CAPI::ConsumerBridge::RHIRuntimeVTable vt) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(Create)(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                    ZCE_NS::CAPI::DecompressorHandle* outInstanceHnd) noexcept;
ZCE_API_IMPORT void ZCE_CALL_CONV ZCE_FNPFX(Release)(ZCE_NS::CAPI::DecompressorFactoryHandle instance) noexcept;
#else
#define ZCE_DECLARE_API_EXTERN_FUNCS(name) extern ZCE_PFN(name) name;
ZCE_DECOMPRESSION_DECOMPRESSORFACTORY_API_APPLY(ZCE_DECLARE_API_EXTERN_FUNCS);
#undef ZCE_DECLARE_API_EXTERN_FUNCS
#endif

namespace ZCE_NS::CAPI
{
    class DecompressorFactoryCAPI : public DecompressorFactory
    {
    public:
        explicit DecompressorFactoryCAPI(DecompressorFactoryHandle handle)
            : m_NativeInstance(handle)
        {
        }

    public:
        ReturnCode SetMemoryLimitPerResource(size_t limitInBytes) noexcept final
        {
            return ZCE_FNPFX(SetMemoryLimitPerResource)(m_NativeInstance, limitInBytes);
        }

        ReturnCode UseDecoder(ZibraVDB::FileDecoder* decoder) noexcept final
        {
            return ZCE_FNPFX(UseDecoder)(m_NativeInstance, decoder);
        }
        ReturnCode UseRHI(RHI::RHIRuntime* rhi) noexcept final
        {
            return ZCE_FNPFX(UseRHI)(m_NativeInstance, RHI::CAPI::ConsumerBridge::VTConvert(rhi));
        }
        ReturnCode Create(Decompressor** outInstance) noexcept final
        {
            DecompressorHandle outHandle{};
            auto code = ZCE_FNPFX(Create)(m_NativeInstance, &outHandle);
            *outInstance = new DecompressorCAPI{outHandle};
            return code;
        }
        void Release() noexcept final
        {
            ZCE_FNPFX(Release)(m_NativeInstance);
            delete this;
        }

    private:
        DecompressorFactoryHandle m_NativeInstance;
    };
} // namespace ZCE_NS::CAPI

#undef ZCE_FNPFX
#pragma endregion DecompressorFactory

#pragma region Namespace Functions
#define ZCE_DECOMPRESSION_FUNCS_EXPORT_FNPFX(name) Zibra_CE_Decompression_##name

#define ZCE_DECOMPRESSION_FUNCS_API_APPLY(macro)                            \
    macro(ZCE_DECOMPRESSION_FUNCS_EXPORT_FNPFX(GetVersion));                \
    macro(ZCE_DECOMPRESSION_FUNCS_EXPORT_FNPFX(CreateDecompressorFactory)); \
    macro(ZCE_DECOMPRESSION_FUNCS_EXPORT_FNPFX(CreateDecoder));             \
    macro(ZCE_DECOMPRESSION_FUNCS_EXPORT_FNPFX(GetFileFormatVersion));      \
    macro(ZCE_DECOMPRESSION_FUNCS_EXPORT_FNPFX(ReleaseDecoder))

#define ZCE_FNPFX(name) ZCE_DECOMPRESSION_FUNCS_EXPORT_FNPFX(name)

typedef Zibra::Version (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetVersion)))();
typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(CreateDecompressorFactory)))(ZCE_NS::CAPI::DecompressorFactoryHandle* outFactory);
typedef ZCE_NS::ReturnCode (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(CreateDecoder)))(const char* filepath, Zibra::CE::ZibraVDB::FileDecoder** outInstance);
typedef uint64_t (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(GetFileFormatVersion)))(Zibra::CE::ZibraVDB::FileDecoder* decoder);
typedef void (ZCE_CALL_CONV *ZCE_PFN(ZCE_FNPFX(ReleaseDecoder)))(Zibra::CE::ZibraVDB::FileDecoder* decoder);

#ifndef ZCE_NO_STATIC_API_DECL
ZCE_API_IMPORT Zibra::Version ZCE_CALL_CONV ZCE_FNPFX(GetVersion)() noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(CreateDecompressorFactory)(ZCE_NS::CAPI::DecompressorFactoryHandle* outFactory) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_CALL_CONV ZCE_FNPFX(CreateDecoder)(const char* filepath, Zibra::CE::ZibraVDB::FileDecoder** outInstance) noexcept;
ZCE_API_IMPORT uint64_t ZCE_CALL_CONV ZCE_FNPFX(GetFileFormatVersion)(Zibra::CE::ZibraVDB::FileDecoder* decoder) noexcept;
ZCE_API_IMPORT void ZCE_CALL_CONV ZCE_FNPFX(ReleaseDecoder)(Zibra::CE::ZibraVDB::FileDecoder* decoder) noexcept;
#else
#define ZCE_DECLARE_API_EXTERN_FUNCS(name) extern ZCE_PFN(name) name;
ZCE_DECOMPRESSION_FUNCS_API_APPLY(ZCE_DECLARE_API_EXTERN_FUNCS);
#undef ZCE_DECLARE_API_EXTERN_FUNCS
#endif

namespace ZCE_NS::CAPI
{
    inline Version GetVersion() noexcept
    {
        return ZCE_FNPFX(GetVersion)();
    }

    inline ReturnCode CreateDecompressorFactory(DecompressorFactory** outFactory) noexcept
    {
        DecompressorFactoryHandle handle{};
        auto code = ZCE_FNPFX(CreateDecompressorFactory)(&handle);
        *outFactory = new DecompressorFactoryCAPI{handle};
        return code;
    }

    inline ReturnCode CreateDecoder(const char* filepath, ZibraVDB::FileDecoder** outInstance) noexcept
    {
        return ZCE_FNPFX(CreateDecoder)(filepath, outInstance);
    }
    inline uint64_t GetFileFormatVersion(ZibraVDB::FileDecoder* decoder) noexcept
    {
        return ZCE_FNPFX(GetFileFormatVersion)(decoder);
    }
    inline void ReleaseDecoder(ZibraVDB::FileDecoder* decoder) noexcept
    {
        ZCE_FNPFX(ReleaseDecoder)(decoder);
    }
} // namespace ZCE_NS::CAPI

#undef ZCE_FNPFX
#pragma endregion Namespace Functions

#define ZCE_DECOMPRESSION_API_APPLY(macro)                       \
    ZCE_DECOMPRESSION_COMPRESSEDFRAMECONTAINER_API_APPLY(macro); \
    ZCE_DECOMPRESSION_FORMATMAPPER_API_APPLY(macro);             \
    ZCE_DECOMPRESSION_DECOMPRESSOR_API_APPLY(macro);             \
    ZCE_DECOMPRESSION_DECOMPRESSORFACTORY_API_APPLY(macro);      \
    ZCE_DECOMPRESSION_FUNCS_API_APPLY(macro)

#pragma region ConsumerBridge

namespace ZCE_NS::CAPI::ConsumerBridge
{
    struct CompressedFrameContainerVTable
    {
        void* obj;
        FrameInfo (*GetInfo)(void*);
        const char* (*GetMetadataByKey)(void*, const char* key);
        size_t (*GetMetadataCount)(void*);
        ReturnCode (*GetMetadataByIndex)(void*, size_t index, MetadataEntry* outEntry);
        void (*Release)(void*);
    };

    inline CompressedFrameContainerVTable VTConvert(CompressedFrameContainer* obj) noexcept
    {
        using T = CompressedFrameContainer;

        CompressedFrameContainerVTable vt{};
        vt.obj = obj;
        vt.GetInfo = [](void* o) { return static_cast<T*>(o)->GetInfo(); };
        vt.GetMetadataByKey = [](void* o, const char* k) { return static_cast<T*>(o)->GetMetadataByKey(k); };
        vt.GetMetadataCount = [](void* o) { return static_cast<T*>(o)->GetMetadataCount(); };
        vt.GetMetadataByIndex = [](void* o, size_t i, MetadataEntry* oe) { return static_cast<T*>(o)->GetMetadataByIndex(i, oe); };
        vt.Release = [](void* o) { return static_cast<T*>(o)->Release(); };
        return vt;
    }

    struct FormatMapperVTable
    {
        void* obj;
        void (*destructor)(void*);
        ReturnCode (*FetchFrame)(void*, float frame, CompressedFrameContainerHandle* outFrame);
        ReturnCode (*FetchFrameInfo)(void*, float frame, FrameInfo* outInfo);
        FrameRange (*GetFrameRange)(void*);
    };

    struct DecompressorVTable
    {
        void* obj;
        void (*destructor)(void*);
        ReturnCode (*Initialize)(void*, RHI::RHIRuntime* rhi);
        void (*Release)(void*);
        ReturnCode (*RegisterResources)(void*, const DecompressorResources& resources);
        DecompressorResourcesRequirements (*GetResourcesRequirements)(void*);
        FormatMapperHandle (*GetFormatMapper)(void*);
        ReturnCode (*DecompressFrame)(void*, const DecompressFrameDesc& desc, DecompressedFrameFeedback* outFeedback);
    };
} // namespace ZCE_NS::CAPI::ConsumerBridge

#pragma endregion ConsumerBridge

#endif // ZCE_NO_CAPI_IMPL

#undef ZCE_NS
#undef ZCE_API_IMPORT
#undef ZCE_CALL_CONV
#pragma endregion CAPI
