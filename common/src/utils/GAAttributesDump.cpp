#include "PrecompiledHeader.h"

#include "GAAttributesDump.h"

namespace Zibra::Utils
{
    static const char* GAStorageToStr(GA_Storage storage) noexcept
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

    static GA_Storage StrTypeToGAStorage(const std::string& strType) noexcept
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

    // Logic for changing types into something that can be safely stored into JSON
    // e.g. int64 values we have to store as strings
    // since JSON numbers are treated as double precision float, and can only precisely store integers up to ~2^53
    // which is not enough for int64
    template <typename DataType>
    struct JSONSafeTypeMapping
    {
        using UnderlyingType = DataType;
    };

    template <>
    struct JSONSafeTypeMapping<int64>
    {
        using UnderlyingType = std::string;
    };

    template <typename DataType>
    typename JSONSafeTypeMapping<DataType>::UnderlyingType ToJSONSafeType(DataType value)
    {
        return value;
    }

    template <>
    typename JSONSafeTypeMapping<int64>::UnderlyingType ToJSONSafeType<int64>(int64 value)
    {
        return std::to_string(value);
    }

    template <typename DataType>
    DataType FromJSONSafeType(typename JSONSafeTypeMapping<DataType>::UnderlyingType value)
    {
        return value;
    }

    template <>
    int64 FromJSONSafeType<int64>(typename JSONSafeTypeMapping<int64>::UnderlyingType value)
    {
        return std::stoll(value);
    }

    template <GA_StorageClass StorageClass, typename DataType, GA_Storage StorageType>
    std::optional<nlohmann::json> SerializeAttributeV2Fundamental(GA_Attribute* attribute, GA_Offset mapOffset)
    {
        nlohmann::json result{};
        const GA_AIFTuple* tuple = attribute->getAIFTuple();
        if (tuple == nullptr)
        {
            return std::nullopt;
        }

        int arraySize = tuple->getTupleSize(attribute);
        std::vector<DataType> data;
        data.resize(arraySize);
        tuple->get(attribute, mapOffset, data.data(), arraySize);
        result["t"] = int(StorageType);
        result["v"] = ToJSONSafeType(data);

        return result;
    }

    template <GA_StorageClass StorageClass>
    std::optional<nlohmann::json> SerializeAttributeV2(GA_Attribute* attribute, GA_Offset mapOffset)
    {
        const GA_AIFTuple* tuple = attribute->getAIFTuple();
        if (tuple == nullptr)
        {
            return std::nullopt;
        }

        const GA_Storage storage = tuple->getStorage(attribute);
        switch (storage)
        {
        case GA_STORE_BOOL:
            return SerializeAttributeV2Fundamental<StorageClass, int32, GA_STORE_BOOL>(attribute, mapOffset);
        case GA_STORE_UINT8:
            return SerializeAttributeV2Fundamental<StorageClass, int32, GA_STORE_UINT8>(attribute, mapOffset);
        case GA_STORE_INT8:
            return SerializeAttributeV2Fundamental<StorageClass, int32, GA_STORE_INT8>(attribute, mapOffset);
        case GA_STORE_INT16:
            return SerializeAttributeV2Fundamental<StorageClass, int32, GA_STORE_INT16>(attribute, mapOffset);
        case GA_STORE_INT32:
            return SerializeAttributeV2Fundamental<StorageClass, int32, GA_STORE_INT32>(attribute, mapOffset);
        case GA_STORE_INT64:
            return SerializeAttributeV2Fundamental<StorageClass, int64, GA_STORE_INT64>(attribute, mapOffset);
        case GA_STORE_REAL16:
            return SerializeAttributeV2Fundamental<StorageClass, fpreal32, GA_STORE_REAL16>(attribute, mapOffset);
        case GA_STORE_REAL32:
            return SerializeAttributeV2Fundamental<StorageClass, fpreal32, GA_STORE_REAL32>(attribute, mapOffset);
        case GA_STORE_REAL64:
            return SerializeAttributeV2Fundamental<StorageClass, fpreal64, GA_STORE_REAL64>(attribute, mapOffset);
        default:
            assert(0);
            return std::nullopt;
        }
    }

