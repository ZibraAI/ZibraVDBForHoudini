#pragma once
#include "Types.h"

namespace Zibra::Utils
{
    const char* GAStorageToStr(GA_Storage storage) noexcept;
    GA_Storage StrTypeToGAStorage(const std::string& strType) noexcept;
    nlohmann::json DumpAttributesForSingleEntity(const GU_Detail* gdp, GA_AttributeOwner attrOwner, GA_Offset mapOffset) noexcept;

    template<typename TargetType>
    struct AttributeStoragePolicy;

    template<>
    struct AttributeStoragePolicy<std::tuple<GU_Detail*, GA_AttributeOwner, GA_Offset>>
    {
        using TargetType = std::tuple<GU_Detail*, GA_AttributeOwner, GA_Offset>;

        static void StoreBool(const TargetType& target, const std::string& attribName, bool value);
        static void StoreIntArray(const TargetType& target, const std::string& attribName, const std::vector<int32>& values, GA_Storage storage);
        static void StoreFloatArray(const TargetType& target, const std::string& attribName, const std::vector<float>& values, GA_Storage storage);
        static void StoreString(const TargetType& target, const std::string& attribName, const std::string& value);
    };

    template<>
    struct AttributeStoragePolicy<openvdb::GridBase::Ptr>
    {
        using TargetType = openvdb::GridBase::Ptr;

        static void StoreBool(TargetType& target, const std::string& attribName, bool value);
        static void StoreIntArray(TargetType& target, const std::string& attribName, const std::vector<int32>& values, const std::string& typeStr);
        static void StoreFloatArray(TargetType& target, const std::string& attribName, const std::vector<float>& values, const std::string& typeStr);
        static void StoreString(TargetType& target, const std::string& attribName, const std::string& value);
    };

    template<>
    struct AttributeStoragePolicy<openvdb::MetaMap>
    {
        using TargetType = openvdb::MetaMap;

        static void StoreBool(TargetType& target, const std::string& attribName, bool value);
        static void StoreIntArray(TargetType& target, const std::string& attribName, const std::vector<int32>& values, const std::string& typeStr);
        static void StoreFloatArray(TargetType& target, const std::string& attribName, const std::vector<float>& values, const std::string& typeStr);
        static void StoreString(TargetType& target, const std::string& attribName, const std::string& value);
    };

    template<typename TargetType>
    MetaAttributesLoadStatus LoadEntityAttributesFromMeta(TargetType& target, const nlohmann::json& meta) noexcept;

} // namespace Zibra::Utils