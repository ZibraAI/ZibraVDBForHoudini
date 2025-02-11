#pragma once

#include <cstdint>
#include <Zibra/CE/Common.h>
#include <Zibra/CE/Math3D.h>
#include <Zibra/RHI.h>


namespace Zibra::CE::ZibraVDB
{
    class FileEncoder;
}

namespace Zibra::CE::Compression
{
    struct ChannelBlock
    {
        float voxels[SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE] = {};
    };

    struct SpatialBlockInfo
    {
        /// Block coords in block space (block dim size = SPARSE_BLOCK_SIZE)
        int32_t coords[3] = {0, 0, 0};
        /// Offset in blocks to channel group start (block that relates to first present channel in group) in SparseFrame::blocks.1
        uint32_t channelBlocksOffset = 0;
        /// Block present channels mask. Bits order must match SparseFrame::orderedChannels order.
        uint32_t channelMask = 0x0;
        /// Block present channels count.
        uint32_t channelCount = 0;
    };
    inline bool operator==(const SpatialBlockInfo& a, const SpatialBlockInfo& b) noexcept
    {
        const bool coords = a.coords[0] == b.coords[0] && a.coords[1] == b.coords[1] && a.coords[2] == b.coords[2];
        return coords && (a.channelBlocksOffset == b.channelBlocksOffset) && (a.channelMask == b.channelMask) &&
               (a.channelCount == b.channelCount);
    }
    /**
     * Packs 3 coords into 32bit
     * @param [in] coords uint3 coords
     * @return packed 32-bit value
     */
    inline uint32_t PackCoords(Math3D::uint3 coords) noexcept
    {
        return coords.x & 1023 | (coords.y & 1023) << 10 | (coords.z & 1023) << 20;
    }
    /**
     * Unpacks 32bit-packed by PackCoords coords
     * @param [in] packedCoords packed 32-bit value
     * @return uint3 coords
     */
    inline Math3D::uint3 UnpackCoords(uint32_t packedCoords) noexcept
    {
        return {packedCoords & 1023, (packedCoords >> 10) & 1023, (packedCoords >> 20) & 1023};
    }

    struct VoxelStatistics
    {
        /// min voxel value for entire channel gird.
        float minValue = 0.f;
        /// max voxel value for entire channel grid.
        float maxValue = 0.f;
        float meanPositiveValue = 0.f;
        float meanNegativeValue = 0.f;
        /// total voxels count per channel grid.
        uint32_t voxelCount = 0;
    };

    struct ChannelInfo
    {
        /// Channel unique name
        const char* name = nullptr;
        Math3D::Transform gridTransform = {};
        VoxelStatistics statistics = {};
    };

    struct SparseFrame
    {
        /// Axis aligned bounding box.
        Math3D::AABB aabb = {};
        size_t spatialInfoCount = 0;
        /// ChannelBlocks array lookup info per spatial block.
        const SpatialBlockInfo* spatialInfo = nullptr;
        /// ChannelBlocks array count.
        size_t blocksCount = 0;
        /// ChannelBlocks array.
        const ChannelBlock* blocks = nullptr;
        const uint32_t* channelIndexPerBlock = nullptr;
        /// Ordered channels count.
        size_t orderedChannelsCount = 0;
        /// Channel infos ordered to match bits order in SpatialBlockInfo::channelMask.
        const ChannelInfo* orderedChannels = nullptr;
        /// Original file size in bytes.
        size_t originalSize = 0;
    };

    struct CompressFrameDesc
    {
        /// Sparse frame representation.
        SparseFrame* frame = nullptr;
        /// Channels names to compress count.
        size_t channelsCount = 0;
        /// Channels names to compress. Will be selected from all present frame channels. Non-present channel names would be skipped.
        const char* const* channels = nullptr;
    };

    struct FrameMappingDecs
    {
        uint32_t framerateNumerator = 30;
        uint32_t framerateDenominator = 1;
        int32_t sequenceStartIndex = 0;
        uint32_t sequenceIndexIncrement = 1;
    };

    class FrameManager
    {
    public:
        virtual ~FrameManager() noexcept = default;

