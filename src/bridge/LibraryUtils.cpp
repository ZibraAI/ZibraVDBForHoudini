#include "LibraryUtils.h"

#include <UT/UT_EnvControl.h>

#include "networking/NetworkRequest.h"

#define ZRHI_CONCAT_HELPER(A, B) A##B
#define ZRHI_PFN(name) ZRHI_CONCAT_HELPER(PFN_, name)
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#pragma region RHIRuntime
#define ZRHI_FNPFX(name) Zibra_RHI_RHIRuntime_##name

ZRHI_PFN(ZRHI_FNPFX(Release)) ZRHI_FNPFX(Release) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(GarbageCollect)) ZRHI_FNPFX(GarbageCollect) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(GetGFXAPI)) ZRHI_FNPFX(GetGFXAPI) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(QueryFeatureSupport)) ZRHI_FNPFX(QueryFeatureSupport) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(SetStablePowerState)) ZRHI_FNPFX(SetStablePowerState) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(CompileComputePSO)) ZRHI_FNPFX(CompileComputePSO) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(CompileGraphicPSO)) ZRHI_FNPFX(CompileGraphicPSO) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ReleaseComputePSO)) ZRHI_FNPFX(ReleaseComputePSO) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ReleaseGraphicPSO)) ZRHI_FNPFX(ReleaseGraphicPSO) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(RegisterBuffer)) ZRHI_FNPFX(RegisterBuffer) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(RegisterTexture2D)) ZRHI_FNPFX(RegisterTexture2D) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(RegisterTexture3D)) ZRHI_FNPFX(RegisterTexture3D) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(CreateBuffer)) ZRHI_FNPFX(CreateBuffer) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(CreateTexture2D)) ZRHI_FNPFX(CreateTexture2D) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(CreateTexture3D)) ZRHI_FNPFX(CreateTexture3D) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(CreateSampler)) ZRHI_FNPFX(CreateSampler) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ReleaseBuffer)) ZRHI_FNPFX(ReleaseBuffer) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ReleaseTexture2D)) ZRHI_FNPFX(ReleaseTexture2D) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ReleaseTexture3D)) ZRHI_FNPFX(ReleaseTexture3D) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ReleaseSampler)) ZRHI_FNPFX(ReleaseSampler) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(CreateDescriptorHeap)) ZRHI_FNPFX(CreateDescriptorHeap) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(CloneDescriptorHeap)) ZRHI_FNPFX(CloneDescriptorHeap) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ReleaseDescriptorHeap)) ZRHI_FNPFX(ReleaseDescriptorHeap) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(CreateQueryHeap)) ZRHI_FNPFX(CreateQueryHeap) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ReleaseQueryHeap)) ZRHI_FNPFX(ReleaseQueryHeap) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(CreateFramebuffer)) ZRHI_FNPFX(CreateFramebuffer) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ReleaseFramebuffer)) ZRHI_FNPFX(ReleaseFramebuffer) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(InitializeSRV_B)) ZRHI_FNPFX(InitializeSRV_B) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(InitializeSRV_T2D)) ZRHI_FNPFX(InitializeSRV_T2D) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(InitializeSRV_T3D)) ZRHI_FNPFX(InitializeSRV_T3D) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(InitializeUAV_B)) ZRHI_FNPFX(InitializeUAV_B) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(InitializeUAV_T2D)) ZRHI_FNPFX(InitializeUAV_T2D) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(InitializeUAV_T3D)) ZRHI_FNPFX(InitializeUAV_T3D) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(InitializeCBV)) ZRHI_FNPFX(InitializeCBV) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(StartRecording)) ZRHI_FNPFX(StartRecording) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(StopRecording)) ZRHI_FNPFX(StopRecording) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(UploadBuffer)) ZRHI_FNPFX(UploadBuffer) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(UploadBufferSlow)) ZRHI_FNPFX(UploadBufferSlow) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(UploadTextureSlow)) ZRHI_FNPFX(UploadTextureSlow) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(GetBufferData)) ZRHI_FNPFX(GetBufferData) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(GetBufferDataImmediately)) ZRHI_FNPFX(GetBufferDataImmediately) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(Dispatch)) ZRHI_FNPFX(Dispatch) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(DrawIndexedInstanced)) ZRHI_FNPFX(DrawIndexedInstanced) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(DrawInstanced)) ZRHI_FNPFX(DrawInstanced) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(DispatchIndirect)) ZRHI_FNPFX(DispatchIndirect) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(DrawInstancedIndirect)) ZRHI_FNPFX(DrawInstancedIndirect) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(DrawIndexedInstancedIndirect)) ZRHI_FNPFX(DrawIndexedInstancedIndirect) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(CopyBufferRegion)) ZRHI_FNPFX(CopyBufferRegion) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ClearFormattedBuffer)) ZRHI_FNPFX(ClearFormattedBuffer) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(SubmitQuery)) ZRHI_FNPFX(SubmitQuery) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ResolveQuery)) ZRHI_FNPFX(ResolveQuery) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(GetTimestampFrequency)) ZRHI_FNPFX(GetTimestampFrequency) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(GetTimestampCalibration)) ZRHI_FNPFX(GetTimestampCalibration) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(GetCPUTimestamp)) ZRHI_FNPFX(GetCPUTimestamp) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(StartDebugRegion)) ZRHI_FNPFX(StartDebugRegion) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(EndDebugRegion)) ZRHI_FNPFX(EndDebugRegion) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(StartFrameCapture)) ZRHI_FNPFX(StartFrameCapture) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(StopFrameCapture)) ZRHI_FNPFX(StopFrameCapture) = nullptr;

