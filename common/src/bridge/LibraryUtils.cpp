#include "PrecompiledHeader.h"

#include "bridge/LibraryUtils.h"

#include "licensing/LicenseManager.h"
#include "utils/Helpers.h"

// clang-format off

#define ZSDK_RUNTIME_FUNCTION_LIST_APPLY(macro) \
    ZRHI_API_APPLY(macro) \
    ZCE_COMPRESSOR_API_APPLY(macro) \
    ZCE_DECOMPRESSION_API_APPLY(macro) \
    ZCE_LICENSING_API_APPLY(macro)

// clang-format on

#define ZSDK_CONCAT_HELPER(A, B) A##B
#define ZSDK_PFN(name) ZSDK_CONCAT_HELPER(PFN_, name)
#define ZSDK_DEFINE_FUNCTION_POINTER(name) ZSDK_PFN(name) name = nullptr;
ZSDK_RUNTIME_FUNCTION_LIST_APPLY(ZSDK_DEFINE_FUNCTION_POINTER)
#undef ZSDK_DEFINE_FUNCTION_POINTER
#undef ZSDK_PFN
#undef ZSDK_CONCAT_HELPER

namespace Zibra::LibraryUtils
{
#if ZIB_TARGET_OS_WIN
#define ZIB_TARGET_OS_NAME "WIN"
#define ZIB_DYNAMIC_LIB_NAME "ZibraVDBSDK.dll"
#elif ZIB_TARGET_OS_LINUX
#define ZIB_TARGET_OS_NAME "LINUX"
#define ZIB_DYNAMIC_LIB_NAME "libZibraVDBSDK.so"
#elif ZIB_TARGET_OS_MAC
#define ZIB_TARGET_OS_NAME "MAC"
// actual dynamic library loadable by dlopen is inside .bundle
#define ZIB_DYNAMIC_LIB_NAME "ZibraVDBSDK.bundle/Contents/MacOS/ZibraVDBSDK"
#else
#error Unsupported platform
#endif

    bool g_IsLibraryLoaded = false;
    Zibra::Version g_CompressionEngineVersion = {};

    bool ValidateLoadedVersion()
    {
        static_assert(CE::Compression::ZCE_COMPRESSION_VERSION.major == ZIB_COMPRESSION_ENGINE_MAJOR_VERSION);
        if (g_CompressionEngineVersion.major != CE::Compression::ZCE_COMPRESSION_VERSION.major)
        {
            return false;
        }
        if (g_CompressionEngineVersion.minor != CE::Compression::ZCE_COMPRESSION_VERSION.minor)
        {
            return g_CompressionEngineVersion.minor > CE::Compression::ZCE_COMPRESSION_VERSION.minor;
        }
        if (g_CompressionEngineVersion.patch != CE::Compression::ZCE_COMPRESSION_VERSION.patch)
        {
            return g_CompressionEngineVersion.patch > CE::Compression::ZCE_COMPRESSION_VERSION.patch;
        }
        if (g_CompressionEngineVersion.build != CE::Compression::ZCE_COMPRESSION_VERSION.build)
        {
            return g_CompressionEngineVersion.build > CE::Compression::ZCE_COMPRESSION_VERSION.build;
        }
        return true;
    }

#if ZIB_TARGET_OS_WIN
    HMODULE g_LibraryHandle = NULL;
#elif ZIB_TARGET_OS_LINUX || ZIB_TARGET_OS_MAC
    void* g_LibraryHandle = nullptr;
#else
#error Unsupported platform
#endif

    bool LoadFunctions() noexcept
    {
#if ZIB_TARGET_OS_WIN
#define ZIB_LOAD_FUNCTION_POINTER(functionName)                                                                             \
    functionName = reinterpret_cast<ZCE_PFN(functionName)>(::GetProcAddress(g_LibraryHandle, ZIB_STRINGIFY(functionName))); \
    if (functionName == nullptr)                                                                                            \
    {                                                                                                                       \
        return false;                                                                                                       \
    }
        ZSDK_RUNTIME_FUNCTION_LIST_APPLY(ZIB_LOAD_FUNCTION_POINTER)
#undef ZIB_LOAD_FUNCTION_POINTER
#elif ZIB_TARGET_OS_LINUX || ZIB_TARGET_OS_MAC
#define ZIB_LOAD_FUNCTION_POINTER(functionName)                                                                  \
    functionName = reinterpret_cast<ZCE_PFN(functionName)>(dlsym(g_LibraryHandle, ZIB_STRINGIFY(functionName))); \
    if (functionName == nullptr)                                                                                 \
    {                                                                                                            \
        return false;                                                                                            \
    }
        ZSDK_RUNTIME_FUNCTION_LIST_APPLY(ZIB_LOAD_FUNCTION_POINTER)
#undef ZIB_LOAD_FUNCTION_POINTER
#else
#error Unsupported platform
#endif
        return true;
    }