    public:
        /**
         * Adds metadata item to frame metadata section.
         * @param [in] key metadata key
         * @param [in] value metadata value
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode AddMetadata(const char* key, const char* value) noexcept = 0;
        /**
         * Finishes frame encoding, adds encoded frame to encoded sequence and releases FrameManager memory.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode Finish() noexcept = 0;
    };

    class Compressor
    {
    public:
        virtual ~Compressor() noexcept = default;

    public:
        /**
         * Initializes compressor instance. Must be called before any other method.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode Initialize() noexcept = 0;
        /**
         * Destructs Compressor instance and releases it's memory.
         * After release pointer becomes invalid.
         */
        virtual void Release() noexcept = 0;

    public:
        /**
         * Starts sequence compression session.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode StartSequence() noexcept = 0;
        /**
         * Adds metadata item to global sequence metadata section.
         * @param [in] key metadata key
         * @param [in] value metadata value
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode AddPerSequenceMetadata(const char* key, const char* value) noexcept = 0;
        /**
         * Finishes sequence compression session.
         * @note Sequence session must be in progress. FrameManagers must be finished in their spawn order.
         * @param [in] desc frame compression description struct
         * @param [out] outFrame result FrameManager instance.
         * @warning While FrameManager is not finished it occupies RAM for whole compressed frame memory.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode CompressFrame(const CompressFrameDesc& desc, FrameManager** outFrame) noexcept = 0;
        /**
         * Finishes sequence compression session.
         * @param [in] outStream - OStream instance for result zibravdb output.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode FinishSequence(OStream* outStream) noexcept = 0;
    };

    class CompressorFactory
    {
    public:
        virtual ~CompressorFactory() noexcept = default;

    public:
        /**
         * Sets RHI instance to use in spawned objects.
         * @param [in] rhi RHI instance.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode UseRHI(RHI::RHIRuntime* rhi) noexcept = 0;
        /**
         * Sets default channel compression quality to use in spawned objects.
         * @param [in] quality Target quality in range [0.0f : 1.0f].
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode SetQuality(float quality) noexcept = 0;
        /**
         * Sets quality override for selected channel to use in spawned objects.
         * @param [in] channelName Target override channel name.
         * @param [in] quality - Target quality for selected channel in range [0.0f : 1.0f].
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode OverrideChannelQuality(const char* channelName, float quality) noexcept = 0;
        /**
         * Sets frame mapping (framerate) settings to use in spawned objects.
         * @param [in] desc frame mapping desc.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode SetFrameMapping(const FrameMappingDecs& desc) noexcept = 0;
        /**
         *
         * @param [out] outInstance - result compressor instance.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode Create(Compressor** outInstance) noexcept = 0;
        /**
         * Destructs CompressorFactory instance and releases it's memory.
         * After release pointer becomes invalid.
         */
        virtual void Release() noexcept = 0;
    };

    ReturnCode CreateCompressorFactory(CompressorFactory** outInstance) noexcept;
} // namespace Zibra::CE::Compression

#pragma region CAPI
#define ZCE_API_IMPORT __declspec(dllimport)
#define ZCE_CONCAT_HELPER(A, B) A##B
#define ZCE_PFN(name) ZCE_CONCAT_HELPER(PFN_, name)
#define ZCE_NS Zibra::CE::Compression

#if !defined(ZCE_NO_CAPI_IMPL) && !defined(ZCE_NO_STATIC_API_DECL) && !defined(ZCE_API_CALL)
#define ZCE_API_CALL(func, ...) func(__VA_ARGS__)
#elif !defined(ZCE_NO_CAPI_IMPL) && defined(ZCE_NO_STATIC_API_DECL) && !defined(ZCE_API_CALL)
#define ZCE_API_CALL(func, ...) func(__VA_ARGS__)
#else
#error "You must define ZCE_API_CALL(func, ...) macro before using CAPI without static function declaration."
#endif

namespace ZCE_NS::CAPI
{
    using FileEncoderHandle = void*;
    using FrameManagerHandle = void*;
    using CompressorHandle = void*;
    using CompressorFactoryHandle = void*;
} // namespace ZCE_NS::CAPI

#pragma region FrameManager
#define ZCE_FNPFX(name) Zibra_CE_Compression_FrameManager_##name

