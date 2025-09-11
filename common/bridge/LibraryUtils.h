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

    void LoadSDKLibrary() noexcept;
    bool IsSDKLibraryLoaded() noexcept;

    constexpr bool IsPlatformSupported() noexcept
    {
#if ZIB_TARGET_OS_WIN || ZIB_TARGET_OS_LINUX || ZIB_TARGET_OS_MAC
        return true;
#else
        #error Unexpected platform
        return false;
#endif
    }

    std::string GetZibSDKVersionString() noexcept;
    std::string ErrorCodeToString(CE::ReturnCode errorCode);
    Version GetZibSDKVersion() noexcept;

    std::vector<std::filesystem::path> GetZibraLibsBasePaths() noexcept;
    //TODO verify this
    std::vector<std::string> GetZibSDKPaths() noexcept;
} // namespace Zibra::LibraryUtils