    template <>
    std::optional<nlohmann::json> SerializeAttributeV2<GA_STORECLASS_STRING>(GA_Attribute* attribute, GA_Offset mapOffset)
    {
        nlohmann::json result{};

        const GA_AIFStringTuple* tuple = attribute->getAIFStringTuple();
        if (tuple == nullptr)
        {
            return std::nullopt;
        }

        int arraySize = tuple->getTupleSize(attribute);
        std::vector<std::string> data;
        data.resize(arraySize);
        for (int i = 0; i < arraySize; ++i)
        {
            const char* value = tuple->getString(attribute, mapOffset, i);
            if (value == nullptr)
            {
                return std::nullopt;
            }
            data[i] = value;
        }
        result["t"] = int(GA_STORE_STRING);
        result["v"] = data;

        return result;
    }

    template <>
    std::optional<nlohmann::json> SerializeAttributeV2<GA_STORECLASS_DICT>(GA_Attribute* attribute, GA_Offset mapOffset)
    {
        nlohmann::json result{};
        const GA_AIFSharedDictTuple* tuple = attribute->getAIFSharedDictTuple();
        if (tuple == nullptr)
        {
            return std::nullopt;
        }

        int arraySize = tuple->getTupleSize(attribute);
        std::vector<std::string> data;
        data.resize(arraySize);
        for (int i = 0; i < arraySize; ++i)
        {
            UT_OptionsHolder dict = tuple->getDict(attribute, mapOffset);
            std::stringstream jsonDump;
            dict->dump(jsonDump);
            data[i] = jsonDump.str();
        }
        result["t"] = int(GA_STORE_DICT);
        result["v"] = data;

        return result;
    }

    nlohmann::json DumpAttributesV2(const GU_Detail* gdp, GA_AttributeOwner attrOwner, GA_Offset mapOffset) noexcept
    {
        nlohmann::json result = nlohmann::json::value_t::object;

        for (auto it = gdp->getAttributeDict(attrOwner).obegin(GA_SCOPE_PUBLIC); !it.atEnd(); it.advance())
        {
            GA_Attribute* attribute = *it;

            std::optional<nlohmann::json> serializedAttribute;
            switch (attribute->getStorageClass())
            {
            case GA_STORECLASS_INT:
                serializedAttribute = SerializeAttributeV2<GA_STORECLASS_INT>(attribute, mapOffset);
                break;
            case GA_STORECLASS_FLOAT:
                serializedAttribute = SerializeAttributeV2<GA_STORECLASS_FLOAT>(attribute, mapOffset);
                break;
            case GA_STORECLASS_STRING:
                serializedAttribute = SerializeAttributeV2<GA_STORECLASS_STRING>(attribute, mapOffset);
                break;
            case GA_STORECLASS_DICT:
                serializedAttribute = SerializeAttributeV2<GA_STORECLASS_DICT>(attribute, mapOffset);
                break;
            default:
                continue;
            }

            if (!serializedAttribute.has_value())
            {
                continue;
            }
            result[attribute->getName().c_str()] = *serializedAttribute;
        }
        return result;
    }

    template <typename DataType>
    bool IsCompatibleType(const nlohmann::json& value);

    template <>
    bool IsCompatibleType<int32>(const nlohmann::json& value)
    {
        return value.is_number_integer();
    }

    template <>
    bool IsCompatibleType<int64>(const nlohmann::json& value)
    {
        return value.is_number_integer();
    }

    template <>
    bool IsCompatibleType<fpreal32>(const nlohmann::json& value)
    {
        return value.is_number();
    }

    template <>
    bool IsCompatibleType<fpreal64>(const nlohmann::json& value)
    {
        return value.is_number();
    }

    template <>
    bool IsCompatibleType<std::string>(const nlohmann::json& value)
    {
        return value.is_string();
    }

    template <GA_StorageClass StorageClass, typename DataType, GA_Storage StorageType>
    void LoadAttributeV1(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset, const nlohmann::json& valuesContainer,
                         const std::string& name)
    {
        if (!valuesContainer.is_array())
        {
            return;
        }

        int arraySize = static_cast<int>(valuesContainer.size());

        std::vector<DataType> valuesArray{};
        valuesArray.reserve(arraySize);
        for (const nlohmann::json& val : valuesContainer)
        {
            if (!IsCompatibleType<DataType>(val))
            {
                return;
            }

            valuesArray.emplace_back(val.get<DataType>());
        }

        GA_Attribute* attribute = gdp->addTuple(StorageType, owner, name, arraySize);
        GA_RWHandleT<DataType> attrRWHandle{attribute};
        attrRWHandle.setV(mapOffset, valuesArray.data(), arraySize);
    }

