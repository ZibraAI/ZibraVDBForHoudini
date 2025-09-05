#include "PrecompiledHeader.h"

#include "LibraryUtils.h"

#include <UT/UT_EnvControl.h>

#include "licensing/HoudiniLicenseManager.h"
#include "utils/Helpers.h"

// Defining used SDK API function pointers and putting it back to appropriate namespaces.
namespace Zibra
{
    namespace RHI
    {
        PFN_CreateRHIFactory CreateRHIFactory = nullptr;
        PFN_GetVersion GetVersion = nullptr;
    } // namespace RHI

    namespace CE
    {
        namespace Decompression
        {
            PFN_CreateFormatMapper CreateFormatMapper = nullptr;
            PFN_GetVersion GetVersion = nullptr;
        } // namespace Decompression
        namespace Compression
        {
            PFN_CreateCompressorFactory CreateCompressorFactory = nullptr;
            PFN_CreateSequenceMerger CreateSequenceMerger = nullptr;
            PFN_GetVersion GetVersion = nullptr;
        } // namespace Compression
        namespace Licensing
        {
            PFN_GetLicenseManager GetLicenseManager = nullptr;
        }
    } // namespace CE
} // namespace Zibra

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
        static_assert(CE::Compression::COMPRESSOR_VERSION.major == ZIB_COMPRESSION_ENGINE_MAJOR_VERSION);
        if (g_CompressionEngineVersion.major != CE::Compression::COMPRESSOR_VERSION.major)
        {
            return false;
        }
        if (g_CompressionEngineVersion.minor != CE::Compression::COMPRESSOR_VERSION.minor)
        {
            return g_CompressionEngineVersion.minor > CE::Compression::COMPRESSOR_VERSION.minor;
        }
        if (g_CompressionEngineVersion.patch != CE::Compression::COMPRESSOR_VERSION.patch)
        {
            return g_CompressionEngineVersion.patch > CE::Compression::COMPRESSOR_VERSION.patch;
        }
        if (g_CompressionEngineVersion.build != CE::Compression::COMPRESSOR_VERSION.build)
        {
            return g_CompressionEngineVersion.build > CE::Compression::COMPRESSOR_VERSION.build;
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

    std::string ErrorCodeToString(Result errorCode)
    {
        assert(ZIB_FAILED(errorCode));

        switch (errorCode)
        {
        case RESULT_SUCCESS:
            return RESULT_SUCCESS_DESCRIPTION;
        case RESULT_TIMEOUT:
            return RESULT_TIMEOUT_DESCRIPTION;
        case RESULT_ERROR:
            return RESULT_ERROR_DESCRIPTION;
        case RESULT_INVALID_ARGUMENTS:
            return RESULT_INVALID_ARGUMENTS_DESCRIPTION;
        case RESULT_INVALID_USAGE:
            return RESULT_INVALID_USAGE_DESCRIPTION;
        case RESULT_UNSUPPORTED:
            return RESULT_UNSUPPORTED_DESCRIPTION;
        case RESULT_UNINITIALIZED:
            return RESULT_UNINITIALIZED_DESCRIPTION;
        case RESULT_ALREADY_INITIALIZED:
            return RESULT_ALREADY_INITIALIZED_DESCRIPTION;
        case RESULT_NOT_FOUND:
            return RESULT_NOT_FOUND_DESCRIPTION;
        case RESULT_ALREADY_PRESENT:
            return RESULT_ALREADY_PRESENT_DESCRIPTION;
        case RESULT_OUT_OF_MEMORY:
            return RESULT_OUT_OF_MEMORY_DESCRIPTION;
        case RESULT_OUT_OF_BOUNDS:
            return RESULT_OUT_OF_BOUNDS_DESCRIPTION;
        case RESULT_IO_ERROR:
            return RESULT_IO_ERROR_DESCRIPTION;
        case RESULT_UNIMPLEMENTED:
            return RESULT_UNIMPLEMENTED_DESCRIPTION;
        case RESULT_UNEXPECTED_ERROR:
            return RESULT_UNEXPECTED_ERROR_DESCRIPTION;
        case RESULT_INVALID_SOURCE:
            return RESULT_INVALID_SOURCE_DESCRIPTION;
        case RESULT_INCOMPATIBLE_SOURCE:
            return RESULT_INCOMPATIBLE_SOURCE_DESCRIPTION;
        case RESULT_CORRUPTED_SOURCE:
            return RESULT_CORRUPTED_SOURCE_DESCRIPTION;
        case RESULT_MERGE_VERSION_MISMATCH:
            return RESULT_MERGE_VERSION_MISMATCH_DESCRIPTION;
        case RESULT_BINARY_FILE_SAVED_AS_TEXT:
            return RESULT_BINARY_FILE_SAVED_AS_TEXT_DESCRIPTION;
        case RESULT_QUEUE_EMPTY:
            return RESULT_QUEUE_EMPTY_DESCRIPTION;
        case RESULT_COMPRESSION_LICENSE_ERROR:
            return RESULT_COMPRESSION_LICENSE_ERROR_DESCRIPTION;
        case RESULT_DECOMPRESSION_LICENSE_ERROR:
            return RESULT_DECOMPRESSION_LICENSE_ERROR_DESCRIPTION;
        case RESULT_DECOMPRESSION_LICENSE_TIER_TOO_LOW:
            return RESULT_DECOMPRESSION_LICENSE_TIER_TOO_LOW_DESCRIPTION;
        case RESULT_FILE_NOT_FOUND:
            return RESULT_FILE_NOT_FOUND_DESCRIPTION;
        default:
            assert(0);
            return "Unknown error: " + std::to_string(errorCode);
        }
    }
} // namespace Zibra::LibraryUtils