#pragma endregion RHIRuntime
#undef ZRHI_FNPFX

#pragma region RHIFactory
#define ZRHI_FNPFX(name) Zibra_RHI_RHIFactory_##name

ZRHI_PFN(ZRHI_FNPFX(SetGFXAPI)) ZRHI_FNPFX(SetGFXAPI) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(UseGFXCore)) ZRHI_FNPFX(UseGFXCore) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(UseForcedAdapter)) ZRHI_FNPFX(UseForcedAdapter) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(UseAutoSelectedAdapter)) ZRHI_FNPFX(UseAutoSelectedAdapter) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ForceSoftwareDevice)) ZRHI_FNPFX(ForceSoftwareDevice) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(ForceEnableDebugLayer)) ZRHI_FNPFX(ForceEnableDebugLayer) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(Create)) ZRHI_FNPFX(Create) = nullptr;
ZRHI_PFN(ZRHI_FNPFX(Release)) ZRHI_FNPFX(Release) = nullptr;

#pragma endregion RHIFactory
#undef ZRHI_FNPFX

#pragma region Functions
#define ZRHI_FNPFX(name) Zibra_RHI_##name

ZRHI_PFN(ZRHI_FNPFX(CreateRHIFactory)) ZRHI_FNPFX(CreateRHIFactory) = nullptr;

#pragma endregion Functions
#undef ZRHI_FNPFX

#define ZCE_CONCAT_HELPER(A, B) A##B
#define ZCE_PFN(name) ZCE_CONCAT_HELPER(PFN_, name)

#pragma region Compression

#pragma region FrameManager
#define ZCE_FNPFX(name) Zibra_CE_Compression_FrameManager_##name

ZCE_PFN(ZCE_FNPFX(AddMetadata)) ZCE_FNPFX(AddMetadata) = nullptr;
ZCE_PFN(ZCE_FNPFX(Finish)) ZCE_FNPFX(Finish) = nullptr;

#undef ZCE_FNPFX
#pragma endregion FrameManager

#pragma region Compressor
#define ZCE_FNPFX(name) Zibra_CE_Compression_Compressor_##name