    template <>
    void LoadAttributeV1<GA_STORECLASS_STRING, std::string, GA_STORE_STRING>(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset,
                                                                             const nlohmann::json& valuesContainer, const std::string& name)
    {
        // V1 doesn't serialize strings as arrays
        if (!valuesContainer.is_string())
        {
            return;
        }

        // V1 doesn't support string arrays, so arraySize is always 1
        GA_Attribute* attribute = gdp->addTuple(GA_STORE_STRING, owner, name, 1);
        GA_RWHandleS attrRWHandle{attribute};
        std::string value = valuesContainer.get<std::string>();
        attrRWHandle.set(mapOffset, value);
    }

    void LoadAttributesV1(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset, const nlohmann::json& meta) noexcept
    {
        if (!meta.is_object())
        {
            return;
        }

        for (const auto& [attribName, attrContainer] : meta.items())
        {
            if (!attrContainer.is_object())
            {
                continue;
            }

            auto typeIter = attrContainer.find("t");
            auto valueIter = attrContainer.find("v");

            if (typeIter == attrContainer.end() || valueIter == attrContainer.end())
            {
                continue;
            }
            if (!typeIter->is_string())
            {
                continue;
            }

            GA_Storage storage = StrTypeToGAStorage(typeIter->template get<std::string>());

            switch (storage)
            {
            case GA_STORE_BOOL:
            case GA_STORE_UINT8:
                // Unimplemented in V1
                break;
            case GA_STORE_INT8:
                LoadAttributeV1<GA_STORECLASS_INT, int32, GA_STORE_INT8>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_INT16:
                LoadAttributeV1<GA_STORECLASS_INT, int32, GA_STORE_INT16>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_INT32:
                LoadAttributeV1<GA_STORECLASS_INT, int32, GA_STORE_INT32>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_INT64:
                LoadAttributeV1<GA_STORECLASS_INT, int32, GA_STORE_INT64>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_REAL16:
                LoadAttributeV1<GA_STORECLASS_FLOAT, float, GA_STORE_REAL16>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_REAL32:
                LoadAttributeV1<GA_STORECLASS_FLOAT, float, GA_STORE_REAL32>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_REAL64:
                LoadAttributeV1<GA_STORECLASS_FLOAT, float, GA_STORE_REAL64>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_STRING:
                LoadAttributeV1<GA_STORECLASS_STRING, std::string, GA_STORE_STRING>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            default:
                continue;
            }
        }
    }

    template <GA_StorageClass StorageClass, typename DataType, GA_Storage StorageType>
    void LoadAttributeV2(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset, const nlohmann::json& valuesContainer,
                         const std::string& name)
    {
        if (!valuesContainer.is_array())
        {
            return;
        }

        int arraySize = static_cast<int>(valuesContainer.size());

        using JSONUnderlyingType = typename JSONSafeTypeMapping<DataType>::UnderlyingType;

        std::vector<DataType> valuesArray{};
        valuesArray.reserve(arraySize);
        for (const nlohmann::json& val : valuesContainer)
        {
            if (!IsCompatibleType<JSONUnderlyingType>(val))
            {
                return;
            }

            valuesArray.emplace_back(FromJSONSafeType<DataType>(val.get<JSONUnderlyingType>()));
        }

        GA_Attribute* attribute = gdp->addTuple(StorageType, owner, name, arraySize);
        GA_RWHandleT<DataType> attrRWHandle{attribute};
        attrRWHandle.setV(mapOffset, valuesArray.data(), arraySize);
    }

    template <>
    void LoadAttributeV2<GA_STORECLASS_STRING, std::string, GA_STORE_STRING>(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset,
                                                                             const nlohmann::json& valuesContainer,
                                                                             const std::string& name)
    {
        if (!valuesContainer.is_array())
        {
            return;
        }

        int arraySize = static_cast<int>(valuesContainer.size());

        std::vector<UT_StringHolder> valuesArray;
        valuesArray.reserve(arraySize);
        for (const nlohmann::json& val : valuesContainer)
        {
            if (!IsCompatibleType<std::string>(val))
            {
                return;
            }

            valuesArray.emplace_back(val.get<std::string>());
        }

        GA_Attribute* attribute = gdp->addStringTuple(owner, name, arraySize);
        GA_RWHandleS attrRWHandle{attribute};
        for (int i = 0; i < arraySize; ++i)
        {
            attrRWHandle.set(mapOffset, i, valuesArray[i]);
        }
    }

