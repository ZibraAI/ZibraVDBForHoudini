#pragma once

#include <Zibra/CE/Common.h>
#include <Zibra/CE/Math3D.h>
#include <Zibra/RHI.h>
#include <cstdint>

namespace Zibra::CE::ZibraVDB
{
    class FileDecoder;
}

namespace Zibra::CE::Decompression
{
    constexpr size_t MAX_CHANNEL_COUNT = 8;

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
     *
     * @warning If you are about changing this structure use checklist:
     * - make sure that you are not exposing specific implementation details. Expose only general abstract data.
     * - think about potential future decompressor implementations. Make resources format as flexible as possible. (No DCT or Huffman
     * params, etc..)
     * - add fields to appropriate buffer items instead of adding new buffer if it is possible.
     * - remember about Dependency Inversion principle!!!
     * - discuss changes with 3d team
     */
    struct DecompressorResources
    {
        /// Payload per channel block.
        RHI::Buffer* decompressionPerChannelBlockData;
        /// Info per channel block
        RHI::Buffer* decompressionPerChannelBlockInfo;
        /// Info per spatial group
        RHI::Buffer* decompressionPerSpatialBlockInfo;
    };

    struct SpatialBlock
    {
        /**
         * Position of spatial block in space.
         * Stored in blocks. To get position in voxels must be multiplied by sparseBlockSize.
         * @range [0; 2^10]
         */
        int32_t coords[3];

        /**
         * Offset to first channel block of this spatial block.
         * @range [0; 2^32]
         */
        uint32_t channelBlocksOffset;

        /**
         * Mask of active channels in this spatial block.
         * @range [0; 2^32]
         */
        uint32_t channelMask;

        /**
         * Count of active channels in this spatial block.
         * @range [0; 8]
         */
        uint32_t channelCount;
    };

    struct ChannelBlock
    {
        /**
         * Dense voxels container.
         * @range [-INF; INF]
         */
        float voxels[SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE] = {};
    };

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
        virtual FrameInfo GetInfo() noexcept = 0;
        /**
         * Finds metadata entry by key and returns payload.
         * @param key - Metadata key
         * @return String payload or nullptr if key is not present.
         * Memory managed by CompressedFrameContainer instance and present while container exists.
         */
        virtual const char* GetMetadataByKey(const char* key) noexcept = 0;
        virtual size_t GetMetadataCount() noexcept = 0;
        virtual ReturnCode GetMetadataByIndex(size_t index, MetadataEntry* outEntry) noexcept = 0;
        virtual void Release() noexcept = 0;
    };

    struct FrameRange
    {
        int32_t start;
        int32_t end;
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
        virtual FrameRange GetFrameRange() noexcept = 0;
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
        virtual ReturnCode DecompressFrame(const DecompressFrameDesc* desc, DecompressedFrameFeedback* outFeedback) noexcept = 0;
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

    ReturnCode CreateDecoder(const char* filepath, ZibraVDB::FileDecoder** outInstance) noexcept;
    uint64_t GetFileFormatVersion(ZibraVDB::FileDecoder* decoder) noexcept;
    void ReleaseDecoder(ZibraVDB::FileDecoder* decoder) noexcept;
} // namespace Zibra::CE::Decompression

#pragma region CAPI
#define ZCE_API_IMPORT __declspec(dllimport)
#define ZCE_CONCAT_HELPER(A, B) A##B
#define ZCE_PFN(name) ZCE_CONCAT_HELPER(PFN_, name)
#define ZCE_NS Zibra::CE::Decompression

#if !defined(ZCE_NO_CAPI_IMPL) && !defined(ZCE_NO_STATIC_API_DECL) && !defined(ZCE_API_CALL)
#define ZCE_API_CALL(func, ...) func(__VA_ARGS__)
#elif !defined(ZCE_NO_CAPI_IMPL) && defined(ZCE_NO_STATIC_API_DECL) && !defined(ZCE_API_CALL)
#error "You must define ZCE_API_CALL(func, ...) macro before using CAPI without static function declaration."
#endif

namespace ZCE_NS::CAPI
{
    using FileDecoderHandle = void*;
    using CompressedFrameContainerHandle = void*;
    using FormatMapperHandle = void*;
    using DecompressorHandle = void*;
    using DecompressorFactoryHandle = void*;
} // namespace ZCE_NS::CAPI

namespace ZCE_NS
{
    using ReturnCode = Zibra::CE::ReturnCode;
    using MetadataEntry = Zibra::CE::MetadataEntry;
} // namespace ZCE_NS