ZCE_PFN(ZCE_FNPFX(Initialize)) ZCE_FNPFX(Initialize) = nullptr;
ZCE_PFN(ZCE_FNPFX(Release)) ZCE_FNPFX(Release) = nullptr;
ZCE_PFN(ZCE_FNPFX(StartSequence)) ZCE_FNPFX(StartSequence) = nullptr;
ZCE_PFN(ZCE_FNPFX(AddPerSequenceMetadata)) ZCE_FNPFX(AddPerSequenceMetadata) = nullptr;
ZCE_PFN(ZCE_FNPFX(CompressFrame)) ZCE_FNPFX(CompressFrame) = nullptr;
ZCE_PFN(ZCE_FNPFX(FinishSequence)) ZCE_FNPFX(FinishSequence) = nullptr;

#undef ZCE_FNPFX
#pragma endregion Compressor

#pragma region CompressorFactory
#define ZCE_FNPFX(name) Zibra_CE_Compression_CompressorFactory_##name

ZCE_PFN(ZCE_FNPFX(UseRHI)) ZCE_FNPFX(UseRHI) = nullptr;
ZCE_PFN(ZCE_FNPFX(SetQuality)) ZCE_FNPFX(SetQuality) = nullptr;
ZCE_PFN(ZCE_FNPFX(OverrideChannelQuality)) ZCE_FNPFX(OverrideChannelQuality) = nullptr;
ZCE_PFN(ZCE_FNPFX(SetFrameMapping)) ZCE_FNPFX(SetFrameMapping) = nullptr;
ZCE_PFN(ZCE_FNPFX(Create)) ZCE_FNPFX(Create) = nullptr;
ZCE_PFN(ZCE_FNPFX(Release)) ZCE_FNPFX(Release) = nullptr;

#undef ZCE_FNPFX
#pragma endregion CompressorFactory

#pragma region Funcs
#define ZCE_FNPFX(name) Zibra_CE_Compression_##name

ZCE_PFN(ZCE_FNPFX(CreateCompressorFactory)) ZCE_FNPFX(CreateCompressorFactory) = nullptr;

#undef ZCE_FNPFX
#pragma endregion Funcs

#pragma endregion Compression

#pragma region Decompression

#pragma region CompressedFrameContainer
#define ZCE_FNPFX(name) Zibra_CE_Decompression_CompressedFrameContainer_##name

ZCE_PFN(ZCE_FNPFX(GetInfo)) ZCE_FNPFX(GetInfo) = nullptr;
ZCE_PFN(ZCE_FNPFX(GetMetadataByKey)) ZCE_FNPFX(GetMetadataByKey) = nullptr;
ZCE_PFN(ZCE_FNPFX(GetMetadataCount)) ZCE_FNPFX(GetMetadataCount) = nullptr;
ZCE_PFN(ZCE_FNPFX(GetMetadataByIndex)) ZCE_FNPFX(GetMetadataByIndex) = nullptr;
ZCE_PFN(ZCE_FNPFX(Release)) ZCE_FNPFX(Release) = nullptr;

#undef ZCE_FNPFX
#pragma endregion CompressedFrameContainer

#pragma region FormatMapper
#define ZCE_FNPFX(name) Zibra_CE_Decompression_FormatMapper_##name

ZCE_PFN(ZCE_FNPFX(FetchFrame)) ZCE_FNPFX(FetchFrame) = nullptr;
ZCE_PFN(ZCE_FNPFX(FetchFrameInfo)) ZCE_FNPFX(FetchFrameInfo) = nullptr;
ZCE_PFN(ZCE_FNPFX(GetFrameRange)) ZCE_FNPFX(GetFrameRange) = nullptr;
ZCE_PFN(ZCE_FNPFX(Release)) ZCE_FNPFX(Release) = nullptr;

#undef ZCE_FNPFX
#pragma endregion FormatMapper

