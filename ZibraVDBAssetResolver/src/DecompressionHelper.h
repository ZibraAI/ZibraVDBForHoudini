#pragma once

#include "utils/DecompressorManager.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace Zibra::AssetResolver
{
    class DecompressionHelper
    {
    public:
        DecompressionHelper();
        ~DecompressionHelper();

        std::string DecompressZibraVDBFile(const std::string& zibraVDBPath, const std::string& tmpDir, int frame = 0);
        void CleanupUnneededDecompressedFiles(const std::string& currentCompressedFile, const std::string& currentDecompressedFile);
        static void CleanupAllDecompressedFilesStatic();
        static void CleanupAllDecompressorManagers();
        void AddDecompressedFile(const std::string& compressedFile, const std::string& decompressedFile);

    private:
        ::Zibra::Helpers::DecompressorManager* GetOrCreateDecompressorManager(const std::string& compressedFile);
        bool LoadSDKLib();

        static std::unordered_map<std::string, std::unordered_set<std::string>> ms_DecompressedFilesDict;
        static std::mutex ms_DecompressedFilesMutex;
        static std::unordered_map<std::string, std::unique_ptr<::Zibra::Helpers::DecompressorManager>> ms_DecompressorManagers;
        static std::mutex ms_DecompressorManagersMutex;
    };
} // namespace Zibra::AssetResolver

PXR_NAMESPACE_CLOSE_SCOPE