#define ZCE_APPLY_COMPRESSION_FRAME_MANAGER_API_FUCNS(apply) \
    apply(AddMetadata)                                       \
    apply(Finish)

typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(AddMetadata)))(ZCE_NS::CAPI::FrameManagerHandle instance, const char* key, const char* value);
typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(Finish)))(ZCE_NS::CAPI::FrameManagerHandle instance);

#ifdef ZRHI_STATIC_API_DECL
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(AddMetadata)(ZCE_NS::CAPI::FrameManagerHandle instance, const char* key, const char* value) noexcept;
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(Finish)(ZCE_NS::CAPI::FrameManagerHandle instance) noexcept;
#else
extern ZCE_PFN(ZCE_FNPFX(AddMetadata)) ZCE_FNPFX(AddMetadata);
extern ZCE_PFN(ZCE_FNPFX(Finish)) ZCE_FNPFX(Finish);
#endif

#ifndef ZCE_NO_CAPI_IMPL
namespace ZCE_NS::CAPI
{
    class FrameManagerCAPI final : public FrameManager
    {
    public:
        explicit FrameManagerCAPI(FrameManagerHandle handle) noexcept
            : m_NativeInstance(handle)
        {
        }

    public:
        ReturnCode AddMetadata(const char* key, const char* value) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(AddMetadata), m_NativeInstance, key, value);
        }
        ReturnCode Finish() noexcept final
        {
            auto status = ZCE_API_CALL(ZCE_FNPFX(Finish), m_NativeInstance);
            delete this;
            return status;
        }

    private:
        FrameManagerHandle m_NativeInstance;
    };
} // namespace ZCE_NS::CAPI
#endif

#undef ZCE_FNPFX
#pragma endregion FrameManager

#pragma region Compressor
#define ZCE_FNPFX(name) Zibra_CE_Compression_Compressor_##name

#define ZCE_APPLY_COMPRESSION_COMPRESSOR_API_FUCNS(apply) \
    apply(Initialize)                                     \
    apply(Release)                                        \
    apply(StartSequence)                                  \
    apply(AddPerSequenceMetadata)                         \
    apply(CompressFrame)                                  \
    apply(FinishSequence)

typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(Initialize)))(ZCE_NS::CAPI::CompressorHandle instance);
typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(Release)))(ZCE_NS::CAPI::CompressorHandle instance);
typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(StartSequence)))(ZCE_NS::CAPI::CompressorHandle instance);
typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(AddPerSequenceMetadata)))(ZCE_NS::CAPI::CompressorHandle instance, const char* key, const char* value);
typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(CompressFrame)))(ZCE_NS::CAPI::CompressorHandle instance, const ZCE_NS::CompressFrameDesc& desc, ZCE_NS::CAPI::FrameManagerHandle* outFrame);
typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(FinishSequence)))(ZCE_NS::CAPI::CompressorHandle instance, Zibra::CE::CAPI::OStreamVTable vt);

#ifdef ZRHI_STATIC_API_DECL
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(Initialize)(ZCE_NS::CAPI::CompressorHandle instance) noexcept;
ZCE_API_IMPORT void ZCE_FNPFX(Release)(ZCE_NS::CAPI::CompressorHandle instance) noexcept;
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(StartSequence)(ZCE_NS::CAPI::CompressorHandle instance) noexcept;
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(AddPerSequenceMetadata)(ZCE_NS::CAPI::CompressorHandle instance, const char* key, const char* value) noexcept;
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(CompressFrame)(ZCE_NS::CAPI::CompressorHandle instance, const ZCE_NS::CompressFrameDesc& desc, ZCE_NS::CAPI::FrameManagerHandle* outFrame) noexcept;
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(FinishSequence)(ZCE_NS::CAPI::CompressorHandle instance, Zibra::CE::CAPI::OStreamVTable vt) noexcept;
#else
extern ZCE_PFN(ZCE_FNPFX(Initialize)) ZCE_FNPFX(Initialize);
extern ZCE_PFN(ZCE_FNPFX(Release)) ZCE_FNPFX(Release);
extern ZCE_PFN(ZCE_FNPFX(StartSequence)) ZCE_FNPFX(StartSequence);
extern ZCE_PFN(ZCE_FNPFX(AddPerSequenceMetadata)) ZCE_FNPFX(AddPerSequenceMetadata);
extern ZCE_PFN(ZCE_FNPFX(CompressFrame)) ZCE_FNPFX(CompressFrame);
extern ZCE_PFN(ZCE_FNPFX(FinishSequence)) ZCE_FNPFX(FinishSequence);
#endif

