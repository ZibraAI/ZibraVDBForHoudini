#pragma once

namespace Zibra::LibraryUtils {
#define ZIB_LIBRARY_FOLDER "zibra/" ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING

    struct Version
    {
        uint32_t major;
        uint32_t minor;
        uint32_t patch;
        uint32_t build;
    };

    void LoadZibSDKLibrary() noexcept;
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
    Version GetLibraryVersion() noexcept;

    bool IsAssetResolverRegistered() noexcept;
    void RegisterAssetResolver() noexcept;

} // namespace Zibra::LibraryUtils
