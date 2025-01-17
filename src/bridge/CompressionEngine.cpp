#include "PrecompiledHeader.h"

#include "CompressionEngine.h"

#include <UT/UT_EnvControl.h>

#include "networking/NetworkRequest.h"

namespace Zibra::CompressionEngine
{

#define ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING \
    ZIB_STRINGIFY(ZIB_COMPRESSION_ENGINE_BRIDGE_MAJOR_VERSION) "_" ZIB_STRINGIFY(ZIB_COMPRESSION_ENGINE_BRIDGE_MINOR_VERSION)

#if ZIB_PLATFORM_WIN
#define ZIB_PLATFORM_NAME "Windows"
#define ZIB_DYNAMIC_LIB_EXTENSION ".dll"
#define ZIB_DYNAMIC_LIB_PREFIX ""
#define ZIB_CALL_CONV __cdecl
#elif ZIB_PLATFORM_MAC
#define ZIB_PLATFORM_NAME "macOS"
#define ZIB_DYNAMIC_LIB_EXTENSION ".dylib"
#define ZIB_DYNAMIC_LIB_PREFIX "lib"
#define ZIB_CALL_CONV
#elif ZIB_PLATFORM_LINUX
#define ZIB_PLATFORM_NAME "Linux"
#define ZIB_DYNAMIC_LIB_EXTENSION ".so"
#define ZIB_DYNAMIC_LIB_PREFIX "lib"
#define ZIB_CALL_CONV
#else
#error Unuspported platform
#endif

    static constexpr ZCE_VersionNumber g_SupportedVersion = {ZIB_COMPRESSION_ENGINE_BRIDGE_MAJOR_VERSION,
                                                             ZIB_COMPRESSION_ENGINE_BRIDGE_MINOR_VERSION};

    const char* g_LibraryPath = "zibra/" ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING "/" ZIB_DYNAMIC_LIB_PREFIX "ZibraVDBHoudiniBridge" ZIB_DYNAMIC_LIB_EXTENSION;
    static constexpr const char* g_LibraryDownloadURL =
        "https://storage.googleapis.com/zibra-storage/ZibraVDBHoudiniBridge_" ZIB_PLATFORM_NAME
        "_" ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING ZIB_DYNAMIC_LIB_EXTENSION;

    bool g_IsLibraryLoaded = false;
    ZCE_VersionNumber g_LoadedLibraryVersion = {0, 0, 0, 0};
    bool g_IsLibraryInitialized = false;

    // May return 0-2 elements
    // 0 elements - environment variable is not set
    // 1 element - environment variable is set and is consistent between Houdini and STL (or one of them returned nullptr)
    // 2 elements - environment variable is set and is different between Houdini and STL (Houdini value is first)
    // This is necessary as either STL or Houdini value can be "bad" under certain circumstances
    std::vector<std::string> GetHoudiniEnvironmentVariable(UT_StrControl envVarEnum, const char* envVarName)
    {
        std::vector<std::string> result;
        const char* envVarHoudini = UT_EnvControl::getString(envVarEnum);
        if (envVarHoudini != nullptr)
        {
            result.push_back(envVarHoudini);
        }

        const char* envVarSTL = std::getenv(envVarName);
        if (envVarSTL != nullptr)
        {
            if (envVarHoudini == nullptr || strcmp(envVarHoudini, envVarSTL) != 0)
            {
                result.push_back(envVarSTL);
            }
        }
        return result;
    }

