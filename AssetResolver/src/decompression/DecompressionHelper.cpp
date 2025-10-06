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
        const auto it = m_DecompressionFiles.find(zibraVDBPath);
        if (it != m_DecompressionFiles.end())
        {
            item = it->second.get();
        }
        else
        {
            auto newDecompressionItem = std::make_unique<DecompressionSequenceItem>(zibraVDBPath);
            item = newDecompressionItem.get();
            m_DecompressionFiles.emplace(zibraVDBPath, std::move(newDecompressionItem));
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