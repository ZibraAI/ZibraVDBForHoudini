#pragma once

#include <json.hpp>
#include <string>
#include <utility>
#include <vector>

#include "Zibra/CE/Addons/OpenVDBCommon.h"

class GU_Detail;
class GEO_PrimVDB;

namespace Zibra::Utils
{
    class MetadataHelper
    {
    public:
        static std::vector<std::pair<std::string, std::string>> DumpAttributes(
            const GU_Detail* gdp,
            const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata) noexcept;

        static void DumpVisualisationAttributes(
            std::vector<std::pair<std::string, std::string>>& attributes,
            const GEO_PrimVDB* vdbPrim) noexcept;

        static nlohmann::json DumpGridsShuffleInfo(
            const std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> gridDescs) noexcept;

        static void DumpDecodeMetadata(
            std::vector<std::pair<std::string, std::string>>& result,
            const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata);
    };
}