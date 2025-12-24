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

        static void ApplyGridMetadata(GU_Detail* gdp, GU_PrimVDB* grid, CE::Decompression::CompressedFrameContainer* frameContainer);
        static void ApplyGridMetadata(openvdb::GridBase::Ptr grid, CE::Decompression::CompressedFrameContainer* frameContainer);

        static void ApplyDetailMetadata(GU_Detail* gdp, CE::Decompression::CompressedFrameContainer* frameContainer);
        static void ApplyDetailMetadata(openvdb::MetaMap* target, CE::Decompression::CompressedFrameContainer* frameContainer);

    private:
        static void ApplyGridAttributeMetadata(GU_Detail* gdp, GU_PrimVDB* grid, CE::Decompression::CompressedFrameContainer* frameContainer);

        static void ApplyGridVisualizationMetadata(GU_PrimVDB* grid, CE::Decompression::CompressedFrameContainer* frameContainer);
    };
} // namespace Zibra::Utils