#pragma region Decompressor
#define ZCE_FNPFX(name) Zibra_CE_Decompression_Decompressor_##name

ZCE_PFN(ZCE_FNPFX(Initialize)) ZCE_FNPFX(Initialize) = nullptr;
ZCE_PFN(ZCE_FNPFX(Release)) ZCE_FNPFX(Release) = nullptr;
ZCE_PFN(ZCE_FNPFX(RegisterResources)) ZCE_FNPFX(RegisterResources) = nullptr;
ZCE_PFN(ZCE_FNPFX(GetResourcesRequirements)) ZCE_FNPFX(GetResourcesRequirements) = nullptr;
ZCE_PFN(ZCE_FNPFX(GetFormatMapper)) ZCE_FNPFX(GetFormatMapper) = nullptr;
ZCE_PFN(ZCE_FNPFX(DecompressFrame)) ZCE_FNPFX(DecompressFrame) = nullptr;

#undef ZCE_FNPFX
#pragma endregion Decompressor

#pragma region DecompressorFactory
#define ZCE_FNPFX(name) Zibra_CE_Decompression_DecompressorFactory_##name

ZCE_PFN(ZCE_FNPFX(UseDecoder)) ZCE_FNPFX(UseDecoder) = nullptr;
ZCE_PFN(ZCE_FNPFX(UseRHI)) ZCE_FNPFX(UseRHI) = nullptr;
ZCE_PFN(ZCE_FNPFX(Create)) ZCE_FNPFX(Create) = nullptr;
ZCE_PFN(ZCE_FNPFX(Release)) ZCE_FNPFX(Release) = nullptr;

#undef ZCE_FNPFX
#pragma endregion DecompressorFactory

#pragma region Functions
#define ZCE_FNPFX(name) Zibra_CE_Decompression_##name

ZCE_PFN(ZCE_FNPFX(CreateDecompressorFactory)) ZCE_FNPFX(CreateDecompressorFactory) = nullptr;
ZCE_PFN(ZCE_FNPFX(CreateDecoder)) ZCE_FNPFX(CreateDecoder) = nullptr;
ZCE_PFN(ZCE_FNPFX(GetFileFormatVersion)) ZCE_FNPFX(GetFileFormatVersion) = nullptr;
ZCE_PFN(ZCE_FNPFX(ReleaseDecoder)) ZCE_FNPFX(ReleaseDecoder) = nullptr;

#undef ZCE_FNPFX
#pragma endregion Functions

#pragma endregion Decompression

 #pragma region Licensing
 #define ZCE_FNPFX(name) Zibra_CE_Licensing_##name

ZCE_PFN(ZCE_FNPFX(CheckoutLicenseWithKey)) ZCE_FNPFX(CheckoutLicenseWithKey) = nullptr;
ZCE_PFN(ZCE_FNPFX(CheckoutLicenseOffline)) ZCE_FNPFX(CheckoutLicenseOffline) = nullptr;
ZCE_PFN(ZCE_FNPFX(GetLicenseStatus)) ZCE_FNPFX(GetLicenseStatus) = nullptr;
ZCE_PFN(ZCE_FNPFX(ReleaseLicense)) ZCE_FNPFX(ReleaseLicense) = nullptr;

 #undef ZCE_FNPFX
 #pragma endregion Licensing

namespace Zibra::LibraryUtils
{

#define ZIB_COMPRESSION_ENGINE_BRIDGE_MAJOR_VERSION 0
#define ZIB_COMPRESSION_ENGINE_BRIDGE_MINOR_VERSION 2

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


    static constexpr const char* g_BaseDirEnv = "HOUDINI_USER_PREF_DIR";
    static constexpr const char* g_AltDirEnv = "HSITE";
    const char* g_LibraryPath =
        "zibra/" ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING "/" ZIB_DYNAMIC_LIB_PREFIX "ZibraVDBSDK" ZIB_DYNAMIC_LIB_EXTENSION;
    static constexpr const char* g_LibraryDownloadURL =
        "https://storage.googleapis.com/zibra-storage/ZibraVDBHoudiniBridge_" ZIB_PLATFORM_NAME
        "_" ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING ZIB_DYNAMIC_LIB_EXTENSION;

