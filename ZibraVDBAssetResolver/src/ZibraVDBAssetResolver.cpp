#include "ZibraVDBAssetResolver.h"

#include <csignal>
#include <pxr/usd/ar/defaultResolver.h>
#include <string>

#include <UT/UT_Exit.h>
#include <SYS/SYS_Hash.h>
#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/usd/ar/defineResolver.h"
#include "pxr/usd/ar/defineResolverContext.h"
#include "pxr/usd/ar/filesystemAsset.h"
#include "pxr/usd/ar/filesystemWritableAsset.h"
#include "pxr/usd/ar/notice.h"

#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfDebug)
{
    TF_DEBUG_ENVIRONMENT_SYMBOL(ZIBRAVDBRESOLVER_RESOLVER, "Print debug output during ZibraVDB path resolution");
    TF_DEBUG_ENVIRONMENT_SYMBOL(ZIBRAVDBRESOLVER_RESOLVER_CONTEXT, "Print debug output during ZibraVDB context creating and modification");
}

namespace {
    class ZibraVDBResolverContext
    {
    public:
        ZibraVDBResolverContext()
        {
            m_TmpDir = TfGetenv("HOUDINI_TEMP_DIR");
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER_CONTEXT).Msg(
                "ZibraVDBResolverContext: Using temp directory: %s\n", m_TmpDir.c_str());
        }
        
        ZibraVDBResolverContext(const std::string &tmpdir)
            : m_TmpDir(tmpdir.empty() ? TfGetenv("HOUDINI_TEMP_DIR") : tmpdir)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER_CONTEXT).Msg(
                "ZibraVDBResolverContext: Using temp directory: %s\n", m_TmpDir.c_str());
        }

        bool operator<(const ZibraVDBResolverContext& rhs) const
        { return m_TmpDir < rhs.m_TmpDir; }
        
        bool operator==(const ZibraVDBResolverContext& rhs) const
        { return m_TmpDir == rhs.m_TmpDir; }
        
        bool operator!=(const ZibraVDBResolverContext& rhs) const
        { return m_TmpDir != rhs.m_TmpDir; }

        const std::string &getTmpDir() const
        { return m_TmpDir; }

    private:
        std::string m_TmpDir;
    };

    size_t
    hash_value(const ZibraVDBResolverContext& context)
    {
        size_t hash = SYShash(context.getTmpDir());
        return hash;
    }
}

AR_DECLARE_RESOLVER_CONTEXT(ZibraVDBResolverContext);

AR_DEFINE_RESOLVER(ZibraVDBResolver, ArResolver);

std::unordered_map<std::string, std::unordered_set<std::string>> ZibraVDBResolver::g_DecompressedFilesDict;
std::mutex ZibraVDBResolver::g_DecompressedFilesMutex;
std::unordered_map<std::string, std::unique_ptr<Zibra::Helpers::DecompressorManager>> ZibraVDBResolver::g_DecompressorManagers;
std::mutex ZibraVDBResolver::g_DecompressorManagersMutex;

ZibraVDBResolver::ZibraVDBResolver()
{
    UT_Exit::addExitCallback([](void*){
        ZibraVDBResolver::CleanupAllDecompressorManagers();
        ZibraVDBResolver::CleanupAllDecompressedFilesStatic();
    }, nullptr);
}

ZibraVDBResolver::~ZibraVDBResolver() = default;

