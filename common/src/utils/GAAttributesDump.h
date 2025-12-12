#pragma once
#include "Types.h"

namespace Zibra::Utils
{
    nlohmann::json DumpAttributesV2(const GU_Detail* gdp, GA_AttributeOwner attrOwner, GA_Offset mapOffset) noexcept;

    // TODO make these templates
    void LoadAttributesV1(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset, const nlohmann::json& meta) noexcept;
    void LoadAttributesV1(openvdb::GridBase::Ptr& grid, const nlohmann::json& meta) noexcept;
    void LoadAttributesV1(openvdb::MetaMap& target, const nlohmann::json& meta) noexcept;

    // TODO make these templates
    void LoadAttributesV2(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset, const nlohmann::json& meta) noexcept;
    void LoadAttributesV2(openvdb::GridBase::Ptr& grid, const nlohmann::json& meta) noexcept;
    void LoadAttributesV2(openvdb::MetaMap& target, const nlohmann::json& meta) noexcept;
} // namespace Zibra::Utils