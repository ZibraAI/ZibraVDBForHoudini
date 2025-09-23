#pragma once

#include "utils/DecompressorManager.h"

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

        std::string DecompressZibraVDBFile(const std::string& zibraVDBPath, const std::string& tmpDir, int frame = 0);
        void AddDecompressedFile(const std::string& compressedFile, const std::string& decompressedFile);
        void CleanupOldFiles(const std::string& currentCompressedFile, const std::string& currentDecompressedFile);
        void CleanupAllDecompressedFiles();
        void CleanupAllDecompressorManagers();

    private:
        DecompressionHelper() = default;
        ~DecompressionHelper() = default;

        bool LoadSDKLib();
        Helpers::DecompressorManager* GetOrCreateDecompressorManager(const std::string& compressedFile);
        size_t GetMaxCachedFramesPerFile();

    private:
        std::unordered_map<std::string, std::deque<std::string>> m_DecompressedFilesDict;
        std::mutex m_DecompressedFilesMutex;
        std::unordered_map<std::string, std::unique_ptr<Helpers::DecompressorManager>> m_DecompressorManagers;
        std::mutex m_DecompressorManagersMutex;
    };
} // namespace Zibra::AssetResolver