    bool LoadLibraryByPath(const std::string& libraryPath) noexcept
    {
#if ZIB_TARGET_OS_WIN
        static_assert(IsPlatformSupported());
        if (libraryPath == "")
        {
            return false;
        }

        char szPath[MAX_PATH];
        ::GetFullPathNameA(libraryPath.c_str(), MAX_PATH, szPath, NULL);
        g_LibraryHandle = ::LoadLibraryExA(szPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

        if (g_LibraryHandle == NULL)
        {
            return false;
        }

        if (!LoadFunctions())
        {
            ::FreeLibrary(g_LibraryHandle);
            g_LibraryHandle = NULL;
            return false;
        }

        g_CompressionEngineVersion = Zibra_CE_Compression_GetVersion();

        if (!ValidateLoadedVersion())
        {
            ::FreeLibrary(g_LibraryHandle);
            g_LibraryHandle = NULL;
            return false;
        }

        return true;
#elif ZIB_TARGET_OS_LINUX || ZIB_TARGET_OS_MAC
        static_assert(IsPlatformSupported());

        assert(g_LibraryHandle == nullptr);

        if (libraryPath == "")
        {
            return false;
        }

#if ZIB_TARGET_OS_MAC
        std::filesystem::path bundlePath = std::filesystem::path(libraryPath).parent_path().parent_path().parent_path();
        // Remove com.apple.quarantine attribute on macOS
        ::removexattr(bundlePath.c_str(), "com.apple.quarantine", 0);
#endif

        g_LibraryHandle = dlopen(libraryPath.c_str(), RTLD_LAZY);

        if (g_LibraryHandle == nullptr)
        {
            return false;
        }

        if (!LoadFunctions())
        {
            dlclose(g_LibraryHandle);
            g_LibraryHandle = nullptr;
            return false;
        }

        g_CompressionEngineVersion = Zibra_CE_Compression_GetVersion();

        if (!ValidateLoadedVersion())
        {
            dlclose(g_LibraryHandle);
            g_LibraryHandle = nullptr;
            return false;
        }

        g_IsLibraryLoaded = true;
        return true;
#else
        // TODO cross-platform support
        assert(0);
        return false;
#endif
    }

    bool TryLoadLibrary() noexcept
    {
        assert(g_IsLibraryLoaded == (g_LibraryHandle != NULL));

        if (g_IsLibraryLoaded)
        {
            return true;
        }

        std::optional<std::string> libraryDirectory = Helpers::GetNormalEnvironmentVariable("ZIBRAVDB_LIBRARY_VER_0_PATH");
        if (!libraryDirectory.has_value())
        {
            return false;
        }

        std::string libraryPath = libraryDirectory.value() + "/" ZIB_DYNAMIC_LIB_NAME;

        g_IsLibraryLoaded = LoadLibraryByPath(libraryPath);
        return g_IsLibraryLoaded;
    }

    bool IsLibraryLoaded() noexcept
    {
        return g_IsLibraryLoaded;
    }

    std::string GetSDKLibraryVersionString() noexcept
    {
        if (!g_IsLibraryLoaded)
        {
            return "";
        }
        return std::to_string(g_CompressionEngineVersion.major) + "." + std::to_string(g_CompressionEngineVersion.minor) + "." +
               std::to_string(g_CompressionEngineVersion.patch) + "." + std::to_string(g_CompressionEngineVersion.build);
    }

    Version ToLibraryUtilsVersion(const Zibra::Version& version) noexcept
    {
        return Version{version.major, version.minor, version.patch, version.build};
    }

    Version GetSDKLibraryVersion() noexcept
    {
        assert(g_IsLibraryLoaded);
        return ToLibraryUtilsVersion(g_CompressionEngineVersion);
    }

    std::string ErrorCodeToString(CE::ReturnCode errorCode)
    {
        switch (errorCode)
        {
        case Zibra::CE::ZCE_SUCCESS:
            assert(0);
            return "";
        case Zibra::CE::ZCE_ERROR:
            return "Unexpected error";
        case Zibra::CE::ZCE_FATAL_ERROR:
            return "Fatal error";
        case Zibra::CE::ZCE_ERROR_NOT_INITIALIZED:
            return "ZibraVDB SDK is not initialized";
        case Zibra::CE::ZCE_ERROR_ALREADY_INITIALIZED:
            return "ZibraVDB SDK is already initialized";
        case Zibra::CE::ZCE_ERROR_INVALID_USAGE:
            return "ZibraVDB SDK invalid call";
        case Zibra::CE::ZCE_ERROR_INVALID_ARGUMENTS:
            return "ZibraVDB SDK invalid arguments";
        case Zibra::CE::ZCE_ERROR_NOT_IMPLEMENTED:
            return "Not implemented";
        case Zibra::CE::ZCE_ERROR_NOT_SUPPORTED:
            return "Unsupported";
        case Zibra::CE::ZCE_ERROR_NOT_FOUND:
            return "Not found";
        case Zibra::CE::ZCE_ERROR_OUT_OF_CPU_MEMORY:
            return "Out of CPU memory";
        case Zibra::CE::ZCE_ERROR_OUT_OF_GPU_MEMORY:
            return "Out of GPU memory";
        case Zibra::CE::ZCE_ERROR_TIME_OUT:
            return "Time out";
        case Zibra::CE::ZCE_ERROR_INVALID_SOURCE:
            return "Invalid file";
        case Zibra::CE::ZCE_ERROR_INCOMPTIBLE_SOURCE:
            return "Incompatible file";
        case Zibra::CE::ZCE_ERROR_CORRUPTED_SOURCE:
            return "Corrupted file";
        case Zibra::CE::ZCE_ERROR_IO_ERROR:
            return "I/O error";
        case Zibra::CE::ZCE_ERROR_LICENSE_TIER_TOO_LOW:
            if (LicenseManager::GetInstance().GetLicenseStatus(LicenseManager::Product::Decompression) == LicenseManager::Status::OK)
            {
                return "Your license does not allow decompression of this effect.";
            }
            else
            {
                return "Decompression of this file requires active license.";
            }
        default:
            assert(0);
            return "Unknown error: " + std::to_string(errorCode);
        }
    }
} // namespace Zibra::LibraryUtils

#undef ZCE_CONCAT_HELPER
#undef ZRHI_CONCAT_HELPER