    bool g_IsLibraryLoaded = false;
    Zibra::CE::Version g_LoadedLibraryVersion = {0, 0, 0, 0};
    bool g_IsLibraryInitialized = false;

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

    //std::string GetDownloadURL()
    //{
    //    return g_LibraryDownloadURL;
    //}

    //bool IsLibrarySupported(const ZCE_VersionNumber& version)
    //{
    //    // Major and minor numbers should match exactly
    //    if (version.major != g_SupportedVersion.major || version.minor != g_SupportedVersion.minor)
    //    {
    //        return false;
    //    }
    //    // Patch and build numbers need to be at greater or equal to the supported version
    //    // If patch number is greater, build number doesn't matter
    //    if (version.patch != g_SupportedVersion.patch)
    //    {
    //        return version.patch >= g_SupportedVersion.patch;
    //    }
    //    return version.build >= g_SupportedVersion.build;
    //}

#if ZIB_PLATFORM_WIN
    HMODULE g_LibraryHandle = NULL;
#else
// TODO cross-platform support
#endif

    bool LoadRHIFunctions() noexcept
    {
#if ZIB_PLATFORM_WIN

#define ZIB_LOAD_FUNCTION_POINTER(functionName)                                                                                       \
    ZRHI_FNPFX(functionName) =                                                                                                        \
        reinterpret_cast<ZRHI_PFN(ZRHI_FNPFX(functionName))>(::GetProcAddress(g_LibraryHandle, STRINGIFY(ZRHI_FNPFX(functionName)))); \
    if (ZRHI_FNPFX(functionName) == nullptr)                                                                                          \
    {                                                                                                                                 \
        return false;                                                                                                                 \
    }
#pragma region RHIRuntime
#define ZRHI_FNPFX(name) Zibra_RHI_RHIRuntime_##name

        ZIB_LOAD_FUNCTION_POINTER(Release);
        ZIB_LOAD_FUNCTION_POINTER(GarbageCollect);
        ZIB_LOAD_FUNCTION_POINTER(GetGFXAPI);
        ZIB_LOAD_FUNCTION_POINTER(QueryFeatureSupport);
        ZIB_LOAD_FUNCTION_POINTER(SetStablePowerState);
        ZIB_LOAD_FUNCTION_POINTER(CompileComputePSO);
        ZIB_LOAD_FUNCTION_POINTER(CompileGraphicPSO);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseComputePSO);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseGraphicPSO);
        ZIB_LOAD_FUNCTION_POINTER(RegisterBuffer);
        ZIB_LOAD_FUNCTION_POINTER(RegisterTexture2D);
        ZIB_LOAD_FUNCTION_POINTER(RegisterTexture3D);
        ZIB_LOAD_FUNCTION_POINTER(CreateBuffer);
        ZIB_LOAD_FUNCTION_POINTER(CreateTexture2D);
        ZIB_LOAD_FUNCTION_POINTER(CreateTexture3D);
        ZIB_LOAD_FUNCTION_POINTER(CreateSampler);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseBuffer);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseTexture2D);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseTexture3D);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseSampler);
        ZIB_LOAD_FUNCTION_POINTER(CreateDescriptorHeap);
        ZIB_LOAD_FUNCTION_POINTER(CloneDescriptorHeap);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseDescriptorHeap);
        ZIB_LOAD_FUNCTION_POINTER(CreateQueryHeap);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseQueryHeap);
        ZIB_LOAD_FUNCTION_POINTER(CreateFramebuffer);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseFramebuffer);
        ZIB_LOAD_FUNCTION_POINTER(InitializeSRV_B);
        ZIB_LOAD_FUNCTION_POINTER(InitializeSRV_T2D);
        ZIB_LOAD_FUNCTION_POINTER(InitializeSRV_T3D);
        ZIB_LOAD_FUNCTION_POINTER(InitializeUAV_B);
        ZIB_LOAD_FUNCTION_POINTER(InitializeUAV_T2D);
        ZIB_LOAD_FUNCTION_POINTER(InitializeUAV_T3D);
        ZIB_LOAD_FUNCTION_POINTER(InitializeCBV);
        ZIB_LOAD_FUNCTION_POINTER(StartRecording);
        ZIB_LOAD_FUNCTION_POINTER(StopRecording);
        ZIB_LOAD_FUNCTION_POINTER(UploadBuffer);
        ZIB_LOAD_FUNCTION_POINTER(UploadBufferSlow);
        ZIB_LOAD_FUNCTION_POINTER(UploadTextureSlow);
        ZIB_LOAD_FUNCTION_POINTER(GetBufferData);
        ZIB_LOAD_FUNCTION_POINTER(GetBufferDataImmediately);
        ZIB_LOAD_FUNCTION_POINTER(Dispatch);
        ZIB_LOAD_FUNCTION_POINTER(DrawIndexedInstanced);
        ZIB_LOAD_FUNCTION_POINTER(DrawInstanced);
        ZIB_LOAD_FUNCTION_POINTER(DispatchIndirect);
        ZIB_LOAD_FUNCTION_POINTER(DrawInstancedIndirect);
        ZIB_LOAD_FUNCTION_POINTER(DrawIndexedInstancedIndirect);
        ZIB_LOAD_FUNCTION_POINTER(CopyBufferRegion);
        ZIB_LOAD_FUNCTION_POINTER(ClearFormattedBuffer);
        ZIB_LOAD_FUNCTION_POINTER(SubmitQuery);
        ZIB_LOAD_FUNCTION_POINTER(ResolveQuery);
        ZIB_LOAD_FUNCTION_POINTER(GetTimestampFrequency);
        ZIB_LOAD_FUNCTION_POINTER(GetTimestampCalibration);
        ZIB_LOAD_FUNCTION_POINTER(GetCPUTimestamp);
        ZIB_LOAD_FUNCTION_POINTER(StartDebugRegion);
        ZIB_LOAD_FUNCTION_POINTER(EndDebugRegion);
        ZIB_LOAD_FUNCTION_POINTER(StartFrameCapture);
        ZIB_LOAD_FUNCTION_POINTER(StopFrameCapture);

