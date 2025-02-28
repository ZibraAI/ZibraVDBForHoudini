#include "PrecompiledHeader.h"

#include "ROP_ZibraVDBCompressor.h"

#include "bridge//Licensing/Licensing.h"
#include "bridge/LibraryUtils.h"
#include "openvdb/OpenVDBDecoder.h"
#include "utils/GAAttributesDump.h"
#include "utils/LibraryDownloadManager.h"

namespace Zibra::ZibraVDBCompressor
{
    using namespace std::literals;

    ROP_ZibraVDBCompressor_Operator::ROP_ZibraVDBCompressor_Operator(ContextType contextType) noexcept
        : OP_Operator(GetNodeName(contextType), GetNodeLabel(contextType), GetConstructorForContext(contextType),
                      ROP_ZibraVDBCompressor::GetTemplatePairs(contextType), GetMinSources(contextType), GetMaxSources(contextType),
                      ROP_ZibraVDBCompressor::GetVariablePair(contextType), GetOperatorFlags(contextType), GetSourceLabels(contextType),
                      GetMaxOutputs(contextType))
        , m_ContextType(contextType)
    {
        setIconName(ZIBRAVDB_ICON_PATH);
        setOpTabSubMenuPath(ZIBRAVDB_NODES_TAB_NAME);
    }

    const UT_StringHolder& ROP_ZibraVDBCompressor_Operator::getDefaultShape() const
    {
        static UT_StringHolder shapeSOP{"clipped_left"};
        switch (m_ContextType)
        {
        case ContextType::SOP:
            return shapeSOP;
        case ContextType::OUT:
            return OP_Operator::getDefaultShape();
        default:
            assert(0);
            return shapeSOP;
        }
    }

    UT_Color ROP_ZibraVDBCompressor_Operator::getDefaultColor() const
    {
        switch (m_ContextType)
        {
        case ContextType::SOP:
            return UT_Color{UT_RGB, 0.65, 0.4, 0.5};
        case ContextType::OUT:
            return OP_Operator::getDefaultColor();
        default:
            assert(0);
            return UT_Color{UT_RGB, 0.65, 0.4, 0.5};
        }
    }

    OP_Constructor ROP_ZibraVDBCompressor_Operator::GetConstructorForContext(ContextType contextType)
    {
        switch (contextType)
        {
        case ContextType::SOP:
            return ROP_ZibraVDBCompressor::ConstructorSOPContext;
        case ContextType::OUT:
            return ROP_ZibraVDBCompressor::ConstructorOUTContext;
        default:
            assert(0);
            return ROP_ZibraVDBCompressor::ConstructorSOPContext;
        }
    }

    unsigned ROP_ZibraVDBCompressor_Operator::GetOperatorFlags(ContextType contextType)
    {
        switch (contextType)
        {
        case ContextType::SOP:
            return OP_FLAG_GENERATOR | OP_FLAG_MANAGER;
        case ContextType::OUT:
            return OP_FLAG_UNORDERED | OP_FLAG_GENERATOR;
        default:
            assert(0);
            return OP_FLAG_GENERATOR;
        }
    }

    unsigned ROP_ZibraVDBCompressor_Operator::GetMinSources(ContextType contextType)
    {
        switch (contextType)
        {
        case ContextType::SOP:
            return 1;
        case ContextType::OUT:
            return 0;
        default:
            assert(0);
            return 1;
        }
    }

    unsigned ROP_ZibraVDBCompressor_Operator::GetMaxSources(ContextType contextType)
    {
        switch (contextType)
        {
        case ContextType::SOP:
            return 1;
        case ContextType::OUT:
            return OP_MULTI_INPUT_MAX;
        default:
            assert(0);
            return 1;
        }
    }

    const char* ROP_ZibraVDBCompressor_Operator::GetNodeLabel(ContextType contextType)
    {
        return NODE_LABEL;
    }

    const char* ROP_ZibraVDBCompressor_Operator::GetNodeName(ContextType contextType)
    {
        switch (contextType)
        {
        case Zibra::ContextType::SOP:
            return NODE_NAME_SOP_CONTEXT;
            break;
        case Zibra::ContextType::OUT:
            return NODE_NAME_OUT_CONTEXT;
            break;
        default:
            assert(0);
            return NODE_NAME_SOP_CONTEXT;
        }
    }