std::string ZibraVDBResolver::_CreateIdentifier(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    if (!IsZibraVDBPath(assetPath))
    {
        return ArDefaultResolver().CreateIdentifier(assetPath, anchorAssetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::CreateIdentifier - ZibraVDB URI detected: '%s'\n", assetPath.c_str());

    int frame = 0;
    std::string extractedPath = ParseZibraVDBURI(assetPath, frame);

    if (TfIsRelativePath(extractedPath) && anchorAssetPath)
    {
        std::string anchorDir = TfGetPathName(anchorAssetPath);
        std::string resolvedPath = TfStringCatPaths(anchorDir, extractedPath);
        std::string normalizedPath = TfNormPath(resolvedPath);

        std::string resolvedURI = normalizedPath;
        if (frame != 0)
        {
            resolvedURI += "?frame=" + std::to_string(frame);
        }

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::CreateIdentifier - Resolved ZibraVDB URI to: '%s'\n", resolvedURI.c_str());
        return resolvedURI;
    }

    return TfNormPath(assetPath);
}

std::string ZibraVDBResolver::_CreateIdentifierForNewAsset(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    if (!IsZibraVDBPath(assetPath))
    {
        return ArDefaultResolver().CreateIdentifierForNewAsset(assetPath, anchorAssetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
        .Msg("ZibraVDBResolver::CreateIdentifierForNewAsset('%s', '%s')\n", assetPath.c_str(), anchorAssetPath.GetPathString().c_str());
    return _CreateIdentifier(assetPath, anchorAssetPath);
}

ArResolvedPath ZibraVDBResolver::_Resolve(const std::string& assetPath) const
{
    if (!IsZibraVDBPath(assetPath))
    {
        return ArDefaultResolver().Resolve(assetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Detected ZibraVDB path: '%s'\n", assetPath.c_str());

    int frame = 0;
    std::string actualFilePath = ParseZibraVDBURI(assetPath, frame);

    if (!TfPathExists(actualFilePath))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - ZibraVDB file does not exist: '%s'\n", actualFilePath.c_str());
        return ArResolvedPath();
    }

    if (!CheckLicenseAndLoadLib())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Failed to initialize ZibraVDB SDK (library loading or license check failed)\n");
        return ArResolvedPath();
    }

    static const std::string theDefaultTempDir(TfGetenv("HOUDINI_TEMP_DIR"));
    std::string tmpDir = theDefaultTempDir;
    
    const ZibraVDBResolverContext* ctx = _GetCurrentContextObject<ZibraVDBResolverContext>();
    if (ctx && !ctx->getTmpDir().empty())
        tmpDir = ctx->getTmpDir();

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
        .Msg("ZibraVDBResolver::_Resolve - Using temp directory: '%s'\n", tmpDir.c_str());

    std::string decompressedPath = DecompressZibraVDBFile(actualFilePath, tmpDir, frame);
    if (!decompressedPath.empty())
    {
        CleanupUnneededDecompressedFiles(actualFilePath, decompressedPath);
        AddDecompressedFile(actualFilePath, decompressedPath);

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Successfully decompressed to: '%s'\n", decompressedPath.c_str());
        return ArResolvedPath(decompressedPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Decompression failed for file '%s' (check temp directory permissions, decompressor manager initialization, frame availability, or compression format)\n", actualFilePath.c_str());
    return ArResolvedPath();
}

ArResolvedPath ZibraVDBResolver::_ResolveForNewAsset(const std::string& assetPath) const
{
    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_ResolveForNewAsset('%s')\n", assetPath.c_str());
    return _Resolve(assetPath);
}

std::shared_ptr<ArAsset> ZibraVDBResolver::_OpenAsset(const ArResolvedPath& resolvedPath) const
{
    return ArFilesystemAsset::Open(resolvedPath);
}

std::shared_ptr<ArWritableAsset> ZibraVDBResolver::_OpenAssetForWrite(const ArResolvedPath& resolvedPath, WriteMode writeMode) const
{
    return ArFilesystemWritableAsset::Create(resolvedPath, writeMode);
}

ArResolverContext ZibraVDBResolver::_CreateContextFromString(const std::string& contextStr) const
{
    return ArResolverContext(ZibraVDBResolverContext(contextStr));
}

bool ZibraVDBResolver::_IsContextDependentPath(const std::string& path) const
{
    return IsZibraVDBPath(path);
}

bool ZibraVDBResolver::IsZibraVDBPath(const std::string& path) const
{
    return path.find(ZIB_VDB_EXT) != std::string::npos;
}

std::string ZibraVDBResolver::ParseZibraVDBURI(const std::string& uri, int& frame) const
{
    frame = 0;

    size_t questionMarkPos = uri.find('?');
    if (questionMarkPos == std::string::npos)
    {
        return uri;
    }
    
    if (uri.find('?', questionMarkPos + 1) != std::string::npos)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::ParseZibraVDBURI - Multiple '?' characters found in URI: '%s'\n", uri.c_str());
        return uri.substr(0, questionMarkPos);
    }

    std::string path = uri.substr(0, questionMarkPos);
    std::string queryString = uri.substr(questionMarkPos + 1);

    std::string frameParam;
    bool foundFrame = false;
    
    size_t start = 0;
    size_t ampPos;
    do
    {
        ampPos = queryString.find('&', start);
        std::string param = (ampPos == std::string::npos) ? queryString.substr(start) : queryString.substr(start, ampPos - start);
        
        if (param.substr(0, 6) == "frame=")
        {
            std::string frameValue = param.substr(6);
            if (foundFrame)
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::ParseZibraVDBURI - Multiple 'frame' parameters found, using last one: '%s'\n", frameValue.c_str());
            }
            frameParam = frameValue;
            foundFrame = true;
        }
        
        start = ampPos + 1;
    } while (ampPos != std::string::npos);

    if (foundFrame && !frameParam.empty())
    {
        frame = std::stoi(frameParam);
    }
    return path;
}

std::string ZibraVDBResolver::DecompressZibraVDBFile(const std::string& zibraVDBPath, const std::string& tmpDir, int frame) const
{
    std::string zibraVDBTmpDir = TfStringCatPaths(tmpDir, "zibravdb_houdini_usd_asset_resolver");
    
    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
        .Msg("ZibraVDBResolver::_DecompressZibraVDBFile - Using temp directory: '%s'\n", zibraVDBTmpDir.c_str());

    if (!TfPathExists(zibraVDBTmpDir))
    {
        if (!TfMakeDirs(zibraVDBTmpDir))
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBResolver::_DecompressZibraVDBFile - Failed to create tmp directory: '%s'\n", zibraVDBTmpDir.c_str());
            return std::string();
        }
    }

    auto* decompressor = GetOrCreateDecompressorManager(zibraVDBPath);
    if (!decompressor)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_DecompressZibraVDBFile - Failed to get decompressor manager for: '%s'\n", zibraVDBPath.c_str());
        return std::string();
    }

    auto sequenceInfo = decompressor->GetSequenceInfo();
    std::string uniqueId = std::to_string(sequenceInfo.fileUUID[0]) + "_" + std::to_string(sequenceInfo.fileUUID[1]);
    std::string outputPath = TfStringCatPaths(zibraVDBTmpDir, uniqueId + "_" + std::to_string(frame) + ".vdb");

    if (TfPathExists(outputPath))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_DecompressZibraVDBFile - File already exists, skipping decompression: '%s'\n", outputPath.c_str());
        return outputPath;
    }

    auto frameRange = decompressor->GetFrameRange();
    if (frame < frameRange.start || frame > frameRange.end)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_DecompressZibraVDBFile - Frame index %d is out of bounds [%d, %d]\n", 
                 frame, frameRange.start, frameRange.end);
        return std::string();
    }

    auto frameContainer = decompressor->FetchFrame(frame);
    if (!frameContainer)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_DecompressZibraVDBFile - Failed to fetch frame %d\n", frame);
        return std::string();
    }

    auto gridShuffle = decompressor->DeserializeGridShuffleInfo(frameContainer);
    openvdb::GridPtrVec vdbGrids;

    auto result = decompressor->DecompressFrame(frameContainer, gridShuffle, &vdbGrids);
    if (result != Zibra::CE::ZCE_SUCCESS || vdbGrids.empty())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_DecompressZibraVDBFile - Failed to decompress frame: %d\n", (int)result);
        decompressor->ReleaseGridShuffleInfo(gridShuffle);
        return std::string();
    }

    // Restore file-level and grid-level metadata
    openvdb::MetaMap fileMetadata;
    RestoreFileMetadataToVDB(frameContainer, fileMetadata);
    RestoreGridMetadataToVDB(frameContainer, vdbGrids);
    
    openvdb::io::File file(outputPath);
    file.write(vdbGrids, fileMetadata);
    file.close();

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
        .Msg("ZibraVDBResolver::_DecompressZibraVDBFile - Successfully decompressed %zu grids to: '%s'\n", vdbGrids.size(),
             outputPath.c_str());

    decompressor->ReleaseGridShuffleInfo(gridShuffle);

    return outputPath;
}

