#pragma once

#include <Zibra/CE/Common.h>
#include <Zibra/Math.h>
#include <Zibra/RHI.h>
#include <Zibra/Stream.h>
#include <cstdint>

#if defined(_MSC_VER)
#define ZCE_API_IMPORT extern "C" __declspec(dllimport)
#define ZCE_CALL_CONV __cdecl
#elif defined(__GNUC__)
#define ZCE_API_IMPORT extern "C"
#define ZCE_CALL_CONV
#else
#error "Unsupported compiler"
#endif

namespace Zibra::CE::Compression
{
    constexpr Version ZCE_COMPRESSION_VERSION = {1, 0, 0, 0};

    struct SparseFrame
    {
        FrameInfo info = {};
        size_t originalSize = 0;
        /// Spatial blocks info array
        const SpatialBlockInfo* spatialInfo = nullptr;
        /// ChannelBlocks array.
        const ChannelBlock* blocks = nullptr;
        const uint32_t* channelIndexPerBlock = nullptr;
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

    class FrameManager
    {
    protected:
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
        virtual ReturnCode FinishAndEncode(OStream* stream) noexcept = 0;
    };

    class Compressor
    {
    protected:
        virtual ~Compressor() noexcept = default;

    public:
        /**
         * Initializes compressor instance. Must be called before any other method.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode Initialize() noexcept = 0;
        /**
         * Compresses single frame
         * @param [in] desc frame compression description struct
         * @param [out] outFrame result FrameManager instance.
         * @warning While FrameManager is not finished it occupies RAM for whole compressed frame memory.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode CompressFrame(const CompressFrameDesc& desc, FrameManager** outFrame) noexcept = 0;
        /**
         * Destructs Compressor instance and releases it's memory.
         * After release pointer becomes invalid.
         */
        virtual void Release() noexcept = 0;
    };

    class SequenceMerger
    {
    protected:
        virtual ~SequenceMerger() noexcept = default;
    public:
        virtual ReturnCode AddSequence(IStream* stream, int32_t frameOffset) noexcept = 0;
        /**
         * Adds metadata item to global sequence metadata section.
         * @param [in] key metadata key
         * @param [in] value metadata value
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode AddMetadata(const char* key, const char* value) noexcept = 0;
        /**
         * Sets frame mapping (framerate) settings to use in spawned objects.
         * @param [in] info frame mapping desc.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode SetPlaybackInfo(const PlaybackInfo& info) noexcept = 0;
        virtual ReturnCode Finish(OStream* stream) noexcept = 0;
    };

    class CompressorFactory
    {
    protected:
        virtual ~CompressorFactory() noexcept = default;

    public:
        /**
         * Sets RHI instance to use in spawned objects.
         * @param [in] rhi RHI instance.
         * @return ZCE_SUCCESS in case of success, ZCE_ERROR_NOT_SUPPORTED if GFXAPI is not supported or error code otherwise.
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
         * Creates a compressor instance.
         * @param outInstance - out compressor instance.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual ReturnCode Create(Compressor** outInstance) noexcept = 0;
        /**
         * Destructs CompressorFactory instance and releases it's memory.
         * After release pointer becomes invalid.
         */
        virtual void Release() noexcept = 0;
    };

    typedef ReturnCode (ZCE_CALL_CONV *PFN_CreateCompressorFactory)(CompressorFactory** outInstance);
#ifdef ZCE_STATIC_LINKING
    ReturnCode CreateCompressorFactory(CompressorFactory** outInstance) noexcept;
#elif ZCE_DYNAMIC_IMPLICIT_LINKING
    ZCE_API_IMPORT ReturnCode ZCE_CALL_CONV Zibra_CE_Compression_CreateCompressorFactory(CompressorFactory** outInstance) noexcept;
#else
    constexpr const char* CreateCompressorFactoryExportName = "Zibra_CE_Compression_CreateCompressorFactory";
#endif

    typedef ReturnCode (ZCE_CALL_CONV *PFN_CreateSequenceMerger)(SequenceMerger** outInstance);
#ifdef ZCE_STATIC_LINKING
    ReturnCode CreateSequenceMerger(SequenceMerger** outInstance) noexcept;
#elif ZCE_DYNAMIC_IMPLICIT_LINKING
    ZCE_API_IMPORT ReturnCode ZCE_CALL_CONV Zibra_CE_Compression_CreateSequenceMerger(SequenceMerger** outInstance) noexcept;
#else
    constexpr const char* CreateSequenceMergerExportName = "Zibra_CE_Compression_CreateSequenceMerger";
#endif

    typedef Version (ZCE_CALL_CONV *PFN_GetVersion)();
#ifdef ZCE_STATIC_LINKING
    Version GetVersion() noexcept;
#elif ZCE_DYNAMIC_IMPLICIT_LINKING
    ZCE_API_IMPORT Version ZCE_CALL_CONV Zibra_CE_Compression_GetVersion() noexcept;
#else
    constexpr const char* GetVersionExportName = "Zibra_CE_Compression_GetVersion";
#endif
} // namespace Zibra::CE::Compression

#undef ZCE_API_IMPORT
#undef ZCE_CALL_CONV
