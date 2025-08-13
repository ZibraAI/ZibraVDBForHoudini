#include "resolver.h"

#include <csignal>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <pxr/usd/ar/defaultResolver.h>
#include <string>

#include "pxr/base/arch/fileSystem.h"
#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/usd/ar/defineResolver.h"
#include "pxr/usd/ar/filesystemAsset.h"
#include "pxr/usd/ar/filesystemWritableAsset.h"
#include "pxr/usd/ar/notice.h"

#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"

PXR_NAMESPACE_OPEN_SCOPE

AR_DEFINE_RESOLVER(ZibraVDBResolver, ArResolver);

std::unordered_map<std::string, std::unordered_set<std::string>> ZibraVDBResolver::s_globalDecompressedFilesDict;
std::mutex ZibraVDBResolver::s_globalDecompressedFilesMutex;
std::unordered_map<std::string, std::unique_ptr<Zibra::Decompression::DecompressorManager>> ZibraVDBResolver::s_decompressorManagers;
std::mutex ZibraVDBResolver::s_decompressorManagersMutex;

ZibraVDBResolver::ZibraVDBResolver()
{
    static bool cleanupRegistered = false;
    if (!cleanupRegistered) {
        std::atexit([]() {
            std::cout << "[ZibraVDBResolver] Process exit cleanup called" << std::endl;
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver - Process exit cleanup called\n");
            _CleanupAllDecompressorManagers();
            _CleanupAllDecompressedFilesStatic();
        });
        
        cleanupRegistered = true;
    }
}

ZibraVDBResolver::~ZibraVDBResolver() = default;

