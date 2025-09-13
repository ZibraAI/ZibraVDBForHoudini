#include <csignal>
#include <string>

#include "DecompressionHelper.h"
#include "ZibraVDBAssetResolver.h"
#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
#include "utils/MetadataHelper.h"
#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace Zibra::AssetResolver
{

    std::unordered_map<std::string, std::unordered_set<std::string>> DecompressionHelper::g_DecompressedFilesDict;
    std::mutex DecompressionHelper::g_DecompressedFilesMutex;
    std::unordered_map<std::string, std::unique_ptr<::Zibra::Helpers::DecompressorManager>> DecompressionHelper::g_DecompressorManagers;
    std::mutex DecompressionHelper::g_DecompressorManagersMutex;

    DecompressionHelper::DecompressionHelper()
    {
    }

    DecompressionHelper::~DecompressionHelper()
    {
    }

    std::string DecompressionHelper::DecompressZibraVDBFile(const std::string& zibraVDBPath, const std::string& tmpDir, int frame)
    {
        std::string zibraVDBTmpDir = TfStringCatPaths(tmpDir, "zibravdb_houdini_usd_asset_resolver");

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Using temp directory: '%s'\n", zibraVDBTmpDir.c_str());

        if (!TfPathExists(zibraVDBTmpDir))
        {
            if (!TfMakeDirs(zibraVDBTmpDir))
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to create tmp directory: '%s'\n", zibraVDBTmpDir.c_str());
                return std::string();
            }
        }

        if (!LoadSDKLib())
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to initialize ZibraVDB SDK\n");
            return std::string();
        }

        ::Zibra::LicenseManager::GetInstance().CheckLicense(::Zibra::LicenseManager::Product::Decompression);

        auto* decompressor = GetOrCreateDecompressorManager(zibraVDBPath);
        if (!decompressor)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to get decompressor manager for: '%s'\n", zibraVDBPath.c_str());
            return std::string();
        }

        auto sequenceInfo = decompressor->GetSequenceInfo();
        std::string uniqueId = std::to_string(sequenceInfo.fileUUID[0]) + "_" + std::to_string(sequenceInfo.fileUUID[1]);
        std::string outputPath = TfStringCatPaths(zibraVDBTmpDir, uniqueId + "_" + std::to_string(frame) + ".vdb");

        if (TfPathExists(outputPath))
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - File already exists, skipping decompression: '%s'\n", outputPath.c_str());
            return outputPath;
        }

        auto frameRange = decompressor->GetFrameRange();
        if (frame < frameRange.start || frame > frameRange.end)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Frame index %d is out of bounds [%d, %d]\n",
                     frame, frameRange.start, frameRange.end);
            return std::string();
        }

        auto frameContainer = decompressor->FetchFrame(frame);
        if (!frameContainer)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to fetch frame %d\n", frame);
            return std::string();
        }

        auto gridShuffle = decompressor->DeserializeGridShuffleInfo(frameContainer);
        openvdb::GridPtrVec vdbGrids;

        auto result = decompressor->DecompressFrame(frameContainer, gridShuffle, &vdbGrids);
        if (result != ::Zibra::CE::ZCE_SUCCESS || vdbGrids.empty())
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to decompress frame: %d\n", (int)result);
            decompressor->ReleaseGridShuffleInfo(gridShuffle);
            return std::string();
        }

        // Restore file-level and grid-level metadata using unified MetadataHelper
        openvdb::MetaMap fileMetadata;
        ::Zibra::Utils::MetadataHelper::ApplyDetailMetadata(fileMetadata, frameContainer);

        // Apply grid metadata to each grid individually
        for (auto& grid : vdbGrids)
        {
            ::Zibra::Utils::MetadataHelper::ApplyGridMetadata(grid, frameContainer);
        }

        openvdb::io::File file(outputPath);
        file.write(vdbGrids, fileMetadata);
        file.close();

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Successfully decompressed %zu grids to: '%s'\n", vdbGrids.size(),
                 outputPath.c_str());

        decompressor->ReleaseGridShuffleInfo(gridShuffle);

        return outputPath;
    }

    void DecompressionHelper::AddDecompressedFile(const std::string& compressedFile, const std::string& decompressedFile)
    {
        std::lock_guard lock(g_DecompressedFilesMutex);
        g_DecompressedFilesDict[compressedFile].insert(decompressedFile);
    }

    void DecompressionHelper::CleanupUnneededDecompressedFiles(const std::string& currentCompressedFile, const std::string& currentDecompressedFile)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupUnneededDecompressedFiles - Cleaning up for compressed file: '%s', keeping: '%s'\n", currentCompressedFile.c_str(), currentDecompressedFile.c_str());

        {
            std::lock_guard lock(g_DecompressedFilesMutex);

            const auto it = g_DecompressedFilesDict.find(currentCompressedFile);
            if (it == g_DecompressedFilesDict.end())
            {
                return;
            }
            auto& fileSet = it->second;
            for (auto fileIt = fileSet.begin(); fileIt != fileSet.end();)
            {
                if (*fileIt != currentDecompressedFile)
                {
                    if (TfPathExists(*fileIt))
                    {
                        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupUnneededDecompressedFiles - Deleting: '%s'\n", fileIt->c_str());
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

    void DecompressionHelper::CleanupAllDecompressedFilesStatic()
    {
        std::lock_guard<std::mutex> lock(g_DecompressedFilesMutex);

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFilesStatic - Starting static cleanup of %zu compressed file entries\n", g_DecompressedFilesDict.size());

        for (auto& [compressedFile, decompressedFiles] : g_DecompressedFilesDict)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFilesStatic - Processing compressed file: '%s' with %zu decompressed files\n", compressedFile.c_str(), decompressedFiles.size());

            for (const std::string& decompressedFile : decompressedFiles)
            {
                if (TfPathExists(decompressedFile))
                {
                    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFilesStatic - Deleting: '%s'\n", decompressedFile.c_str());
                    TfDeleteFile(decompressedFile);
                }
                else
                {
                    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFilesStatic - File already gone: '%s'\n", decompressedFile.c_str());
                }
            }
        }
        g_DecompressedFilesDict.clear();

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFilesStatic - Static cleanup completed\n");
    }

    ::Zibra::Helpers::DecompressorManager* DecompressionHelper::GetOrCreateDecompressorManager(const std::string& compressedFile)
    {
        std::lock_guard<std::mutex> lock(g_DecompressorManagersMutex);

        // First, check if we can get UUID from an existing manager for this file path
        auto it = g_DecompressorManagers.find(compressedFile);
        if (it != g_DecompressorManagers.end())
        {
            return it->second.get();
        }

        auto manager = std::make_unique<::Zibra::Helpers::DecompressorManager>();
        auto result = manager->Initialize();
        if (result != ::Zibra::CE::ZCE_SUCCESS)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::GetOrCreateDecompressorManager - Failed to initialize DecompressorManager: %d\n", (int)result);
            return nullptr;
        }

        UT_String filename(compressedFile.c_str());
        result = manager->RegisterDecompressor(filename);
        if (result != ::Zibra::CE::ZCE_SUCCESS)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::GetOrCreateDecompressorManager - Failed to register decompressor: %d\n", (int)result);
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
                .Msg("ZibraVDBDecompressionManager::GetOrCreateDecompressorManager - Using existing manager for UUID: %s\n", uuidKey.c_str());
            return uuidIt->second.get();
        }

        auto* managerPtr = manager.get();
        g_DecompressorManagers.emplace(uuidKey, std::move(manager));

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::GetOrCreateDecompressorManager - Created new manager for file: '%s', UUID: %s\n", compressedFile.c_str(), uuidKey.c_str());

        return managerPtr;
    }

    void DecompressionHelper::CleanupAllDecompressorManagers()
    {
        std::lock_guard<std::mutex> lock(g_DecompressorManagersMutex);

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupAllDecompressorManagers - Cleaning up %zu managers\n", g_DecompressorManagers.size());

        for (auto& [compressedFile, manager] : g_DecompressorManagers)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupAllDecompressorManagers - Releasing manager for: '%s'\n", compressedFile.c_str());
            manager->Release();
        }

        g_DecompressorManagers.clear();

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupAllDecompressorManagers - Cleanup completed\n");
    }

    bool DecompressionHelper::LoadSDKLib()
    {
        if (::Zibra::LibraryUtils::IsSDKLibraryLoaded())
        {
            return true;
        }

        ::Zibra::LibraryUtils::LoadSDKLibrary();
        if (!::Zibra::LibraryUtils::IsSDKLibraryLoaded())
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::LoadSDKLib - Library not loaded, cannot initialize resolver\n");
            return false;
        }

        return true;
    }

} // namespace Zibra::AssetResolver

PXR_NAMESPACE_CLOSE_SCOPE