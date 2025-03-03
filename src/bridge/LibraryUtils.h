#pragma once

#include "../PrecompiledHeader.h"

namespace Zibra::LibraryUtils {

    void LoadLibrary() noexcept;
    void DownloadLibrary() noexcept;
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