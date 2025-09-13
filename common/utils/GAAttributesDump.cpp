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

    MetaAttributesLoadStatus LoadEntityAttributesFromMeta(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset,
                                                          const nlohmann::json& meta) noexcept
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
        for (const auto& [attribName, attrContainer] : meta.items())
        {
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!attrContainer.is_object());
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!attrContainer.contains("t") || !attrContainer.contains("v"));
            auto typeContainer = attrContainer["t"];
            auto valContainer = attrContainer["v"];
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!typeContainer.is_string() || valContainer.is_object() || valContainer.is_null());

            GA_Storage storage = StrTypeToGAStorage(typeContainer);

            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(storage == GA_STORE_INVALID);
            GA_Attribute* attr = gdp->addTuple(storage, owner, attribName, static_cast<int>(valContainer.size()));

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
                GA_RWHandleI attrRWHandle{attr};
                std::vector<int32> values{};
                values.reserve(valContainer.size());
                for (int32_t val : valContainer)
                {
                    values.emplace_back(val);
                }
                attrRWHandle.setV(mapOffset, values.data(), static_cast<int>(values.size()));
                break;
            }
            case GA_STORE_REAL16:
            case GA_STORE_REAL32:
            case GA_STORE_REAL64: {
                GA_RWHandleF attrRWHandle{attr};
                std::vector<float> values{};
                values.reserve(valContainer.size());
                for (float val : valContainer)
                {
                    values.emplace_back(static_cast<float>(val));
                }
                attrRWHandle.setV(mapOffset, values.data(), static_cast<int>(values.size()));
                break;
            }
            case GA_STORE_STRING: {
                GA_RWHandleS attrRWHandle{attr};
                std::string value = valContainer;
                attrRWHandle.set(mapOffset, value);
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

    MetaAttributesLoadStatus LoadEntityAttributesFromMeta(openvdb::GridBase::Ptr& grid,
                                                          const nlohmann::json& meta) noexcept
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
        for (const auto& [attribName, attrContainer] : meta.items())
        {
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!attrContainer.is_object());
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!attrContainer.contains("t") || !attrContainer.contains("v"));
            auto typeContainer = attrContainer["t"];
            auto valContainer = attrContainer["v"];
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!typeContainer.is_string() || valContainer.is_object() || valContainer.is_null());

            std::string metaName = "houdini_attr_" + attribName; // Prefix to distinguish from native OpenVDB metadata

            // Convert Houdini attribute data to OpenVDB metadata
            std::string typeStr = typeContainer;
            if (typeStr == "bool")
            {
                if (valContainer.is_boolean())
                {
                    grid->insertMeta(metaName + "_type", openvdb::StringMetadata(typeStr));
                    grid->insertMeta(metaName, openvdb::StringMetadata(valContainer.get<bool>() ? "true" : "false"));
                }
            }
            else if (typeStr == "int8" || typeStr == "int16" || typeStr == "int32" || typeStr == "int64")
            {
                if (valContainer.is_array())
                {
                    std::vector<int32_t> values = valContainer;
                    grid->insertMeta(metaName + "_type", openvdb::StringMetadata(typeStr));

                    // For arrays, store as JSON string
                    grid->insertMeta(metaName, openvdb::StringMetadata(valContainer.dump()));
                }
            }
            else if (typeStr == "float16" || typeStr == "float32" || typeStr == "float64")
            {
                if (valContainer.is_array())
                {
                    std::vector<float> values = valContainer;
                    grid->insertMeta(metaName + "_type", openvdb::StringMetadata(typeStr));

                    // For arrays, store as JSON string
                    grid->insertMeta(metaName, openvdb::StringMetadata(valContainer.dump()));
                }
            }
            else if (typeStr == "string")
            {
                if (valContainer.is_string())
                {
                    grid->insertMeta(metaName + "_type", openvdb::StringMetadata(typeStr));
                    grid->insertMeta(metaName, openvdb::StringMetadata(valContainer.get<std::string>()));
                }
            }
            else
            {
                // Unknown type, skip
                status = MetaAttributesLoadStatus::ERROR_PARTIALLY_INVALID_METADATA;
                continue;
            }
        }
        return status;

#undef ZIB_LOCAL_HELPER_WARNING_AND_RETURN
#undef ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE
    }

    MetaAttributesLoadStatus LoadEntityAttributesFromMeta(openvdb::MetaMap& fileMetadata,
                                                          const nlohmann::json& meta) noexcept
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
        for (const auto& [attribName, attrContainer] : meta.items())
        {
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!attrContainer.is_object());
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!attrContainer.contains("t") || !attrContainer.contains("v"));
            auto typeContainer = attrContainer["t"];
            auto valContainer = attrContainer["v"];
            ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE(!typeContainer.is_string() || valContainer.is_object() || valContainer.is_null());

            std::string metaName = "houdini_attr_" + attribName; // Prefix to distinguish from native OpenVDB metadata

            // Convert Houdini attribute data to OpenVDB metadata
            std::string typeStr = typeContainer;
            if (typeStr == "bool")
            {
                if (valContainer.is_boolean())
                {
                    fileMetadata.insertMeta(metaName + "_type", openvdb::StringMetadata(typeStr));
                    fileMetadata.insertMeta(metaName, openvdb::StringMetadata(valContainer.get<bool>() ? "true" : "false"));
                }
            }
            else if (typeStr == "int8" || typeStr == "int16" || typeStr == "int32" || typeStr == "int64")
            {
                if (valContainer.is_array())
                {
                    std::vector<int32_t> values = valContainer;
                    fileMetadata.insertMeta(metaName + "_type", openvdb::StringMetadata(typeStr));

                    // For arrays, store as JSON string
                    fileMetadata.insertMeta(metaName, openvdb::StringMetadata(valContainer.dump()));
                }
            }
            else if (typeStr == "float16" || typeStr == "float32" || typeStr == "float64")
            {
                if (valContainer.is_array())
                {
                    std::vector<float> values = valContainer;
                    fileMetadata.insertMeta(metaName + "_type", openvdb::StringMetadata(typeStr));

                    // For arrays, store as JSON string
                    fileMetadata.insertMeta(metaName, openvdb::StringMetadata(valContainer.dump()));
                }
            }
            else if (typeStr == "string")
            {
                if (valContainer.is_string())
                {
                    fileMetadata.insertMeta(metaName + "_type", openvdb::StringMetadata(typeStr));
                    fileMetadata.insertMeta(metaName, openvdb::StringMetadata(valContainer.get<std::string>()));
                }
            }
            else
            {
                // Unknown type, skip
                status = MetaAttributesLoadStatus::ERROR_PARTIALLY_INVALID_METADATA;
                continue;
            }
        }
        return status;

#undef ZIB_LOCAL_HELPER_WARNING_AND_RETURN
#undef ZIB_LOCAL_HELPER_WARNING_AND_CONTINUE
    }

} // namespace Zibra::Utils