    unsigned ROP_ZibraVDBCompressor_Operator::GetMaxOutputs(ContextType contextType)
    {
        switch (contextType)
        {
        case ContextType::SOP:
            return 0;
        case ContextType::OUT:
            return 1;
        default:
            assert(0);
            return 0;
        }
    }

    const char** ROP_ZibraVDBCompressor_Operator::GetSourceLabels(ContextType contextType)
    {
        static const char* SOP_LABELS[] = {"OpenVDB to Compress", nullptr};
        static const char* OUT_LABELS[] = {nullptr};

        switch (contextType)
        {
        case ContextType::SOP:
            return SOP_LABELS;
        case ContextType::OUT:
            return OUT_LABELS;
        default:
            assert(0);
            return SOP_LABELS;
        }
    }

    OP_Node* ROP_ZibraVDBCompressor::ConstructorSOPContext(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new ROP_ZibraVDBCompressor{ContextType::SOP, net, name, op};
    }

    OP_Node* ROP_ZibraVDBCompressor::ConstructorOUTContext(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new ROP_ZibraVDBCompressor{ContextType::OUT, net, name, op};
    }

    std::vector<PRM_Template>& ROP_ZibraVDBCompressor::GetTemplateListContainer(ContextType contextType)
    {
        static std::vector<PRM_Template> templateListSOP;
        static std::vector<PRM_Template> templateListOUT;
        switch (contextType)
        {
        case ContextType::SOP:
            return templateListSOP;
        case ContextType::OUT:
            return templateListOUT;
        default:
            assert(0);
            return templateListSOP;
        }
    }

    PRM_Template* ROP_ZibraVDBCompressor::GetTemplateList(ContextType contextType) noexcept
    {
        std::vector<PRM_Template>& templateList = GetTemplateListContainer(contextType);

        if (!templateList.empty())
        {
            return templateList.data();
        }

        if (contextType == ContextType::OUT)
        {
            static PRM_Name theInputSOP(INPUT_SOP_PARAM_NAME, "SOP Path");
            templateList.push_back(PRM_Template{PRM_STRING, PRM_TYPE_DYNAMIC_PATH, 1, &theInputSOP, 0, 0, 0, 0, &PRM_SpareData::sopPath});
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

        templateList.push_back(theRopTemplates[ROP_TPRERENDER_TPLATE]);
        templateList.push_back(theRopTemplates[ROP_PRERENDER_TPLATE]);
        templateList.push_back(theRopTemplates[ROP_LPRERENDER_TPLATE]);
        templateList.push_back(theRopTemplates[ROP_TPREFRAME_TPLATE]);
        templateList.push_back(theRopTemplates[ROP_PREFRAME_TPLATE]);
        templateList.push_back(theRopTemplates[ROP_LPREFRAME_TPLATE]);
        templateList.push_back(theRopTemplates[ROP_TPOSTFRAME_TPLATE]);
        templateList.push_back(theRopTemplates[ROP_POSTFRAME_TPLATE]);
        templateList.push_back(theRopTemplates[ROP_LPOSTFRAME_TPLATE]);
        templateList.push_back(theRopTemplates[ROP_TPOSTRENDER_TPLATE]);
        templateList.push_back(theRopTemplates[ROP_POSTRENDER_TPLATE]);
        templateList.push_back(theRopTemplates[ROP_LPOSTRENDER_TPLATE]);

        static PRM_Name theDownloadLibraryButtonName(DOWNLOAD_LIBRARY_BUTTON_NAME, "Download Library");
        templateList.emplace_back(PRM_CALLBACK, 1, &theDownloadLibraryButtonName, nullptr, nullptr, nullptr,
                                  &ROP_ZibraVDBCompressor::DownloadLibrary);

        templateList.emplace_back();
        return templateList.data();
    }

    OP_TemplatePair* ROP_ZibraVDBCompressor::GetTemplatePairs(ContextType contextType) noexcept
    {
        static PRM_Template ROPTemplates[] = {theRopTemplates[ROP_RENDER_TPLATE],   theRopTemplates[ROP_RENDERBACKGROUND_TPLATE],
                                              theRopTemplates[ROP_PREVIEW_TPLATE],  theRopTemplates[ROP_RENDERDIALOG_TPLATE],
                                              theRopTemplates[ROP_TRANGE_TPLATE],   theRopTemplates[ROP_FRAMERANGE_TPLATE],
                                              theRopTemplates[ROP_TAKENAME_TPLATE], PRM_Template()};

        static OP_TemplatePair BaseSOPContext{GetTemplateList(ContextType::SOP)};
        static OP_TemplatePair ROPPair2SOPContext{ROPTemplates, &BaseSOPContext};

        static OP_TemplatePair BaseOUTContext{GetTemplateList(ContextType::OUT)};
        static OP_TemplatePair ROPPair2OUTContext{ROPTemplates, &BaseOUTContext};

        switch (contextType)
        {
        case ContextType::SOP:
            return &ROPPair2SOPContext;
        case ContextType::OUT:
            return &ROPPair2OUTContext;
        default:
            assert(0);
            return &ROPPair2SOPContext;
        }
    }

    OP_VariablePair* ROP_ZibraVDBCompressor::GetVariablePair(ContextType contextType) noexcept
    {
        static OP_VariablePair pair{ROP_Node::myVariableList};
        return &pair;
    }

    ROP_ZibraVDBCompressor::ROP_ZibraVDBCompressor(ContextType contextType, OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : ROP_Node{net, name, entry}
        , m_ContextType(contextType)
    {
    }

    ROP_ZibraVDBCompressor::~ROP_ZibraVDBCompressor() noexcept = default;

    int ROP_ZibraVDBCompressor::startRender(const int nFrames, const fpreal tStart, const fpreal tEnd)
    {
        using namespace std::string_literals;
        if (!Zibra::LibraryUtils::IsPlatformSupported())
        {
            addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED);
            return ROP_ABORT_RENDER;
        }

        Zibra::LibraryUtils::LoadLibrary();

        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING);
            return ROP_ABORT_RENDER;
        }

