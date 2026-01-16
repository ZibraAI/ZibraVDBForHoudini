#include "PrecompiledHeader.h"

#include "ROP_ZibraVDBCompressor.h"

#include "Zibra/CE/Addons/OpenVDBFrameLoader.h"
#include "Zibra/CE/Common.h"
#include "bridge/LibraryUtils.h"
#include "licensing/HoudiniLicenseManager.h"
#include "ui/PluginManagementWindow.h"
#include "utils/GAAttributesDump.h"
#include "utils/Helpers.h"

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor_Operator::getDefaultShape)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor_Operator::getDefaultColor)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor_Operator::GetConstructorForContext)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor_Operator::GetOperatorFlags)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor_Operator::GetMinSources)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor_Operator::GetMaxSources)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor_Operator::GetNodeLabel)>);

        return NODE_LABEL;
    }

    const char* ROP_ZibraVDBCompressor_Operator::GetNodeName(ContextType contextType)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor_Operator::GetNodeName)>);

        switch (contextType)
        {
        case ContextType::SOP:
            return NODE_NAME_SOP_CONTEXT;
            break;
        case ContextType::OUT:
            return NODE_NAME_OUT_CONTEXT;
            break;
        default:
            assert(0);
            return NODE_NAME_SOP_CONTEXT;
        }
    }

    unsigned ROP_ZibraVDBCompressor_Operator::GetMaxOutputs(ContextType contextType)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor_Operator::GetMaxOutputs)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor_Operator::GetSourceLabels)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::ConstructorSOPContext)>);

        return new ROP_ZibraVDBCompressor{ContextType::SOP, net, name, op};
    }

    OP_Node* ROP_ZibraVDBCompressor::ConstructorOUTContext(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::ConstructorOUTContext)>);

        return new ROP_ZibraVDBCompressor{ContextType::OUT, net, name, op};
    }

    std::vector<PRM_Template>& ROP_ZibraVDBCompressor::GetTemplateListContainer(ContextType contextType) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::GetTemplateListContainer)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::GetTemplateList)>);

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

        static PRM_Name theOpenPluginManagementButtonName(OPEN_PLUGIN_MANAGEMENT_BUTTON_NAME, "Open Plugin Management");

        templateList.emplace_back(PRM_CALLBACK, 1, &theOpenPluginManagementButtonName, nullptr, nullptr, nullptr,
                                  &ROP_ZibraVDBCompressor::OpenManagementWindow);

        templateList.emplace_back();
        return templateList.data();
    }

    OP_TemplatePair* ROP_ZibraVDBCompressor::GetTemplatePairs(ContextType contextType) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::GetTemplatePairs)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::GetVariablePair)>);

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
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::startRender)>);

        using namespace std::string_literals;
        if (!LibraryUtils::IsPlatformSupported())
        {
            addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED);
            return ROP_ABORT_RENDER;
        }

        if (!LibraryUtils::TryLoadLibrary())
        {
            addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING);
            return ROP_ABORT_RENDER;
        }

        if (!HoudiniLicenseManager::GetInstance().CheckLicense(HoudiniLicenseManager::Product::Compression))
        {
            if (HoudiniLicenseManager::GetInstance().IsAnyLicenseValid())
            {
                addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_LICENSE_NO_COMPRESSION);
            }
            else
            {
                addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_LICENSE_ERROR);
            }
            return ROP_ABORT_RENDER;
        }

        Analytics::AnalyticsManager::GetInstance().SendEventUsage();

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

        m_FilePerFrameMode = isParmTimeDependent(FILENAME_PARAM_NAME);

        const GU_Detail* gdp = m_InputSOP->getCookedGeoHandle(ctx, 0).gdp();
        if (!gdp)
        {
            addError(ROP_MESSAGE, "Failed to cook input SOP geometry.");
            return ROP_ABORT_RENDER;
        }

        m_OrderedChannelNames.clear();
        m_CurrentChannelCount = 0;
        m_OrderedChannelNames.reserve(CE::MAX_CHANNEL_COUNT);
        const GEO_Primitive* prim;
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                const GEO_PrimVDB* vdbPrim = dynamic_cast<const GEO_PrimVDB*>(prim);
                const char* gridName = vdbPrim->getGridName();
                const openvdb::GridBase& baseGrid = vdbPrim->getConstGrid();

                int underlyingChannels = 0;

                if (baseGrid.baseTree().isType<openvdb::Vec3STree>())
                {
                    underlyingChannels = 3;
                }
                else if (baseGrid.baseTree().isType<openvdb::FloatTree>())
                {
                    underlyingChannels = 1;
                }
                else
                {
                    std::string m = "Grid "s + gridName + " has unsupported grid type.";
                    addError(ROP_MESSAGE, m.c_str());
                    return ROP_ABORT_RENDER;
                }

                if (m_CurrentChannelCount + underlyingChannels > CE::MAX_CHANNEL_COUNT)
                {
                    std::string m = "Can not compress more than " + std::to_string(CE::MAX_CHANNEL_COUNT) + " channels.";
                    addError(ROP_MESSAGE, m.c_str());
                    return ROP_ABORT_RENDER;
                }

                const auto voxelSize = openvdb::Vec3f(baseGrid.voxelSize());
                const auto hasUniformVoxelSize = (std::abs(voxelSize[0] - voxelSize[1]) < std::numeric_limits<float>::epsilon()) &&
                                                 (std::abs(voxelSize[1] - voxelSize[2]) < std::numeric_limits<float>::epsilon());
                if (!hasUniformVoxelSize)
                {
                    std::string m = "Voxel size of VDB grid '"s + gridName + "' is non uniform. ZibraVDB supports only uniform voxels.";
                    addError(ROP_MESSAGE, m.c_str());
                    return ROP_ABORT_RENDER;
                }
                m_OrderedChannelNames.emplace_back(gridName);
                m_CurrentChannelCount += underlyingChannels;
            }
        }

        if (CreateCompressor(ctx) == ROP_ABORT_RENDER)
        {
            return ROP_ABORT_RENDER;
        }

        if (error() < UT_ERROR_ABORT)
            executePreRenderScript(tStart);

        m_FrameCount = 0;
        m_Resolution = {};
        m_CompressionStartTime = std::chrono::system_clock::now();

        return ROP_CONTINUE_RENDER;
    }

    ROP_RENDER_CODE ROP_ZibraVDBCompressor::renderFrame(const fpreal time, UT_Interrupt* boss)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::renderFrame)>);

        using namespace std::literals;

        assert(LibraryUtils::IsLibraryLoaded());

        executePreFrameScript(time);

        OP_Context ctx(time);
        const int thread = ctx.getThread();

        UT_String filename = "";
        evalString(filename, FILENAME_PARAM_NAME, nullptr, 0, time);
        std::error_code ec;
        // Intentionally ignoring errors
        // This is needed for relative paths to work properly
        // If path is invalid, we'll error out later on
        std::filesystem::create_directories(std::filesystem::path{filename.c_str()}.parent_path(), ec);

        OP_AutoLockInputs inputs{this};
        if (inputs.lock(ctx) >= UT_ERROR_ABORT)
            return ROP_RETRY_RENDER;

        const GU_Detail* gdp = m_InputSOP->getCookedGeoHandle(ctx, 0).gdp();
        if (!gdp)
        {
            addError(ROP_MESSAGE, "Failed to cook input SOP geometry.");
            return ROP_ABORT_RENDER;
        }

        std::set<std::string> channelNamesUniqueStorage{};
        std::vector<const char*> orderedChannelNames{};
        std::vector<openvdb::GridBase::ConstPtr> volumes{};
        const GEO_Primitive* prim;
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                const GEO_PrimVDB* vdbPrim = dynamic_cast<const GEO_PrimVDB*>(prim);
                const char* gridName = vdbPrim->getGridName();
                const openvdb::GridBase::ConstPtr baseGrid = vdbPrim->getConstGridPtr();

                int underlyingChannels = 0;

                if (baseGrid->baseTree().isType<openvdb::Vec3STree>())
                {
                    underlyingChannels = 3;
                }
                else if (baseGrid->baseTree().isType<openvdb::FloatTree>())
                {
                    underlyingChannels = 1;
                }
                else
                {
                    std::string m = "Grid "s + gridName + " has unsupported grid type.";
                    addError(ROP_MESSAGE, m.c_str());
                    return ROP_ABORT_RENDER;
                }

                if (channelNamesUniqueStorage.find(gridName) != channelNamesUniqueStorage.cend())
                {
                    std::string m = "ZibraVDB uses grid name as unique key. Node input contains duplicate of '"s + gridName + "' VDB prim.";
                    addError(ROP_MESSAGE, m.c_str());
                    return ROP_ABORT_RENDER;
                }

                const auto gridDimensions = baseGrid->evalActiveVoxelBoundingBox().dim();
                constexpr size_t MAX_GRID_DIMENSION = 4096;
                if (gridDimensions.x() > MAX_GRID_DIMENSION || gridDimensions.y() > MAX_GRID_DIMENSION ||
                    gridDimensions.z() > MAX_GRID_DIMENSION)
                {
                    addError(ROP_MESSAGE, "Grid dimension for one of the axis is larger than maximum supported (4096 voxels).");
                    return ROP_ABORT_RENDER;
                }

                volumes.emplace_back(std::move(baseGrid));
                orderedChannelNames.push_back(gridName);
                channelNamesUniqueStorage.insert(gridName);

                auto iter = std::find(m_OrderedChannelNames.begin(), m_OrderedChannelNames.end(), gridName);
                if (iter == m_OrderedChannelNames.end())
                {
                    if (m_CurrentChannelCount + underlyingChannels > CE::MAX_CHANNEL_COUNT)
                    {
                        std::string m = "Can not compress more than " + std::to_string(CE::MAX_CHANNEL_COUNT) + " channels.";
                        addError(ROP_MESSAGE, m.c_str());
                        return ROP_ABORT_RENDER;
                    }

                    m_OrderedChannelNames.emplace_back(gridName);
                    m_CurrentChannelCount += underlyingChannels;
                }
            }
        }

        CE::Compression::FrameManager* frameManager = nullptr;

        CE::Addons::OpenVDBUtils::FrameLoader vdbFrameLoader{volumes.data(), volumes.size()};

        const CE::Compression::SparseFrame* frame = vdbFrameLoader.LoadFrame();
        const auto& gridsShuffleInfo = vdbFrameLoader.GetGridsShuffleInfo();

        Result res = CompressFrame(*frame, &frameManager);
        if (ZIB_FAILED(res))
        {
            std::string errorMessage = "Failed to compress frame: " + LibraryUtils::ErrorCodeToString(res);
            addError(ROP_MESSAGE, errorMessage.c_str());
            return ROP_ABORT_RENDER;
        }

        const auto& frameAABB = frame->info.aabb;
        m_Resolution[0] = std::max(m_Resolution[0], CE::SPARSE_BLOCK_SIZE * uint32_t(frameAABB.maxX - frameAABB.minX));
        m_Resolution[1] = std::max(m_Resolution[1], CE::SPARSE_BLOCK_SIZE * uint32_t(frameAABB.maxY - frameAABB.minY));
        m_Resolution[2] = std::max(m_Resolution[2], CE::SPARSE_BLOCK_SIZE * uint32_t(frameAABB.maxZ - frameAABB.minZ));

        vdbFrameLoader.ReleaseFrame(frame);

        auto frameMetadata = DumpAttributes(gdp);
        frameMetadata.push_back({"chShuffle", DumpGridsShuffleInfo(gridsShuffleInfo).dump()});
        for (const auto& [key, val] : frameMetadata)
        {
            frameManager->AddMetadata(key.c_str(), val.c_str());
        }
        if (m_FilePerFrameMode)
        {
            std::ofstream outFrameFile{filename, std::ios::binary};
            if (!outFrameFile.is_open() || outFrameFile.fail())
            {
                addError(ROP_MESSAGE, "Failed to open file for writing.");
                return ROP_ABORT_RENDER;
            }
            STDOStreamWrapper streamWrapper{outFrameFile};
            res = frameManager->FinishAndEncode(&streamWrapper);
            outFrameFile.close();
        }
        else
        {
            auto frameOutStream = new OStreamRAMWrapper{};
            res = frameManager->FinishAndEncode(frameOutStream);
            m_BakedFrames.emplace(int32_t(ctx.getFrame()), frameOutStream);
        }
        if (ZIB_FAILED(res))
        {
            std::string errorMessage = "Failed to dump frame data: " + LibraryUtils::ErrorCodeToString(res);
            addError(ROP_MESSAGE, errorMessage.c_str());
            return ROP_ABORT_RENDER;
        }

        ++m_FrameCount;

        if (error() < UT_ERROR_ABORT)
        {
            executePostFrameScript(time);
        }

        return ROP_CONTINUE_RENDER;
    }

    ROP_RENDER_CODE ROP_ZibraVDBCompressor::endRender()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::endRender)>);

        if (!LibraryUtils::IsLibraryLoaded())
        {
            return ROP_ABORT_RENDER;
        }

        if (m_Compressor)
        {
            m_Compressor->Release();
            m_Compressor = nullptr;
        }
        if (m_RHIRuntime)
        {
            m_RHIRuntime->Release();
            m_RHIRuntime = nullptr;
        }

        if (!m_FilePerFrameMode)
        {
            ZIB_ON_SCOPE_EXIT([&]() {
                for (auto item : m_BakedFrames)
                {
                    delete item.second;
                }
                m_BakedFrames.clear();
            });

            if (m_BakedFrames.empty())
            {
                addWarning(ROP_MESSAGE, "Sequence is empty. No frames were compressed.");
            }
            else
            {
                UT_String filename = "";
                evalString(filename, FILENAME_PARAM_NAME, nullptr, 0, m_StartTime);
                Result res = MergeSequence(filename.c_str());
                if (ZIB_FAILED(res))
                {
                    std::string errorMessage = "Failed to merge sequence frames: " + LibraryUtils::ErrorCodeToString(res);
                    addError(ROP_MESSAGE, errorMessage.c_str());
                    return ROP_ABORT_RENDER;
                }
            }
        }

        if (error() < UT_ERROR_ABORT)
        {
            std::chrono::system_clock::time_point compressionEndTime = std::chrono::system_clock::now();

            std::chrono::milliseconds duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(compressionEndTime - m_CompressionStartTime);
            float durationInSeconds = duration.count() / 1000.0f;

            std::string endMessage = std::to_string(m_FrameCount) + " frames compressed in " + std::to_string(durationInSeconds) +
                                     " seconds.\nMax frame size: (" + std::to_string(m_Resolution[0]) + ", " +
                                     std::to_string(m_Resolution[1]) + ", " + std::to_string(m_Resolution[2]) + ")";
            addMessage(ROP_MESSAGE, endMessage.c_str());

            if (Analytics::AnalyticsManager::GetInstance().IsAnalyticsEnabled())
            {
                Analytics::CompressionEventData eventData;
                eventData.frameCount = m_FrameCount;
                eventData.channels = m_OrderedChannelNames;
                eventData.resolution = m_Resolution;
                eventData.quality = m_Quality;
                eventData.usingPerChannelCompressionSettings = !m_PerChannelCompressionSettings.empty();
                for (const auto& [channelName, quality] : m_PerChannelCompressionSettings)
                {
                    eventData.perChannelCompressionSettings.push_back(
                        Analytics::CompressionPerChannelSettings{channelName.c_str(), quality});
                }
                eventData.compressionTime = durationInSeconds;

                Analytics::AnalyticsManager::GetInstance().SendEventCompression(eventData);
            }

            executePostRenderScript(m_EndTime);
        }

        return error() < UT_ERROR_ABORT ? ROP_CONTINUE_RENDER : ROP_ABORT_RENDER;
    }

    ROP_RENDER_CODE ROP_ZibraVDBCompressor::CreateCompressor(const OP_Context& ctx) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::CreateCompressor)>);

        UT_String filename = "";
        evalString(filename, FILENAME_PARAM_NAME, nullptr, 0, ctx.getTime());

        std::error_code ec;
        // Intentionally ignoring errors
        // This is needed for relative paths to work properly
        // If path is invalid, we'll error out later on
        std::filesystem::create_directories(std::filesystem::path{filename.c_str()}.parent_path(), ec);

        const int renderMode = static_cast<int>(evalInt("trange", 0, ctx.getTime()));
        const float frameInc = renderMode == 0 ? 1 : evalFloat("f", 2, ctx.getTime());
        CE::PlaybackInfo playbackInfo{};
        playbackInfo.sequenceIndexIncrement = frameInc;

        m_Quality = static_cast<float>(evalFloat(QUALITY_PARAM_NAME, 0, ctx.getTime()));

        UT_String usePerChannelCompressionSettingsString;
        evalString(usePerChannelCompressionSettingsString, USE_PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME, 0, ctx.getTime());
        m_PerChannelCompressionSettings.clear();

        if (usePerChannelCompressionSettingsString == "on")
        {
            // Just in case evalInt return invalid number.
            constexpr int maxPerChannelSettingsCount = 1024;
            const int perChannelSettingsCount =
                std::max(0, std::min(static_cast<int>(evalInt(PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME, 0, ctx.getTime())),
                                     maxPerChannelSettingsCount));

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
                evalString(channelNameStr, channelNameParamNameStr.c_str(), 0, ctx.getTime());

                if (std::find(m_OrderedChannelNames.begin(), m_OrderedChannelNames.end(), channelNameStr.toStdString()) ==
                    m_OrderedChannelNames.end())
                {
                    continue;
                }
                float quality = static_cast<float>(evalFloat(qualityParamNameStr.c_str(), 0, ctx.getTime()));

                m_PerChannelCompressionSettings.emplace_back(channelNameStr, quality);
                m_PerChannelCompressionSettings.back().first.harden();
            }
        }

        Result res = InitCompressor();
        if (ZIB_FAILED(res))
        {
            std::string errorMessage = "Failed to initialize compressor: " + LibraryUtils::ErrorCodeToString(res);
            addError(ROP_MESSAGE, errorMessage.c_str());
            return ROP_ABORT_RENDER;
        }

        return ROP_CONTINUE_RENDER;
    }

    Result ROP_ZibraVDBCompressor::InitCompressor() noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::InitCompressor)>);

        if (!LibraryUtils::IsLibraryLoaded())
        {
            assert(0);
            return RESULT_UNEXPECTED_ERROR;
        }

        RHI::RHIFactory* RHIFactory = nullptr;
        Result res = RHI::CreateRHIFactory(&RHIFactory);
        if (ZIB_FAILED(res))
        {
            return res;
        }

        res = RHIFactory->SetGFXAPI(Helpers::SelectGFXAPI());
        if (ZIB_FAILED(res))
        {
            return res;
        }

        if (Helpers::NeedForceSoftwareDevice())
        {
            res = RHIFactory->ForceSoftwareDevice();
            if (ZIB_FAILED(res))
            {
                return res;
            }
        }

        res = RHIFactory->Create(&m_RHIRuntime);
        if (ZIB_FAILED(res))
        {
            return res;
        }

        res = m_RHIRuntime->Initialize();
        if (ZIB_FAILED(res))
        {
            m_RHIRuntime->Release();
            m_RHIRuntime = nullptr;
            return res;
        }

        CE::Compression::CompressorFactory* compressorFactory = nullptr;
        res = CE::Compression::CreateCompressorFactory(&compressorFactory);
        if (ZIB_FAILED(res))
        {
            return res;
        }
        ZIB_ON_SCOPE_EXIT([&]() { compressorFactory->Release(); });

        res = compressorFactory->UseRHI(m_RHIRuntime);
        if (ZIB_FAILED(res))
        {
            return res;
        }
        res = compressorFactory->SetQuality(m_Quality);
        if (ZIB_FAILED(res))
        {
            return res;
        }

        for (const auto& [channelName, quality] : m_PerChannelCompressionSettings)
        {
            res = compressorFactory->OverrideChannelQuality(channelName.c_str(), quality);
            if (ZIB_FAILED(res))
            {
                return res;
            }
        }

        res = compressorFactory->Create(&m_Compressor);
        if (ZIB_FAILED(res))
        {
            return res;
        }

        return m_Compressor->Initialize();
    }

    Result ROP_ZibraVDBCompressor::CompressFrame(const CE::Compression::SparseFrame& desc,
                                                 CE::Compression::FrameManager** outManager) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::CompressFrame)>);

        if (!m_RHIRuntime || !m_Compressor)
        {
            assert(0);
            return RESULT_UNEXPECTED_ERROR;
        }

        Result res = m_RHIRuntime->StartRecording();
        if (ZIB_FAILED(res))
        {
            return res;
        }

        res = m_Compressor->CompressFrame(desc, outManager);
        if (ZIB_FAILED(res))
        {
            return res;
        }

        res = m_RHIRuntime->StopRecording();
        if (ZIB_FAILED(res))
        {
            return res;
        }
        return RESULT_SUCCESS;
    }

    Result ROP_ZibraVDBCompressor::MergeSequence(const std::filesystem::path& outPath) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::MergeSequence)>);

        CE::Compression::SequenceMerger* merger = nullptr;
        Result res = CE::Compression::CreateSequenceMerger(&merger);
        if (ZIB_FAILED(res))
            return res;

        std::vector<MemoryIStream*> views{};
        ZIB_ON_SCOPE_EXIT([&]() {
            for (auto item : views)
            {
                delete item;
            }
        });

        for (auto [frameNumber, frameStream] : m_BakedFrames)
        {
            auto& container = frameStream->GetContainer();
            res = merger->AddSequence(views.emplace_back(new MemoryIStream{container.data(), container.size()}), frameNumber);
            if (ZIB_FAILED(res))
                return res;
        }

        merger->AddMetadata("__ORIGIN", "ZibraVDBForHoudini");
