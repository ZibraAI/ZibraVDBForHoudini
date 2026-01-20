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

    [[nodiscard]] bool TryLoadLibrary() noexcept;
    [[nodiscard]] bool IsLibraryLoaded() noexcept;

    constexpr bool IsPlatformSupported() noexcept
    {
#if ZIB_TARGET_OS_WIN || ZIB_TARGET_OS_LINUX || ZIB_TARGET_OS_MAC
        return true;
#else
        #error Unexpected platform
        return false;
#endif
    }

    Version GetSDKLibraryVersion() noexcept;
    std::string GetSDKLibraryVersionString() noexcept;

    std::string ErrorCodeToString(CE::ReturnCode errorCode);
} // namespace Zibra::LibraryUtils
