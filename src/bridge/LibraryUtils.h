#pragma once

#include "../PrecompiledHeader.h"

namespace Zibra::LibraryUtils {

#define ZIB_COMPRESSION_MAJOR_VERSION 0
#define ZIB_DECOMPRESSION_MAJOR_VERSION 0
#define ZIB_RHI_MAJOR_VERSION 2

#define ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING \
    ZIB_STRINGIFY(ZIB_COMPRESSION_MAJOR_VERSION) "_" ZIB_STRINGIFY(ZIB_DECOMPRESSION_MAJOR_VERSION) "_" ZIB_STRINGIFY(ZIB_RHI_MAJOR_VERSION)
#define ZIB_LIBRARY_FOLDER "zibra/" ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING

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

} // namespace Zibra::LibraryUtils