bool ZibraVDBResolver::CheckLicenseAndLoadLib() const
{
    if (Zibra::LibraryUtils::IsZibSDKLoaded())
    {
        Zibra::LicenseManager::GetInstance().CheckLicense(Zibra::LicenseManager::Product::Decompression);
        return true;
    }

    Zibra::LibraryUtils::LoadZibSDKLibrary();
    if (!Zibra::LibraryUtils::IsZibSDKLoaded())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::CheckLicenseAndLoadLib - Library not loaded, cannot initialize resolver\n");
        return false;
    }

    Zibra::LicenseManager::GetInstance().CheckLicense(Zibra::LicenseManager::Product::Decompression);
    return true;
}

void ZibraVDBResolver::AddDecompressedFile(const std::string& compressedFile, const std::string& decompressedFile) const
{
    std::lock_guard<std::mutex> lock(g_DecompressedFilesMutex);
    g_DecompressedFilesDict[compressedFile].insert(decompressedFile);
}

void ZibraVDBResolver::CleanupUnneededDecompressedFiles(const std::string& currentCompressedFile, const std::string& currentDecompressedFile) const
{
    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupUnneededDecompressedFiles - Cleaning up for compressed file: '%s', keeping: '%s'\n", currentCompressedFile.c_str(), currentDecompressedFile.c_str());

    {
        std::lock_guard<std::mutex> lock(g_DecompressedFilesMutex);

        auto it = g_DecompressedFilesDict.find(currentCompressedFile);
        if (it != g_DecompressedFilesDict.end())
        {
            auto& fileSet = it->second;
            for (auto fileIt = fileSet.begin(); fileIt != fileSet.end();)
            {
                if (*fileIt != currentDecompressedFile)
                {
                    if (TfPathExists(*fileIt))
                    {
                        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupUnneededDecompressedFiles - Deleting: '%s'\n", fileIt->c_str());
                        TfDeleteFile(*fileIt);
                    }
                    fileIt = fileSet.erase(fileIt);
                }
                else
                {
                    ++fileIt;
                }
            }
        }
    }
}