#ifndef ZCE_NO_CAPI_IMPL
namespace ZCE_NS::CAPI
{
    class CompressorCAPI final : public Compressor
    {
    public:
        explicit CompressorCAPI(CompressorHandle handle) noexcept
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
        ReturnCode StartSequence() noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(StartSequence), m_NativeInstance);
        }
        ReturnCode AddPerSequenceMetadata(const char* key, const char* value) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(AddPerSequenceMetadata), m_NativeInstance, key, value);
        }
        ReturnCode CompressFrame(const CompressFrameDesc& desc, FrameManager** outFrame) noexcept final
        {
            FrameManagerHandle frameManagerHandle;
            auto status = ZCE_API_CALL(ZCE_FNPFX(CompressFrame), m_NativeInstance, desc, &frameManagerHandle);
            *outFrame = new FrameManagerCAPI{frameManagerHandle};
            return status;
        }
        ReturnCode FinishSequence(OStream* outStream) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(FinishSequence), m_NativeInstance, CE::CAPI::VTConvert(outStream));
        }

    private:
        CompressorHandle m_NativeInstance;
    };
}
#endif

#undef ZCE_FNPFX
#pragma endregion Compressor

#pragma region CompressorFactory
#define ZCE_FNPFX(name) Zibra_CE_Compression_CompressorFactory_##name

#define ZCE_APPLY_COMPRESSION_COMPRESSOR_FACTORY_API_FUCNS(apply) \
    apply(UseRHI)                                                 \
    apply(SetQuality)                                             \
    apply(OverrideChannelQuality)                                 \
    apply(SetFrameMapping)                                        \
    apply(Create)                                                 \
    apply(Release)


typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(UseRHI)))(ZCE_NS::CAPI::CompressorFactoryHandle instance, Zibra::RHI::CAPI::ConsumerBridge::RHIRuntimeVTable vt);
typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(SetQuality)))(ZCE_NS::CAPI::CompressorFactoryHandle instance, float quality);
typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(OverrideChannelQuality)))(ZCE_NS::CAPI::CompressorFactoryHandle instance, const char* channelName, float quality);
typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(SetFrameMapping)))(ZCE_NS::CAPI::CompressorFactoryHandle instance, const ZCE_NS::FrameMappingDecs& desc);
typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(Create)))(ZCE_NS::CAPI::CompressorFactoryHandle instance, ZCE_NS::CAPI::CompressorHandle* outInstance);
typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(Release)))(ZCE_NS::CAPI::CompressorFactoryHandle instance);

#ifdef ZRHI_STATIC_API_DECL
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(UseRHI)(ZCE_NS::CAPI::CompressorFactoryHandle instance, Zibra::RHI::CAPI::ConsumerBridge::RHIRuntimeVTable vt) noexcept;
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(SetQuality)(ZCE_NS::CAPI::CompressorFactoryHandle instance, float quality) noexcept;
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(OverrideChannelQuality)(ZCE_NS::CAPI::CompressorFactoryHandle instance, const char* channelName, float quality) noexcept;
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(SetFrameMapping)(ZCE_NS::CAPI::CompressorFactoryHandle instance, const ZCE_NS::FrameMappingDecs& desc) noexcept;
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(Create)(ZCE_NS::CAPI::CompressorFactoryHandle instance, ZCE_NS::CAPI::CompressorHandle* outInstance) noexcept;
ZCE_API_IMPORT void ZCE_FNPFX(Release)(ZCE_NS::CAPI::CompressorFactoryHandle instance) noexcept;
#else
extern ZCE_PFN(ZCE_FNPFX(UseRHI)) ZCE_FNPFX(UseRHI);
extern ZCE_PFN(ZCE_FNPFX(SetQuality)) ZCE_FNPFX(SetQuality);
extern ZCE_PFN(ZCE_FNPFX(OverrideChannelQuality)) ZCE_FNPFX(OverrideChannelQuality);
extern ZCE_PFN(ZCE_FNPFX(SetFrameMapping)) ZCE_FNPFX(SetFrameMapping);
extern ZCE_PFN(ZCE_FNPFX(Create)) ZCE_FNPFX(Create);
extern ZCE_PFN(ZCE_FNPFX(Release)) ZCE_FNPFX(Release);
#endif

