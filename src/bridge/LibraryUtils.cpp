#include "LibraryUtils.h"

#include <UT/UT_EnvControl.h>

#include "networking/NetworkRequest.h"
#include "utils/Helpers.h"

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

// clang-format off

#define ZRHI_RUNTIME_EXPORT_NAME(name) Zibra_RHI_RHIRuntime_##name
#define ZRHI_FACTORY_EXPORT_NAME(name) Zibra_RHI_RHIFactory_##name
#define ZRHI_GLOBAL_EXPORT_NAME(name) Zibra_RHI_##name
#define ZCE_FRAME_MANAGER_EXPORT_NAME(name) Zibra_CE_Compression_FrameManager_##name
#define ZCE_COMPRESSOR_EXPORT_NAME(name) Zibra_CE_Compression_Compressor_##name
#define ZCE_COMPRESSOR_FACTORY_EXPORT_NAME(name) Zibra_CE_Compression_CompressorFactory_##name
#define ZCE_COMPRESSOR_GLOBAL_EXPORT_NAME(name) Zibra_CE_Compression_##name
#define ZCE_DECOMPRESSION_COMPRESSED_FRAME_CONTAINER_EXPORT_NAME(name) Zibra_CE_Decompression_CompressedFrameContainer_##name
#define ZCE_DECOMPRESSION_FORMAT_MAPPER_EXPORT_NAME(name) Zibra_CE_Decompression_FormatMapper_##name
#define ZCE_DECOMPRESSION_EXPORT_NAME(name) Zibra_CE_Decompression_Decompressor_##name
#define ZCE_DECOMPRESSION_DECOMPRESSOR_FACTORY_EXPORT_NAME(name) Zibra_CE_Decompression_DecompressorFactory_##name
#define ZCE_DECOMPRESSION_GLOBAL_EXPORT_NAME(name) Zibra_CE_Decompression_##name
#define ZCE_LICENSING_EXPORT_NAME(name) Zibra_CE_Licensing_##name

