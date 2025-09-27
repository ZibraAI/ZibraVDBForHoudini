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

    std::string DecompressionHelper::DecompressZibraVDBFile(const std::string& zibraVDBPath, const std::string& zibraVDBTmpDir, int frame)
    {
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
        LicenseManager::GetInstance().CheckLicense(LicenseManager::Product::Decompression);

        std::lock_guard lock(m_DecompressionFilesMutex);

        DecompressionItem* item;
        if (const auto it = m_DecompressionFiles.find(zibraVDBPath); it != m_DecompressionFiles.end())
        {
            item = it->second.get();
        }
        else
        {
            item = AddDecompressionItem(zibraVDBPath, zibraVDBTmpDir);
            if (!item)
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to create decompression item for: '%s'\n",
                         zibraVDBPath.c_str());
                return {};
            }
        }

        if (frame < item->frameRange.start || frame > item->frameRange.end)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Frame index %d is out of bounds [%d, %d]\n", frame,
                     item->frameRange.start, item->frameRange.end);
            return {};
        }

        std::string outputPath = ComposeDecompressedFrameFileName(zibraVDBTmpDir, item->uuid, frame);
        if (std::find(item->decompressedFrames.begin(), item->decompressedFrames.end(), frame) != item->decompressedFrames.end() ||
            TfPathExists(outputPath))
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - File already exists, skipping decompression: '%s'\n",
                     outputPath.c_str());
            AddDecompressedFrame(item, frame);
            CleanupOldFrames(item, frame);
            return outputPath;
        }

        auto frameContainer = item->decompressor->FetchFrame(frame);
        if (!frameContainer)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to fetch frame %d\n", frame);
            return {};
        }

        auto gridShuffle = item->decompressor->DeserializeGridShuffleInfo(frameContainer);
        openvdb::GridPtrVec vdbGrids;

        auto result = item->decompressor->DecompressFrame(frameContainer, gridShuffle, &vdbGrids);
        if (result != CE::ZCE_SUCCESS || vdbGrids.empty())
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to decompress frame: %d\n", (int)result);
            item->decompressor->ReleaseGridShuffleInfo(gridShuffle);
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

        item->decompressor->ReleaseGridShuffleInfo(gridShuffle);

        AddDecompressedFrame(item, frame);
        CleanupOldFrames(item, frame);

        return outputPath;
    }

    std::string DecompressionHelper::ComposeDecompressedFrameFileName(const std::string& dir, const std::string& name, int frameToDelete)
    {
        return TfStringCatPaths(dir, name + "." + std::to_string(frameToDelete) + ".vdb");
    }

    void DecompressionHelper::AddDecompressedFrame(DecompressionItem* item, int frame)
    {
        auto& fileQueue = item->decompressedFrames;
        // If frame already present in queue - we just move it to the end so it becomes most recent
        if (const auto frameIt = std::find(fileQueue.begin(), fileQueue.end(), frame); frameIt != fileQueue.end())
        {
            fileQueue.erase(frameIt);
        }
        fileQueue.push_back(frame);
    }

    void DecompressionHelper::CleanupOldFrames(DecompressionItem* item, int currentFrame)
    {
        auto& fileQueue = item->decompressedFrames;

        while (fileQueue.size() > GetMaxCachedFramesPerFile())
        {
            int frameToDelete = fileQueue.front();
            if (frameToDelete == currentFrame)
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("ZibraVDBDecompressionManager::CleanupOldFiles - Current frame requested to delete. Skipping '%d'\n", frameToDelete);
            }
            const std::string fileToDelete = ComposeDecompressedFrameFileName(item->tmpDir, item->uuid, frameToDelete);
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::CleanupOldFiles - Deleting old file: '%s'\n", fileToDelete.c_str());
            if (!TfPathExists(fileToDelete))
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("ZibraVDBDecompressionManager::CleanupOldFiles - Failed to delete old frame: '%s'. File does no exists.\n",
                         fileToDelete.c_str());
            }
            if (!TfDeleteFile(fileToDelete))
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("ZibraVDBDecompressionManager::CleanupOldFiles - Failed to delete old frame: '%s'.\n", fileToDelete.c_str());
            }
            fileQueue.pop_front();
        }
    }


    std::unique_ptr<Helpers::DecompressorManager> DecompressionHelper::CreateDecompressorManager(const std::string& compressedFile)
    {
        auto manager = std::make_unique<Helpers::DecompressorManager>();
        auto result = manager->Initialize();
        if (result != CE::ZCE_SUCCESS)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::CreateDecompressorManager - Failed to initialize DecompressorManager: %d\n",
                     (int)result);
            return nullptr;
        }

        UT_String filename(compressedFile.c_str());
        result = manager->RegisterDecompressor(filename);
        if (result != CE::ZCE_SUCCESS)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::CreateDecompressorManager - Failed to register decompressor: %d\n", (int)result);
            return nullptr;
        }

        return std::move(manager);
    }

    DecompressionHelper::DecompressionItem* DecompressionHelper::AddDecompressionItem(const std::string& zibraVDBPath, const std::string& tmpDir)
    {
        auto item = std::make_unique<DecompressionItem>();
        item->decompressor = std::move(CreateDecompressorManager(zibraVDBPath));
        if (!item->decompressor)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::AddDecompressionItem - Failed to create decompressor manager for: '%s'\n",
                     zibraVDBPath.c_str());
            return nullptr;
        }
        
        item->tmpDir = tmpDir;
        item->uuid = Helpers::FormatUUID(item->decompressor->GetSequenceInfo().fileUUID);
        item->frameRange = item->decompressor->GetFrameRange();

        auto* itemPtr = item.get();
        m_DecompressionFiles.emplace(zibraVDBPath, std::move(item));
        
        return itemPtr;
    }

    void DecompressionHelper::Cleanup()
    {
        std::lock_guard lock(m_DecompressionFilesMutex);

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Starting cleanup of %zu compressed file "
                 "entries\n",
                 m_DecompressionFiles.size());

        for (auto& [compressedFile, item] : m_DecompressionFiles)
        {
            auto& fileQueue = item->decompressedFrames;
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Processing compressed file: '%s' with %zu "
                     "decompressed files\n",
                     compressedFile.c_str(), fileQueue.size());

            for (int decompressedFrame : fileQueue)
            {
                auto decompressedFile = ComposeDecompressedFrameFileName(item->tmpDir, item->uuid, decompressedFrame);
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Deleting: '%s'\n", decompressedFile.c_str());
                if (!TfPathExists(decompressedFile))
                {
                    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                        .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - File already gone: '%s'\n",
                             decompressedFile.c_str());
                }
                if (!TfDeleteFile(decompressedFile))
                {
                    TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                        .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Failed to delete file: '%s'\n",
                             decompressedFile.c_str());
                }
            }
            fileQueue.clear();
        }

        m_DecompressionFiles.clear();
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Cleanup completed\n");
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