#pragma region CompressedFrameContainer
#define ZCE_FNPFX(name) Zibra_CE_Decompression_CompressedFrameContainer_##name

typedef ZCE_NS::FrameInfo (*ZCE_PFN(ZCE_FNPFX(GetInfo)))(ZCE_NS::CAPI::CompressedFrameContainerHandle instance);
typedef const char* (*ZCE_PFN(ZCE_FNPFX(GetMetadataByKey)))(ZCE_NS::CAPI::CompressedFrameContainerHandle instance, const char* key);
typedef size_t (*ZCE_PFN(ZCE_FNPFX(GetMetadataCount)))(ZCE_NS::CAPI::CompressedFrameContainerHandle instance);
typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(GetMetadataByIndex)))(ZCE_NS::CAPI::CompressedFrameContainerHandle instance, size_t index, ZCE_NS::MetadataEntry* outEntry);
typedef void (*ZCE_PFN(ZCE_FNPFX(Release)))(ZCE_NS::CAPI::CompressedFrameContainerHandle instance);

#ifdef ZRHI_STATIC_API_DECL
ZCE_API_IMPORT ZCE_NS::FrameInfo ZCE_FNPFX(GetInfo)(ZCE_NS::CAPI::CompressedFrameContainerHandle instance) noexcept;
ZCE_API_IMPORT const char* ZCE_FNPFX(GetMetadataByKey)(ZCE_NS::CAPI::CompressedFrameContainerHandle instance, const char* key) noexcept;
ZCE_API_IMPORT size_t ZCE_FNPFX(GetMetadataCount)(ZCE_NS::CAPI::CompressedFrameContainerHandle instance) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(GetMetadataByIndex)(ZCE_NS::CAPI::CompressedFrameContainerHandle instance, size_t index,
                                                                ZCE_NS::MetadataEntry* outEntry) noexcept;
ZCE_API_IMPORT void ZCE_FNPFX(Release)(ZCE_NS::CAPI::CompressedFrameContainerHandle instance) noexcept;
#else
extern ZCE_PFN(ZCE_FNPFX(GetInfo)) ZCE_FNPFX(GetInfo);
extern ZCE_PFN(ZCE_FNPFX(GetMetadataByKey)) ZCE_FNPFX(GetMetadataByKey);
extern ZCE_PFN(ZCE_FNPFX(GetMetadataCount)) ZCE_FNPFX(GetMetadataCount);
extern ZCE_PFN(ZCE_FNPFX(GetMetadataByIndex)) ZCE_FNPFX(GetMetadataByIndex);
extern ZCE_PFN(ZCE_FNPFX(Release)) ZCE_FNPFX(Release);
#endif

#ifndef ZCE_NO_CAPI_IMPL
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
        FrameInfo GetInfo() noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(GetInfo), m_NativeInstance);
        }
        const char* GetMetadataByKey(const char* key) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(GetMetadataByKey), m_NativeInstance, key);
        }
        size_t GetMetadataCount() noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(GetMetadataCount), m_NativeInstance);
        }
        ReturnCode GetMetadataByIndex(size_t index, MetadataEntry* outEntry) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(GetMetadataByIndex), m_NativeInstance, index, outEntry);
        }
        void Release() noexcept final
        {
            ZCE_API_CALL(ZCE_FNPFX(Release), m_NativeInstance);
            delete this;
        }

    private:
        CompressedFrameContainerHandle m_NativeInstance;
    };
} // namespace ZCE_NS::CAPI
#endif

#undef ZCE_FNPFX
#pragma endregion CompressedFrameContainer

#pragma region FormatMapper
#define ZCE_FNPFX(name) Zibra_CE_Decompression_FormatMapper_##name

typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(FetchFrame)))(ZCE_NS::CAPI::FormatMapperHandle instance, float frame, ZCE_NS::CAPI::CompressedFrameContainerHandle* outFrame);
typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(FetchFrameInfo)))(ZCE_NS::CAPI::FormatMapperHandle instance, float frame, ZCE_NS::FrameInfo* outInfo);
typedef ZCE_NS::FrameRange (*ZCE_PFN(ZCE_FNPFX(GetFrameRange)))(ZCE_NS::CAPI::FormatMapperHandle instance);

#ifdef ZRHI_STATIC_API_DECL
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(FetchFrame)(ZCE_NS::CAPI::FormatMapperHandle instance, float frame,
                                                        ZCE_NS::CAPI::CompressedFrameContainerHandle* outFrame) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(FetchFrameInfo)(ZCE_NS::CAPI::FormatMapperHandle instance, float frame,
                                                            ZCE_NS::FrameInfo* outInfo) noexcept;