void ZibraVDBResolver::CleanupAllDecompressedFilesStatic()
{
    std::lock_guard<std::mutex> lock(g_DecompressedFilesMutex);

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressedFilesStatic - Starting static cleanup of %zu compressed file entries\n", g_DecompressedFilesDict.size());

    for (auto& [compressedFile, decompressedFiles] : g_DecompressedFilesDict)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressedFilesStatic - Processing compressed file: '%s' with %zu decompressed files\n", compressedFile.c_str(), decompressedFiles.size());

        for (const std::string& decompressedFile : decompressedFiles)
        {
            if (TfPathExists(decompressedFile))
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressedFilesStatic - Deleting: '%s'\n", decompressedFile.c_str());
                TfDeleteFile(decompressedFile);
            }
            else
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressedFilesStatic - File already gone: '%s'\n", decompressedFile.c_str());
            }
        }
    }
    g_DecompressedFilesDict.clear();

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressedFilesStatic - Static cleanup completed\n");
}

Zibra::Helpers::DecompressorManager* ZibraVDBResolver::GetOrCreateDecompressorManager(const std::string& compressedFile) const
{
    std::lock_guard<std::mutex> lock(g_DecompressorManagersMutex);

    // First, check if we can get UUID from an existing manager for this file path
    auto it = g_DecompressorManagers.find(compressedFile);
    if (it != g_DecompressorManagers.end())
    {
        return it->second.get();
    }

    auto manager = std::make_unique<Zibra::Helpers::DecompressorManager>();
    auto result = manager->Initialize();
    if (result != Zibra::CE::ZCE_SUCCESS)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_GetOrCreateDecompressorManager - Failed to initialize DecompressorManager: %d\n", (int)result);
        return nullptr;
    }

    UT_String filename(compressedFile.c_str());
    result = manager->RegisterDecompressor(filename);
    if (result != Zibra::CE::ZCE_SUCCESS)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_GetOrCreateDecompressorManager - Failed to register decompressor: %d\n", (int)result);
        return nullptr;
    }

    // Get UUID from the registered decompressor to use as key
    auto sequenceInfo = manager->GetSequenceInfo();
    std::string uuidKey = std::to_string(sequenceInfo.fileUUID[0]) + "_" + std::to_string(sequenceInfo.fileUUID[1]);

    // Check if a manager with this UUID already exists
    auto uuidIt = g_DecompressorManagers.find(uuidKey);
    if (uuidIt != g_DecompressorManagers.end())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_GetOrCreateDecompressorManager - Using existing manager for UUID: %s\n", uuidKey.c_str());
        return uuidIt->second.get();
    }

    auto* managerPtr = manager.get();
    g_DecompressorManagers.emplace(uuidKey, std::move(manager));

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
        .Msg("ZibraVDBResolver::_GetOrCreateDecompressorManager - Created new manager for file: '%s', UUID: %s\n", compressedFile.c_str(), uuidKey.c_str());

    return managerPtr;
}

void ZibraVDBResolver::CleanupAllDecompressorManagers()
{
    std::lock_guard<std::mutex> lock(g_DecompressorManagersMutex);

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressorManagers - Cleaning up %zu managers\n", g_DecompressorManagers.size());

    for (auto& [compressedFile, manager] : g_DecompressorManagers)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressorManagers - Releasing manager for: '%s'\n", compressedFile.c_str());
        manager->Release();
    }

    g_DecompressorManagers.clear();

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressorManagers - Cleanup completed\n");
}

