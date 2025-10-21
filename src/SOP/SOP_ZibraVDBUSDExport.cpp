#include "PrecompiledHeader.h"

#include "SOP_ZibraVDBUSDExport.h"

#include "SOP_ZibraVDBDecompressor.h"

namespace Zibra::ZibraVDBUSDExport
{
    using namespace std::literals;

    OP_Node* SOP_ZibraVDBUSDExport::Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new SOP_ZibraVDBUSDExport{net, name, op};
    }

    PRM_Template* SOP_ZibraVDBUSDExport::GetTemplateList() noexcept
    {
        static PRM_Name theFileName(FILENAME_PARAM_NAME, "Out File");
        static PRM_Default theFileDefault(0, "$HIP/vol/$HIPNAME.$OS.zibravdb");

        static PRM_Name theQualityName(QUALITY_PARAM_NAME, "Quality");
        static PRM_Default theQualityDefault(0.6, nullptr);
        static PRM_Range theQualityRange(PRM_RANGE_RESTRICTED, 0.0f, PRM_RANGE_RESTRICTED, 1.0f);

        static PRM_Name theUsePerChannelCompressionSettingsName(USE_PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME,
                                                                "Use per Channel Compression Settings");

        static PRM_Name thePerChannelCompressionSettingsFieldsNames[] = {
            PRM_Name("perchname#", "Channel Name"),
            PRM_Name("perchquality#", "Channel Quality"),
        };

        static PRM_Template thePerChannelCompressionSettingsTemplates[] = {
            PRM_Template(PRM_STRING, 1, &thePerChannelCompressionSettingsFieldsNames[0]),
            PRM_Template(PRM_FLT, 1, &thePerChannelCompressionSettingsFieldsNames[1], &theQualityDefault, nullptr, &theQualityRange),
            PRM_Template()};

        static PRM_Name thePerChannelCompressionSettingsName[] = {
            PRM_Name(PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME, "Number of Channels"),
        };
        static PRM_Conditional thePerChannelCompressionSettingsNameCondition("{ useperchsettings == \"off\" }", PRM_CONDTYPE_DISABLE);

        static PRM_Name theOpenPluginManagementButtonName(OPEN_PLUGIN_MANAGEMENT_BUTTON_NAME, "Open Plugin Management");

        static PRM_Template templateList[] = {PRM_Template(PRM_FILE, 1, &theFileName, &theFileDefault),
                                              PRM_Template(PRM_FLT, 1, &theQualityName, &theQualityDefault, nullptr, &theQualityRange),
                                              PRM_Template(PRM_TOGGLE, 1, &theUsePerChannelCompressionSettingsName),
                                              PRM_Template(PRM_MULTITYPE_LIST, thePerChannelCompressionSettingsTemplates, 2,
                                                           &thePerChannelCompressionSettingsName[0], nullptr, nullptr, nullptr, nullptr,
                                                           &thePerChannelCompressionSettingsNameCondition),
                                              PRM_Template(PRM_CALLBACK, 1, &theOpenPluginManagementButtonName, nullptr, nullptr, nullptr,
                                                           &Zibra::ZibraVDBDecompressor::SOP_ZibraVDBDecompressor::OpenManagementWindow),
                                              PRM_Template()};
        return templateList;
    }

    SOP_ZibraVDBUSDExport::SOP_ZibraVDBUSDExport(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : SOP_Node(net, name, entry)
    {
        flags().setTimeDep(true);
    }

    OP_ERROR SOP_ZibraVDBUSDExport::cookMySop(OP_Context& context)
    {
#define SHOW_ERROR_AND_RETURN(message) \
    addError(SOP_MESSAGE, message);    \
    return error(context);

        if (nConnectedInputs() == 0)
        {
            SHOW_ERROR_AND_RETURN("No input geometry connected")
        }

        gdp->clearAndDestroy();
        if (lockInput(0, context) > 0)
        {
            SHOW_ERROR_AND_RETURN("Failed to lock input geometry")
        }

        if (duplicateSource(0, context) > 0)
        {
            SHOW_ERROR_AND_RETURN("Failed to duplicate input geometry")
        }
        unlockInput(0);

        if (gdp->getNumPrimitives() == 0)
        {
            addWarning(SOP_MESSAGE,
                       ("No primitives in input geometry - frame " + std::to_string(context.getFrame()) + " may be empty").c_str());
            return error(context);
        }

        GA_RWHandleS savePathAttrib(gdp, GA_ATTRIB_PRIMITIVE, "usdvolumesavepath");
        if (!savePathAttrib.isValid())
        {
            GA_RWAttributeRef attrRef = gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "usdvolumesavepath", 1);
            if (attrRef.isValid())
            {
                savePathAttrib = GA_RWHandleS(attrRef.getAttribute());
            }
            else
            {
                SHOW_ERROR_AND_RETURN("Failed to create primitive attribute")
            }
        }

        m_AvailableChannels.clear();
        const GEO_Primitive* prim;
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                const auto vdbPrim = dynamic_cast<const GEO_PrimVDB*>(prim);
                const auto gridName = vdbPrim->getGridName();
                if (m_AvailableChannels.size() >= CE::MAX_CHANNEL_COUNT)
                {
                    const std::string error = "Input has quantity of VDB primitives greater than " + std::to_string(CE::MAX_CHANNEL_COUNT) +
                                              " supported. Skipping '"s + gridName + "'.";
                    addError(SOP_MESSAGE, error.c_str());
                    break;
                }

                UT_String outputFilepath;
                evalString(outputFilepath, FILENAME_PARAM_NAME, 0, context.getTime());
                std::string new_path = outputFilepath.toStdString() + "?"
                                     + Helpers::URI_NODE_PARAM + "=" + getFullPath().toStdString() + "&"
                                     + Helpers::URI_FRAME_PARAM + "=" + std::to_string(context.getFrame());
                savePathAttrib.set(prim->getMapOffset(), new_path.c_str());
                m_AvailableChannels.insert(gridName);
            }
        }

        return error(context);