#define ZSDK_RUNTIME_FUNCTION_LIST_APPLY(macro) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(Initialize)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(Release)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(GarbageCollect)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(GetGFXAPI)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(QueryFeatureSupport)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(SetStablePowerState)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(CompileComputePSO)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(CompileGraphicPSO)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(ReleaseComputePSO)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(ReleaseGraphicPSO)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(RegisterBuffer)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(RegisterTexture2D)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(RegisterTexture3D)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(CreateBuffer)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(CreateTexture2D)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(CreateTexture3D)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(CreateSampler)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(ReleaseBuffer)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(ReleaseTexture2D)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(ReleaseTexture3D)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(ReleaseSampler)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(CreateDescriptorHeap)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(CloneDescriptorHeap)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(ReleaseDescriptorHeap)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(CreateQueryHeap)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(ReleaseQueryHeap)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(CreateFramebuffer)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(ReleaseFramebuffer)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(InitializeSRV_B)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(InitializeSRV_T2D)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(InitializeSRV_T3D)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(InitializeUAV_B)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(InitializeUAV_T2D)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(InitializeUAV_T3D)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(InitializeCBV)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(StartRecording)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(StopRecording)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(UploadBuffer)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(UploadBufferSlow)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(UploadTextureSlow)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(GetBufferData)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(GetBufferDataImmediately)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(Dispatch)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(DrawIndexedInstanced)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(DrawInstanced)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(DispatchIndirect)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(DrawInstancedIndirect)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(DrawIndexedInstancedIndirect)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(CopyBufferRegion)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(ClearFormattedBuffer)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(SubmitQuery)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(ResolveQuery)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(GetTimestampFrequency)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(GetTimestampCalibration)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(GetCPUTimestamp)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(StartDebugRegion)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(EndDebugRegion)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(StartFrameCapture)) \
    macro(ZRHI_RUNTIME_EXPORT_NAME(StopFrameCapture)) \
    macro(ZRHI_FACTORY_EXPORT_NAME(SetGFXAPI)) \
    macro(ZRHI_FACTORY_EXPORT_NAME(UseGFXCore)) \
    macro(ZRHI_FACTORY_EXPORT_NAME(UseForcedAdapter)) \
    macro(ZRHI_FACTORY_EXPORT_NAME(UseAutoSelectedAdapter)) \
    macro(ZRHI_FACTORY_EXPORT_NAME(ForceSoftwareDevice)) \
    macro(ZRHI_FACTORY_EXPORT_NAME(ForceEnableDebugLayer)) \
    macro(ZRHI_FACTORY_EXPORT_NAME(Create)) \
    macro(ZRHI_FACTORY_EXPORT_NAME(Release)) \
    macro(ZRHI_GLOBAL_EXPORT_NAME(GetVersion)) \
    macro(ZRHI_GLOBAL_EXPORT_NAME(CreateRHIFactory)) \
    macro(ZCE_FRAME_MANAGER_EXPORT_NAME(AddMetadata)) \
    macro(ZCE_FRAME_MANAGER_EXPORT_NAME(Finish)) \
    macro(ZCE_COMPRESSOR_EXPORT_NAME(Initialize)) \
    macro(ZCE_COMPRESSOR_EXPORT_NAME(Release)) \
    macro(ZCE_COMPRESSOR_EXPORT_NAME(StartSequence)) \
    macro(ZCE_COMPRESSOR_EXPORT_NAME(AddPerSequenceMetadata)) \
    macro(ZCE_COMPRESSOR_EXPORT_NAME(CompressFrame)) \
    macro(ZCE_COMPRESSOR_EXPORT_NAME(FinishSequence)) \
    macro(ZCE_COMPRESSOR_FACTORY_EXPORT_NAME(UseRHI)) \
    macro(ZCE_COMPRESSOR_FACTORY_EXPORT_NAME(SetQuality)) \
    macro(ZCE_COMPRESSOR_FACTORY_EXPORT_NAME(OverrideChannelQuality)) \
    macro(ZCE_COMPRESSOR_FACTORY_EXPORT_NAME(SetFrameMapping)) \
    macro(ZCE_COMPRESSOR_FACTORY_EXPORT_NAME(Create)) \
    macro(ZCE_COMPRESSOR_FACTORY_EXPORT_NAME(Release)) \
    macro(ZCE_COMPRESSOR_GLOBAL_EXPORT_NAME(GetVersion)) \
    macro(ZCE_COMPRESSOR_GLOBAL_EXPORT_NAME(CreateCompressorFactory)) \
    macro(ZCE_DECOMPRESSION_COMPRESSED_FRAME_CONTAINER_EXPORT_NAME(GetInfo)) \
    macro(ZCE_DECOMPRESSION_COMPRESSED_FRAME_CONTAINER_EXPORT_NAME(GetMetadataByKey)) \
    macro(ZCE_DECOMPRESSION_COMPRESSED_FRAME_CONTAINER_EXPORT_NAME(GetMetadataCount)) \
    macro(ZCE_DECOMPRESSION_COMPRESSED_FRAME_CONTAINER_EXPORT_NAME(GetMetadataByIndex)) \
    macro(ZCE_DECOMPRESSION_COMPRESSED_FRAME_CONTAINER_EXPORT_NAME(Release)) \
    macro(ZCE_DECOMPRESSION_FORMAT_MAPPER_EXPORT_NAME(FetchFrame)) \
    macro(ZCE_DECOMPRESSION_FORMAT_MAPPER_EXPORT_NAME(FetchFrameInfo)) \
    macro(ZCE_DECOMPRESSION_FORMAT_MAPPER_EXPORT_NAME(GetFrameRange)) \
    macro(ZCE_DECOMPRESSION_FORMAT_MAPPER_EXPORT_NAME(Release)) \
    macro(ZCE_DECOMPRESSION_EXPORT_NAME(Initialize)) \
    macro(ZCE_DECOMPRESSION_EXPORT_NAME(Release)) \
    macro(ZCE_DECOMPRESSION_EXPORT_NAME(RegisterResources)) \
    macro(ZCE_DECOMPRESSION_EXPORT_NAME(GetResourcesRequirements)) \
    macro(ZCE_DECOMPRESSION_EXPORT_NAME(GetFormatMapper)) \
    macro(ZCE_DECOMPRESSION_EXPORT_NAME(DecompressFrame)) \
    macro(ZCE_DECOMPRESSION_DECOMPRESSOR_FACTORY_EXPORT_NAME(UseDecoder)) \
    macro(ZCE_DECOMPRESSION_DECOMPRESSOR_FACTORY_EXPORT_NAME(UseRHI)) \
    macro(ZCE_DECOMPRESSION_DECOMPRESSOR_FACTORY_EXPORT_NAME(Create)) \
    macro(ZCE_DECOMPRESSION_DECOMPRESSOR_FACTORY_EXPORT_NAME(Release)) \
    macro(ZCE_DECOMPRESSION_GLOBAL_EXPORT_NAME(GetVersion)) \
    macro(ZCE_DECOMPRESSION_GLOBAL_EXPORT_NAME(CreateDecompressorFactory)) \
    macro(ZCE_DECOMPRESSION_GLOBAL_EXPORT_NAME(CreateDecoder)) \
    macro(ZCE_DECOMPRESSION_GLOBAL_EXPORT_NAME(GetFileFormatVersion)) \
    macro(ZCE_DECOMPRESSION_GLOBAL_EXPORT_NAME(ReleaseDecoder)) \
    macro(ZCE_LICENSING_EXPORT_NAME(CheckoutLicenseWithKey)) \
    macro(ZCE_LICENSING_EXPORT_NAME(CheckoutLicenseOffline)) \
    macro(ZCE_LICENSING_EXPORT_NAME(GetLicenseStatus)) \
    macro(ZCE_LICENSING_EXPORT_NAME(ReleaseLicense))

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

