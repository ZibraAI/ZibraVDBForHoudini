#pragma once

#include "../PrecompiledHeader.h"

namespace Zibra::LibraryUtils {

    void LoadLibrary() noexcept;

    void DownloadLibrary() noexcept;

    bool IsLibraryLoaded() noexcept;

    bool IsPlatformSupported() noexcept;

} // namespace Zibra::LibraryUtils