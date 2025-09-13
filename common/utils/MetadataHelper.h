#pragma once

#include <json.hpp>
#include <string>
#include <utility>
#include <vector>

#include "Zibra/CE/Addons/OpenVDBCommon.h"
#include "Zibra/CE/Decompression.h"
#include <openvdb/openvdb.h>

class GU_Detail;
class GEO_PrimVDB;
class GU_PrimVDB;

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

        // Metadata restoration functions
        static void ApplyGridMetadata(
            GU_Detail* gdp, GU_PrimVDB* vdbPrim,
            CE::Decompression::CompressedFrameContainer* frameContainer);

        static void ApplyGridAttributeMetadata(
            GU_Detail* gdp, GU_PrimVDB* vdbPrim,
            CE::Decompression::CompressedFrameContainer* frameContainer);

        static void ApplyGridVisualizationMetadata(
            GU_PrimVDB* vdbPrim,
            CE::Decompression::CompressedFrameContainer* frameContainer);

        static void ApplyDetailMetadata(
            GU_Detail* gdp,
            CE::Decompression::CompressedFrameContainer* frameContainer);

        // OpenVDB/AssetResolver overloads (same function names, different parameters)
        static void ApplyGridMetadata(openvdb::GridBase::Ptr& vdbGrid, CE::Decompression::CompressedFrameContainer* frameContainer);

        static void ApplyGridAttributeMetadata(
            openvdb::GridBase::Ptr& vdbGrid,
            CE::Decompression::CompressedFrameContainer* frameContainer);

        static void ApplyGridVisualizationMetadata(
            openvdb::GridBase::Ptr& vdbGrid,
            CE::Decompression::CompressedFrameContainer* frameContainer);

        static void ApplyDetailMetadata(
            openvdb::MetaMap& fileMetadata,
            CE::Decompression::CompressedFrameContainer* frameContainer);
    };
}