    template <>
    void LoadAttributeV2<GA_STORECLASS_DICT, std::string, GA_STORE_DICT>(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset,
                                                                         const nlohmann::json& valuesContainer, const std::string& name)
    {
        if (!valuesContainer.is_array())
        {
            return;
        }

        int arraySize = static_cast<int>(valuesContainer.size());

        std::vector<UT_OptionsHolder> valuesArray;
        valuesArray.reserve(arraySize);
        for (const nlohmann::json& val : valuesContainer)
        {
            if (!IsCompatibleType<std::string>(val))
            {
                return;
            }

            std::string jsonString = val.get<std::string>();

            UT_JSONValue jsonObject;
            std::istringstream stringStream(jsonString);

            if (!jsonObject.parseValue(stringStream))
            {
                return;
            }

            if (!jsonObject.isMap())
            {
                return;
            }

            const UT_JSONValueMap* jsonMap = jsonObject.getMap();

            UT_Options options;
            options.load(*jsonMap, false);

            valuesArray.emplace_back(&options);
        }

        GA_Attribute* attribute = gdp->addDictTuple(owner, name, arraySize);
        GA_RWHandleDict attrRWHandle{attribute};
        for (int i = 0; i < arraySize; ++i)
        {
            attrRWHandle.set(mapOffset, i, valuesArray[i]);
        }
    }

    void LoadAttributesV2(GU_Detail* gdp, GA_AttributeOwner owner, GA_Offset mapOffset, const nlohmann::json& meta) noexcept
    {
        if (!meta.is_object())
        {
            return;
        }

        for (const auto& [attribName, attrContainer] : meta.items())
        {
            if (!attrContainer.is_object())
            {
                continue;
            }

            auto typeIter = attrContainer.find("t");
            auto valueIter = attrContainer.find("v");

            if (typeIter == attrContainer.end() || valueIter == attrContainer.end())
            {
                continue;
            }
            if (!typeIter->is_number_unsigned())
            {
                continue;
            }

            GA_Storage storage = static_cast<GA_Storage>(typeIter->get<int>());

            switch (storage)
            {
            case GA_STORE_BOOL:
                LoadAttributeV2<GA_STORECLASS_INT, int32, GA_STORE_BOOL>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_UINT8:
                LoadAttributeV2<GA_STORECLASS_INT, int32, GA_STORE_UINT8>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_INT8:
                LoadAttributeV2<GA_STORECLASS_INT, int32, GA_STORE_INT8>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_INT16:
                LoadAttributeV2<GA_STORECLASS_INT, int32, GA_STORE_INT16>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_INT32:
                LoadAttributeV2<GA_STORECLASS_INT, int32, GA_STORE_INT32>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_INT64:
                LoadAttributeV2<GA_STORECLASS_INT, int64, GA_STORE_INT64>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_REAL16:
                LoadAttributeV2<GA_STORECLASS_FLOAT, fpreal32, GA_STORE_REAL16>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_REAL32:
                LoadAttributeV2<GA_STORECLASS_FLOAT, fpreal32, GA_STORE_REAL32>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_REAL64:
                LoadAttributeV2<GA_STORECLASS_FLOAT, fpreal64, GA_STORE_REAL64>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_STRING:
                LoadAttributeV2<GA_STORECLASS_STRING, std::string, GA_STORE_STRING>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            case GA_STORE_DICT:
                LoadAttributeV2<GA_STORECLASS_DICT, std::string, GA_STORE_DICT>(gdp, owner, mapOffset, *valueIter, attribName);
                break;
            default:
                continue;
            }
        }
    }

    template<typename MetaTarget>
    void StoreOpenVDBIntMeta(MetaTarget target, const std::string& name, const nlohmann::json::const_iterator& valueIter)
    {
        if (valueIter->is_number_integer())
        {
            int value = valueIter->template get<int>();
            target->insertMeta(name, openvdb::Int32Metadata(value));
        }
        else if (valueIter->is_array())
        {
            size_t arraySize = valueIter->size();
            
            std::vector<int> values;
            values.reserve(arraySize);
            for (const auto& val : *valueIter)
            {
                if (!val.is_number_integer())
                {
                    return;
                }
                values.push_back(val.template get<int>());
            }
            
            switch (arraySize)
            {
            case 1:
                target->insertMeta(name, openvdb::Int32Metadata(values[0]));
                break;
            case 2:
                target->insertMeta(name, openvdb::Vec2IMetadata(openvdb::Vec2i(values.data())));
                break;
            case 3:
                target->insertMeta(name, openvdb::Vec3IMetadata(openvdb::Vec3i(values.data())));
                break;
            default:
                return;
            }
        }
    }