#undef SHOW_ERROR_AND_RETURN
    }

    std::vector<std::pair<UT_String, float>> SOP_ZibraVDBUSDExport::GetPerChannelCompressionSettings() const noexcept
    {
        std::vector<std::pair<UT_String, float>> perChannelCompressionSettings;

        int perChannelSettingsCount = static_cast<int>(evalInt(PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME, 0, 0));
        perChannelSettingsCount = std::clamp(perChannelSettingsCount, 0, static_cast<int>(CE::MAX_CHANNEL_COUNT));
        for (int i = 0; i < perChannelSettingsCount; ++i)
        {
            // Houdini starts count of parameters in list from 1 (not 0).
            const int perChannelSettingsID = i + 1;
            const std::string channelNameParamNameStr =
                PER_CHANNEL_COMPRESSION_SETTINGS_CHANNEL_NAME_PARAM_NAME + std::to_string(perChannelSettingsID);
            const std::string qualityParamNameStr =
                PER_CHANNEL_COMPRESSION_SETTINGS_QUALITY_PARAM_NAME + std::to_string(perChannelSettingsID);

            if (!hasParm(channelNameParamNameStr.c_str()))
            {
                continue;
            }

            UT_String channelNameStr;
            evalString(channelNameStr, channelNameParamNameStr.c_str(), 0, 0);

            if (m_AvailableChannels.find(channelNameStr.toStdString()) == m_AvailableChannels.end())
            {
                continue;
            }
            float quality = static_cast<float>(evalFloat(qualityParamNameStr.c_str(), 0, 0));

            perChannelCompressionSettings.emplace_back(channelNameStr, quality);
            perChannelCompressionSettings.back().first.harden();
        }

        return perChannelCompressionSettings;
    }

    bool SOP_ZibraVDBUSDExport::UsePerChannelCompressionSettings() const noexcept
    {
        return evalInt(USE_PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME, 0, 0) != 0;
    }

    float SOP_ZibraVDBUSDExport::GetCompressionQuality() const noexcept
    {
        return static_cast<float>(evalFloat(QUALITY_PARAM_NAME, 0, 0));
    }

} // namespace Zibra::ZibraVDBUSDExport