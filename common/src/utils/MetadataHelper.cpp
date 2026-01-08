#include "PrecompiledHeader.h"

#include "utils/MetadataHelper.h"

#include "GAAttributesDump.h"

namespace Zibra::Utils
{
    using namespace std::literals;

    void MetadataHelper::ApplyGridMetadata(GU_Detail* gdp, GU_PrimVDB* grid, CE::Decompression::CompressedFrameContainer* frameContainer)
    {
        ApplyGridAttributeMetadata(gdp, grid, frameContainer);
        ApplyGridVisualizationMetadata(grid, frameContainer);
    }


    void MetadataHelper::ApplyGridAttributeMetadata(GU_Detail* gdp, GU_PrimVDB* grid, CE::Decompression::CompressedFrameContainer* frameContainer)
    {
        {
            const std::string attributeMetadataNameV2 = "houdiniPrimitiveAttributesV2_"s + grid->getGridName();
            const char* metadataEntryV2 = frameContainer->GetMetadataByKey(attributeMetadataNameV2.c_str());
            if (metadataEntryV2)
            {
                auto primAttribMeta = nlohmann::json::parse(metadataEntryV2);
                LoadAttributesV2(gdp, GA_ATTRIB_PRIMITIVE, grid->getMapOffset(), primAttribMeta);
                return;
            }
        }
        {
            const std::string attributeMetadataNameV1 = "houdiniPrimitiveAttributes_"s + grid->getGridName();
            const char* metadataEntryV1 = frameContainer->GetMetadataByKey(attributeMetadataNameV1.c_str());
            if (metadataEntryV1)
            {
                auto primAttribMeta = nlohmann::json::parse(metadataEntryV1);
                Utils::LoadAttributesV1(gdp, GA_ATTRIB_PRIMITIVE, grid->getMapOffset(), primAttribMeta);
                return;
            }
        }
    }

    void MetadataHelper::ApplyGridMetadata(openvdb::GridBase::Ptr grid, CE::Decompression::CompressedFrameContainer* frameContainer)
    {
        const std::string attributeMetadataNameV2 = "houdiniPrimitiveAttributesV2_"s + grid->getName();
        const char* metadataEntryV2 = frameContainer->GetMetadataByKey(attributeMetadataNameV2.c_str());
        if (metadataEntryV2)
        {
            auto primAttribMeta = nlohmann::json::parse(metadataEntryV2);
            LoadAttributesV2(grid.get(), primAttribMeta);
        }
    }

    void MetadataHelper::ApplyDetailMetadata(GU_Detail* gdp, CE::Decompression::CompressedFrameContainer* frameContainer)
    {
        {
            const char* detailMetadataV2 = frameContainer->GetMetadataByKey("houdiniDetailAttributesV2");

            if (detailMetadataV2)
            {
                auto detailAttribMeta = nlohmann::json::parse(detailMetadataV2);
                Utils::LoadAttributesV2(gdp, GA_ATTRIB_DETAIL, 0, detailAttribMeta);
                return;
            }
        }
        {
            const char* detailMetadataV1 = frameContainer->GetMetadataByKey("houdiniDetailAttributes");

            if (detailMetadataV1)
            {
                auto detailAttribMeta = nlohmann::json::parse(detailMetadataV1);
                Utils::LoadAttributesV1(gdp, GA_ATTRIB_DETAIL, 0, detailAttribMeta);
                return;
            }
        }
    }

    void MetadataHelper::ApplyDetailMetadata(openvdb::MetaMap* target, CE::Decompression::CompressedFrameContainer* frameContainer)
    {
        const char* detailMetadataV2 = frameContainer->GetMetadataByKey("houdiniDetailAttributesV2");

        if (detailMetadataV2)
        {
            auto detailAttribMeta = nlohmann::json::parse(detailMetadataV2);
            LoadAttributesV2(target, detailAttribMeta);
        }
    }

    void MetadataHelper::ApplyGridVisualizationMetadata(GU_PrimVDB* grid, CE::Decompression::CompressedFrameContainer* frameContainer)
    {
        const std::string keyPrefix = "houdiniVisualizationAttributes_"s + grid->getGridName();

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
            GEO_VolumeOptions visOptions{};
            visOptions.myMode = static_cast<GEO_VolumeVis>(std::stoi(visModeMetadata));
            visOptions.myIso = std::stof(visIsoMetadata);
            visOptions.myDensity = std::stof(visDensityMetadata);
            visOptions.myLod = static_cast<GEO_VolumeVisLod>(std::stoi(visLodMetadata));
            grid->setVisOptions(visOptions);
        }
    }

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

                nlohmann::json primAttrDump = Utils::DumpAttributesV2(gdp, GA_ATTRIB_PRIMITIVE, prim->getMapOffset());
                std::string primKeyName = "houdiniPrimitiveAttributesV2_"s + vdbPrim->getGridName();
                result.emplace_back(primKeyName, primAttrDump.dump());

                DumpVisualisationAttributes(result, vdbPrim);
            }
        }

        nlohmann::json detailAttrDump = Utils::DumpAttributesV2(gdp, GA_ATTRIB_DETAIL, 0);
        result.emplace_back("houdiniDetailAttributesV2", detailAttrDump.dump());

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