#include "PrecompiledHeader.h"

#include "GAAttributesDump.h"

namespace Zibra::Utils
{

    const char* GAStorageToStr(GA_Storage storage) noexcept
    {
        switch (storage)
        {
        case GA_STORE_BOOL:
            return "bool";
        case GA_STORE_UINT8:
            return "uint8";
        case GA_STORE_INT8:
            return "int8";
        case GA_STORE_INT16:
            return "int16";
        case GA_STORE_INT32:
            return "int32";
        case GA_STORE_INT64:
            return "int64";
        case GA_STORE_REAL16:
            return "float16";
        case GA_STORE_REAL32:
            return "float32";
        case GA_STORE_REAL64:
            return "float64";
        case GA_STORE_STRING:
            return "string";
        case GA_STORE_DICT:
            return "dict";
        case GA_STORE_INVALID:
        default:
            return "invalid";
        }
    }

    GA_Storage StrTypeToGAStorage(const std::string& strType) noexcept
    {
        if (strType == "bool")
        {
            return GA_STORE_BOOL;
        }
        if (strType == "uint8")
        {
            return GA_STORE_UINT8;
        }
        if (strType == "int8")
        {
            return GA_STORE_INT8;
        }
        if (strType == "int16")
        {
            return GA_STORE_INT16;
        }
        if (strType == "int32")
        {
            return GA_STORE_INT32;
        }
        if (strType == "int64")
        {
            return GA_STORE_INT64;
        }
        if (strType == "float16")
        {
            return GA_STORE_REAL16;
        }
        if (strType == "float32")
        {
            return GA_STORE_REAL32;
        }
        if (strType == "float64")
        {
            return GA_STORE_REAL64;
        }
        if (strType == "string")
        {
            return GA_STORE_STRING;
        }
        return GA_STORE_INVALID;
    }

    nlohmann::json DumpAttributesForSingleEntity(const GU_Detail* gdp, GA_AttributeOwner attrOwner, GA_Offset mapOffset) noexcept
    {
        nlohmann::json result = nlohmann::json::value_t::object;

        for (auto it = gdp->getAttributeDict(attrOwner).obegin(GA_SCOPE_PUBLIC); !it.atEnd(); it.advance())
        {
            GA_Attribute* attr = *it;

            nlohmann::json attrDump{};

            switch (attr->getStorageClass())
            {
            case GA_STORECLASS_INT:
            case GA_STORECLASS_FLOAT: {
                auto tuple = attr->getAIFTuple();

                const GA_Storage storage = tuple->getStorage(attr);
                switch (storage)
                {
                case GA_STORE_BOOL: {
                    int64 data;
                    tuple->get(attr, mapOffset, data, 0);
                    attrDump["t"] = GAStorageToStr(storage);
                    attrDump["v"] = static_cast<bool>(data);
                    break;
                }
                case GA_STORE_UINT8:
                    continue;
                case GA_STORE_INT8:
                case GA_STORE_INT16:
                case GA_STORE_INT32:
                case GA_STORE_INT64: {
                    std::vector<int64> data{};
                    data.resize(attr->getTupleSize());
                    tuple->get(attr, mapOffset, data.data(), static_cast<int>(data.size()));
                    attrDump["t"] = GAStorageToStr(storage);
                    attrDump["v"] = data;
                    break;
                }
                case GA_STORE_REAL16:
                case GA_STORE_REAL32:
                case GA_STORE_REAL64: {
                    std::vector<fpreal64> data{};
                    data.resize(attr->getTupleSize());
                    tuple->get(attr, mapOffset, data.data(), static_cast<int>(data.size()));
                    attrDump["t"] = GAStorageToStr(storage);
                    attrDump["v"] = data;
                }
                default:
                    break;
                }

                break;
            }
            case GA_STORECLASS_STRING: {
                auto strTuple = attr->getAIFStringTuple();
                const char* attrString = (strTuple && attr->getTupleSize()) ? strTuple->getString(attr, mapOffset, 0) : "";
                attrDump["t"] = "string";
                attrDump["v"] = attrString ? attrString : "";
                break;
            }
            case GA_STORECLASS_DICT:
            case GA_STORECLASS_OTHER:
            case GA_STORECLASS_INVALID:
            default:
                continue;
            }

            result[attr->getName().c_str()] = attrDump;
        }
        return result;
    }