    // Returns vector of paths that can be used to search for the library
    // First element is the path used for downloading the library
    // Other elements are alternative load paths for manual library installation
    std::vector<std::string> GetLibraryPaths()
    {
        std::vector<std::string> result;

        const std::pair<UT_StrControl, const char*> basePathEnvVars[] = {
            {ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR"},
            {ENV_HSITE, "HSITE"},
        };

        for (const auto& [envVarEnum, envVarName] : basePathEnvVars)
        {
            const std::vector<std::string> baseDirs = GetHoudiniEnvironmentVariable(envVarEnum, envVarName);
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

    std::string GetDownloadURL()
    {
        return g_LibraryDownloadURL;
    }

    bool IsLibrarySupported(const ZCE_VersionNumber& version)
    {
        // Major and minor numbers should match exactly
        if (version.major != g_SupportedVersion.major || version.minor != g_SupportedVersion.minor)
        {
            return false;
        }
        // Patch and build numbers need to be at greater or equal to the supported version
        // If patch number is greater, build number doesn't matter
        if (version.patch != g_SupportedVersion.patch)
        {
            return version.patch >= g_SupportedVersion.patch;
        }
        return version.build >= g_SupportedVersion.build;
    }

#define ZIB_DECLARE_FUNCTION_POINTER(functionName, returnType, ...) \
    typedef returnType(ZIB_CALL_CONV * functionName##Type)(__VA_ARGS__);          \
    functionName##Type Bridge##functionName = nullptr;

    ZIB_DECLARE_FUNCTION_POINTER(GetVersion, ZCE_VersionNumber);
    ZIB_DECLARE_FUNCTION_POINTER(InitializeCompressionEngine, ZCE_Result);
    ZIB_DECLARE_FUNCTION_POINTER(DeinitializeCompressionEngine, ZCE_Result);

    ZIB_DECLARE_FUNCTION_POINTER(IsLicenseValid, ZCE_Result, ZCE_Product);

    ZIB_DECLARE_FUNCTION_POINTER(CreateCompressorInstance, ZCE_Result, uint32_t*, const ZCE_CompressionSettings*);
    ZIB_DECLARE_FUNCTION_POINTER(ReleaseCompressorInstance, ZCE_Result, uint32_t);

    ZIB_DECLARE_FUNCTION_POINTER(StartSequence, ZCE_Result, uint32_t);
    ZIB_DECLARE_FUNCTION_POINTER(CompressFrame, ZCE_Result, uint32_t, ZCE_FrameContainer*);
    ZIB_DECLARE_FUNCTION_POINTER(FinishSequence, ZCE_Result, uint32_t);
    ZIB_DECLARE_FUNCTION_POINTER(AbortSequence, ZCE_Result, uint32_t);

    ZIB_DECLARE_FUNCTION_POINTER(CreateDecompressorInstance, ZCE_Result, uint32_t*);
    ZIB_DECLARE_FUNCTION_POINTER(ReleaseDecompressorInstance, ZCE_Result, uint32_t);

    ZIB_DECLARE_FUNCTION_POINTER(GetSequenceInfo, ZCE_Result, uint32_t, ZCE_SequenceInfo*);
    ZIB_DECLARE_FUNCTION_POINTER(SetInputFile, ZCE_Result, uint32_t, const char*);
    ZIB_DECLARE_FUNCTION_POINTER(DecompressFrame, ZCE_Result, uint32_t, uint32_t, ZCE_DecompressedFrameContainer**);
    ZIB_DECLARE_FUNCTION_POINTER(FreeFrameData, ZCE_Result, ZCE_DecompressedFrameContainer*);
#undef ZIB_DECLARE_FUNCTION_POINTER

#if ZIB_PLATFORM_WIN
    HMODULE g_LibraryHandle = NULL;
#elif ZIB_PLATFORM_LINUX
    void* g_LibraryHandle = nullptr;
#else
// TODO macOS support
#error Unimplemented
#endif

    bool LoadFunctions() noexcept
    {
#if ZIB_PLATFORM_WIN
#define ZIB_LOAD_FUNCTION_POINTER(functionName)                                                                    \
    Bridge##functionName = reinterpret_cast<functionName##Type>(::GetProcAddress(g_LibraryHandle, #functionName)); \
    if (Bridge##functionName == nullptr)                                                                           \
    {                                                                                                              \
        return false;                                                                                              \
    }

        ZIB_LOAD_FUNCTION_POINTER(GetVersion);
        ZIB_LOAD_FUNCTION_POINTER(InitializeCompressionEngine);
        ZIB_LOAD_FUNCTION_POINTER(DeinitializeCompressionEngine);

        ZIB_LOAD_FUNCTION_POINTER(IsLicenseValid);

        ZIB_LOAD_FUNCTION_POINTER(CreateCompressorInstance);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseCompressorInstance);

        ZIB_LOAD_FUNCTION_POINTER(StartSequence);
        ZIB_LOAD_FUNCTION_POINTER(CompressFrame);
        ZIB_LOAD_FUNCTION_POINTER(FinishSequence);
        ZIB_LOAD_FUNCTION_POINTER(AbortSequence);

        ZIB_LOAD_FUNCTION_POINTER(CreateDecompressorInstance);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseDecompressorInstance);

        ZIB_LOAD_FUNCTION_POINTER(GetSequenceInfo);
        ZIB_LOAD_FUNCTION_POINTER(SetInputFile);
        ZIB_LOAD_FUNCTION_POINTER(DecompressFrame);
        ZIB_LOAD_FUNCTION_POINTER(FreeFrameData);

#undef ZIB_LOAD_FUNCTION_POINTER
#elif ZIB_PLATFORM_LINUX
#define ZIB_LOAD_FUNCTION_POINTER(functionName)                                                         \
    Bridge##functionName = reinterpret_cast<functionName##Type>(dlsym(g_LibraryHandle, #functionName)); \
    if (Bridge##functionName == nullptr)                                                                \
    {                                                                                                   \
        return false;                                                                                   \
    }

        ZIB_LOAD_FUNCTION_POINTER(GetVersion);
        ZIB_LOAD_FUNCTION_POINTER(InitializeCompressionEngine);
        ZIB_LOAD_FUNCTION_POINTER(DeinitializeCompressionEngine);

        ZIB_LOAD_FUNCTION_POINTER(IsLicenseValid);

        ZIB_LOAD_FUNCTION_POINTER(CreateCompressorInstance);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseCompressorInstance);

        ZIB_LOAD_FUNCTION_POINTER(StartSequence);
        ZIB_LOAD_FUNCTION_POINTER(CompressFrame);
        ZIB_LOAD_FUNCTION_POINTER(FinishSequence);
        ZIB_LOAD_FUNCTION_POINTER(AbortSequence);

        ZIB_LOAD_FUNCTION_POINTER(CreateDecompressorInstance);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseDecompressorInstance);

        ZIB_LOAD_FUNCTION_POINTER(GetSequenceInfo);
        ZIB_LOAD_FUNCTION_POINTER(SetInputFile);
        ZIB_LOAD_FUNCTION_POINTER(DecompressFrame);
        ZIB_LOAD_FUNCTION_POINTER(FreeFrameData);

#undef ZIB_LOAD_FUNCTION_POINTER
#else
// TODO macOS support
#error Unimplemented
#endif
        return true;
    }

    bool LoadLibraryByPath(const std::string& libraryPath) noexcept
    {
        assert(!g_IsLibraryLoaded);

#if ZIB_PLATFORM_WIN
        static_assert(IsPlatformSupported());

        assert(g_LibraryHandle == NULL);

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

        g_LoadedLibraryVersion = BridgeGetVersion();
        if (!IsLibrarySupported(g_LoadedLibraryVersion))
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

        g_LoadedLibraryVersion = BridgeGetVersion();
        if (!IsLibrarySupported(g_LoadedLibraryVersion))
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
        if (!IsPlatformSupported())
        {
            assert(0);
            return;
        }

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

        ZCE_Result result = BridgeInitializeCompressionEngine();
        g_IsLibraryInitialized = (result == ZCE_Result::SUCCESS);
    }

    void DownloadLibrary() noexcept
    {
        const std::string downloadURL = GetDownloadURL();
        const std::string libraryPath = GetLibraryPaths()[0];
        bool success = NetworkRequest::DownloadFile(downloadURL, libraryPath);
        if (!success)
        {
            return;
        }

        LoadLibrary();
    }

    bool IsLibraryLoaded() noexcept
    {
        return g_IsLibraryLoaded;
    }

    bool IsLibraryInitialized() noexcept
    {
        return g_IsLibraryInitialized;
    }

    bool IsLicenseValid(ZCE_Product product) noexcept
    {
        if (!IsLibraryLoaded())
        {
            assert(0);
            return false;
        }

        ZCE_Result result = BridgeIsLicenseValid(product);
        assert(result == ZCE_Result::SUCCESS || result == ZCE_Result::LICENSE_IS_NOT_VERIFIED);
        return result == ZCE_Result::SUCCESS;
    }

    ZCE_VersionNumber GetLoadedLibraryVersion() noexcept
    {
        if (!IsLibraryLoaded())
        {
            assert(0);
            return {0, 0, 0, 0};
        }

        return g_LoadedLibraryVersion;
    }

    std::string GetLoadedLibraryVersionString() noexcept
    {
        if (!IsLibraryLoaded())
        {
            return "";
        }

        const ZCE_VersionNumber version = GetLoadedLibraryVersion();
        return std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch) + "." +
               std::to_string(version.build);
    }

    uint32_t CreateCompressorInstance(const ZCE_CompressionSettings* settings) noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return UINT32_MAX;
        }

