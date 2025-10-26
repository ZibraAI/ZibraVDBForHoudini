#pragma once

#include "utils/DecompressorManager.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::AssetResolver
{
    class DecompressionSequenceItem
    {
    public:
        explicit DecompressionSequenceItem(const std::string& zibraVDBPath);
        ~DecompressionSequenceItem();

        DecompressionSequenceItem(const DecompressionSequenceItem&) = delete;
        DecompressionSequenceItem& operator=(const DecompressionSequenceItem&) = delete;

        std::string DecompressFrame(int frame);

    private:
        void AddNewFrame(int frame);

        static const std::string& GetTempDir();
        static int GetMaxCachedFrames();
        std::string ComposeDecompressedFrameFilePath(int frame) const;

        std::unique_ptr<Helpers::DecompressorManager> CreateDecompressorManager(const std::string& compressedFile);

    private:

        std::deque<int> m_DecompressedFrames;
        std::unique_ptr<Helpers::DecompressorManager> m_Decompressor;
        CE::Decompression::FrameRange m_FrameRange{};
        std::string m_UUIDString;
    };
} // namespace Zibra::AssetResolver