ZCE_API_IMPORT ZCE_NS::FrameRange ZCE_FNPFX(GetFrameRange)(ZCE_NS::CAPI::FormatMapperHandle instance) noexcept;
#else
extern ZCE_PFN(ZCE_FNPFX(FetchFrame)) ZCE_FNPFX(FetchFrame);
extern ZCE_PFN(ZCE_FNPFX(FetchFrameInfo)) ZCE_FNPFX(FetchFrameInfo);
extern ZCE_PFN(ZCE_FNPFX(GetFrameRange)) ZCE_FNPFX(GetFrameRange);
#endif

#ifndef ZCE_NO_CAPI_IMPL
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
            auto code = ZCE_API_CALL(ZCE_FNPFX(FetchFrame), m_NativeInstance, frame, &outHandle);
            *outFrame = new CompressedFrameContainerCAPI{outHandle};
            return code;
        }
        ReturnCode FetchFrameInfo(float frame, FrameInfo* outInfo) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(FetchFrameInfo), m_NativeInstance, frame, outInfo);
        }
        FrameRange GetFrameRange() noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(GetFrameRange), m_NativeInstance);
        }

    private:
        FormatMapperHandle m_NativeInstance;
    };
} // namespace ZCE_NS::CAPI
#endif

#undef ZCE_FNPFX
#pragma endregion FormatMapper

#pragma region Decompressor
#define ZCE_FNPFX(name) Zibra_CE_Decompression_Decompressor_##name

typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(Initialize)))(ZCE_NS::CAPI::DecompressorHandle instance);
typedef void (*ZCE_PFN(ZCE_FNPFX(Release)))(ZCE_NS::CAPI::DecompressorHandle instance);
typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(RegisterResources)))(ZCE_NS::CAPI::DecompressorHandle instance, const ZCE_NS::DecompressorResources& resources);
typedef ZCE_NS::DecompressorResourcesRequirements (*ZCE_PFN(ZCE_FNPFX(GetResourcesRequirements)))(ZCE_NS::CAPI::DecompressorHandle instance);
typedef ZCE_NS::CAPI::FormatMapperHandle (*ZCE_PFN(ZCE_FNPFX(GetFormatMapper)))(ZCE_NS::CAPI::DecompressorHandle instance);
typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(DecompressFrame)))(ZCE_NS::CAPI::DecompressorHandle instance,
                                                                  const ZCE_NS::DecompressFrameDesc* desc,
                                                                  ZCE_NS::DecompressedFrameFeedback* outFeedback);
typedef ZCE_NS::MaxDimensionsPerSubmit (*ZCE_PFN(ZCE_FNPFX(GetMaxDimensionsPerSubmit)))(ZCE_NS::CAPI::DecompressorHandle instance);

#ifdef ZRHI_STATIC_API_DECL
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(Initialize)(ZCE_NS::CAPI::DecompressorHandle instance) noexcept;
ZCE_API_IMPORT void ZCE_FNPFX(Release)(ZCE_NS::CAPI::DecompressorHandle instance) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(RegisterResources)(ZCE_NS::CAPI::DecompressorHandle instance,
                                                               const ZCE_NS::DecompressorResources& resources) noexcept;
ZCE_API_IMPORT ZCE_NS::DecompressorResourcesRequirements ZCE_FNPFX(GetResourcesRequirements)(
    ZCE_NS::CAPI::DecompressorHandle instance) noexcept;
ZCE_API_IMPORT ZCE_NS::CAPI::FormatMapperHandle ZCE_FNPFX(GetFormatMapper)(ZCE_NS::CAPI::DecompressorHandle instance) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(DecompressFrame)(ZCE_NS::CAPI::DecompressorHandle instance,
                                                             const ZCE_NS::DecompressFrameDesc* desc,
                                                             ZCE_NS::DecompressedFrameFeedback* outFeedback) noexcept;
ZCE_API_IMPORT ZCE_NS::MaxDimensionsPerSubmit ZCE_FNPFX(GetMaxDimensionsPerSubmit)(ZCE_NS::CAPI::DecompressorHandle instance) noexcept;
#else
extern ZCE_PFN(ZCE_FNPFX(Initialize)) ZCE_FNPFX(Initialize);
extern ZCE_PFN(ZCE_FNPFX(Release)) ZCE_FNPFX(Release);
extern ZCE_PFN(ZCE_FNPFX(RegisterResources)) ZCE_FNPFX(RegisterResources);
extern ZCE_PFN(ZCE_FNPFX(GetResourcesRequirements)) ZCE_FNPFX(GetResourcesRequirements);
extern ZCE_PFN(ZCE_FNPFX(GetFormatMapper)) ZCE_FNPFX(GetFormatMapper);
extern ZCE_PFN(ZCE_FNPFX(DecompressFrame)) ZCE_FNPFX(DecompressFrame);
extern ZCE_PFN(ZCE_FNPFX(GetMaxDimensionsPerSubmit)) ZCE_FNPFX(GetMaxDimensionsPerSubmit);
#endif