        if (!CE::Licensing::CAPI::CheckoutLicenseWithKey(LicenseManager::GetKey().c_str()) &&
            !CE::Licensing::CAPI::CheckoutLicenseOffline(LicenseManager::GetOfflineLicense().c_str()))
        {
            addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_LICENSE_ERROR);
            return ROP_ABORT_RENDER;
        }

        if (!m_RHIWrapper)
        {
            m_RHIWrapper = new RHIWrapper();
            m_RHIWrapper->Initialize();
        }

        m_EndTime = tEnd;
        m_StartTime = tStart;

        using namespace std::literals;
        OP_Context ctx(tStart);
        const int thread = ctx.getThread();

        OP_AutoLockInputs inputs{this};
        if (inputs.lock(ctx) >= UT_ERROR_ABORT)
            return false;

        switch (m_ContextType)
        {
        case ContextType::SOP: {
            // 0 index referring to the node's first input
            m_InputSOP = CAST_SOPNODE(getInputFollowingOutputs(0));
            if (!m_InputSOP)
            {
                addError(ROP_MESSAGE, "No inputs detected. First input must be an OpenVDB source.");
                return ROP_ABORT_RENDER;
            }
            break;
        }
        case ContextType::OUT: {
            // Reads soppath parameter
            UT_String SOPPath = "";
            evalString(SOPPath, INPUT_SOP_PARAM_NAME, 0, 0, tStart);
            OP_Node* node = findNode(SOPPath);
            m_InputSOP = CAST_SOPNODE(node);
            if (!m_InputSOP)
            {
                addError(ROP_MESSAGE, "No inputs detected. Please make sure that SOP Path is correct.");
                return ROP_ABORT_RENDER;
            }
            break;
        }
        default: {
            assert(0);
            addError(ROP_MESSAGE, "Internal Error, Unkown context type.");
            return ROP_ABORT_RENDER;
        }
        }

        const GU_Detail* gdp = m_InputSOP->getCookedGeoHandle(ctx, 0).gdp();
        if (!gdp)
        {
            addError(ROP_MESSAGE, "Failed to cook input SOP geometry.");
            return ROP_ABORT_RENDER;
        }

        m_OrderedChannelNames.clear();
        m_OrderedChannelNames.reserve(8);
        const GEO_Primitive* prim;
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

                const auto voxelSize = openvdb::Vec3f(vdbPrim->getGrid().voxelSize());
                const auto hasUniformVoxelSize = (std::abs(voxelSize[0] - voxelSize[1]) < std::numeric_limits<float>::epsilon()) &&
                                                 (std::abs(voxelSize[1] - voxelSize[2]) < std::numeric_limits<float>::epsilon());
                if (!hasUniformVoxelSize)
                {
                    std::string m = "Voxel size of VDB grid '"s + gridName + "' is non uniform. ZibraVDB supports only uniform voxels.";
                    addError(ROP_MESSAGE, m.c_str());
                    return ROP_ABORT_RENDER;
                }
                m_OrderedChannelNames.emplace_back(gridName);
            }
        }
        if (m_OrderedChannelNames.empty())
        {
            addError(ROP_MESSAGE, "No input VDB primitives found.");
            return ROP_ABORT_RENDER;
        }

        if (CreateCompressor(tStart) == ROP_ABORT_RENDER)
        {
            addError(ROP_MESSAGE, "Failed to create compressor instance.");
            return ROP_ABORT_RENDER;
        }

        if (m_Compressor->StartSequence() != CE::ReturnCode::ZCE_SUCCESS)
        {
            addError(ROP_MESSAGE, "Failed to start sequence compression.");
            return ROP_ABORT_RENDER;
        }

        UT_String filename = "";
        evalString(filename, FILENAME_PARAM_NAME, nullptr, 0, tStart);
        m_Ofstream.open(filename, std::ios::binary);

        if (!m_Ofstream.is_open())
        {
            addError(ROP_MESSAGE, "Failed to open output file.");
            return ROP_ABORT_RENDER;
        }

        if (error() < UT_ERROR_ABORT)
            executePreRenderScript(tStart);

        return ROP_CONTINUE_RENDER;
    }

    ROP_RENDER_CODE ROP_ZibraVDBCompressor::renderFrame(const fpreal time, UT_Interrupt* boss)
    {
        using namespace std::literals;

        assert(Zibra::LibraryUtils::IsLibraryLoaded());

        if (CE::Licensing::CAPI::GetLicenseStatus(CE::Licensing::ProductType::Compression) != CE::Licensing::LicenseStatus::OK)
        {
            addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_LICENSE_ERROR);
            return ROP_ABORT_RENDER;
        }

        executePreFrameScript(time);

        OP_Context ctx(time);
        const int thread = ctx.getThread();

        OP_AutoLockInputs inputs{this};
        if (inputs.lock(ctx) >= UT_ERROR_ABORT)
            return ROP_RETRY_RENDER;

        const GU_Detail* gdp = m_InputSOP->getCookedGeoHandle(ctx, 0).gdp();
        if (!gdp)
        {
            addError(ROP_MESSAGE, "Failed to cook input SOP geometry");
            return ROP_ABORT_RENDER;
        }

        constexpr size_t ACTIVE_VOXELS_CAP = (1024ull * 1024ull * 1024ull * 2ull) / sizeof(float);
        size_t totalFrameActiveVoxelsCount = 0;

        std::set<std::string> channelNamesUniqueStorage{};
        std::vector<const char*> orderedChannelNames{};
        std::vector<openvdb::GridBase::ConstPtr> volumes{};
        const GEO_Primitive* prim;
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                auto vdbPrim = dynamic_cast<const GEO_PrimVDB*>(prim);
                const char* gridName = vdbPrim->getGridName();
                if (channelNamesUniqueStorage.find(gridName) != channelNamesUniqueStorage.cend())
                {
                    std::string m = "ZibraVDB uses grid name as unique key. Node input contains duplicate of '"s + gridName + "' VDB prim.";
                    addError(ROP_MESSAGE, m.c_str());
                    return ROP_ABORT_RENDER;
                }
                if (vdbPrim->getStorageType() != UT_VDB_FLOAT)
                {
                    std::string m = "Unsupported value type for '"s + gridName + "' prim. Only float grids supported.";
                    addError(ROP_MESSAGE, m.c_str());
                    return ROP_ABORT_RENDER;
                }

                volumes.emplace_back(vdbPrim->getGridPtr());
                orderedChannelNames.push_back(gridName);
                channelNamesUniqueStorage.insert(gridName);

                totalFrameActiveVoxelsCount += vdbPrim->getGrid().activeVoxelCount();
            }
        }
        channelNamesUniqueStorage.clear();
        if (volumes.empty())
        {
            std::string m = "Node input at frame "s + std::to_string(ctx.getFrame()) +
                            " has no VDB grids."
                            " Frame will be skipped in resulting sequence.";
            addWarning(ROP_MESSAGE, m.c_str());
            return ROP_CONTINUE_RENDER;
        }
        if (totalFrameActiveVoxelsCount > ACTIVE_VOXELS_CAP)
        {
            addError(ROP_MESSAGE, "Total frame active voxels count is higher that maximum supported (536 870 912 voxels).");
            return ROP_ABORT_RENDER;
        }

        Zibra::CE::Compression::CompressFrameDesc compressFrameDesc{};
        compressFrameDesc.channelsCount = orderedChannelNames.size();
        compressFrameDesc.channels = orderedChannelNames.data();

        Zibra::CE::Compression::FrameManager* frameManager;

        OpenVDBSupport::OpenVDBDecoder reader{volumes.data(), orderedChannelNames.data(), orderedChannelNames.size()};
        OpenVDBSupport::DecodeMetadata decodeMetadata{};
        compressFrameDesc.frame = reader.DecodeFrame(decodeMetadata);

        m_RHIWrapper->StartRecording();

        auto status = m_Compressor->CompressFrame(compressFrameDesc, &frameManager);
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            addError(ROP_MESSAGE, "Failed to compress frame.");
            return ROP_ABORT_RENDER;
        }

        m_RHIWrapper->StopRecording();

        auto attrDump = DumpAttributes(gdp, decodeMetadata);
        for (const auto& [key, val] : attrDump)
        {
            frameManager->AddMetadata(key.c_str(), val.c_str());
        }

        status = frameManager->Finish();
        if (status != CE::ReturnCode::ZCE_SUCCESS)
        {
            addError(ROP_MESSAGE, "Failed to dump frame data.");
            return ROP_ABORT_RENDER;
        }

        if (error() < UT_ERROR_ABORT)
        {
            executePostFrameScript(time);
        }
        return ROP_CONTINUE_RENDER;
    }

    ROP_RENDER_CODE ROP_ZibraVDBCompressor::endRender()
    {
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            return ROP_ABORT_RENDER;
        }

        if (m_Compressor)
        {
            if (error() < UT_ERROR_ABORT)
            {
                Zibra::CE::STDOStreamWrapper ostream(m_Ofstream);
                m_Compressor->FinishSequence(&ostream);
            }
            else
            {
                // TODO
                throw std::exception("Abort not implemented");
            }

            m_Compressor->Release();
        }

        m_Ofstream.close();
        if (error() < UT_ERROR_ABORT)
        {
            executePostRenderScript(m_EndTime);
        }
        return error() < UT_ERROR_ABORT ? ROP_CONTINUE_RENDER : ROP_ABORT_RENDER;
    }

    ROP_RENDER_CODE ROP_ZibraVDBCompressor::CreateCompressor(const fpreal tStart)
    {
        UT_String filename = "";
        evalString(filename, FILENAME_PARAM_NAME, nullptr, 0, tStart);
        std::filesystem::create_directories(std::filesystem::path{filename.c_str()}.parent_path());

        if (!m_Factory)
        {
            m_Factory = Zibra::CE::Compression::CAPI::CreateCompressorFactory();
            if (m_Factory == nullptr)
            {
                addError(ROP_MESSAGE, "Failed to create compressor factory.");
                return ROP_ABORT_RENDER;
            }

            auto status = m_Factory->UseRHI(m_RHIWrapper->GetRHIRuntime());
            if (status != CE::ReturnCode::ZCE_SUCCESS)
            {
                addError(ROP_MESSAGE, "Failed assign RHI to compressor factory.");
                return ROP_ABORT_RENDER;
            }
        }

        const float startFrame = evalFloat("f", 0, tStart);
        const float frameInc = evalFloat("f", 2, tStart);
        CE::Compression::FrameMappingDecs frameMappingDesc;
        frameMappingDesc.sequenceStartIndex = startFrame;
        frameMappingDesc.sequenceIndexIncrement = frameInc;
        m_Factory->SetFrameMapping(frameMappingDesc);

        float defaultQuality = static_cast<float>(evalFloat(QUALITY_PARAM_NAME, 0, tStart));
        m_Factory->SetQuality(defaultQuality);

        UT_String usePerChannelCompressionSettingsString;
        evalString(usePerChannelCompressionSettingsString, USE_PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME, 0, tStart);

        if (usePerChannelCompressionSettingsString == "on")
        {
            // Just in case evalInt return invalid number.
            constexpr int maxPerChannelSettingsCount = 1024;
            const int perChannelSettingsCount = std::max(
                0, std::min(static_cast<int>(evalInt(PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME, 0, tStart)), maxPerChannelSettingsCount));

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
                evalString(channelNameStr, channelNameParamNameStr.c_str(), 0, tStart);

                if (std::find(m_OrderedChannelNames.begin(), m_OrderedChannelNames.end(), channelNameStr.toStdString()) ==
                    m_OrderedChannelNames.end())
                {
                    continue;
                }

                const char* channelName = channelNameStr.c_str();
                float quality = static_cast<float>(evalFloat(qualityParamNameStr.c_str(), 0, tStart));

                m_Factory->OverrideChannelQuality(channelName, quality);
            }
        }

        auto status = m_Factory->Create(&m_Compressor);
        if (status != Zibra::CE::ReturnCode::ZCE_SUCCESS)
        {
            addError(ROP_MESSAGE, "Failed to create compressor instance.");
            return ROP_ABORT_RENDER;
        }

        status = m_Compressor->Initialize();
        if (status != Zibra::CE::ReturnCode::ZCE_SUCCESS)
        {
            addError(ROP_MESSAGE, "Failed to initialize compressor.");
            return ROP_ABORT_RENDER;
        }

        return ROP_CONTINUE_RENDER;
    }

    void ROP_ZibraVDBCompressor::getOutputFile(UT_String& filename)
    {
        evalString(filename, "filename", nullptr, 0, m_StartTime);
    }

    std::vector<std::pair<std::string, std::string>> ROP_ZibraVDBCompressor::DumpAttributes(
        const GU_Detail* gdp, const OpenVDBSupport::DecodeMetadata& decodeMetadata) noexcept
    {
        std::vector<std::pair<std::string, std::string>> result{};

        const GEO_Primitive* prim;
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                auto vdbPrim = dynamic_cast<const GEO_PrimVDB*>(prim);

                nlohmann::json primAttrDump = Utils::DumpAttributesForSingleEntity(gdp, GA_ATTRIB_PRIMITIVE, prim->getMapOffset());
                std::string primKeyName = "houdiniPrimitiveAttributes_"s + vdbPrim->getGridName();
                result.emplace_back(primKeyName, primAttrDump.dump());

                DumpVisualisationAttributes(result, vdbPrim);
            }
        }

        nlohmann::json detailAttrDump = Utils::DumpAttributesForSingleEntity(gdp, GA_ATTRIB_DETAIL, 0);
        result.emplace_back("houdiniDetailAttributes", detailAttrDump.dump());

        DumpDecodeMetadata(result, decodeMetadata);
        return result;
    }

    void ROP_ZibraVDBCompressor::DumpVisualisationAttributes(std::vector<std::pair<std::string, std::string>>& attributes,
                                                             const GEO_PrimVDB* vdbPrim)
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

    int ROP_ZibraVDBCompressor::DownloadLibrary(void* data, int index, fpreal32 time, const PRM_Template* tplate)
    {
        Zibra::UI::LibraryDownloadManager::DownloadLibrary();
        return 0;
    }

    void ROP_ZibraVDBCompressor::DumpDecodeMetadata(std::vector<std::pair<std::string, std::string>>& result,
                                                    const OpenVDBSupport::DecodeMetadata& decodeMetadata)
    {
        std::ostringstream oss;
        oss << decodeMetadata.offsetX << " " << decodeMetadata.offsetY << " " << decodeMetadata.offsetZ;
        result.emplace_back("houdiniDecodeMetadata", oss.str());
    }

} // namespace Zibra::ZibraVDBCompressor