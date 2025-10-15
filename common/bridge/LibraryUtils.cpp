#include "PrecompiledHeader.h"

#include "LibraryUtils.h"

#if !LABS_BUILD
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/type.h>
#endif

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
#define ZIB_TARGET_OS_NAME "Windows"
#define ZIB_DYNAMIC_LIB_NAME "ZibraVDBSDK.dll"
#elif ZIB_TARGET_OS_LINUX
#define ZIB_TARGET_OS_NAME "Linux"
#define ZIB_DYNAMIC_LIB_NAME "libZibraVDBSDK.so"
#elif ZIB_TARGET_OS_MAC
#define ZIB_TARGET_OS_NAME "macOS"
// actual dynamic library loadable by dlopen is inside .bundle
#define ZIB_DYNAMIC_LIB_NAME "ZibraVDBSDK.bundle/Contents/MacOS/ZibraVDBSDK"
#else
#error Unsupported platform
#endif

    bool g_IsLibraryLoaded = false;
    bool g_IsLibraryInitialized = false;
    Zibra::Version g_CompressionEngineVersion = {};

    std::vector<std::filesystem::path> GetLibrariesBasePaths() noexcept
    {
        std::vector<std::filesystem::path> result{};
        const std::pair<UT_StrControl, const char*> basePathEnvVars[] = {
            // HSite and HQRoot have priority over HOUDINI_USER_PREF_DIR
            // So that same version of library shared between multiple computers
            {ENV_MAX_STR_CONTROLS, "HOUDINI_ZIBRAVDB_LIBRARY_DIR"},
            {ENV_HSITE, "HSITE"},
            {ENV_MAX_STR_CONTROLS, "HQROOT"},
            {ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR"},
        };
        for (const auto& [envVarEnum, envVarName] : basePathEnvVars)
        {
            const auto baseDirs = Helpers::GetHoudiniEnvironmentVariable(envVarEnum, envVarName);
            for (const std::string& baseDir : baseDirs)
            {
                result.emplace_back(baseDir);
            }
        }
        return result;
    }

    // Returns vector of paths that can be used to search for the library
    // First element is the path used for downloading the library
    // Other elements are alternative load paths for manual library installation
    std::vector<std::string> GetSDKLibraryPaths() noexcept
    {
        std::vector<std::string> result;
        for (const auto path : GetLibrariesBasePaths()) {
            auto newPath = path / ZIB_LIBRARY_FOLDER / ZIB_DYNAMIC_LIB_NAME;
            result.emplace_back(newPath.string());
        }

        assert(!result.empty());

        if (result.empty())
        {
            result.push_back("");
        }

        return result;
    }

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

    void LoadSDKLibrary() noexcept
    {
        assert(g_IsLibraryLoaded == (g_LibraryHandle != NULL));

        if (g_IsLibraryLoaded)
        {
            return;
        }

        const std::vector<std::string> libraryPaths = GetSDKLibraryPaths();

        bool isLoaded = false;
        for (const std::string& libraryPath : libraryPaths)
        {
            if (LoadLibraryByPath(libraryPath))
            {
                isLoaded = true;
                break;
            }
        }
        if (!isLoaded)
        {
            return;
        }

        g_IsLibraryLoaded = true;
    }

    bool IsSDKLibraryLoaded() noexcept
    {
        return g_IsLibraryLoaded;
    }

#if !LABS_BUILD
    bool IsAssetResolverRegistered()
    {
        PXR_NS::TfType resolverType = PXR_NS::TfType::FindByName("ZibraVDBResolver");
        return !resolverType.IsUnknown();
    }

    void RegisterAssetResolver() noexcept
    {
        if (IsAssetResolverRegistered())
        {
            return;
        }

#if ZIB_TARGET_OS_WIN
#define PLUG_INFO_FOLDER "ZibraVDBResolver_win"
#elif ZIB_TARGET_OS_LINUX
#define PLUG_INFO_FOLDER "ZibraVDBResolver_linux"
#elif ZIB_TARGET_OS_MAC
#define PLUG_INFO_FOLDER "ZibraVDBResolver_mac"
#endif

        auto baseLibPaths = Zibra::LibraryUtils::GetLibrariesBasePaths();
        for (const auto& baseLibPath : baseLibPaths)
        {
            if (baseLibPath.empty() || !std::filesystem::exists(baseLibPath))
            {
                continue;
            }

            std::filesystem::path resourcesPath = baseLibPath / "dso"/ "usd_plugins" / PLUG_INFO_FOLDER / "resources";
            if (!std::filesystem::exists(resourcesPath) || !std::filesystem::is_directory(resourcesPath))
            {
                continue;
            }

            PXR_NS::PlugRegistry::GetInstance().RegisterPlugins(resourcesPath.string());
            if (IsAssetResolverRegistered())
            {
                break;
            }
        }

        if (!IsAssetResolverRegistered())
        {
            assert(false && "Failed to register ZibraVDBResolver. Make sure the library file is present.");
        }
    }
#endif

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