void ZibraVDBResolver::RestoreFileMetadataToVDB(Zibra::CE::Decompression::CompressedFrameContainer* frameContainer, openvdb::MetaMap& fileMetadata) const
{
    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_RestoreFileMetadataToVDB - Starting file metadata restoration\n");

    const char* fileMetadataEntry = frameContainer->GetMetadataByKey("houdiniFileMetadata");
    
    if (fileMetadataEntry)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_RestoreFileMetadataToVDB - Found file metadata\n");
        
        try
        {
            auto fileMeta = nlohmann::json::parse(fileMetadataEntry);
            
            for (auto& [metaName, metaValue] : fileMeta.items())
            {
                if (metaValue.is_string())
                {
                    fileMetadata.insertMeta(metaName, openvdb::StringMetadata(metaValue.get<std::string>()));
                    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("  Restored file metadata: %s = %s\n", metaName.c_str(), metaValue.get<std::string>().c_str());
                }
                else if (metaValue.is_number_float())
                {
                    fileMetadata.insertMeta(metaName, openvdb::FloatMetadata(metaValue.get<float>()));
                    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("  Restored file metadata: %s = %f\n", metaName.c_str(), metaValue.get<float>());
                }
                else if (metaValue.is_number_integer())
                {
                    fileMetadata.insertMeta(metaName, openvdb::Int32Metadata(metaValue.get<int>()));
                    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("  Restored file metadata: %s = %d\n", metaName.c_str(), metaValue.get<int>());
                }
                else if (metaValue.is_array())
                {
                    // Handle arrays - convert to string for VDB metadata
                    fileMetadata.insertMeta(metaName, openvdb::StringMetadata(metaValue.dump()));
                    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("  Restored file metadata (array): %s = %s\n", metaName.c_str(), metaValue.dump().c_str());
                }
                else
                {
                    fileMetadata.insertMeta(metaName, openvdb::StringMetadata(metaValue.dump()));
                    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("  Restored file metadata: %s = %s\n", metaName.c_str(), metaValue.dump().c_str());
                }
            }
        }
        catch (const nlohmann::json::exception& e)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_RestoreFileMetadataToVDB - Failed to parse file metadata: %s\n", e.what());
        }
    }
    else
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_RestoreFileMetadataToVDB - No file metadata found\n");
    }
    
    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_RestoreFileMetadataToVDB - File metadata restoration completed\n");
}

void ZibraVDBResolver::RestoreGridMetadataToVDB(Zibra::CE::Decompression::CompressedFrameContainer* frameContainer, openvdb::GridPtrVec& vdbGrids) const
{
    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_RestoreGridMetadataToVDB - Starting grid metadata restoration\n");

    for (auto& grid : vdbGrids)
    {
        const std::string gridName = grid->getName();
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_RestoreGridMetadataToVDB - Processing grid: %s\n", gridName.c_str());

        // Restore OpenVDB grid metadata
        std::string gridMetadataName = "houdiniOpenVDBGridMetadata_" + gridName;
        const char* gridMetadataEntry = frameContainer->GetMetadataByKey(gridMetadataName.c_str());
        
        if (gridMetadataEntry)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_RestoreGridMetadataToVDB - Found OpenVDB metadata for grid %s\n", gridName.c_str());
            
            try
            {
                auto gridMeta = nlohmann::json::parse(gridMetadataEntry);
                
                for (auto& [metaName, metaValue] : gridMeta.items())
                {
                    if (metaValue.is_string())
                    {
                        grid->insertMeta(metaName, openvdb::StringMetadata(metaValue.get<std::string>()));
                        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("  Restored grid metadata: %s = %s\n", metaName.c_str(), metaValue.get<std::string>().c_str());
                    }
                    else if (metaValue.is_number_float())
                    {
                        grid->insertMeta(metaName, openvdb::FloatMetadata(metaValue.get<float>()));
                        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("  Restored grid metadata: %s = %f\n", metaName.c_str(), metaValue.get<float>());
                    }
                    else if (metaValue.is_number_integer())
                    {
                        grid->insertMeta(metaName, openvdb::Int32Metadata(metaValue.get<int>()));
                        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("  Restored grid metadata: %s = %d\n", metaName.c_str(), metaValue.get<int>());
                    }
                    else
                    {
                        grid->insertMeta(metaName, openvdb::StringMetadata(metaValue.dump()));
                        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("  Restored grid metadata: %s = %s\n", metaName.c_str(), metaValue.dump().c_str());
                    }
                }
            }
            catch (const nlohmann::json::exception& e)
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_RestoreGridMetadataToVDB - Failed to parse OpenVDB metadata for grid %s: %s\n", gridName.c_str(), e.what());
            }
        }
        else
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_RestoreGridMetadataToVDB - No OpenVDB metadata found for grid %s\n", gridName.c_str());
        }
    }
    
    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_RestoreGridMetadataToVDB - Grid metadata restoration completed\n");
}

PXR_NAMESPACE_CLOSE_SCOPE