#ifdef WIN32
        merger->AddMetadata("__PLATFORM", "Windows");
#elif __APPLE__
        merger->AddMetadata("__PLATFORM", "Apple");
#elif __linux__
        merger->AddMetadata("__PLATFORM", "Linux");
#endif

        std::ofstream outFile{outPath, std::ios::binary};
        if (!outFile.is_open())
        {
            return RESULT_IO_ERROR;
        }

        STDOStreamWrapper wrapper{outFile};
        res = merger->Finish(&wrapper);
        merger->Release();
        outFile.close();
        return res;
    }

    void ROP_ZibraVDBCompressor::getOutputFile(UT_String& filename)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::getOutputFile)>);

        evalString(filename, "filename", nullptr, 0, m_StartTime);
    }

    std::vector<std::pair<std::string, std::string>> ROP_ZibraVDBCompressor::DumpAttributes(const GU_Detail* gdp) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::DumpAttributes)>);

        std::vector<std::pair<std::string, std::string>> result{};

        const GEO_Primitive* prim;
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                auto vdbPrim = dynamic_cast<const GEO_PrimVDB*>(prim);

                nlohmann::json primAttrDump = Utils::DumpAttributesV2(gdp, GA_ATTRIB_PRIMITIVE, prim->getMapOffset());
                std::string primKeyName = "houdiniPrimitiveAttributesV2_"s + vdbPrim->getGridName();
                result.emplace_back(primKeyName, primAttrDump.dump());

                DumpVisualisationAttributes(result, vdbPrim);
            }
        }

        nlohmann::json detailAttrDump = Utils::DumpAttributesV2(gdp, GA_ATTRIB_DETAIL, 0);
        result.emplace_back("houdiniDetailAttributesV2", detailAttrDump.dump());

        return result;
    }

    void ROP_ZibraVDBCompressor::DumpVisualisationAttributes(std::vector<std::pair<std::string, std::string>>& attributes,
                                                             const GEO_PrimVDB* vdbPrim) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::DumpVisualisationAttributes)>);

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

    nlohmann::json ROP_ZibraVDBCompressor::DumpGridsShuffleInfo(const std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc>& gridDescs) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::DumpGridsShuffleInfo)>);

        static std::map<CE::Addons::OpenVDBUtils::GridVoxelType, std::string> voxelTypeToString = {
            {CE::Addons::OpenVDBUtils::GridVoxelType::Float1, "Float1"}, {CE::Addons::OpenVDBUtils::GridVoxelType::Float3, "Float3"}};

        nlohmann::json result = nlohmann::json::array();
        for (const CE::Addons::OpenVDBUtils::VDBGridDesc& gridDesc : gridDescs)
        {
            nlohmann::json serializedDesc = nlohmann::json{
                {"gridName", gridDesc.gridName},
                {"voxelType", voxelTypeToString.at(gridDesc.voxelType)},
            };
            for (size_t i = 0; i < std::size(gridDesc.chSource); ++i)
            {
                std::string name{"chSource"};
                if (gridDesc.chSource[i])
                {
                    serializedDesc[name + std::to_string(i)] = gridDesc.chSource[i];
                }
                else
                {
                    serializedDesc[name + std::to_string(i)] = nullptr;
                }
            }
            result.emplace_back(serializedDesc);
        }
        return result;
    }

    int ROP_ZibraVDBCompressor::OpenManagementWindow(void* data, int index, fpreal32 time, const PRM_Template* tplate) noexcept
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&ROP_ZibraVDBCompressor::OpenManagementWindow)>);

        PluginManagementWindow::ShowWindow();
        return 0;
    }
} // namespace Zibra::ZibraVDBCompressor
