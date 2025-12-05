#pragma once

namespace Zibra::Utils
{
    nlohmann::json DumpAttributesV2(const GU_Detail* gdp, GA_AttributeOwner attrOwner, GA_Offset mapOffset) noexcept;
    void LoadAttributesV1(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset, const nlohmann::json& meta) noexcept;
    void LoadAttributesV2(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset, const nlohmann::json& meta) noexcept;

} // namespace Zibra::Utils
