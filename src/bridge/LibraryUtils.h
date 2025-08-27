#pragma once

// Declaring used SDK API function pointers
namespace Zibra
{
    namespace RHI
    {
        extern PFN_CreateRHIFactory CreateRHIFactory;
        extern PFN_GetVersion GetVersion;
    }

    namespace CE
    {
        namespace Decompression
        {
            extern PFN_CreateFormatMapper CreateFormatMapper;
            extern PFN_GetVersion GetVersion;
        }
        namespace Compression
        {
            extern PFN_CreateCompressorFactory CreateCompressorFactory;
            extern PFN_CreateSequenceMerger CreateSequenceMerger;
            extern PFN_GetVersion GetVersion;
        }
        namespace Licensing
        {
            extern PFN_GetLicenseManager GetLicenseManager;
        }
    }
}

namespace Zibra::LibraryUtils {
#define ZIB_LIBRARY_FOLDER "zibra/" ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING

    struct Version
    {
        uint32_t major;
        uint32_t minor;
        uint32_t patch;
        uint32_t build;
    };

    void LoadLibrary() noexcept;
    bool IsLibraryLoaded() noexcept;

    constexpr bool IsPlatformSupported() noexcept
    {
#if ZIB_TARGET_OS_WIN || ZIB_TARGET_OS_LINUX || ZIB_TARGET_OS_MAC
        return true;
#else
        #error Unexpected platform
        return false;
#endif
    }

    std::string GetLibraryVersionString() noexcept;
    std::string ErrorCodeToString(Result errorCode);
    Version GetLibraryVersion() noexcept;

} // namespace Zibra::LibraryUtils
