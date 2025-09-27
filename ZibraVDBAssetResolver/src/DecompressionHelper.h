#pragma once

#include "utils/DecompressorManager.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::AssetResolver
{
    class DecompressionHelper
    {
    private:
        struct DecompressionItem
        {
            std::deque<int> decompressedFrames;
            std::unique_ptr<Helpers::DecompressorManager> decompressor;
            CE::Decompression::FrameRange frameRange;
            std::string tmpDir;
            std::string uuid;
        };

    public:
        DecompressionHelper(const DecompressionHelper&) = delete;
        DecompressionHelper& operator=(const DecompressionHelper&) = delete;
        DecompressionHelper(DecompressionHelper&&) = delete;
        DecompressionHelper& operator=(DecompressionHelper&&) = delete;

        static DecompressionHelper& GetInstance();
        std::string DecompressZibraVDBFile(const std::string& zibraVDBPath, const std::string& tmpDir, int frame = 0);
        void Cleanup();

    private:
        DecompressionHelper() = default;
        ~DecompressionHelper() = default;

        bool LoadSDKLib();
        std::unique_ptr<Helpers::DecompressorManager> CreateDecompressorManager(const std::string& compressedFile);
        DecompressionItem* AddDecompressionItem(const std::string& zibraVDBPath, const std::string& tmpDir);
        void AddDecompressedFrame(DecompressionItem* item, int frame);
        void CleanupOldFrames(DecompressionItem* item, int recentFrame);
        size_t GetMaxCachedFramesPerFile();
        std::string ComposeDecompressedFrameFileName(const std::string& dir, const std::string& name, int frame);

    private:
        std::unordered_map<std::string, std::unique_ptr<DecompressionItem>> m_DecompressionFiles;
        std::mutex m_DecompressionFilesMutex;
    };
} // namespace Zibra::AssetResolver