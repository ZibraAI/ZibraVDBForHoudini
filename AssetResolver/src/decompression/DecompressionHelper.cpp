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

    std::string DecompressionHelper::DecompressZibraVDBFile(const std::string& zibraVDBPath, int frame)
    {
        std::lock_guard lock(m_DecompressionFilesMutex);

        if (!LoadSDKLib())
        {
            TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
                .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Failed to initialize ZibraVDB SDK\n");
            return {};
        }

        // License may or may not be required depending on .zibravdb file
        // So we need to trigger license check, but if it fails we proceed with decompression
        LicenseManager::GetInstance().CheckLicense(LicenseManager::Product::Decompression);

        DecompressionSequenceItem* item;
        const auto pathIt = m_PathToUUIDMap.find(zibraVDBPath);
        if (pathIt != m_PathToUUIDMap.end())
        {
            const std::string& uuid = pathIt->second;
            item = m_DecompressionFiles[uuid].get();
        }
        else
        {
            auto newDecompressionItem = std::make_unique<DecompressionSequenceItem>(zibraVDBPath);
            const std::string& uuid = newDecompressionItem->GetUUID();

            m_PathToUUIDMap.emplace(zibraVDBPath, uuid);

            const auto uuidIt = m_DecompressionFiles.find(uuid);
            if (uuidIt != m_DecompressionFiles.end())
            {
                item = uuidIt->second.get();
            }
            else
            {
                item = newDecompressionItem.get();
                m_DecompressionFiles.emplace(uuid, std::move(newDecompressionItem));
            }
        }

        return item->DecompressFrame(frame);
    }

    void DecompressionHelper::Cleanup()
    {
        std::lock_guard lock(m_DecompressionFilesMutex);

        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Starting cleanup of %zu compressed file "
                 "entries\n",
                 m_DecompressionFiles.size());

        m_PathToUUIDMap.clear();
        m_DecompressionFiles.clear();
        TF_DEBUG(ZIBRAVDBRESOLVER_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Cleanup completed\n");
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