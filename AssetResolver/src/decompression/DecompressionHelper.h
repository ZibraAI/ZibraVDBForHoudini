#pragma once

#include "DecompressionSequenceItem.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::AssetResolver
{
    class DecompressionHelper
    {
    public:
        DecompressionHelper(const DecompressionHelper&) = delete;
        DecompressionHelper& operator=(const DecompressionHelper&) = delete;
        DecompressionHelper(DecompressionHelper&&) = delete;
        DecompressionHelper& operator=(DecompressionHelper&&) = delete;
        ~DecompressionHelper();

        static DecompressionHelper& GetInstance();
        static void DeleteInstance();
        std::string DecompressZibraVDBFile(const std::string& zibraVDBPath, int frame);

    private:
        DecompressionHelper() = default;

    private:
        static DecompressionHelper* ms_Instance;

        std::map<std::string, std::string> m_PathToUUIDMap;
        std::map<std::string, std::unique_ptr<DecompressionSequenceItem>> m_DecompressionFiles;
        std::mutex m_DecompressionFilesMutex;
    };
} // namespace Zibra::AssetResolver