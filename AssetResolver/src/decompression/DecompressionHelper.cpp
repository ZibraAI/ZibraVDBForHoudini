#include "PrecompiledHeader.h"

#include "DecompressionHelper.h"

#include "ZibraVDBAssetResolver.h"
#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
#include "utils/Helpers.h"
#include "utils/MetadataHelper.h"

namespace Zibra::AssetResolver
{
    std::unique_ptr<DecompressionHelper> DecompressionHelper::ms_Instance = nullptr;

    DecompressionHelper& DecompressionHelper::GetInstance()
    {
        if (!ms_Instance)
        {
            ms_Instance = std::unique_ptr<DecompressionHelper>(new DecompressionHelper());
        }
        return *ms_Instance;
    }

    bool DecompressionHelper::HasInstance()
    {
        return ms_Instance != nullptr;
    }

    std::string DecompressionHelper::DecompressZibraVDBFile(const std::string& zibraVDBPath, int frame)
    {
        std::lock_guard lock(m_DecompressionFilesMutex);

        if (!LibraryUtils::TryLoadLibrary())
        {
            TF_DEBUG(ZIBRAVDB_RESOLVER)
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
            const auto fileIt = m_DecompressionFiles.find(uuid);
            if (fileIt != m_DecompressionFiles.end())
            {
                item = fileIt->second.get();
            }
            else
            {
                // UUID exists in m_PathToUUIDMap but not in m_DecompressionFiles - this should never happen
                TF_DEBUG(ZIBRAVDB_RESOLVER)
                    .Msg("ZibraVDBDecompressionManager::DecompressZibraVDBFile - Inconsistent state: UUID found in path map but not in files map for path '%s'\n",
                         zibraVDBPath.c_str());
                return {};
            }
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

        TF_DEBUG(ZIBRAVDB_RESOLVER)
            .Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Starting cleanup of %zu compressed file "
                 "entries\n",
                 m_DecompressionFiles.size());

        m_PathToUUIDMap.clear();
        m_DecompressionFiles.clear();
        TF_DEBUG(ZIBRAVDB_RESOLVER).Msg("ZibraVDBDecompressionManager::CleanupAllDecompressedFiles - Cleanup completed\n");
    }

} // namespace Zibra::AssetResolver