        uint32_t compressorID = 0;
        ZCE_Result result = BridgeCreateCompressorInstance(&compressorID, settings);
        assert(result == ZCE_Result::SUCCESS);
        return compressorID;
    }

    bool CompressFrame(uint32_t instanceID, ZCE_FrameContainer* frameData) noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return false;
        }

        ZCE_Result result = BridgeCompressFrame(instanceID, frameData);
        return result == ZCE_Result::SUCCESS;
    }

    void FinishSequence(uint32_t instanceID) noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return;
        }

        ZCE_Result result = BridgeFinishSequence(instanceID);
        assert(result == ZCE_Result::SUCCESS);
    }

    void AbortSequence(uint32_t instanceID) noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return;
        }

        ZCE_Result result = BridgeAbortSequence(instanceID);
        assert(result == ZCE_Result::SUCCESS);
    }

    void ReleaseCompressorInstance(uint32_t instanceID) noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return;
        }

        ZCE_Result result = BridgeReleaseCompressorInstance(instanceID);
        assert(result == ZCE_Result::SUCCESS);
    }

    void StartSequence(uint32_t instanceID) noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return;
        }

        ZCE_Result result = BridgeStartSequence(instanceID);
        assert(result == ZCE_Result::SUCCESS);
    }

    uint32_t CreateDecompressorInstance() noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return UINT32_MAX;
        }

        uint32_t decompressorID = 0;
        ZCE_Result result = BridgeCreateDecompressorInstance(&decompressorID);
        assert(result == ZCE_Result::SUCCESS);
        return decompressorID;
    }

    bool SetInputFile(uint32_t instanceID, const char* inputFilePath) noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return false;
        }

        ZCE_Result result = BridgeSetInputFile(instanceID, const_cast<char*>(inputFilePath));
        return result == ZCE_Result::SUCCESS;
    }

    void DecompressFrame(uint32_t instanceID, uint32_t frameIndex, ZCE_DecompressedFrameContainer** frameData) noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return;
        }

        ZCE_Result result = BridgeDecompressFrame(instanceID, frameIndex, frameData);
        assert(result == ZCE_Result::SUCCESS);
    }

    void ReleaseDecompressorInstance(uint32_t instanceID) noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return;
        }

        ZCE_Result result = BridgeReleaseDecompressorInstance(instanceID);
        assert(result == ZCE_Result::SUCCESS);
    }

    void FreeFrameData(ZCE_DecompressedFrameContainer* frameData) noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return;
        }

        ZCE_Result result = BridgeFreeFrameData(frameData);
        assert(result == ZCE_Result::SUCCESS);
    }

    void GetSequenceInfo(uint32_t instanceID, ZCE_SequenceInfo* sequenceInfo) noexcept
    {
        if (!IsLibraryInitialized())
        {
            assert(0);
            return;
        }

        ZCE_Result result = BridgeGetSequenceInfo(instanceID, sequenceInfo);
        assert(result == ZCE_Result::SUCCESS);
    }

} // namespace Zibra::CompressionEngine