std::string ZibraVDBResolver::_CreateIdentifier(const std::string& assetPath, const ArResolvedPath& anchorAssetPath) const
{
    if (!_IsZibraVDBPath(assetPath))
    {
        return ArDefaultResolver().CreateIdentifier(assetPath, anchorAssetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::CreateIdentifier - ZibraVDB URI detected: '%s'\n", assetPath.c_str());

    int frame = 0;
    std::string extractedPath = _ParseZibraVDBURI(assetPath, frame);

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
    if (!_IsZibraVDBPath(assetPath))
    {
        return ArDefaultResolver().CreateIdentifierForNewAsset(assetPath, anchorAssetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
        .Msg("ZibraVDBResolver::CreateIdentifierForNewAsset('%s', '%s')\n", assetPath.c_str(), anchorAssetPath.GetPathString().c_str());
    return _CreateIdentifier(assetPath, anchorAssetPath);
}

ArResolvedPath ZibraVDBResolver::_Resolve(const std::string& assetPath) const
{
    if (!_IsZibraVDBPath(assetPath))
    {
        return ArDefaultResolver().Resolve(assetPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Detected ZibraVDB path: '%s'\n", assetPath.c_str());

    int frame = 0;
    std::string actualFilePath = _ParseZibraVDBURI(assetPath, frame);

    if (!TfPathExists(actualFilePath))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - ZibraVDB file does not exist: '%s'\n", actualFilePath.c_str());
        return ArResolvedPath();
    }

    if (!CheckLicenseAndLoadLib())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_Resolve - Failed to load ZibraVDBSDK lib.");
        return ArResolvedPath();
    }

    std::string decompressedPath = _DecompressZibraVDBFile(actualFilePath, frame);
    if (!decompressedPath.empty())
    {
        _CleanupUnneededDecompressedFiles(actualFilePath, decompressedPath);
        _AddDecompressedFile(actualFilePath, decompressedPath);

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Successfully decompressed to: '%s'\n", decompressedPath.c_str());
        return ArResolvedPath(decompressedPath);
    }

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_Resolve - Failed to decompress ZibraVDB file: '%s'\n", actualFilePath.c_str());
    return ArResolvedPath();
}

ArResolvedPath ZibraVDBResolver::_ResolveForNewAsset(const std::string& assetPath) const
{
    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_ResolveForNewAsset('%s')\n", assetPath.c_str());
    return _Resolve(assetPath);
}

std::shared_ptr<ArAsset> ZibraVDBResolver::_OpenAsset(const ArResolvedPath& resolvedPath) const
{
    std::cout << "[ZibraVDBResolver] _OpenAsset called with path: " << resolvedPath.GetPathString() << std::endl;
    return ArFilesystemAsset::Open(resolvedPath);
}

std::shared_ptr<ArWritableAsset> ZibraVDBResolver::_OpenAssetForWrite(const ArResolvedPath& resolvedPath, WriteMode writeMode) const
{
    return ArFilesystemWritableAsset::Create(resolvedPath, writeMode);
}

ArResolverContext ZibraVDBResolver::_CreateDefaultContext() const
{
    return {};
}

ArResolverContext ZibraVDBResolver::_CreateDefaultContextForAsset(const std::string& assetPath) const
{
    return _CreateDefaultContext();
}

bool ZibraVDBResolver::_IsContextDependentPath(const std::string& path) const
{
    return _IsZibraVDBPath(path);
}

bool ZibraVDBResolver::_IsZibraVDBPath(const std::string& path) const
{
    return path.find(ZIBRAVDB_EXTENSION) != std::string::npos;
}

std::string ZibraVDBResolver::_ParseZibraVDBURI(const std::string& uri, int& frame) const
{
    frame = 0;

    std::string path = uri;

    size_t paramPos = path.find("?frame=");
    if (paramPos != std::string::npos)
    {
        std::string frameStr = path.substr(paramPos + 7);
        frame = std::stoi(frameStr);
        path = path.substr(0, paramPos);
    }
    return path;
}

// bool ZibraVDBResolver::CheckLicenseAndLoadLib() const
// {
//     std::cout << "[ZibraVDBResolver] CheckLicenseAndLoadLib called" << std::endl;
//     std::cout << "[ZibraVDBResolver] Checking license..." << std::endl;
//     // License may or may not be required depending on .zibravdb file
//     // So we need to trigger license check, but if it fails we proceed with decompression
//
//     if (Zibra::LibraryUtils::IsLibraryLoaded())
//     {
//         std::cout << "[ZibraVDBResolver] ZibraVDB library already loaded" << std::endl;
//         return true;
//     }
//
//     std::cout << "[ZibraVDBResolver] Loading ZibraVDB library..." << std::endl;
//     Zibra::LibraryUtils::LoadZibSDKLibrary();
//     if (!Zibra::LibraryUtils::IsLibraryLoaded())
//     {
//         std::cout << "[ZibraVDBResolver] ERROR: Library not loaded, cannot initialize resolver" << std::endl;
//         TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::ZibraVDBResolver - Library not loaded, cannot initialize resolver\n");
//         return false;
//     }
//     std::cout << "[ZibraVDBResolver] ZibraVDB library loaded successfully" << std::endl;
//     return true;
// }

std::string ZibraVDBResolver::_DecompressZibraVDBFile(const std::string& zibraVDBPath, int frame) const
{
    std::string outputDir = TfGetPathName(zibraVDBPath);
    std::string tmpDir = TfStringCatPaths(outputDir, "tmp");
    std::string baseName = TfGetBaseName(zibraVDBPath);

    if (!TfPathExists(tmpDir))
    {
        if (!TfMakeDirs(tmpDir))
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBResolver::_DecompressZibraVDBFile - Failed to create tmp directory: '%s'\n", tmpDir.c_str());
            return std::string();
        }
    }

    const size_t extLength = strlen(ZIBRAVDB_EXTENSION);
    if (baseName.size() > extLength && baseName.substr(baseName.size() - extLength) == ZIBRAVDB_EXTENSION)
    {
        baseName = baseName.substr(0, baseName.size() - extLength);
    }
    std::string outputPath = TfStringCatPaths(tmpDir, baseName + "_frame" + std::to_string(frame) + ".vdb");

    if (TfPathExists(outputPath))
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_DecompressZibraVDBFile - File already exists, skipping decompression: '%s'\n", outputPath.c_str());
        return outputPath;
    }

    auto* decompressor = _GetOrCreateDecompressorManager(zibraVDBPath);
    if (!decompressor)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBResolver::_DecompressZibraVDBFile - Failed to get decompressor manager for: '%s'\n", zibraVDBPath.c_str());
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

    openvdb::io::File file(outputPath);
    file.write(vdbGrids);
    file.close();

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
        .Msg("ZibraVDBResolver::_DecompressZibraVDBFile - Successfully decompressed %zu grids to: '%s'\n", vdbGrids.size(),
             outputPath.c_str());

    decompressor->ReleaseGridShuffleInfo(gridShuffle);

    return outputPath;
}

bool ZibraVDBResolver::CheckLicenseAndLoadLib() const
{
    // License may or may not be required depending on .zibravdb file
    // So we need to trigger license check, but if it fails we proceed with decompression
    Zibra::LicenseManager::GetInstance().CheckLicense(Zibra::LicenseManager::Product::Decompression);

    if (Zibra::LibraryUtils::IsLibraryLoaded())
    {
        return true;
    }

    Zibra::LibraryUtils::LoadZibSDKLibrary();
    if (!Zibra::LibraryUtils::IsLibraryLoaded())
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::ZibraVDBResolver - Library not loaded, cannot initialize resolver\n");
        return false;
    }
    return true;
}

void ZibraVDBResolver::_AddDecompressedFile(const std::string& compressedFile, const std::string& decompressedFile) const
{
    std::lock_guard<std::mutex> lock(s_globalDecompressedFilesMutex);
    s_globalDecompressedFilesDict[compressedFile].insert(decompressedFile);
}

void ZibraVDBResolver::_CleanupUnneededDecompressedFiles(const std::string& currentCompressedFile, const std::string& currentDecompressedFile) const
{
    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupUnneededDecompressedFiles - Cleaning up for compressed file: '%s', keeping: '%s'\n", currentCompressedFile.c_str(), currentDecompressedFile.c_str());

    {
        std::lock_guard<std::mutex> lock(s_globalDecompressedFilesMutex);

        auto it = s_globalDecompressedFilesDict.find(currentCompressedFile);
        if (it != s_globalDecompressedFilesDict.end())
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

void ZibraVDBResolver::_CleanupAllDecompressedFilesStatic()
{
    std::lock_guard<std::mutex> lock(s_globalDecompressedFilesMutex);

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressedFilesStatic - Starting static cleanup of %zu compressed file entries\n", s_globalDecompressedFilesDict.size());

    for (auto& [compressedFile, decompressedFiles] : s_globalDecompressedFilesDict)
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
    s_globalDecompressedFilesDict.clear();

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressedFilesStatic - Static cleanup completed\n");
}

Zibra::Decompression::DecompressorManager* ZibraVDBResolver::_GetOrCreateDecompressorManager(const std::string& compressedFile) const
{
    std::lock_guard<std::mutex> lock(s_decompressorManagersMutex);

    auto it = s_decompressorManagers.find(compressedFile);
    if (it != s_decompressorManagers.end())
    {
        return it->second.get();
    }

    auto manager = std::make_unique<Zibra::Decompression::DecompressorManager>();
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

    auto* managerPtr = manager.get();
    s_decompressorManagers[compressedFile] = std::move(manager);

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
        .Msg("ZibraVDBResolver::_GetOrCreateDecompressorManager - Created new manager for: '%s'\n", compressedFile.c_str());

    return managerPtr;
}

void ZibraVDBResolver::_CleanupAllDecompressorManagers()
{
    std::lock_guard<std::mutex> lock(s_decompressorManagersMutex);

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressorManagers - Cleaning up %zu managers\n", s_decompressorManagers.size());

    for (auto& [compressedFile, manager] : s_decompressorManagers)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressorManagers - Releasing manager for: '%s'\n", compressedFile.c_str());
        manager->Release();
    }

    s_decompressorManagers.clear();

    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBResolver::_CleanupAllDecompressorManagers - Cleanup completed\n");
}

PXR_NAMESPACE_CLOSE_SCOPE