#ifndef ZCE_NO_CAPI_IMPL
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
            return ZCE_API_CALL(ZCE_FNPFX(Initialize), m_NativeInstance);
        }
        void Release() noexcept final
        {
            ZCE_API_CALL(ZCE_FNPFX(Release), m_NativeInstance);
            delete this;
        }
        ReturnCode RegisterResources(const DecompressorResources& resources) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(RegisterResources), m_NativeInstance, resources);
        }
        DecompressorResourcesRequirements GetResourcesRequirements() noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(GetResourcesRequirements), m_NativeInstance);
        }
        FormatMapper* GetFormatMapper() noexcept final
        {
            return new FormatMapperCAPI{ZCE_API_CALL(ZCE_FNPFX(GetFormatMapper), m_NativeInstance)};
        }
        ReturnCode DecompressFrame(const DecompressFrameDesc* desc, DecompressedFrameFeedback* outFeedback) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(DecompressFrame), m_NativeInstance, desc, outFeedback);
        }

        [[nodiscard]] MaxDimensionsPerSubmit GetMaxDimensionsPerSubmit() const noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(GetMaxDimensionsPerSubmit), m_NativeInstance);
        }

    private:
        DecompressorHandle m_NativeInstance;
    };
} // namespace ZCE_NS::CAPI
#endif

#undef ZCE_FNPFX
#pragma endregion Decompressor

#pragma region DecompressorFactory
#define ZCE_FNPFX(name) Zibra_CE_Decompression_DecompressorFactory_##name

typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(SetMemoryLimitPerResource)))(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                                            size_t limitInBytes);
typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(UseDecoder)))(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                             Zibra::CE::ZibraVDB::FileDecoder* decoder);
typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(UseRHI)))(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                         Zibra::RHI::CAPI::ConsumerBridge::RHIRuntimeVTable vt);
typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(Create)))(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                         ZCE_NS::CAPI::DecompressorHandle* outInstanceHnd);
typedef void (*ZCE_PFN(ZCE_FNPFX(Release)))(ZCE_NS::CAPI::DecompressorFactoryHandle instance);

#ifdef ZRHI_STATIC_API_DECL
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(SetMemoryLimitPerResource)(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                                       size_t limitInBytes) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(UseDecoder)(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                        Zibra::CE::ZibraVDB::FileDecoder* decoder) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(UseRHI)(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                    Zibra::RHI::CAPI::ConsumerBridge::RHIRuntimeVTable vt) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(Create)(ZCE_NS::CAPI::DecompressorFactoryHandle instance,
                                                    ZCE_NS::CAPI::DecompressorHandle* outInstanceHnd) noexcept;
ZCE_API_IMPORT void ZCE_FNPFX(Release)(ZCE_NS::CAPI::DecompressorFactoryHandle instance) noexcept;
#else
extern ZCE_PFN(ZCE_FNPFX(SetMemoryLimitPerResource)) ZCE_FNPFX(SetMemoryLimitPerResource);
extern ZCE_PFN(ZCE_FNPFX(UseDecoder)) ZCE_FNPFX(UseDecoder);
extern ZCE_PFN(ZCE_FNPFX(UseRHI)) ZCE_FNPFX(UseRHI);
extern ZCE_PFN(ZCE_FNPFX(Create)) ZCE_FNPFX(Create);
extern ZCE_PFN(ZCE_FNPFX(Release)) ZCE_FNPFX(Release);
#endif