#define ZIB_COMPRESSION_MAJOR_VERSION 0
#define ZIB_DECOMPRESSION_MAJOR_VERSION 0
#define ZIB_RHI_MAJOR_VERSION 1

#define ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING \
    ZIB_STRINGIFY(ZIB_COMPRESSION_MAJOR_VERSION) "_" ZIB_STRINGIFY(ZIB_DECOMPRESSION_MAJOR_VERSION) "_" ZIB_STRINGIFY(ZIB_RHI_MAJOR_VERSION)

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

    static constexpr const char* g_BaseDirEnv = "HOUDINI_USER_PREF_DIR";
    static constexpr const char* g_AltDirEnv = "HSITE";
    const char* g_LibraryPath =
        "zibra/" ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING "/" ZIB_DYNAMIC_LIB_PREFIX "ZibraVDBSDK" ZIB_DYNAMIC_LIB_EXTENSION;
    static constexpr const char* g_LibraryDownloadURL =
        "https://storage.googleapis.com/zibra-storage/ZibraVDB_Houdini_" ZIB_PLATFORM_NAME
        "_" ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING ZIB_DYNAMIC_LIB_EXTENSION;

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

    std::string GetDownloadURL()
    {
        return g_LibraryDownloadURL;
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
#define ZIB_STRINGIZE(x) #x
#if ZIB_PLATFORM_WIN
#define ZIB_LOAD_FUNCTION_POINTER(functionName)                                                                          \
    functionName = reinterpret_cast<ZCE_PFN(functionName)>(::GetProcAddress(g_LibraryHandle, ZIB_STRINGIZE(functionName))); \
    if (functionName == nullptr)                                                                                         \
    {                                                                                                                    \
        return false;                                                                                                    \
    }
        ZSDK_RUNTIME_FUNCTION_LIST_APPLY(ZIB_LOAD_FUNCTION_POINTER)
#undef ZIB_LOAD_FUNCTION_POINTER
#elif ZIB_PLATFORM_LINUX
#define ZIB_LOAD_FUNCTION_POINTER(functionName)                                                    \
    functionName = reinterpret_cast<ZCE_PFN(functionName)>(dlsym(g_LibraryHandle, ZIB_STRINGIZE(functionName))); \
    if (functionName == nullptr)                                                                   \
    {                                                                                              \
        return false;                                                                              \
    }
        ZSDK_RUNTIME_FUNCTION_LIST_APPLY(ZIB_LOAD_FUNCTION_POINTER)
#undef ZIB_LOAD_FUNCTION_POINTER
#else
#error Unuspported platform
#endif
#undef ZIB_STRINGIZE
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

#undef STRINGIFY
#undef STRINGIFY_HELPER
#undef ZCE_CONCAT_HELPER
#undef ZRHI_CONCAT_HELPER
