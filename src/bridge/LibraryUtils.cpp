#include "PrecompiledHeader.h"

#include "LibraryUtils.h"

#include <UT/UT_EnvControl.h>

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
    Zibra::Version g_CompressionLibraryVersion = {};
    Zibra::Version g_DecompressionLibraryVersion = {};
    Zibra::Version g_RHILibraryVersion = {};

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
        if (g_CompressionLibraryVersion.major != ZIB_COMPRESSION_MAJOR_VERSION)
        {
            return false;
        }
        if (g_DecompressionLibraryVersion.major != ZIB_DECOMPRESSION_MAJOR_VERSION)
        {
            return false;
        }
        if (g_RHILibraryVersion.major != ZIB_RHI_MAJOR_VERSION)
        {
            return false;
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

        g_CompressionLibraryVersion = CE::Compression::GetVersion();
        g_DecompressionLibraryVersion = CE::Decompression::GetVersion();
        g_RHILibraryVersion = RHI::GetVersion();

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

        g_CompressionLibraryVersion = CE::Compression::GetVersion();
        g_DecompressionLibraryVersion = CE::Decompression::GetVersion();
        g_RHILibraryVersion = RHI::GetVersion();

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
        std::string result = std::to_string(g_CompressionLibraryVersion.major) + "." + std::to_string(g_CompressionLibraryVersion.minor) +
                             "." + std::to_string(g_CompressionLibraryVersion.patch) + "." +
                             std::to_string(g_CompressionLibraryVersion.build);
        result += " / ";
        result += std::to_string(g_DecompressionLibraryVersion.major) + "." + std::to_string(g_DecompressionLibraryVersion.minor) + "." +
                  std::to_string(g_DecompressionLibraryVersion.patch) + "." + std::to_string(g_DecompressionLibraryVersion.build);
        result += " / ";
        result += std::to_string(g_RHILibraryVersion.major) + "." + std::to_string(g_RHILibraryVersion.minor) + "." +
                  std::to_string(g_RHILibraryVersion.patch) + "." + std::to_string(g_RHILibraryVersion.build);
        return result;
    }

    Version ToLibraryUtilsVersion(const Zibra::Version& version) noexcept
    {
        return Version{version.major, version.minor, version.patch, version.build};
    }

    Version GetLibraryVersion() noexcept
    {
        assert(g_IsLibraryLoaded);
        const auto& compression = g_CompressionLibraryVersion;
        const auto& decompression = g_DecompressionLibraryVersion;

        // Return newer out of compression and decompression library versions
        if (compression.major > decompression.major)
        {
            return ToLibraryUtilsVersion(compression);
        }
        else if (g_CompressionLibraryVersion.major < g_DecompressionLibraryVersion.major)
        {
            return ToLibraryUtilsVersion(decompression);
        }

        if (compression.minor > decompression.minor)
        {
            return ToLibraryUtilsVersion(compression);
        }
        else if (compression.minor < decompression.minor)
        {
            return ToLibraryUtilsVersion(decompression);
        }

        if (compression.patch > decompression.patch)
        {
            return ToLibraryUtilsVersion(compression);
        }
        else if (compression.patch < decompression.patch)
        {
            return ToLibraryUtilsVersion(decompression);
        }

        return compression.build > decompression.build ? ToLibraryUtilsVersion(compression) : ToLibraryUtilsVersion(decompression);
    }

} // namespace Zibra::LibraryUtils