#undef ZRHI_FNPFX
#pragma endregion RHIRuntime

#pragma region RHIFactory
#define ZRHI_FNPFX(name) Zibra_RHI_RHIFactory_##name

        ZIB_LOAD_FUNCTION_POINTER(SetGFXAPI);
        ZIB_LOAD_FUNCTION_POINTER(UseGFXCore);
        ZIB_LOAD_FUNCTION_POINTER(UseForcedAdapter);
        ZIB_LOAD_FUNCTION_POINTER(UseAutoSelectedAdapter);
        ZIB_LOAD_FUNCTION_POINTER(ForceSoftwareDevice);
        ZIB_LOAD_FUNCTION_POINTER(ForceEnableDebugLayer);
        ZIB_LOAD_FUNCTION_POINTER(Create);
        ZIB_LOAD_FUNCTION_POINTER(Release);

#undef ZRHI_FNPFX
#pragma endregion RHIFactory

#pragma region Functions
#define ZRHI_FNPFX(name) Zibra_RHI_##name

        ZIB_LOAD_FUNCTION_POINTER(CreateRHIFactory);

#undef ZRHI_FNPFX
#pragma endregion Functions

#undef ZIB_LOAD_FUNCTION_POINTER
#else
        // TODO cross-platform support
#endif
        return true;
    }

    bool LoadCEFunctions() noexcept
    {
#if ZIB_PLATFORM_WIN

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#define ZIB_LOAD_FUNCTION_POINTER(functionName)                                                                          \
    ZCE_FNPFX(functionName) =                                                                                            \
        reinterpret_cast<ZCE_PFN(ZCE_FNPFX(functionName))>(::GetProcAddress(g_LibraryHandle, STRINGIFY(ZCE_FNPFX(functionName))));  \
    if (ZCE_FNPFX(functionName) == nullptr)                                                                              \
    {                                                                                                                    \
        return false;                                                                                                    \
    }
#pragma region Licensing
#define ZCE_FNPFX(name) Zibra_CE_Licensing_##name

        ZIB_LOAD_FUNCTION_POINTER(CheckoutLicenseWithKey);
        ZIB_LOAD_FUNCTION_POINTER(CheckoutLicenseOffline);
        ZIB_LOAD_FUNCTION_POINTER(GetLicenseStatus);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseLicense);

