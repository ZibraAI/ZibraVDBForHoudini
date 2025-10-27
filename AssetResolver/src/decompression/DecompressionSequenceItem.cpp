#include "PrecompiledHeader.h"

#include "DecompressionSequenceItem.h"
#include "ZibraVDBAssetResolver.h"
#include "utils/Helpers.h"
#include "utils/MetadataHelper.h"

namespace Zibra::AssetResolver
{
    std::string InitializeTempDir()
    {
        std::string dir = TfStringCatPaths(TfGetenv("HOUDINI_TEMP_DIR"), ZIB_TMP_FILES_FOLDER_NAME);
        if (!TfPathExists(dir))
        {
            if (!TfMakeDirs(dir))
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("DecompressionItem::GetTempDir - Failed to create tmp directory: '%s'\n", dir.c_str());
            }
        }
        return dir;
    }

    int InitializeMaxCachedFrames()
    {
        const char* envValue = std::getenv("ZIB_MAX_CACHED_FILES_COUNT");
        int numOfCachedFrames;
        if (envValue && Helpers::TryParseInt(envValue, numOfCachedFrames))
        {
            if (numOfCachedFrames > 0)
            {
                return numOfCachedFrames;
            }
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZIB_MAX_CACHED_FILES_COUNT is set to %d which is invalid. Falling back to default value: %d\n", numOfCachedFrames,
                     ZIB_MAX_CACHED_FRAMES_DEFAULT);
        }
        return ZIB_MAX_CACHED_FRAMES_DEFAULT;
    }

    const std::string& DecompressionSequenceItem::GetTempDir()
    {
        static std::string tempDir = InitializeTempDir();
        return tempDir;
    }

    int DecompressionSequenceItem::GetMaxCachedFrames()
    {
        static int maxCachedFrames = InitializeMaxCachedFrames();
        return maxCachedFrames;
    }

    DecompressionSequenceItem::DecompressionSequenceItem(const std::string& zibraVDBPath)
    {
        m_Decompressor = CreateDecompressorManager(zibraVDBPath);
        if (!m_Decompressor)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("DecompressionItem::DecompressionItem - Failed to create decompressor manager for: '%s'\n", zibraVDBPath.c_str());
            throw std::runtime_error("Failed to create decompressor manager for: " + zibraVDBPath);
        }

        m_UUIDString = Helpers::FormatUUIDString(m_Decompressor->GetSequenceInfo().fileUUID);
        m_FrameRange = m_Decompressor->GetFrameRange();
    }

    DecompressionSequenceItem::~DecompressionSequenceItem()
    {
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("DecompressionItem::~DecompressionItem - Cleaning up %zu decompressed frames for UUID: %s\n",
                 m_DecompressedFrames.size(), m_UUIDString.c_str());

        for (int frame : m_DecompressedFrames)
        {
            std::string fileToDelete = ComposeDecompressedFrameFilePath(frame);
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("DecompressionItem::~DecompressionItem - Deleting: '%s'\n", fileToDelete.c_str());

            if (!TfPathExists(fileToDelete))
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("DecompressionItem::~DecompressionItem - File doesn't exist: '%s'\n", fileToDelete.c_str());
                continue;
            }

            if (!TfDeleteFile(fileToDelete))
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("DecompressionItem::~DecompressionItem - Failed to delete file: '%s'\n", fileToDelete.c_str());
            }
        }
    }

    std::string DecompressionSequenceItem::DecompressFrame(int frame)
    {
        if (frame < m_FrameRange.start || frame > m_FrameRange.end)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("DecompressionItem::DecompressFrame - Frame index %d is out of bounds [%d, %d]\n", frame, m_FrameRange.start,
                     m_FrameRange.end);
            return {};
        }

        std::string outputPath = ComposeDecompressedFrameFilePath(frame);
        if (std::find(m_DecompressedFrames.begin(), m_DecompressedFrames.end(), frame) != m_DecompressedFrames.end() ||
            TfPathExists(outputPath))
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("DecompressionItem::DecompressFrame - File already exists, skipping decompression: '%s'\n", outputPath.c_str());
            AddNewFrame(frame);
            return outputPath;
        }

        const auto frameContainer = m_Decompressor->FetchFrame(frame);
        if (!frameContainer)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("DecompressionItem::DecompressFrame - Failed to fetch frame %d\n", frame);
            return {};
        }

        auto gridShuffle = m_Decompressor->DeserializeGridShuffleInfo(frameContainer);
        openvdb::GridPtrVec vdbGrids;

        auto result = m_Decompressor->DecompressFrame(frameContainer, gridShuffle, &vdbGrids);
        if (result != CE::ZCE_SUCCESS || vdbGrids.empty())
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("DecompressionItem::DecompressFrame - Failed to decompress frame: %d\n", (int)result);
            m_Decompressor->ReleaseGridShuffleInfo(gridShuffle);
            frameContainer->Release();
            return {};
        }

        // Restore file-level and grid-level metadata using unified MetadataHelper
        openvdb::MetaMap fileMetadata;
        switch (Utils::MetadataHelper::ApplyDetailMetadata(fileMetadata, frameContainer))
        {
        case MetaAttributesLoadStatus::FATAL_ERROR_INVALID_METADATA:
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("DecompressionItem::DecompressFrame - Corrupted metadata for channel. Canceling attributes transfer.\n");
            break;
        case MetaAttributesLoadStatus::ERROR_PARTIALLY_INVALID_METADATA:
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("DecompressionItem::DecompressFrame - Partially corrupted metadata for channel. Skipping invalid attributes.\n");
            break;
        default:
            break;
        }

        // Apply grid metadata to each grid individually
        for (auto& grid : vdbGrids)
        {
            switch (Utils::MetadataHelper::ApplyGridMetadata(grid, frameContainer))
            {
            case MetaAttributesLoadStatus::FATAL_ERROR_INVALID_METADATA:
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("DecompressionItem::DecompressFrame - Corrupted metadata for grid '%s'. Canceling attributes transfer.\n", grid->getName().c_str());
                break;
            case MetaAttributesLoadStatus::ERROR_PARTIALLY_INVALID_METADATA:
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("DecompressionItem::DecompressFrame - Partially corrupted metadata for grid '%s'. Skipping invalid attributes.\n", grid->getName().c_str());
                break;
            default:
                break;
            }
        }

        openvdb::io::File file(outputPath);
        file.write(vdbGrids, fileMetadata);
        file.close();

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("DecompressionItem::DecompressFrame - Successfully decompressed %zu grids to: '%s'\n", vdbGrids.size(),
                 outputPath.c_str());

        m_Decompressor->ReleaseGridShuffleInfo(gridShuffle);
        frameContainer->Release();

        AddNewFrame(frame);

        return outputPath;
    }

    std::string DecompressionSequenceItem::ComposeDecompressedFrameFilePath(int frame) const
    {
        return TfStringCatPaths(GetTempDir(), m_UUIDString + "." + std::to_string(frame) + ".vdb");
    }

    void DecompressionSequenceItem::AddNewFrame(int frame)
    {
        // If frame already present in queue - we just move it to the end so it becomes most recent
        const auto frameIt = std::find(m_DecompressedFrames.begin(), m_DecompressedFrames.end(), frame);
        if (frameIt != m_DecompressedFrames.end())
        {
            m_DecompressedFrames.erase(frameIt);
        }
        m_DecompressedFrames.push_back(frame);

        while (m_DecompressedFrames.size() > GetMaxCachedFrames())
        {
            const int frameToDelete = m_DecompressedFrames.front();
            const std::string fileToDelete = ComposeDecompressedFrameFilePath(frameToDelete);
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("DecompressionItem::AddNewFrame - Deleting old file: '%s'\n", fileToDelete.c_str());
            if (!TfPathExists(fileToDelete))
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("DecompressionItem::AddNewFrame - Failed to delete old frame: '%s'. File does no exists.\n",
                         fileToDelete.c_str());
            }
            if (!TfDeleteFile(fileToDelete))
            {
                TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                    .Msg("DecompressionItem::AddNewFrame - Failed to delete old frame: '%s'.\n", fileToDelete.c_str());
            }
            m_DecompressedFrames.pop_front();
        }
    }

    std::unique_ptr<Helpers::DecompressorManager> DecompressionSequenceItem::CreateDecompressorManager(const std::string& compressedFile)
    {
        auto manager = std::make_unique<Helpers::DecompressorManager>();
        auto result = manager->Initialize();
        if (result != CE::ZCE_SUCCESS)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("DecompressionItem::CreateDecompressorManager - Failed to initialize DecompressorManager: %d\n", (int)result);
            return nullptr;
        }

        UT_String filename(compressedFile.c_str());
        result = manager->RegisterDecompressor(filename);
        if (result != CE::ZCE_SUCCESS)
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("DecompressionItem::CreateDecompressorManager - Failed to register decompressor: %d\n", (int)result);
            return nullptr;
        }

        return std::move(manager);
    }

} // namespace Zibra::AssetResolver
