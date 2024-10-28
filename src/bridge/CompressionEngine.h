#pragma once

namespace Zibra::CompressionEngine
{

#define ZIB_COMPRESSION_ENGINE_BRIDGE_MAJOR_VERSION 0
#define ZIB_COMPRESSION_ENGINE_BRIDGE_MINOR_VERSION 2

#define ZIB_BLOCK_SIZE_LOG_2 3
#define ZIB_BLOCK_SIZE (1 << ZIB_BLOCK_SIZE_LOG_2)
#define ZIB_BLOCK_ELEMENT_COUNT ZIB_BLOCK_SIZE* ZIB_BLOCK_SIZE* ZIB_BLOCK_SIZE

    enum class ZCE_Result : uint32_t
    {
        SUCCESS = 0,

        ERROR = 100,
        FATAL_ERROR = 110,

        NOT_INITIALIZED = 200,
        ALREADY_INITIALIZED = 201,

        INVALID_ARGUMENTS = 300,
        INSTANCE_NOT_FOUND = 301,

        IO_FAILED_TO_OPEN_FILE = 400,
        FAILED_TO_PARSE_FILE = 401,

        LICENSE_IS_NOT_VERIFIED = 1000,
    };

    enum class ZCE_Product : uint32_t
    {
        Compression = 0,
        Render = 1,
        Count = 2
    };

    struct ZCE_CompressionSettingsPerChannel
    {
        const char* channelName;
        float quality;
    };

    struct ZCE_CompressionSettings
    {
        const char* outputFilePath;
        float quality;
        int perChannelSettingsCount;
        ZCE_CompressionSettingsPerChannel* perChannelSettings;
    };

    struct ZCE_MetadataEntry
    {
        const char* key;
        const char* value;
    };

    struct ZCE_Transform
    {
        float raw[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    };

    struct ZCE_AABB
    {
        int32_t minX;
        int32_t minY;
        int32_t minZ;
        int32_t maxX;
        int32_t maxY;
        int32_t maxZ;
    };

    struct ZCE_SpatialBlock
    {
        int32_t coords[3] = {0, 0, 0};
        uint32_t channelBlocksOffset = 0;
        uint32_t channelMask = 0x0;
        uint32_t channelCount = 0;
    };

    struct ZCE_ChannelBlock
    {
        float voxels[ZIB_BLOCK_ELEMENT_COUNT] = {};
    };

    struct ZCE_GridStatistics
    {
        float minValue = 0.f;
        float maxValue = 0.f;
        float meanPositiveValue = 0.f;
        float meanNegativeValue = 0.f;
        uint32_t voxelCount = 0;
    };

    struct ZCE_PerChannelGridInfo
    {
        ZCE_Transform gridTransform = {};
        ZCE_GridStatistics statistics = {};
    };

    struct ZCE_SparseFrameData
    {
        ZCE_AABB boundingBox;
        uint64_t originalSize;
        ZCE_SpatialBlock* spatialBlocks;
        ZCE_ChannelBlock* channelBlocks;
        int32_t* channelIndexPerBlock;
        char** channelNames;
        uint32_t spatialBlockCount;
        uint32_t channelBlockCount;
        uint32_t channelCount;
        ZCE_PerChannelGridInfo* perChannelGridInfo;
    };

    struct ZCE_FrameContainer
    {
        ZCE_SparseFrameData frameData;
        const ZCE_MetadataEntry* metadata;
        uint32_t metadataCount;
    };

    struct ZCE_FramePerChannelInfo
    {
        ZCE_Transform gridTransform;
        float minGridValue;
        float maxGridValue;
    };
    struct ZCE_FrameInfo
    {
        size_t sparseBlockSize;
        size_t channelCount;
        char** channelNames;
        ZCE_FramePerChannelInfo* perChannelInfo;
        uint32_t AABBSize[3];
        uint32_t spatialBlockCount;
        uint32_t channelBlockCount;
    };

    struct ZCE_DecompressedFrameData
    {
        ZCE_SpatialBlock* spatialBlocks;
        ZCE_ChannelBlock* channelBlocks;
    };

    struct ZCE_DecompressedFrameContainer
    {
        ZCE_FrameInfo frameInfo;
        ZCE_DecompressedFrameData frameData;
        ZCE_MetadataEntry* metadata;
        uint32_t metadataCount;
    };

    struct ZCE_SequenceInfo
    {
        int32_t frameRangeBegin;
        int32_t frameRangeEnd;
    };

    struct ZCE_VersionNumber
    {
        uint32_t major;
        uint32_t minor;
        uint32_t patch;
        uint32_t build;
    };

    constexpr bool IsPlatformSupported() noexcept
    {
#if ZIB_PLATFORM_WIN
        return true;
#else
        return false;
#endif
    }

    // Loads library from disk if it is present.
    // After successfull load, IsLibraryLoaded() will return true.
    void LoadLibrary() noexcept;
    // Downloads library from the internet and loads it.
    void DownloadLibrary() noexcept;

    // Returns true if library is loaded, false otherwise.
    bool IsLibraryLoaded() noexcept;
    // Checks if license is valid for the given product.
    // Licenes validation happens in during library load
    // To re-validate the license you need to restart Houdini
    bool IsLicenseValid(ZCE_Product product) noexcept;
    // Return version number of the loaded library
    // Major and Minor versions will always match
    // ZIB_COMPRESSION_ENGINE_BRIDGE_MAJOR_VERSION/ZIB_COMPRESSION_ENGINE_BRIDGE_MINOR_VERSION
    // Patch and Build versions may differ
    // You should only call this if library is loaded
    ZCE_VersionNumber GetLoadedLibraryVersion() noexcept;
    // Returns same data as GetLoadedLibraryVersion() but in string format
    // Returns empty string when library is not loaded
    std::string GetLoadedLibraryVersionString() noexcept;

    [[nodiscard]] uint32_t CreateCompressorInstance(const ZCE_CompressionSettings* settings) noexcept;
    void ReleaseCompressorInstance(uint32_t instanceID) noexcept;

    void StartSequence(uint32_t instanceID) noexcept;
    void CompressFrame(uint32_t instanceID, ZCE_FrameContainer* frameData) noexcept;
    void FinishSequence(uint32_t instanceID) noexcept;
    void AbortSequence(uint32_t instanceID) noexcept;

    [[nodiscard]] uint32_t CreateDecompressorInstance() noexcept;
    bool SetInputFile(uint32_t instanceID, const char* inputFilePath) noexcept;
    void GetSequenceInfo(uint32_t instanceID, ZCE_SequenceInfo* sequenceInfo) noexcept;
    void DecompressFrame(uint32_t instanceID, uint32_t frameIndex, ZCE_DecompressedFrameContainer** frameContainer) noexcept;
    void FreeFrameData(ZCE_DecompressedFrameContainer* frameContainer) noexcept;
    void ReleaseDecompressorInstance(uint32_t instanceID) noexcept;
} // namespace Zibra::CompressionEngine