    void AttributeStoragePolicy<std::tuple<GU_Detail*, GA_AttributeOwner, GA_Offset>>::StoreBool(const TargetType& target,
                                                                                                 const std::string& attribName, bool value)
    {
        auto [gdp, owner, mapOffset] = target;
        GA_Attribute* attr = gdp->addTuple(GA_STORE_BOOL, owner, attribName, 1);
        GA_RWHandleI attrRWHandle{attr};
        attrRWHandle.set(mapOffset, value ? 1 : 0);
    }

    void AttributeStoragePolicy<std::tuple<GU_Detail*, GA_AttributeOwner, GA_Offset>>::StoreIntArray(const TargetType& target,
                                                                                                     const std::string& attribName,
                                                                                                     const std::vector<int32>& values,
                                                                                                     GA_Storage storage)
    {
        auto [gdp, owner, mapOffset] = target;
        GA_Attribute* attr = gdp->addTuple(storage, owner, attribName, static_cast<int>(values.size()));
        GA_RWHandleI attrRWHandle{attr};
        attrRWHandle.setV(mapOffset, values.data(), static_cast<int>(values.size()));
    }

    void AttributeStoragePolicy<std::tuple<GU_Detail*, GA_AttributeOwner, GA_Offset>>::StoreFloatArray(const TargetType& target,
                                                                                                       const std::string& attribName,
                                                                                                       const std::vector<float>& values,
                                                                                                       GA_Storage storage)
    {
        auto [gdp, owner, mapOffset] = target;
        GA_Attribute* attr = gdp->addTuple(storage, owner, attribName, static_cast<int>(values.size()));
        GA_RWHandleF attrRWHandle{attr};
        attrRWHandle.setV(mapOffset, values.data(), static_cast<int>(values.size()));
    }

    void AttributeStoragePolicy<std::tuple<GU_Detail*, GA_AttributeOwner, GA_Offset>>::StoreString(const TargetType& target,
                                                                                                   const std::string& attribName,
                                                                                                   const std::string& value)
    {
        auto [gdp, owner, mapOffset] = target;
        GA_Attribute* attr = gdp->addTuple(GA_STORE_STRING, owner, attribName, 1);
        GA_RWHandleS attrRWHandle{attr};
        attrRWHandle.set(mapOffset, value);
    }

    void AttributeStoragePolicy<openvdb::GridBase::Ptr>::StoreBool(TargetType& target, const std::string& attribName, bool value)
    {
        target->insertMeta(attribName, openvdb::BoolMetadata(value));
    }

    void AttributeStoragePolicy<openvdb::GridBase::Ptr>::StoreIntArray(TargetType& target, const std::string& attribName,
                                                                       const std::vector<int32>& values, const std::string& typeStr)
    {
        switch (values.size())
        {
        case 1:
            target->insertMeta(attribName, openvdb::Int32Metadata(values[0]));
            break;
        case 2:
            target->insertMeta(attribName, openvdb::Vec2IMetadata(openvdb::Vec2i(values[0], values[1])));
            break;
        case 3:
            target->insertMeta(attribName, openvdb::Vec3IMetadata(openvdb::Vec3i(values[0], values[1], values[2])));
            break;
        default:
            assert(false && "Only arrays with 1-3 elements are supported");
            break;
        }
    }

    void AttributeStoragePolicy<openvdb::GridBase::Ptr>::StoreFloatArray(TargetType& target, const std::string& attribName,
                                                                         const std::vector<float>& values, GA_Storage /*storage*/)
    {
        switch (values.size())
        {
        case 1:
            target->insertMeta(attribName, openvdb::FloatMetadata(values[0]));
            break;
        case 2:
            target->insertMeta(attribName, openvdb::Vec2SMetadata(openvdb::Vec2s(values[0], values[1])));
            break;
        case 3:
            target->insertMeta(attribName, openvdb::Vec3SMetadata(openvdb::Vec3s(values[0], values[1], values[2])));
            break;
        default:
            assert(false && "Only arrays with 1-3 elements are supported");
            break;
        }
    }

    void AttributeStoragePolicy<openvdb::GridBase::Ptr>::StoreString(TargetType& target, const std::string& attribName,
                                                                     const std::string& value)
    {
        target->insertMeta(attribName, openvdb::StringMetadata(value));
    }

    void AttributeStoragePolicy<openvdb::MetaMap>::StoreBool(TargetType& target, const std::string& attribName, bool value)
    {
        target.insertMeta(attribName, openvdb::BoolMetadata(value));
    }