#undef ZCE_FNPFX
#pragma endregion Licensing

#pragma region Compression

#pragma region FrameManager
#define ZCE_FNPFX(name) Zibra_CE_Compression_FrameManager_##name

        ZIB_LOAD_FUNCTION_POINTER(AddMetadata);
        ZIB_LOAD_FUNCTION_POINTER(Finish);

#undef ZCE_FNPFX
#pragma endregion FrameManager

#pragma region Compressor
#define ZCE_FNPFX(name) Zibra_CE_Compression_Compressor_##name

        ZIB_LOAD_FUNCTION_POINTER(Initialize);
        ZIB_LOAD_FUNCTION_POINTER(Release);
        ZIB_LOAD_FUNCTION_POINTER(StartSequence);
        ZIB_LOAD_FUNCTION_POINTER(AddPerSequenceMetadata);
        ZIB_LOAD_FUNCTION_POINTER(CompressFrame);
        ZIB_LOAD_FUNCTION_POINTER(FinishSequence);

#undef ZCE_FNPFX
#pragma endregion Compressor

#pragma region CompressorFactory
#define ZCE_FNPFX(name) Zibra_CE_Compression_CompressorFactory_##name

        ZIB_LOAD_FUNCTION_POINTER(UseRHI);
        ZIB_LOAD_FUNCTION_POINTER(SetQuality);
        ZIB_LOAD_FUNCTION_POINTER(OverrideChannelQuality);
        ZIB_LOAD_FUNCTION_POINTER(SetFrameMapping);
        ZIB_LOAD_FUNCTION_POINTER(Create);
        ZIB_LOAD_FUNCTION_POINTER(Release);

#undef ZCE_FNPFX
#pragma endregion CompressorFactory

#pragma region Funcs
#define ZCE_FNPFX(name) Zibra_CE_Compression_##name

        ZIB_LOAD_FUNCTION_POINTER(CreateCompressorFactory);

#undef ZCE_FNPFX
#pragma endregion Funcs

#pragma endregion Compression


#pragma region Decompression

#pragma region CompressedFrameContainer
#define ZCE_FNPFX(name) Zibra_CE_Decompression_CompressedFrameContainer_##name

        ZIB_LOAD_FUNCTION_POINTER(GetInfo);
        ZIB_LOAD_FUNCTION_POINTER(GetMetadataByKey);
        ZIB_LOAD_FUNCTION_POINTER(GetMetadataCount);
        ZIB_LOAD_FUNCTION_POINTER(GetMetadataByIndex);
        ZIB_LOAD_FUNCTION_POINTER(Release);

#undef ZCE_FNPFX
#pragma endregion CompressedFrameContainer

#pragma region FormatMapper
#define ZCE_FNPFX(name) Zibra_CE_Decompression_FormatMapper_##name

        ZIB_LOAD_FUNCTION_POINTER(FetchFrame);
        ZIB_LOAD_FUNCTION_POINTER(FetchFrameInfo);
        ZIB_LOAD_FUNCTION_POINTER(GetFrameRange);
        ZIB_LOAD_FUNCTION_POINTER(Release);

