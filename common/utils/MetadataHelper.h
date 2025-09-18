#pragma once

#include <json.hpp>

#include "Zibra/CE/Addons/OpenVDBCommon.h"
#include "Zibra/CE/Decompression.h"
#include <GU/GU_PrimVDB.h>

namespace Zibra::Utils
{
    template <typename GridType>
    struct MetadataPolicy;

    template <>
    struct MetadataPolicy<std::pair<GU_Detail*, GU_PrimVDB*>>
    {
        static std::string GetGridName(const std::pair<GU_Detail*, GU_PrimVDB*>& context)
        {
            return context.second->getGridName();
        }
        static void ApplyVisualizationMetadata(const std::pair<GU_Detail*, GU_PrimVDB*>& context, const std::string& visModeMetadata,
                                               const std::string& visIsoMetadata, const std::string& visDensityMetadata,
                                               const std::string& visLodMetadata);
        static void ApplyAttributeMetadata(const std::pair<GU_Detail*, GU_PrimVDB*>& context, const nlohmann::json& primAttribMeta);
    };

    template <>
    struct MetadataPolicy<openvdb::GridBase::Ptr>
    {
        static std::string GetGridName(const openvdb::GridBase::Ptr& vdbGrid)
        {
            return vdbGrid->getName();
        }
        static void ApplyVisualizationMetadata(openvdb::GridBase::Ptr& vdbGrid, const std::string& visModeMetadata,
                                               const std::string& visIsoMetadata, const std::string& visDensityMetadata,
                                               const std::string& visLodMetadata);
        static void ApplyAttributeMetadata(openvdb::GridBase::Ptr& vdbGrid, const nlohmann::json& primAttribMeta);
    };

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

        template <typename GridType>
        static void ApplyGridMetadata(GridType& grid, CE::Decompression::CompressedFrameContainer* frameContainer);

        template <typename TargetType>
        static void ApplyDetailMetadata(TargetType& target, CE::Decompression::CompressedFrameContainer* frameContainer);

    private:
        template <typename GridType>
        static void ApplyGridAttributeMetadata(GridType& grid, CE::Decompression::CompressedFrameContainer* frameContainer);

        template <typename GridType>
        static void ApplyGridVisualizationMetadata(GridType& grid, CE::Decompression::CompressedFrameContainer* frameContainer);
    };
} // namespace Zibra::Utils