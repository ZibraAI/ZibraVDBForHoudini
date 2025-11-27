#pragma once

#include <GU/GU_PrimVDB.h>
#include <json.hpp>

#include "Types.h"
#include "Zibra/CE/Addons/OpenVDBCommon.h"
#include "Zibra/CE/Decompression.h"

namespace Zibra::Utils
{
    class MetadataHelper
    {
    public:
        static std::vector<std::pair<std::string, std::string>> DumpAttributes(
            const GU_Detail* gdp, const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata) noexcept;

        static void DumpVisualisationAttributes(std::vector<std::pair<std::string, std::string>>& attributes,
                                                const GEO_PrimVDB* vdbPrim) noexcept;

        static nlohmann::json DumpGridsShuffleInfo(std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> gridDescs) noexcept;

        static void DumpDecodeMetadata(std::vector<std::pair<std::string, std::string>>& result,
                                       const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata);

        static MetaAttributesLoadStatus ApplyGridMetadata(std::pair<GU_Detail*, GU_PrimVDB*>& grid, CE::Decompression::CompressedFrameContainer* frameContainer);
        static MetaAttributesLoadStatus ApplyGridMetadata(openvdb::GridBase::Ptr& grid, CE::Decompression::CompressedFrameContainer* frameContainer);

        static MetaAttributesLoadStatus ApplyDetailMetadata(GU_Detail* gdp, CE::Decompression::CompressedFrameContainer* frameContainer);
        static MetaAttributesLoadStatus ApplyDetailMetadata(openvdb::MetaMap& target, CE::Decompression::CompressedFrameContainer* frameContainer);

    private:
        static MetaAttributesLoadStatus ApplyGridAttributeMetadata(std::pair<GU_Detail*, GU_PrimVDB*>& grid, CE::Decompression::CompressedFrameContainer* frameContainer);
        static MetaAttributesLoadStatus ApplyGridAttributeMetadata(openvdb::GridBase::Ptr& grid, CE::Decompression::CompressedFrameContainer* frameContainer);

        static void ApplyGridVisualizationMetadata(GU_PrimVDB* grid, CE::Decompression::CompressedFrameContainer* frameContainer);
    };
} // namespace Zibra::Utils