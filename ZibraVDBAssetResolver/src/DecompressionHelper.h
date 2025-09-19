#pragma once

#include "utils/DecompressorManager.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::AssetResolver
{
    class DecompressionHelper
    {
    public:
        static std::string DecompressZibraVDBFile(const std::string& zibraVDBPath, const std::string& tmpDir, int frame = 0);
        static void AddDecompressedFile(const std::string& compressedFile, const std::string& decompressedFile);
        static void CleanupOldFiles(const std::string& currentCompressedFile, const std::string& currentDecompressedFile);
        static void CleanupAllDecompressedFilesStatic();
        static void CleanupAllDecompressorManagers();

    private:
        static bool LoadSDKLib();
        static Helpers::DecompressorManager* GetOrCreateDecompressorManager(const std::string& compressedFile);

    private:
        static size_t ms_MaxCachedFramesPerFile;
        static std::unordered_map<std::string, std::deque<std::string>> ms_DecompressedFilesDict;
        static std::mutex ms_DecompressedFilesMutex;
        static std::unordered_map<std::string, std::unique_ptr<Helpers::DecompressorManager>> ms_DecompressorManagers;
        static std::mutex ms_DecompressorManagersMutex;
    };
} // namespace Zibra::AssetResolver