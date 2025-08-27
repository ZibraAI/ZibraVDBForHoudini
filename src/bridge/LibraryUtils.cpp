#include "PrecompiledHeader.h"

#include "LibraryUtils.h"

#include <UT/UT_EnvControl.h>

#include "licensing/LicenseManager.h"
#include "utils/Helpers.h"

// Defining used SDK API function pointers and putting it back to appropriate namespaces.
namespace Zibra
{
    namespace RHI
    {
        PFN_CreateRHIFactory CreateRHIFactory = nullptr;
        PFN_GetVersion GetVersion = nullptr;
    }

    namespace CE
    {
        namespace Decompression
        {
            PFN_CreateFormatMapper CreateFormatMapper = nullptr;
            PFN_GetVersion GetVersion = nullptr;
        }
        namespace Compression
        {
            PFN_CreateCompressorFactory CreateCompressorFactory = nullptr;
            PFN_CreateSequenceMerger CreateSequenceMerger = nullptr;
            PFN_GetVersion GetVersion = nullptr;
        }
        namespace Licensing
        {
            PFN_GetLicenseManager GetLicenseManager = nullptr;
        }
    }
}

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

#define ZIB_LIBRARY_PATH ZIB_LIBRARY_FOLDER "/" ZIB_DYNAMIC_LIB_NAME

    bool g_IsLibraryLoaded = false;
    bool g_IsLibraryInitialized = false;
    Zibra::Version g_CompressionEngineVersion = {};

    // Returns vector of paths that can be used to search for the library
    // First element is the path used for downloading the library
    // Other elements are alternative load paths for manual library installation
    std::vector<std::string> GetLibraryPaths()
    {
        std::vector<std::string> result;

        const std::pair<UT_StrControl, const char*> basePathEnvVars[] = {
            // HSite and HQRoot have priority over HOUDINI_USER_PREF_DIR
            // So that same version of library shared between multiple computers
            {ENV_HSITE, "HSITE"},
            {ENV_MAX_STR_CONTROLS, "HQROOT"},
            {ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR"},
        };

        for (const auto& [envVarEnum, envVarName] : basePathEnvVars)
        {
            const std::vector<std::string> baseDirs = Helpers::GetHoudiniEnvironmentVariable(envVarEnum, envVarName);
            for (const std::string& baseDir : baseDirs)
            {
                std::filesystem::path libraryPath = std::filesystem::path(baseDir) / ZIB_LIBRARY_PATH;
                result.push_back(libraryPath.string());
            }
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
#define ZIB_LOAD_FUNCTION_POINTER(functionName, exportName)                                             \
    functionName = reinterpret_cast<PFN_##functionName>(::GetProcAddress(g_LibraryHandle, exportName)); \
    if (functionName == nullptr)                                                                        \
    {                                                                                                   \
        OutputDebugStringA("Failed to load API function:" #functionName);                               \
        return false;                                                                                   \
    }

#elif ZIB_TARGET_OS_LINUX || ZIB_TARGET_OS_MAC
#define ZIB_LOAD_FUNCTION_POINTER(functionName, exportName)                                  \
    functionName = reinterpret_cast<PFN_##functionName>(dlsym(g_LibraryHandle, exportName)); \
    if (functionName == nullptr)                                                             \
    {                                                                                        \
        return false;                                                                        \
    }
#else
#error Unsupported platform
#endif

        {
            using namespace Zibra::RHI;
            ZIB_LOAD_FUNCTION_POINTER(CreateRHIFactory, CreateRHIFactoryExportName);
            ZIB_LOAD_FUNCTION_POINTER(GetVersion, GetVersionExportName);
        }
        {
            using namespace Zibra::CE::Decompression;
            ZIB_LOAD_FUNCTION_POINTER(CreateFormatMapper, CreateFormatMapperExportName);
            ZIB_LOAD_FUNCTION_POINTER(GetVersion, GetVersionExportName);
        }
        {
            using namespace Zibra::CE::Compression;
            ZIB_LOAD_FUNCTION_POINTER(CreateCompressorFactory, CreateCompressorFactoryExportName);
            ZIB_LOAD_FUNCTION_POINTER(CreateSequenceMerger, CreateSequenceMergerExportName);
            ZIB_LOAD_FUNCTION_POINTER(GetVersion, GetVersionExportName);
        }
        {
            using namespace Zibra::CE::Licensing;
            ZIB_LOAD_FUNCTION_POINTER(GetLicenseManager, GetLicenseManagerExportName);
        }
#undef ZIB_LOAD_FUNCTION_POINTER
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

        g_CompressionEngineVersion = CE::Compression::GetVersion();

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

        g_CompressionEngineVersion = CE::Compression::GetVersion();

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

    void LoadLibrary() noexcept
    {
        assert(g_IsLibraryLoaded == (g_LibraryHandle != NULL));

        if (g_IsLibraryLoaded)
        {
            return;
        }

        const std::vector<std::string> libraryPaths = GetLibraryPaths();

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

    bool IsLibraryLoaded() noexcept
    {
        return g_IsLibraryLoaded;
    }

    std::string GetLibraryVersionString() noexcept
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

    Version GetLibraryVersion() noexcept
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

