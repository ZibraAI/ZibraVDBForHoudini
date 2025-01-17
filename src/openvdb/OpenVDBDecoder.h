#pragma once

#include "bridge/CompressionEngine.h"

namespace Zibra::OpenVDBSupport
{
    struct DecodeMetadata
    {
        int32_t offsetX = 0;
        int32_t offsetY = 0;
        int32_t offsetZ = 0;
    };

    class OpenVDBDecoder
    {
    public:
        explicit OpenVDBDecoder(openvdb::GridBase::ConstPtr* grids, const char* const* orderedChannelNames,
                                size_t orderedChannelNamesCount) noexcept;
        explicit OpenVDBDecoder(openvdb::io::Stream& stream, const char* const* orderedChannelNames,
                                size_t orderedChannelNamesCount) noexcept;
        CompressionEngine::ZCE_SparseFrameData DecodeFrame(DecodeMetadata& decodeMetadata) noexcept;
        static void FreeFrame(CompressionEngine::ZCE_SparseFrameData& frame);

    private:
        std::map<std::string, openvdb::FloatGrid::ConstPtr> m_Grids = {};
        std::map<std::string, uint8_t> m_ChannelMapping = {};
        std::vector<std::string> m_OrderedChannelNames = {};
    };
} // namespace Zibra::OpenVDBSupport