#ifndef ZCE_NO_CAPI_IMPL
namespace ZCE_NS::CAPI
{
    class CompressorFactoryCAPI final : public CompressorFactory
    {
    public:
        explicit CompressorFactoryCAPI(CompressorFactoryHandle handle) noexcept
            : m_NativeInstance(handle)
        {
        }

    public:
        ReturnCode UseRHI(RHI::RHIRuntime* rhi) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(UseRHI), m_NativeInstance, RHI::CAPI::ConsumerBridge::VTConvert(rhi));
        }
        ReturnCode SetQuality(float quality) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(SetQuality), m_NativeInstance, quality);
        }
        ReturnCode OverrideChannelQuality(const char* channelName, float quality) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(OverrideChannelQuality), m_NativeInstance, channelName, quality);
        }
        ReturnCode SetFrameMapping(const FrameMappingDecs& desc) noexcept final
        {
            return ZCE_API_CALL(ZCE_FNPFX(SetFrameMapping), m_NativeInstance, desc);
        }
        ReturnCode Create(Compressor** outInstance) noexcept final
        {
            CompressorHandle handle;
            auto status = ZCE_API_CALL(ZCE_FNPFX(Create), m_NativeInstance, &handle);
            *outInstance = new CompressorCAPI{handle};
            return status;
        }
        void Release() noexcept final
        {
            ZCE_API_CALL(ZCE_FNPFX(Release), m_NativeInstance);
            delete this;
        }

    private:
        CompressorFactoryHandle m_NativeInstance;
    };
}
#endif

#undef ZCE_FNPFX
#pragma endregion CompressorFactory

#pragma region Fucns
#define ZCE_FNPFX(name) Zibra_CE_Compression_##name

#define ZCE_APPLY_COMPRESSION_FUNCS_API_FUCNS(apply) \
    apply(CreateCompressorFactory)                   \

typedef Zibra::CE::ReturnCode (*ZCE_PFN(ZCE_FNPFX(CreateCompressorFactory)))(ZCE_NS::CAPI::CompressorFactoryHandle* outInstance);



#ifdef ZRHI_STATIC_API_DECL
ZCE_API_IMPORT Zibra::CE::ReturnCode ZCE_FNPFX(CreateCompressorFactory)(ZCE_NS::CAPI::CompressorFactoryHandle* outInstance) noexcept;
#else
extern ZCE_PFN(ZCE_FNPFX(CreateCompressorFactory)) ZCE_FNPFX(CreateCompressorFactory);
#endif

#ifndef ZCE_NO_CAPI_IMPL
namespace ZCE_NS::CAPI
{
    inline CompressorFactory* CreateCompressorFactory() noexcept
    {
        CompressorFactoryHandle handle = nullptr;
        CE::ReturnCode result = ZCE_API_CALL(ZCE_FNPFX(CreateCompressorFactory), &handle);
        if (result != CE::ZCE_SUCCESS)
        {
            return nullptr;
        }
        return new CompressorFactoryCAPI{handle};
    }
}
#endif

#undef ZCE_FNPFX
#pragma endregion Fucns

#define ZCE_APPLY_COMPRESSION_ALL_API(apply)                  \
    ZCE_APPLY_COMPRESSION_FRAME_MANAGER_API_FUCNS(apply)      \
    ZCE_APPLY_COMPRESSION_COMPRESSOR_API_FUCNS(apply)         \
    ZCE_APPLY_COMPRESSION_COMPRESSOR_FACTORY_API_FUCNS(apply) \
    ZCE_APPLY_COMPRESSION_FUNCS_API_FUCNS(apply)

#undef ZCE_NS
#undef ZCE_PFN
#undef ZCE_CONCAT_HELPER
#undef ZCE_API_IMPORT
#pragma endregion CAPI