#undef ZCE_FNPFX
#pragma endregion FormatMapper

#pragma region Decompressor
#define ZCE_FNPFX(name) Zibra_CE_Decompression_Decompressor_##name

        ZIB_LOAD_FUNCTION_POINTER(Initialize);
        ZIB_LOAD_FUNCTION_POINTER(Release);
        ZIB_LOAD_FUNCTION_POINTER(RegisterResources);
        ZIB_LOAD_FUNCTION_POINTER(GetResourcesRequirements);
        ZIB_LOAD_FUNCTION_POINTER(GetFormatMapper);
        ZIB_LOAD_FUNCTION_POINTER(DecompressFrame);

#undef ZCE_FNPFX
#pragma endregion Decompressor

#pragma region DecompressorFactory
#define ZCE_FNPFX(name) Zibra_CE_Decompression_DecompressorFactory_##name

        ZIB_LOAD_FUNCTION_POINTER(UseDecoder);
        ZIB_LOAD_FUNCTION_POINTER(UseRHI);
        ZIB_LOAD_FUNCTION_POINTER(Create);
        ZIB_LOAD_FUNCTION_POINTER(Release);

#undef ZCE_FNPFX
#pragma endregion DecompressorFactory

#pragma region Functions
#define ZCE_FNPFX(name) Zibra_CE_Decompression_##name

        ZIB_LOAD_FUNCTION_POINTER(CreateDecompressorFactory);
        ZIB_LOAD_FUNCTION_POINTER(CreateDecoder);
        ZIB_LOAD_FUNCTION_POINTER(GetFileFormatVersion);
        ZIB_LOAD_FUNCTION_POINTER(ReleaseDecoder);

#undef ZCE_FNPFX
#pragma endregion Functions

#pragma endregion Decompression

#undef STRINGIFY
#undef STRINGIFY_HELPER
#undef ZRHI_PFN
#undef ZRHI_CONCAT_HELPER
#undef ZIB_LOAD_FUNCTION_POINTER
#else
        // TODO cross-platform support
#endif
        return true;
    }

    bool LoadLibraryByPath(const std::string& libraryPath) noexcept
    {
#if ZIB_PLATFORM_WIN
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

        if (!LoadCEFunctions() || !LoadRHIFunctions())
        {
            ::FreeLibrary(g_LibraryHandle);
            g_LibraryHandle = NULL;
            return false;
        }

        // g_LoadedLibraryVersion = BridgeGetVersion();
        // if (!IsLibrarySupported(g_LoadedLibraryVersion))
        // {
        //     ::FreeLibrary(g_LibraryHandle);
        //     g_LibraryHandle = NULL;
        //     return false;
        // }

        return true;
#else
        // TODO cross-platform support
#endif
    }

    void LoadLibrary() noexcept
    {
#if ZIB_PLATFORM_WIN
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
#else
        // TODO cross-platform support
#endif
    }

    void DownloadLibrary() noexcept
    {
        //const std::string downloadURL = GetDownloadURL();
        //const std::string libraryPath = GetLibraryPaths()[0];
        //bool success = NetworkRequest::DownloadFile(downloadURL, libraryPath);
        //if (!success)
        //{
        //    return;
        //}

        //LoadLibrary();
    }

    bool IsLibraryLoaded() noexcept
    {
        return g_IsLibraryLoaded;
    }

    bool IsPlatformSupported() noexcept
    {
#if ZIB_PLATFORM_WIN
        return true;
#else
        return false;
#endif
    }
} // namespace Zibra::LibraryUtils

#undef STRINGIFY
#undef STRINGIFY_HELPER
#undef ZCE_PFN
#undef ZCE_CONCAT_HELPER
#undef ZRHI_PFN
#undef ZRHI_CONCAT_HELPER