    template<typename MetaTarget>
    void StoreOpenVDBInt64Meta(MetaTarget target, const std::string& name, const nlohmann::json::const_iterator& valueIter)
    {
        if (valueIter->is_string())
        {
            int64 value = std::stoll(valueIter->template get<std::string>());
            target->insertMeta(name, openvdb::Int64Metadata(value));
        }
        else if (valueIter->is_number_integer())
        {
            int64 value = valueIter->template get<int64>();
            target->insertMeta(name, openvdb::Int64Metadata(value));
        }
        else if (valueIter->is_array())
        {
            size_t arraySize = valueIter->size();
            
            std::vector<int64> values;
            values.reserve(arraySize);
            for (const auto& val : *valueIter)
            {
                if (val.is_string())
                {
                    values.push_back(std::stoll(val.template get<std::string>()));
                }
                else if (val.is_number_integer())
                {
                    values.push_back(val.template get<int64>());
                }
                else
                {
                    return;
                }
            }
            
            switch (arraySize)
            {
            case 1:
                target->insertMeta(name, openvdb::Int64Metadata(values[0]));
                break;
            case 2:
                target->insertMeta(name, openvdb::Vec2IMetadata(openvdb::Vec2i(static_cast<int>(values[0]), static_cast<int>(values[1]))));
                break;
            case 3:
                target->insertMeta(name, openvdb::Vec3IMetadata(openvdb::Vec3i(static_cast<int>(values[0]), static_cast<int>(values[1]), static_cast<int>(values[2]))));
                break;
            default:
                return;
            }
        }
    }

    template<typename MetaTarget>
    void StoreOpenVDBFloatMeta(MetaTarget target, const std::string& name, const nlohmann::json::const_iterator& valueIter)
    {
        if (valueIter->is_number())
        {
            float value = valueIter->template get<float>();
            target->insertMeta(name, openvdb::FloatMetadata(value));
        }
        else if (valueIter->is_array())
        {
            size_t arraySize = valueIter->size();
            
            std::vector<float> values;
            values.reserve(arraySize);
            for (const auto& val : *valueIter)
            {
                if (!val.is_number())
                {
                    return;
                }
                values.push_back(val.template get<float>());
            }
            
            switch (arraySize)
            {
            case 1:
                target->insertMeta(name, openvdb::FloatMetadata(values[0]));
                break;
            case 2:
                target->insertMeta(name, openvdb::Vec2DMetadata(openvdb::Vec2d(values[0], values[1])));
                break;
            case 3:
                target->insertMeta(name, openvdb::Vec3DMetadata(openvdb::Vec3d(values[0], values[1], values[2])));
                break;
            default:
                return;
            }
        }
    }

    template<typename MetaTarget>
    void StoreOpenVDBDoubleMeta(MetaTarget target, const std::string& name, const nlohmann::json::const_iterator& valueIter)
    {
        if (valueIter->is_number())
        {
            double value = valueIter->template get<double>();
            target->insertMeta(name, openvdb::DoubleMetadata(value));
        }
        else if (valueIter->is_array())
        {
            size_t arraySize = valueIter->size();
            
            std::vector<double> values;
            values.reserve(arraySize);
            for (const auto& val : *valueIter)
            {
                if (!val.is_number())
                {
                    return;
                }
                values.push_back(val.template get<double>());
            }
            
            switch (arraySize)
            {
            case 1:
                target->insertMeta(name, openvdb::DoubleMetadata(values[0]));
                break;
            case 2:
                target->insertMeta(name, openvdb::Vec2DMetadata(openvdb::Vec2d(values.data())));
                break;
            case 3:
                target->insertMeta(name, openvdb::Vec3DMetadata(openvdb::Vec3d(values.data())));
                break;
            default:
                return;
            }
        }
    }