#ifndef ZCE_NO_CAPI_IMPL
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
            return ZCE_API_CALL(ZCE_FNPFX(SetMemoryLimitPerResource), m_NativeInstance, limitInBytes);
        }

        ReturnCode UseDecoder(ZibraVDB::FileDecoder* decoder) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(UseDecoder), m_NativeInstance, decoder);
        }
        ReturnCode UseRHI(RHI::RHIRuntime* rhi) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(UseRHI), m_NativeInstance, RHI::CAPI::ConsumerBridge::VTConvert(rhi));
        }
        ReturnCode Create(Decompressor** outInstance) noexcept final
        {
            DecompressorHandle outHandle{};
            auto code = ZCE_API_CALL(ZCE_FNPFX(Create), m_NativeInstance, &outHandle);
            *outInstance = new DecompressorCAPI{outHandle};
            return code;
        }
        void Release() noexcept final
        {
            ZCE_API_CALL(ZCE_FNPFX(Release), m_NativeInstance);
            delete this;
        }

    private:
        DecompressorFactoryHandle m_NativeInstance;
    };
} // namespace ZCE_NS::CAPI
#endif

#undef ZCE_FNPFX
#pragma endregion DecompressorFactory

#pragma region Namespace Functions
#define ZCE_FNPFX(name) Zibra_CE_Decompression_##name

typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(CreateDecompressorFactory)))(ZCE_NS::CAPI::DecompressorFactoryHandle* outFactory);
typedef ZCE_NS::ReturnCode (*ZCE_PFN(ZCE_FNPFX(CreateDecoder)))(const char* filepath, Zibra::CE::ZibraVDB::FileDecoder** outInstance);
typedef uint64_t (*ZCE_PFN(ZCE_FNPFX(GetFileFormatVersion)))(Zibra::CE::ZibraVDB::FileDecoder* decoder);
typedef void (*ZCE_PFN(ZCE_FNPFX(ReleaseDecoder)))(Zibra::CE::ZibraVDB::FileDecoder* decoder);

#ifdef ZRHI_STATIC_API_DECL
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(CreateDecompressorFactory)(ZCE_NS::CAPI::DecompressorFactoryHandle* outFactory) noexcept;
ZCE_API_IMPORT ZCE_NS::ReturnCode ZCE_FNPFX(CreateDecoder)(const char* filepath, Zibra::CE::ZibraVDB::FileDecoder** outInstance) noexcept;
ZCE_API_IMPORT uint64_t ZCE_FNPFX(GetFileFormatVersion)(Zibra::CE::ZibraVDB::FileDecoder* decoder) noexcept;
ZCE_API_IMPORT void ZCE_FNPFX(ReleaseDecoder)(Zibra::CE::ZibraVDB::FileDecoder* decoder) noexcept;
#else
extern ZCE_PFN(ZCE_FNPFX(CreateDecompressorFactory)) ZCE_FNPFX(CreateDecompressorFactory);
extern ZCE_PFN(ZCE_FNPFX(CreateDecoder)) ZCE_FNPFX(CreateDecoder);
extern ZCE_PFN(ZCE_FNPFX(GetFileFormatVersion)) ZCE_FNPFX(GetFileFormatVersion);
extern ZCE_PFN(ZCE_FNPFX(ReleaseDecoder)) ZCE_FNPFX(ReleaseDecoder);
#endif

#ifndef ZCE_NO_CAPI_IMPL
namespace ZCE_NS::CAPI
{
    inline ReturnCode CreateDecompressorFactory(DecompressorFactory** outFactory) noexcept
    {
        DecompressorFactoryHandle handle{};
        auto code = ZCE_API_CALL(ZCE_FNPFX(CreateDecompressorFactory), &handle);
        *outFactory = new DecompressorFactoryCAPI{handle};
        return code;
    }

    inline ReturnCode CreateDecoder(const char* filepath, ZibraVDB::FileDecoder** outInstance) noexcept
    {
        return ZCE_API_CALL(ZCE_FNPFX(CreateDecoder), filepath, outInstance);
    }
    inline uint64_t GetFileFormatVersion(ZibraVDB::FileDecoder* decoder) noexcept
    {
        return ZCE_API_CALL(ZCE_FNPFX(GetFileFormatVersion), decoder);
    }
    inline void ReleaseDecoder(ZibraVDB::FileDecoder* decoder) noexcept
    {
        ZCE_API_CALL(ZCE_FNPFX(ReleaseDecoder), decoder);
    }
} // namespace ZCE_NS::CAPI
#endif

#undef ZCE_FNPFX
#pragma endregion Namespace Functions

#pragma region ConsumerBridge
#ifndef ZCE_NO_CAPI_IMPL
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
        ReturnCode (*DecompressFrame)(void*, CompressedFrameContainerHandle frame);
    };
} // namespace ZCE_NS::CAPI::ConsumerBridge
#endif

#pragma endregion ConsumerBridge

#undef ZCE_NS
#undef ZCE_PFN
#undef ZCE_CONCAT_HELPER
#undef ZCE_API_IMPORT
#pragma endregion CAPI
