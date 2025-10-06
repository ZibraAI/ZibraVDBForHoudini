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

        static DecompressionHelper& GetInstance();
        std::string DecompressZibraVDBFile(const std::string& zibraVDBPath, int frame);
        void Cleanup();

    private:
        DecompressionHelper() = default;
        ~DecompressionHelper() = default;

        bool LoadSDKLib();

    private:
        std::unordered_map<std::string, std::unique_ptr<DecompressionSequenceItem>> m_DecompressionFiles;
        std::mutex m_DecompressionFilesMutex;
    };
} // namespace Zibra::AssetResolver