    template<typename MetaTarget>
    void LoadAttributesV1(MetaTarget target, const nlohmann::json& meta) noexcept
    {
        if (!meta.is_object())
        {
            return;
        }

        for (const auto& [attribName, attrContainer] : meta.items())
        {
            if (!attrContainer.is_object())
            {
                continue;
            }

            auto typeIter = attrContainer.find("t");
            auto valueIter = attrContainer.find("v");

            if (typeIter == attrContainer.end() || valueIter == attrContainer.end())
            {
                continue;
            }
            if (!typeIter->is_string())
            {
                continue;
            }

            GA_Storage storage = StrTypeToGAStorage(typeIter->template get<std::string>());

            switch (storage)
            {
            case GA_STORE_BOOL:
            case GA_STORE_UINT8:
                // Unimplemented in V1
                break;
            case GA_STORE_INT8:
            case GA_STORE_INT16:
            case GA_STORE_INT32:
                StoreOpenVDBIntMeta(target, attribName, valueIter);
                break;
            case GA_STORE_INT64:
                StoreOpenVDBInt64Meta(target, attribName, valueIter);
                break;
            case GA_STORE_REAL16:
            case GA_STORE_REAL32:
                StoreOpenVDBFloatMeta(target, attribName, valueIter);
                break;
            case GA_STORE_REAL64:
                StoreOpenVDBDoubleMeta(target, attribName, valueIter);
                break;
            case GA_STORE_STRING:
                target->insertMeta(attribName, openvdb::StringMetadata(valueIter->template get<std::string>()));
                break;
            default:
                continue;
            }
        }
    }

    template<typename MetaTarget>
    void LoadAttributesV2(MetaTarget target, const nlohmann::json& meta) noexcept
    {
        if (!meta.is_object())
        {
            return;
        }

        for (const auto& [attribName, attrContainer] : meta.items())
        {
            if (!attrContainer.is_object())
            {
                continue;
            }

            auto typeIter = attrContainer.find("t");
            auto valueIter = attrContainer.find("v");

            if (typeIter == attrContainer.end() || valueIter == attrContainer.end())
            {
                continue;
            }
            if (!typeIter->is_number_unsigned())
            {
                continue;
            }

            GA_Storage storage = static_cast<GA_Storage>(typeIter->template get<int>());

            switch (storage)
            {
            case GA_STORE_BOOL:
                if (valueIter->is_boolean())
                {
                    target->insertMeta(attribName, openvdb::BoolMetadata(valueIter->template get<bool>()));
                }
                break;
            case GA_STORE_UINT8:
                if (valueIter->is_number_unsigned())
                {
                    target->insertMeta(attribName, openvdb::Int32Metadata(valueIter->template get<int>()));
                }
                break;
            case GA_STORE_INT8:
            case GA_STORE_INT16:
            case GA_STORE_INT32:
                StoreOpenVDBIntMeta(target, attribName, valueIter);
                break;
            case GA_STORE_INT64:
                StoreOpenVDBInt64Meta(target, attribName, valueIter);
                break;
            case GA_STORE_REAL16:
            case GA_STORE_REAL32:
                StoreOpenVDBFloatMeta(target, attribName, valueIter);
                break;
            case GA_STORE_REAL64:
                StoreOpenVDBDoubleMeta(target, attribName, valueIter);
                break;
            case GA_STORE_STRING:
            case GA_STORE_DICT:
                target->insertMeta(attribName, openvdb::StringMetadata(valueIter->template get<std::string>()));
                break;
            default:
                continue;
            }
        }
    }

    template void LoadAttributesV1<openvdb::GridBase::Ptr>(openvdb::GridBase::Ptr target, const nlohmann::json& meta) noexcept;
    template void LoadAttributesV1<openvdb::MetaMap*>(openvdb::MetaMap* target, const nlohmann::json& meta) noexcept;
    template void LoadAttributesV2<openvdb::GridBase::Ptr>(openvdb::GridBase::Ptr target, const nlohmann::json& meta) noexcept;
    template void LoadAttributesV2<openvdb::MetaMap*>(openvdb::MetaMap* target, const nlohmann::json& meta) noexcept;

    void LoadAttributesV1(openvdb::GridBase::Ptr target, const nlohmann::json& meta) noexcept
    {
        LoadAttributesV1<openvdb::GridBase::Ptr>(target, meta);
    }

    void LoadAttributesV1(openvdb::MetaMap* target, const nlohmann::json& meta) noexcept  
    {
        LoadAttributesV1<openvdb::MetaMap*>(target, meta);
    }

    void LoadAttributesV2(openvdb::GridBase::Ptr target, const nlohmann::json& meta) noexcept
    {
        LoadAttributesV2<openvdb::GridBase::Ptr>(target, meta);
    }

    void LoadAttributesV2(openvdb::MetaMap* target, const nlohmann::json& meta) noexcept
    {
        LoadAttributesV2<openvdb::MetaMap*>(target, meta);
    }
} // namespace Zibra::Utils