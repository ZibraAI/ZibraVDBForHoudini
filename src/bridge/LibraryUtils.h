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

    void LoadLibrary() noexcept;
    bool IsLibraryLoaded() noexcept;

    constexpr bool IsPlatformSupported() noexcept
    {
#if ZIB_PLATFORM_WIN || ZIB_PLATFORM_LINUX
        return true;
#else
        return false;
#endif
    }

    std::string GetLibraryVersionString() noexcept;
    Version GetLibraryVersion() noexcept;

} // namespace Zibra::LibraryUtils