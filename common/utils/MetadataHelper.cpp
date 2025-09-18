#include "PrecompiledHeader.h"

#include "MetadataHelper.h"

#include "GAAttributesDump.h"

namespace Zibra::Utils
{
    using namespace std::literals;

    void MetadataPolicy<std::pair<GU_Detail*, GU_PrimVDB*>>::ApplyVisualizationMetadata(const std::pair<GU_Detail*, GU_PrimVDB*>& context, const std::string& visModeMetadata,
                                                                                       const std::string& visIsoMetadata, const std::string& visDensityMetadata,
                                                                                       const std::string& visLodMetadata)
    {
        GEO_VolumeOptions visOptions{};
        visOptions.myMode = static_cast<GEO_VolumeVis>(std::stoi(visModeMetadata));
        visOptions.myIso = std::stof(visIsoMetadata);
        visOptions.myDensity = std::stof(visDensityMetadata);
        visOptions.myLod = static_cast<GEO_VolumeVisLod>(std::stoi(visLodMetadata));
        context.second->setVisOptions(visOptions);
    }

    void MetadataPolicy<openvdb::GridBase::Ptr>::ApplyVisualizationMetadata(openvdb::GridBase::Ptr& vdbGrid, const std::string& visModeMetadata,
                                                                          const std::string& visIsoMetadata, const std::string& visDensityMetadata,
                                                                          const std::string& visLodMetadata)
    {
        vdbGrid->insertMeta("houdini_vis_mode", openvdb::StringMetadata(visModeMetadata));
        vdbGrid->insertMeta("houdini_vis_iso", openvdb::StringMetadata(visIsoMetadata));
        vdbGrid->insertMeta("houdini_vis_density", openvdb::StringMetadata(visDensityMetadata));
        vdbGrid->insertMeta("houdini_vis_lod", openvdb::StringMetadata(visLodMetadata));
    }

    void MetadataPolicy<std::pair<GU_Detail*, GU_PrimVDB*>>::ApplyAttributeMetadata(const std::pair<GU_Detail*, GU_PrimVDB*>& context, const nlohmann::json& primAttribMeta)
    {
        auto target = std::make_tuple(context.first, GA_ATTRIB_PRIMITIVE, context.second->getMapOffset());
        LoadEntityAttributesFromMeta(target, primAttribMeta);
    }

    void MetadataPolicy<openvdb::GridBase::Ptr>::ApplyAttributeMetadata(openvdb::GridBase::Ptr& vdbGrid, const nlohmann::json& primAttribMeta)
    {
        LoadEntityAttributesFromMeta(vdbGrid, primAttribMeta);
    }

    template<typename GridType>
    void MetadataHelper::ApplyGridMetadata(GridType& grid, CE::Decompression::CompressedFrameContainer* frameContainer)
    {
        ApplyGridAttributeMetadata(grid, frameContainer);
        ApplyGridVisualizationMetadata(grid, frameContainer);
    }

    template<typename TargetType>
    void MetadataHelper::ApplyDetailMetadata(TargetType& target, CE::Decompression::CompressedFrameContainer* frameContainer)
    {
        const char* detailMetadata = frameContainer->GetMetadataByKey("houdiniDetailAttributes");

        if (!detailMetadata)
        {
            return;
        }

        auto detailAttribMeta = nlohmann::json::parse(detailMetadata);
        LoadEntityAttributesFromMeta(target, detailAttribMeta);
    }

    template<typename GridType>
    void MetadataHelper::ApplyGridAttributeMetadata(GridType& grid, CE::Decompression::CompressedFrameContainer* frameContainer)
    {
        using PolicyType = MetadataPolicy<GridType>;
        const std::string attributeMetadataName = "houdiniPrimitiveAttributes_" + PolicyType::GetGridName(grid);

        const char* metadataEntry = frameContainer->GetMetadataByKey(attributeMetadataName.c_str());

        if (metadataEntry)
        {
            auto primAttribMeta = nlohmann::json::parse(metadataEntry);
            PolicyType::ApplyAttributeMetadata(grid, primAttribMeta);
        }
    }

    template<typename GridType>
    void MetadataHelper::ApplyGridVisualizationMetadata(GridType& grid, CE::Decompression::CompressedFrameContainer* frameContainer)
    {
        using PolicyType = MetadataPolicy<GridType>;
        const std::string keyPrefix = "houdiniVisualizationAttributes_" + PolicyType::GetGridName(grid);

        const std::string keyVisMode = keyPrefix + "_mode";
        const char* visModeMetadata = frameContainer->GetMetadataByKey(keyVisMode.c_str());

        const std::string keyVisIso = keyPrefix + "_iso";
        const char* visIsoMetadata = frameContainer->GetMetadataByKey(keyVisIso.c_str());

        const std::string keyVisDensity = keyPrefix + "_density";
        const char* visDensityMetadata = frameContainer->GetMetadataByKey(keyVisDensity.c_str());

        const std::string keyVisLod = keyPrefix + "_lod";
        const char* visLodMetadata = frameContainer->GetMetadataByKey(keyVisLod.c_str());

        if (visModeMetadata && visIsoMetadata && visDensityMetadata && visLodMetadata)
        {
            PolicyType::ApplyVisualizationMetadata(grid, visModeMetadata, visIsoMetadata, visDensityMetadata, visLodMetadata);
        }
    }

    template void MetadataHelper::ApplyGridMetadata<std::pair<GU_Detail*, GU_PrimVDB*>>(std::pair<GU_Detail*, GU_PrimVDB*>& context, CE::Decompression::CompressedFrameContainer* frameContainer);
    template void MetadataHelper::ApplyGridMetadata<openvdb::GridBase::Ptr>(openvdb::GridBase::Ptr& vdbGrid, CE::Decompression::CompressedFrameContainer* frameContainer);

