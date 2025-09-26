#include "PrecompiledHeader.h"

#include "DecompressionHelper.h"

#include "ZibraVDBAssetResolver.h"
#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
#include "utils/Helpers.h"
#include "utils/MetadataHelper.h"

namespace Zibra::AssetResolver
{
    DecompressionHelper& DecompressionHelper::GetInstance()
    {
        static DecompressionHelper instance;
        return instance;
    }

    size_t DecompressionHelper::GetMaxCachedFramesPerFile()
    {
        static size_t maxCachedFrames = []() -> size_t {
            const char* envValue = std::getenv("ZIB_MAX_CACHED_FILES_COUNT");
            if (envValue)
            {
                try
                {
                    return std::stoull(envValue);
                }
                catch (const std::exception& e)
                {
                    // Fall back to default on parse error
                }
            }
            return ZIB_MAX_CACHED_FRAMES_DEFAULT;
        }();

        return maxCachedFrames;
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
                    .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to create tmp directory: '%s'\n",
                         zibraVDBTmpDir.c_str());
                return {};
            }
        }

        if (!LoadSDKLib())
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to initialize ZibraVDB SDK\n");
            return {};
        }

        // License may or may not be required depending on .zibravdb file
        // So we need to trigger license check, but if it fails we proceed with decompression
        LicenseManager::GetInstance().CheckLicense(::Zibra::LicenseManager::Product::Decompression);

        auto* decompressor = GetOrCreateDecompressorManager(zibraVDBPath);
        if (!decompressor)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to get decompressor manager for: '%s'\n",
                     zibraVDBPath.c_str());
            return {};
        }

        auto sequenceInfo = decompressor->GetSequenceInfo();
        std::string uniqueId = Helpers::FormatUUID(sequenceInfo.fileUUID);
        std::string outputPath = TfStringCatPaths(zibraVDBTmpDir, uniqueId + "." + std::to_string(frame) + ".vdb");

        if (TfPathExists(outputPath))
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - File already exists, skipping decompression: '%s'\n",
                     outputPath.c_str());
            return outputPath;
        }

        auto frameRange = decompressor->GetFrameRange();
        if (frame < frameRange.start || frame > frameRange.end)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Frame index %d is out of bounds [%d, %d]\n", frame,
                     frameRange.start, frameRange.end);
            return {};
        }

        auto frameContainer = decompressor->FetchFrame(frame);
        if (!frameContainer)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to fetch frame %d\n", frame);
            return {};
        }

        auto gridShuffle = decompressor->DeserializeGridShuffleInfo(frameContainer);
        openvdb::GridPtrVec vdbGrids;

        auto result = decompressor->DecompressFrame(frameContainer, gridShuffle, &vdbGrids);
        if (result != CE::ZCE_SUCCESS || vdbGrids.empty())
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to decompress frame: %d\n", (int)result);
            decompressor->ReleaseGridShuffleInfo(gridShuffle);
            return {};
        }

        // Restore file-level and grid-level metadata using unified MetadataHelper
        openvdb::MetaMap fileMetadata;
        Utils::MetadataHelper::ApplyDetailMetadata(fileMetadata, frameContainer);

        // Apply grid metadata to each grid individually
        for (auto& grid : vdbGrids)
        {
            Utils::MetadataHelper::ApplyGridMetadata(grid, frameContainer);
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
        std::lock_guard lock(m_DecompressedFilesMutex);
        auto& fileQueue = m_DecompressedFilesDict[compressedFile];

        // If file present in queue - we just move it to the end so it become most recent one
        auto it = std::find(fileQueue.begin(), fileQueue.end(), decompressedFile);
        if (it != fileQueue.end())
        {
            fileQueue.erase(it);
        }
        fileQueue.push_back(decompressedFile);
    }

    void DecompressionHelper::CleanupOldFiles(const std::string& currentCompressedFile, const std::string& currentDecompressedFile)
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::CleanupOldFiles - Cleanup for compressed file: '%s', keeping most recent %zu frames\n",
                 currentCompressedFile.c_str(), GetMaxCachedFramesPerFile());

        std::lock_guard lock(m_DecompressedFilesMutex);

        const auto it = m_DecompressedFilesDict.find(currentCompressedFile);
        if (it == m_DecompressedFilesDict.end())
        {
            return;
        }

        auto& fileQueue = it->second;
        auto fileIt = std::find(fileQueue.begin(), fileQueue.end(), currentDecompressedFile);
        if (fileIt != fileQueue.end())
        {
            if (fileIt != fileQueue.end() - 1)
            {
                fileQueue.erase(fileIt);
                fileQueue.push_back(currentDecompressedFile);
            }
        }

        while (fileQueue.size() > GetMaxCachedFramesPerFile())
        {
            const std::string& fileToDelete = fileQueue.front();
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::CleanupOldFiles - Deleting old file: '%s'\n", fileToDelete.c_str());

            if (TfPathExists(fileToDelete))
            {
                TfDeleteFile(fileToDelete);
            }

            fileQueue.pop_front();
        }
    }

    void DecompressionHelper::CleanupAllDecompressedFiles()
    {
        std::lock_guard<std::mutex> lock(m_DecompressedFilesMutex);

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Starting cleanup of %zu compressed file "
                 "entries\n",
                 m_DecompressedFilesDict.size());

        for (auto& [compressedFile, fileQueue] : m_DecompressedFilesDict)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Processing compressed file: '%s' with %zu "
                     "decompressed files\n",
                     compressedFile.c_str(), fileQueue.size());

            for (const std::string& decompressedFile : fileQueue)
            {
                if (TfPathExists(decompressedFile))
                {
                    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                        .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Deleting: '%s'\n",
                             decompressedFile.c_str());
                    TfDeleteFile(decompressedFile);
                }
                else
                {
                    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                        .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - File already gone: '%s'\n",
                             decompressedFile.c_str());
                }
            }
        }
        m_DecompressedFilesDict.clear();

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Cleanup completed\n");
    }

    Helpers::DecompressorManager* DecompressionHelper::GetOrCreateDecompressorManager(const std::string& compressedFile)
    {
        std::lock_guard<std::mutex> lock(m_DecompressorManagersMutex);

        // Check if we can get UUID from an existing manager for this file path
        auto it = m_DecompressorManagers.find(compressedFile);
        if (it != m_DecompressorManagers.end())
        {
            return it->second.get();
        }

        auto manager = std::make_unique<Helpers::DecompressorManager>();
        auto result = manager->Initialize();
        if (result != CE::ZCE_SUCCESS)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::GetOrCreateDecompressorManager - Failed to initialize DecompressorManager: %d\n",
                     (int)result);
            return nullptr;
        }

        UT_String filename(compressedFile.c_str());
        result = manager->RegisterDecompressor(filename);
        if (result != CE::ZCE_SUCCESS)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::GetOrCreateDecompressorManager - Failed to register decompressor: %d\n", (int)result);
            return nullptr;
        }

        // Get UUID from the registered decompressor to use as key
        auto sequenceInfo = manager->GetSequenceInfo();
        std::string uuidKey = Helpers::FormatUUID(sequenceInfo.fileUUID);

        // Check if a manager with this UUID already exists
        auto uuidIt = m_DecompressorManagers.find(uuidKey);
        if (uuidIt != m_DecompressorManagers.end())
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::GetOrCreateDecompressorManager - Using existing manager for UUID: %s\n",
                     uuidKey.c_str());
            return uuidIt->second.get();
        }

        auto* managerPtr = manager.get();
        m_DecompressorManagers.emplace(uuidKey, std::move(manager));

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::GetOrCreateDecompressorManager - Created new manager for file: '%s', UUID: %s\n",
                 compressedFile.c_str(), uuidKey.c_str());

        return managerPtr;
    }

    void DecompressionHelper::CleanupAllDecompressorManagers()
    {
        std::lock_guard<std::mutex> lock(m_DecompressorManagersMutex);

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressorManagers - Cleaning up %zu managers\n",
                 m_DecompressorManagers.size());

        for (auto& [compressedFile, manager] : m_DecompressorManagers)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressorManagers - Releasing manager for: '%s'\n",
                     compressedFile.c_str());
            manager->Release();
        }

        m_DecompressorManagers.clear();

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupAllDecompressorManagers - Cleanup completed\n");
    }

    bool DecompressionHelper::LoadSDKLib()
    {
        if (LibraryUtils::IsSDKLibraryLoaded())
        {
            return true;
        }

        LibraryUtils::LoadSDKLibrary();
        if (!LibraryUtils::IsSDKLibraryLoaded())
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::LoadSDKLib - Library not loaded, cannot initialize resolver\n");
            return false;
        }

        return true;
    }

} // namespace Zibra::AssetResolver