    void AttributeStoragePolicy<openvdb::MetaMap>::StoreIntArray(TargetType& target, const std::string& attribName,
                                                                 const std::vector<int32>& values, const std::string& typeStr)
    {
        switch (values.size())
        {
        case 1:
            target.insertMeta(attribName, openvdb::Int32Metadata(values[0]));
            break;
        case 2:
            target.insertMeta(attribName, openvdb::Vec2IMetadata(openvdb::Vec2i(values[0], values[1])));
            break;
        case 3:
            target.insertMeta(attribName, openvdb::Vec3IMetadata(openvdb::Vec3i(values[0], values[1], values[2])));
            break;
        default:
            assert(false && "Only arrays with 1-3 elements are supported");
            break;
        }
    }

    void AttributeStoragePolicy<openvdb::MetaMap>::StoreFloatArray(TargetType& target, const std::string& attribName,
                                                                   const std::vector<float>& values, GA_Storage /*storage*/)
    {
        switch (values.size())
        {
        case 1:
            target.insertMeta(attribName, openvdb::FloatMetadata(values[0]));
            break;
        case 2:
            target.insertMeta(attribName, openvdb::Vec2SMetadata(openvdb::Vec2s(values[0], values[1])));
            break;
        case 3:
            target.insertMeta(attribName, openvdb::Vec3SMetadata(openvdb::Vec3s(values[0], values[1], values[2])));
            break;
        default:
            assert(false && "Only arrays with 1-3 elements are supported");
            break;
        }
    }

    void AttributeStoragePolicy<openvdb::MetaMap>::StoreString(TargetType& target, const std::string& attribName, const std::string& value)
    {
        target.insertMeta(attribName, openvdb::StringMetadata(value));
    }

    template MetaAttributesLoadStatus LoadEntityAttributesFromMeta<std::tuple<GU_Detail*, GA_AttributeOwner, GA_Offset>>(
        std::tuple<GU_Detail*, GA_AttributeOwner, GA_Offset>& target, const nlohmann::json& meta) noexcept;
    template MetaAttributesLoadStatus LoadEntityAttributesFromMeta<openvdb::GridBase::Ptr>(openvdb::GridBase::Ptr& target,
                                                                                           const nlohmann::json& meta) noexcept;
    template MetaAttributesLoadStatus LoadEntityAttributesFromMeta<openvdb::MetaMap>(openvdb::MetaMap& target,
                                                                                     const nlohmann::json& meta) noexcept;

    template <typename TargetType>
    MetaAttributesLoadStatus LoadEntityAttributesFromMeta(TargetType& target, const nlohmann::json& meta) noexcept
    {
#define ZIB_LOCAL_HELPER_WARNING_AND_RETURN(cond)                      \
    if (cond)                                                          \
    {                                                                  \
        return MetaAttributesLoadStatus::FATAL_ERROR_INVALID_METADATA; \
    }
#define ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(cond)                          \
    if (cond)                                                                \
    {                                                                        \
        status = MetaAttributesLoadStatus::ERROR_PARTIALLY_INVALID_METADATA; \
        continue;                                                            \
    }

        ZIB_LOCAL_HELPER_WARNING_AND_RETURN(!meta.is_object());

        MetaAttributesLoadStatus status = MetaAttributesLoadStatus::SUCCESS;
        using PolicyType = AttributeStoragePolicy<TargetType>;

        for (const auto& [attribName, attrContainer] : meta.items())
        {
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!attrContainer.is_object());
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!attrContainer.contains("t") || !attrContainer.contains("v"));
            auto typeContainer = attrContainer["t"];
            auto valContainer = attrContainer["v"];
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!typeContainer.is_string() || valContainer.is_object() || valContainer.is_null());

            GA_Storage storage = StrTypeToGAStorage(typeContainer);

            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(storage == GA_STORE_INVALID);

            switch (storage)
            {
            case GA_STORE_BOOL:
                // TODO: finish
                break;
            case GA_STORE_UINT8:
                // TODO: finish
                break;
            case GA_STORE_INT8:
            case GA_STORE_INT16:
            case GA_STORE_INT32:
            case GA_STORE_INT64: {
                PolicyType::StoreBool(target, attribName, valContainer.template get<bool>());
                break;
            }
            case GA_STORE_REAL16:
            case GA_STORE_REAL32:
            case GA_STORE_REAL64: {
                PolicyType::StoreFloatArray(target, attribName, valContainer, storage);
                break;
            }
            case GA_STORE_STRING: {
                PolicyType::StoreString(target, attribName, valContainer.template get<std::string>());
                break;
            }
            default:
                continue;
            }
        }
        return status;

#undef ZIB_LOCAL_HELPER_WARNING_AND_RETURN
#undef ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE
    }

} // namespace Zibra::Utils