    template void MetadataHelper::ApplyGridAttributeMetadata<std::pair<GU_Detail*, GU_PrimVDB*>>(std::pair<GU_Detail*, GU_PrimVDB*>& context, CE::Decompression::CompressedFrameContainer* frameContainer);
    template void MetadataHelper::ApplyGridAttributeMetadata<openvdb::GridBase::Ptr>(openvdb::GridBase::Ptr& vdbGrid, CE::Decompression::CompressedFrameContainer* frameContainer);

    template void MetadataHelper::ApplyGridVisualizationMetadata<std::pair<GU_Detail*, GU_PrimVDB*>>(std::pair<GU_Detail*, GU_PrimVDB*>& context, CE::Decompression::CompressedFrameContainer* frameContainer);
    template void MetadataHelper::ApplyGridVisualizationMetadata<openvdb::GridBase::Ptr>(openvdb::GridBase::Ptr& vdbGrid, CE::Decompression::CompressedFrameContainer* frameContainer);

    template void MetadataHelper::ApplyDetailMetadata<std::tuple<GU_Detail*, GA_AttributeOwner, GA_Offset>>(std::tuple<GU_Detail*, GA_AttributeOwner, GA_Offset>& target, CE::Decompression::CompressedFrameContainer* frameContainer);
    template void MetadataHelper::ApplyDetailMetadata<openvdb::MetaMap>(openvdb::MetaMap& fileMetadata, CE::Decompression::CompressedFrameContainer* frameContainer);


    std::vector<std::pair<std::string, std::string>> MetadataHelper::DumpAttributes(
        const GU_Detail* gdp, const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata) noexcept
    {
        std::vector<std::pair<std::string, std::string>> result{};

        const GEO_Primitive* prim;
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                auto vdbPrim = dynamic_cast<const GEO_PrimVDB*>(prim);

                nlohmann::json primAttrDump = Utils::DumpAttributesForSingleEntity(gdp, GA_ATTRIB_PRIMITIVE, prim->getMapOffset());
                std::string primKeyName = "houdiniPrimitiveAttributes_"s + vdbPrim->getGridName();
                result.emplace_back(primKeyName, primAttrDump.dump());

                DumpVisualisationAttributes(result, vdbPrim);
            }
        }

        nlohmann::json detailAttrDump = Utils::DumpAttributesForSingleEntity(gdp, GA_ATTRIB_DETAIL, 0);
        result.emplace_back("houdiniDetailAttributes", detailAttrDump.dump());

        DumpDecodeMetadata(result, encodingMetadata);
        return result;
    }

    void MetadataHelper::DumpVisualisationAttributes(std::vector<std::pair<std::string, std::string>>& attributes,
                                                     const GEO_PrimVDB* vdbPrim) noexcept
    {
        const std::string keyPrefix = "houdiniVisualizationAttributes_"s + vdbPrim->getGridName();

        std::string keyVisMode = keyPrefix + "_mode";
        std::string valueVisMode = std::to_string(static_cast<int>(vdbPrim->getVisualization()));
        attributes.emplace_back(std::move(keyVisMode), std::move(valueVisMode));

        std::string keyVisIso = keyPrefix + "_iso";
        std::string valueVisIso = std::to_string(vdbPrim->getVisIso());
        attributes.emplace_back(std::move(keyVisIso), std::move(valueVisIso));

        std::string keyVisDensity = keyPrefix + "_density";
        std::string valueVisDensity = std::to_string(vdbPrim->getVisDensity());
        attributes.emplace_back(std::move(keyVisDensity), std::move(valueVisDensity));

        std::string keyVisLod = keyPrefix + "_lod";
        std::string valueVisLod = std::to_string(static_cast<int>(vdbPrim->getVisLod()));
        attributes.emplace_back(std::move(keyVisLod), std::move(valueVisLod));
    }

    nlohmann::json MetadataHelper::DumpGridsShuffleInfo(const std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> gridDescs) noexcept
    {
        static std::map<CE::Addons::OpenVDBUtils::GridVoxelType, std::string> voxelTypeToString = {
            {CE::Addons::OpenVDBUtils::GridVoxelType::Float1, "Float1"}, {CE::Addons::OpenVDBUtils::GridVoxelType::Float3, "Float3"}};

        nlohmann::json result = nlohmann::json::array();
        for (const CE::Addons::OpenVDBUtils::VDBGridDesc& gridDesc : gridDescs)
        {
            nlohmann::json serializedDesc = nlohmann::json{
                {"gridName", gridDesc.gridName},
                {"voxelType", voxelTypeToString.at(gridDesc.voxelType)},
            };
            for (size_t i = 0; i < std::size(gridDesc.chSource); ++i)
            {
                std::string name{"chSource"};
                if (gridDesc.chSource[i])
                {
                    serializedDesc[name + std::to_string(i)] = gridDesc.chSource[i];
                }
                else
                {
                    serializedDesc[name + std::to_string(i)] = nullptr;
                }
            }
            result.emplace_back(serializedDesc);
        }
        return result;
    }

    void MetadataHelper::DumpDecodeMetadata(std::vector<std::pair<std::string, std::string>>& result,
                                            const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata)
    {
        std::ostringstream oss;
        oss << encodingMetadata.offsetX << " " << encodingMetadata.offsetY << " " << encodingMetadata.offsetZ;
        result.emplace_back("houdiniDecodeMetadata", oss.str());
    }

} // namespace Zibra::Utils