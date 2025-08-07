#include "PrecompiledHeader.h"

#include "SOP_ZibraVDBUSDExport.h"

#include <GA/GA_Handle.h>
#include <GA/GA_Iterator.h>
#include <GA/GA_Types.h>
#include <GEO/GEO_Primitive.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPoly.h>
#include <UT/UT_Error.h>
#include <algorithm>
#include <iostream>

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
        static std::vector<PRM_Template> templateList;
        if (!templateList.empty())
        {
            return templateList.data();
        }

        static PRM_Name theFileName(FILENAME_PARAM_NAME, "Out File");
        static PRM_Default theFileDefault(0, "$HIP/vol/$HIPNAME.$OS.zibravdb");

        templateList.emplace_back(PRM_FILE, 1, &theFileName, &theFileDefault);

        static PRM_Name theQualityName(QUALITY_PARAM_NAME, "Quality");
        static PRM_Default theQualityDefault(0.6, nullptr);
        static PRM_Range theQualityRange(PRM_RANGE_RESTRICTED, 0.0f, PRM_RANGE_RESTRICTED, 1.0f);

        templateList.emplace_back(PRM_FLT, 1, &theQualityName, &theQualityDefault, nullptr, &theQualityRange);

        static PRM_Name theUsePerChannelCompressionSettingsName(USE_PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME,
                                                                "Use per Channel Compression Settings");

        templateList.emplace_back(PRM_TOGGLE, 1, &theUsePerChannelCompressionSettingsName);

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

        templateList.emplace_back(PRM_MULTITYPE_LIST, thePerChannelCompressionSettingsTemplates, 2,
                                  &thePerChannelCompressionSettingsName[0], nullptr, nullptr, nullptr, nullptr,
                                  &thePerChannelCompressionSettingsNameCondition);

        static PRM_Name theOpenPluginManagementButtonName(OPEN_PLUGIN_MANAGEMENT_BUTTON_NAME, "Open Plugin Management");

        templateList.emplace_back(PRM_CALLBACK, 1, &theOpenPluginManagementButtonName, nullptr, nullptr, nullptr,
                                  &Zibra::ZibraVDBDecompressor::SOP_ZibraVDBDecompressor::OpenManagementWindow);

        templateList.emplace_back();
        return templateList.data();
    }

    SOP_ZibraVDBUSDExport::SOP_ZibraVDBUSDExport(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : SOP_Node(net, name, entry)
    {
        mySopFlags.setManagesDataIDs(true);
    }

    OP_ERROR SOP_ZibraVDBUSDExport::cookMySop(OP_Context& context)
    {
        int num_inputs = nConnectedInputs();
        if (num_inputs == 0)
        {
            addError(SOP_MESSAGE, "No input geometry connected");
            return error();
        }

        gdp->clearAndDestroy();
        OP_ERROR lock_error = lockInput(0, context);
        if (lock_error != 0)
        {
            addError(SOP_MESSAGE, "Failed to lock input geometry");
            return error();
        }

        OP_ERROR dup_error = duplicateSource(0, context);
        unlockInput(0);
        if (dup_error != 0)
        {
            addError(SOP_MESSAGE, "Failed to duplicate input geometry");
            return error();
        }

        if (gdp->getNumPrimitives() == 0)
        {
            exint current_frame = context.getFrame();
            addWarning(SOP_MESSAGE, ("No primitives in input geometry - frame " + std::to_string(current_frame) + " may be empty").c_str());
            return error();
        }

        GA_RWHandleS savePathAttrib(gdp, GA_ATTRIB_PRIMITIVE, "usdvolumesavepath");
        if (!savePathAttrib.isValid())
        {
            GA_RWAttributeRef attr_ref = gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "usdvolumesavepath", 1);
            if (attr_ref.isValid())
            {
                savePathAttrib = GA_RWHandleS(attr_ref.getAttribute());
            }
            else
            {
                addError(SOP_MESSAGE, "Failed to create primitive attribute");
                return error();
            }
        }

        m_OrderedChannelNames.clear();
        m_OrderedChannelNames.reserve(8);
        const GEO_Primitive *prim;
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                auto vdbPrim = dynamic_cast<const GEO_PrimVDB*>(prim);
                auto gridName = vdbPrim->getGridName();
                if (m_OrderedChannelNames.size() >= 8)
                {
                    std::string m = "Input has quantity of VDB primitives greater than 8 supported. Skipping '"s + gridName + "'.";
                    addError(ROP_MESSAGE, m.c_str());
                    break;
                }
                exint current_frame = context.getFrame();

                UT_String output_filepath;
                evalString(output_filepath, FILENAME_PARAM_NAME, 0, context.getTime());
                std::string filename = output_filepath.toStdString();

                std::string node_path = getFullPath().toStdString();
                std::string encoded_node_path = node_path;
                size_t pos = 0;
                while ((pos = encoded_node_path.find('/', pos)) != std::string::npos)
                {
                    encoded_node_path.replace(pos, 1, "%2F");
                    pos += 3;
                }

                std::string new_path = filename + "?node=" + encoded_node_path +
                                       "&frame=" + std::to_string(current_frame);
                savePathAttrib.set(prim->getMapOffset(), new_path.c_str());
                m_OrderedChannelNames.emplace_back(gridName);
            }
        }

        return error();
    }

    bool SOP_ZibraVDBUSDExport::updateParmsFlags()
    {
        bool changed = SOP_Node::updateParmsFlags();
        flags().setTimeDep(true);
        return changed;
    }

    std::vector<std::pair<UT_String, float>> SOP_ZibraVDBUSDExport::getPerChannelCompressionSettings() const noexcept
    {
        std::vector<std::pair<UT_String, float>> perChannelCompressionSettings;

        // Just in case evalInt return invalid number.
        constexpr int maxPerChannelSettingsCount = 1024;
        const int perChannelSettingsCount = std::max(
            0, std::min(static_cast<int>(evalInt(PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME, 0, 0)), maxPerChannelSettingsCount));

        for (int i = 0; i < perChannelSettingsCount; ++i)
        {
            // Houdini starts count of parameters in list from 1 (not 0).
            const auto perChannelSettingsID = i + 1;
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

            if (std::find(m_OrderedChannelNames.begin(), m_OrderedChannelNames.end(), channelNameStr.toStdString()) ==
                m_OrderedChannelNames.end())
            {
                continue;
            }
            float quality = static_cast<float>(evalFloat(qualityParamNameStr.c_str(), 0, 0));

            perChannelCompressionSettings.emplace_back(channelNameStr, quality);
            perChannelCompressionSettings.back().first.harden();
        }

        return perChannelCompressionSettings;
    }

    bool SOP_ZibraVDBUSDExport::usePerChannelCompressionSettings() const noexcept
    {
        return evalInt(USE_PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME, 0, 0) != 0;
    }

    float SOP_ZibraVDBUSDExport::getCompressionQuality() const noexcept
    {
        return static_cast<float>(evalFloat(QUALITY_PARAM_NAME, 0, 0));
    }
} // namespace Zibra::ZibraVDBUSDExport