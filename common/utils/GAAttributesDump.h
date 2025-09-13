#pragma once

namespace Zibra::Utils
{
    enum class MetaAttributesLoadStatus
    {
        FATAL_ERROR_INVALID_METADATA,
        ERROR_PARTIALLY_INVALID_METADATA,
        SUCCESS,
    };

    const char* GAStorageToStr(GA_Storage storage) noexcept;
    GA_Storage StrTypeToGAStorage(const std::string& strType) noexcept;
    nlohmann::json DumpAttributesForSingleEntity(const GU_Detail* gdp, GA_AttributeOwner attrOwner, GA_Offset mapOffset) noexcept;
    MetaAttributesLoadStatus LoadEntityAttributesFromMeta(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset,
                                                          const nlohmann::json& meta) noexcept;

    // OpenVDB overload - loads Houdini attributes as OpenVDB metadata
    MetaAttributesLoadStatus LoadEntityAttributesFromMeta(openvdb::GridBase::Ptr& grid,
                                                          const nlohmann::json& meta) noexcept;

    // OpenVDB overload for file-level metadata
    MetaAttributesLoadStatus LoadEntityAttributesFromMeta(openvdb::MetaMap& fileMetadata,
                                                          const nlohmann::json& meta) noexcept;

} // namespace Zibra::Utils