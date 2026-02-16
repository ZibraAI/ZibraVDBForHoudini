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
    constexpr Version COMPRESSOR_VERSION = {1, 0, 0, 0};

    struct SparseFrame
    {
        FrameInfo info = {};
        size_t originalSize = 0;
        /// Spatial blocks info array
        const SpatialBlock* spatialInfo = nullptr;
        /// ChannelBlocks array.
        const ChannelBlock* blocks = nullptr;
        const uint32_t* channelIndexPerBlock = nullptr;
    };

    struct ChannelSpecificQuality
    {
        /**
         * Name of the channel to set custom quality setting to
         * Must not be null
         */
        const char* channelName = nullptr;
        /**
         * Quality to use for the channel
         * Must be in range 0.0 - 1.0
         */
        float quality = -1.0f;
    };

    struct CompressorCreateDesc
    {
        /**
         * RHIRuntime object to use for compression
         * Must not be null
         * User is responsible keeping this object alive for whole lifetime of compressor
         * It is invalid to use any other RHIRuntime object with created comrpessor
         */
        RHI::RHIRuntime* RHI = nullptr;
        /**
         * Quality to use for compression
         * Must be in range 0.0 - 1.0
         */
        float quality = 0.6f;
        /**
         * Pointer to buffer containing ChannelSpecificQuality entries
         * Allows setting different quality for different channels
         * Must not contain duplicate channels
         * If channelSpecificQualityCount is not 0, this must not be null
         * If channelSpecificQualityCount is 0, this must be null
         */
        const ChannelSpecificQuality* channelSpecificQuality = nullptr;
        /**
         * Number of elements in channelSpecificQuality buffer
         */
        uint32_t channelSpecificQualityCount = 0;
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
        virtual Result AddMetadata(const char* key, const char* value) noexcept = 0;
        /**
         * Finishes frame encoding, adds encoded frame to encoded sequence and releases FrameManager memory.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual Result FinishAndEncode(OStream* stream) noexcept = 0;
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
        virtual Result Initialize() noexcept = 0;
        /**
         * Compresses single frame
         * @param [in] desc frame compression description struct
         * @param [out] outFrame result FrameManager instance.
         * @warning While FrameManager is not finished it occupies RAM for whole compressed frame memory.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual Result CompressFrame(const SparseFrame& desc, FrameManager** outFrame) noexcept = 0;
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
        virtual Result AddSequence(IStream* stream, int32_t frameOffset) noexcept = 0;
        /**
         * Adds metadata item to global sequence metadata section.
         * @param [in] key metadata key
         * @param [in] value metadata value
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual Result AddMetadata(const char* key, const char* value) noexcept = 0;
        /**
         * Sets frame mapping (framerate) settings to use in spawned objects.
         * @param [in] info frame mapping desc.
         * @return ZCE_SUCCESS in case of success or error code otherwise.
         */
        virtual Result SetPlaybackInfo(const PlaybackInfo& info) noexcept = 0;
        virtual Result Finish(OStream* stream) noexcept = 0;
        virtual void Release() noexcept = 0;
    };

    typedef Result(ZCE_CALL_CONV* PFN_CreateCompressor)(const CompressorCreateDesc* createDesc, Compressor** outInstance);
#ifdef ZCE_STATIC_LINKING
    Result CreateCompressor(const CompressorCreateDesc* createDesc, Compressor** outInstance) noexcept;
#elif defined(ZCE_DYNAMIC_IMPLICIT_LINKING)
    ZCE_API_IMPORT Result ZCE_CALL_CONV Zibra_CE_Compression_CreateCompressor(const CompressorCreateDesc* createDesc, Compressor** outInstance) noexcept;
#else
    constexpr const char* CreateCompressorExportName = "Zibra_CE_Compression_CreateCompressor";
#endif

    typedef Result(ZCE_CALL_CONV* PFN_CreateSequenceMerger)(SequenceMerger** outInstance);
#ifdef ZCE_STATIC_LINKING
    Result CreateSequenceMerger(SequenceMerger** outInstance) noexcept;
#elif defined(ZCE_DYNAMIC_IMPLICIT_LINKING)
    ZCE_API_IMPORT Result ZCE_CALL_CONV Zibra_CE_Compression_CreateSequenceMerger(SequenceMerger** outInstance) noexcept;
#else
    constexpr const char* CreateSequenceMergerExportName = "Zibra_CE_Compression_CreateSequenceMerger";
#endif

    typedef Version(ZCE_CALL_CONV* PFN_GetVersion)();
#ifdef ZCE_STATIC_LINKING
    Version GetVersion() noexcept;
#elif defined(ZCE_DYNAMIC_IMPLICIT_LINKING)
    ZCE_API_IMPORT Version ZCE_CALL_CONV Zibra_CE_Compression_GetVersion() noexcept;
#else
    constexpr const char* GetVersionExportName = "Zibra_CE_Compression_GetVersion";
#endif
} // namespace Zibra::CE::Compression

#undef ZCE_API_IMPORT
#undef ZCE_CALL_CONV
