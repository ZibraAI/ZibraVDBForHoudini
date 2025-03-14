#include "PrecompiledHeader.h"

#include "LibraryUtils.h"

#include <UT/UT_EnvControl.h>

#include "networking/NetworkRequest.h"
#include "utils/Helpers.h"

// clang-format off

#define ZSDK_RUNTIME_FUNCTION_LIST_APPLY(macro) \
    ZRHI_API_APPLY(macro) \
    ZCE_COMPRESSOR_API_APPLY(macro) \
    ZCE_DECOMPRESSION_API_APPLY(macro) \
    ZRHI_LICENSING_API_APPLY(macro) \

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
#if ZIB_PLATFORM_WIN
#define ZIB_PLATFORM_NAME "Windows"
#define ZIB_DYNAMIC_LIB_EXTENSION ".dll"
#define ZIB_DYNAMIC_LIB_PREFIX ""
#elif ZIB_PLATFORM_MAC
#define ZIB_PLATFORM_NAME "macOS"
#define ZIB_DYNAMIC_LIB_EXTENSION ".dylib"
#define ZIB_DYNAMIC_LIB_PREFIX "lib"
#elif ZIB_PLATFORM_LINUX
#define ZIB_PLATFORM_NAME "Linux"
#define ZIB_DYNAMIC_LIB_EXTENSION ".so"
#define ZIB_DYNAMIC_LIB_PREFIX "lib"
#else
#error Unuspported platform
#endif

    const char* g_LibraryPath = ZIB_LIBRARY_FOLDER "/" ZIB_DYNAMIC_LIB_PREFIX "ZibraVDBSDK" ZIB_DYNAMIC_LIB_EXTENSION;

    bool g_IsLibraryLoaded = false;
    bool g_IsLibraryInitialized = false;
    Zibra::CE::Version g_CompressionLibraryVersion = {};
    Zibra::CE::Version g_DecompressionLibraryVersion = {};
    Zibra::RHI::Version g_RHILibraryVersion = {};

    // Returns vector of paths that can be used to search for the library
    // First element is the path used for downloading the library
    // Other elements are alternative load paths for manual library installation
    std::vector<std::string> GetLibraryPaths()
    {
        std::vector<std::string> result;

        const std::pair<UT_StrControl, const char*> basePathEnvVars[] = {
            {ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR"},
            {ENV_HSITE, "HSITE"},
            {ENV_MAX_STR_CONTROLS, "HQROOT"},
        };

        for (const auto& [envVarEnum, envVarName] : basePathEnvVars)
        {
            const std::vector<std::string> baseDirs = Helpers::GetHoudiniEnvironmentVariable(envVarEnum, envVarName);
            for (const std::string& baseDir : baseDirs)
            {
                std::filesystem::path libraryPath = std::filesystem::path(baseDir) / g_LibraryPath;
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

#if ZIB_PLATFORM_WIN
    HMODULE g_LibraryHandle = NULL;
#elif ZIB_PLATFORM_LINUX
    void* g_LibraryHandle = nullptr;
#else
#error Unsupported platform
#endif

    bool LoadFunctions() noexcept
    {
#if ZIB_PLATFORM_WIN
#define ZIB_LOAD_FUNCTION_POINTER(functionName)                                                                          \
    functionName = reinterpret_cast<ZCE_PFN(functionName)>(::GetProcAddress(g_LibraryHandle, ZIB_STRINGIFY(functionName))); \
    if (functionName == nullptr)                                                                                         \
    {                                                                                                                    \
        return false;                                                                                                    \
    }
        ZSDK_RUNTIME_FUNCTION_LIST_APPLY(ZIB_LOAD_FUNCTION_POINTER)
#undef ZIB_LOAD_FUNCTION_POINTER
#elif ZIB_PLATFORM_LINUX
#define ZIB_LOAD_FUNCTION_POINTER(functionName)                                                    \
    functionName = reinterpret_cast<ZCE_PFN(functionName)>(dlsym(g_LibraryHandle, ZIB_STRINGIFY(functionName))); \
    if (functionName == nullptr)                                                                   \
    {                                                                                              \
        return false;                                                                              \
    }
        ZSDK_RUNTIME_FUNCTION_LIST_APPLY(ZIB_LOAD_FUNCTION_POINTER)
#undef ZIB_LOAD_FUNCTION_POINTER
#else
#error Unuspported platform
#endif
        return true;
    }

    bool LoadLibraryByPath(const std::string& libraryPath) noexcept
    {
#if ZIB_PLATFORM_WIN
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

        g_CompressionLibraryVersion = Zibra_CE_Compression_GetVersion();
        g_DecompressionLibraryVersion = Zibra_CE_Decompression_GetVersion();
        g_RHILibraryVersion = Zibra_RHI_GetVersion();

        if (!ValidateLoadedVersion())
        {
            ::FreeLibrary(g_LibraryHandle);
            g_LibraryHandle = NULL;
            return false;
        }

        return true;
#elif ZIB_PLATFORM_LINUX 
        static_assert(IsPlatformSupported());

        assert(g_LibraryHandle == nullptr);

        if (libraryPath == "")
        {
            return false;
        }

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

        g_CompressionLibraryVersion = Zibra_CE_Compression_GetVersion();
        g_DecompressionLibraryVersion = Zibra_CE_Decompression_GetVersion();
        g_RHILibraryVersion = Zibra_RHI_GetVersion();

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
        std::string result= std::to_string(g_CompressionLibraryVersion.major) + "." + std::to_string(g_CompressionLibraryVersion.minor) + "." +
                  std::to_string(g_CompressionLibraryVersion.patch) + "." + std::to_string(g_CompressionLibraryVersion.build);
        result += " / ";
        result += std::to_string(g_DecompressionLibraryVersion.major) + "." + std::to_string(g_DecompressionLibraryVersion.minor) + "." +
                  std::to_string(g_DecompressionLibraryVersion.patch) + "." + std::to_string(g_DecompressionLibraryVersion.build);
        result += " / ";
        result += std::to_string(g_RHILibraryVersion.major) + "." + std::to_string(g_RHILibraryVersion.minor) + "." +
                  std::to_string(g_RHILibraryVersion.patch) + "." + std::to_string(g_RHILibraryVersion.build);
        return result;
    }

} // namespace Zibra::LibraryUtils

#undef ZCE_CONCAT_HELPER
#undef ZRHI_CONCAT_HELPER
