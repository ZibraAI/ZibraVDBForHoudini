#include "PrecompiledHeader.h"

#include "ROP_InspectVolume.h"

#ifdef ZIB_ENABLE_DEBUG_NODES

namespace Zibra::InspectVolume
{
    ROP_InspectVolume_Operator::ROP_InspectVolume_Operator(ContextType contextType) noexcept
        : OP_Operator(GetNodeName(contextType), GetNodeLabel(contextType), GetConstructorForContext(contextType),
                      ROP_InspectVolume::GetTemplatePairs(contextType), GetMinSources(contextType), GetMaxSources(contextType),
                      ROP_InspectVolume::GetVariablePair(contextType), GetOperatorFlags(contextType), GetSourceLabels(contextType),
                      GetMaxOutputs(contextType))
        , m_ContextType(contextType)
    {
        setIconName(ZIBRAVDB_ICON_PATH);
        setOpTabSubMenuPath(ZIBRAVDB_NODES_TAB_NAME);
    }

    const UT_StringHolder& ROP_InspectVolume_Operator::getDefaultShape() const
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

    UT_Color ROP_InspectVolume_Operator::getDefaultColor() const
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

    OP_Constructor ROP_InspectVolume_Operator::GetConstructorForContext(ContextType contextType)
    {
        switch (contextType)
        {
        case ContextType::SOP:
            return ROP_InspectVolume::ConstructorSOPContext;
        case ContextType::OUT:
            return ROP_InspectVolume::ConstructorOUTContext;
        default:
            assert(0);
            return ROP_InspectVolume::ConstructorSOPContext;
        }
    }

    unsigned ROP_InspectVolume_Operator::GetOperatorFlags(ContextType contextType)
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

    unsigned ROP_InspectVolume_Operator::GetMinSources(ContextType contextType)
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

    unsigned ROP_InspectVolume_Operator::GetMaxSources(ContextType contextType)
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

    const char* ROP_InspectVolume_Operator::GetNodeLabel(ContextType contextType)
    {
        return NODE_LABEL;
    }

    const char* ROP_InspectVolume_Operator::GetNodeName(ContextType contextType)
    {
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

    unsigned ROP_InspectVolume_Operator::GetMaxOutputs(ContextType contextType)
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

    const char** ROP_InspectVolume_Operator::GetSourceLabels(ContextType contextType)
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

    OP_Node* ROP_InspectVolume::ConstructorSOPContext(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new ROP_InspectVolume{ContextType::SOP, net, name, op};
    }

    OP_Node* ROP_InspectVolume::ConstructorOUTContext(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new ROP_InspectVolume{ContextType::OUT, net, name, op};
    }

    std::vector<PRM_Template>& ROP_InspectVolume::GetTemplateListContainer(ContextType contextType) noexcept
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

    PRM_Template* ROP_InspectVolume::GetTemplateList(ContextType contextType) noexcept
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

        static PRM_Name theSampleCoord(SAMPLE_COORD_NAME, "Sample coordinate");
        templateList.emplace_back(PRM_INT_XYZ_E, 3, &theSampleCoord);

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
        templateList.emplace_back();

        return templateList.data();
    }

    OP_TemplatePair* ROP_InspectVolume::GetTemplatePairs(ContextType contextType) noexcept
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

    OP_VariablePair* ROP_InspectVolume::GetVariablePair(ContextType contextType) noexcept
    {
        static OP_VariablePair pair{ROP_Node::myVariableList};
        return &pair;
    }

    ROP_InspectVolume::ROP_InspectVolume(ContextType contextType, OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : ROP_Node{net, name, entry}
        , m_ContextType(contextType)
    {
    }

    ROP_InspectVolume::~ROP_InspectVolume() noexcept = default;

    int ROP_InspectVolume::startRender(const int nFrames, const fpreal tStart, const fpreal tEnd)
    {
        using namespace std::string_literals;

        m_EndTime = tEnd;
        m_StartTime = tStart;

        using namespace std::literals;
        OP_Context ctx(tStart);

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

        if (error() < UT_ERROR_ABORT)
            executePreRenderScript(tStart);

        return ROP_CONTINUE_RENDER;
    }

    // UTvdbType is not in Houdini 20.5
    static const char* VDBTypeToString(UT_VDBType vdbType)
    {
        switch (vdbType)
        {
        case UT_VDB_INVALID:
            return "INVALID";
        case UT_VDB_FLOAT:
            return "FLOAT";
        case UT_VDB_DOUBLE:
            return "DOUBLE";
        case UT_VDB_INT32:
            return "INT32";
        case UT_VDB_INT64:
            return "INT64";
        case UT_VDB_BOOL:
            return "BOOL";
        case UT_VDB_VEC3F:
            return "VEC3F";
        case UT_VDB_VEC3D:
            return "VEC3D";
        case UT_VDB_VEC3I:
            return "VEC3I";
        case UT_VDB_POINTINDEX:
            return "POINTINDEX";
        case UT_VDB_POINTDATA:
            return "POINTDATA";
        default:
            return "UNKNOWN";
        }
    }

    ROP_RENDER_CODE ROP_InspectVolume::renderFrame(const fpreal time, UT_Interrupt* boss)
    {
        using namespace std::literals;

        executePreFrameScript(time);

        OP_Context ctx(time);

        OP_AutoLockInputs inputs{this};
        if (inputs.lock(ctx) >= UT_ERROR_ABORT)
            return ROP_RETRY_RENDER;

        const GU_Detail* gdp = m_InputSOP->getCookedGeoHandle(ctx, 0).gdp();
        if (!gdp)
        {
            addError(ROP_MESSAGE, "Failed to cook input SOP geometry.");
            return ROP_ABORT_RENDER;
        }

        std::string constructedMessage;

        std::string frameMessage = "Inspecting at time "s + std::to_string(time);
        constructedMessage += frameMessage + "\n";

        int primCount = 0;
        const GEO_Primitive* prim;
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                ++primCount;

                const GEO_PrimVDB* vdbPrim = dynamic_cast<const GEO_PrimVDB*>(prim);
                const char* gridName = vdbPrim->getGridName();
                openvdb::GridBase::ConstPtr grid = vdbPrim->getGridPtr();
                openvdb::CoordBBox bbox = grid->evalActiveVoxelBoundingBox();

                {
                    std::string message = "Inspecting VDB Grid "s + gridName;
                    constructedMessage += message + "\n";
                }

                {
                    UT_VDBType vdbType = vdbPrim->getStorageType();
                    std::string message = "Type: "s + VDBTypeToString(vdbType);
                    constructedMessage += message + "\n";
                }

                {
                    const GEO_VolumeOptions& visOptions = vdbPrim->getVisOptions();
                    GEO_VolumeVis myMode;
                    fpreal myIso;
                    fpreal myDensity;
                    GEO_VolumeVisLod myLod;
                    GEO_VolumeTypeInfo myTypeInfo;
                    fpreal myTiles;
                    std::string message = "Visualisation options: visMode - "s + GEOgetVolumeVisToken(visOptions.myMode) + ", ISO - "s +
                                          std::to_string(visOptions.myIso) + ", Density - "s + std::to_string(visOptions.myDensity) +
                                          ", LOD - " + GEOgetVolumeVisLodToken(visOptions.myLod) + ", Type Info - "s +
                                          GEOgetVolumeTypeInfoToken(visOptions.myTypeInfo) + ", Tiles - " +
                                          std::to_string(visOptions.myTiles);
                    constructedMessage += message + "\n";
                }

                {
                    std::string message = "Bounding Box: ("s + std::to_string(bbox.min().x()) + ","s + std::to_string(bbox.min().y()) +
                                          ","s + std::to_string(bbox.min().z()) + ") - ("s + std::to_string(bbox.max().x()) + ","s +
                                          std::to_string(bbox.max().y()) + ","s + std::to_string(bbox.max().z()) + ")"s;
                    constructedMessage += message + "\n";
                }

                if (!bbox.empty())
                {
                    const int sampleCoord[3] = {static_cast<int>(evalInt(SAMPLE_COORD_NAME, 0, time)),
                                                static_cast<int>(evalInt(SAMPLE_COORD_NAME, 1, time)),
                                                static_cast<int>(evalInt(SAMPLE_COORD_NAME, 2, time))};

                    std::string message = "Avarage value at position ("s + std::to_string(sampleCoord[0]) + ","s +
                                          std::to_string(sampleCoord[1]) + ","s + std::to_string(sampleCoord[2]) + ") is ";
                    if (grid->baseTree().isType<openvdb::Vec3STree>())
                    {
                        openvdb::Vec3SGrid::ConstPtr vecGrid = openvdb::gridConstPtrCast<openvdb::Vec3SGrid>(grid);
                        openvdb::Vec3SGrid::ConstAccessor accessor = vecGrid->getAccessor();
                        float avarageValues[3] = {};
                        int sampleCount = 0;
                        for (int dx = -10; dx <= 10; ++dx)
                        {
                            for (int dy = -10; dy <= 10; ++dy)
                            {
                                for (int dz = -10; dz <= 10; ++dz)
                                {
                                    openvdb::Coord currentCoord =
                                        openvdb::Coord(sampleCoord[0] + dx, sampleCoord[1] + dy, sampleCoord[2] + dz);
                                    openvdb::Vec3d value = accessor.getValue(currentCoord);
                                    avarageValues[0] += value.x();
                                    avarageValues[1] += value.y();
                                    avarageValues[2] += value.z();
                                    ++sampleCount;
                                }
                            }
                        }
                        avarageValues[0] /= sampleCount;
                        avarageValues[1] /= sampleCount;
                        avarageValues[2] /= sampleCount;

                        message += "("s + std::to_string(avarageValues[0]) + ","s + std::to_string(avarageValues[1]) + ","s +
                                   std::to_string(avarageValues[2]) + ")"s;
                        constructedMessage += message + "\n";
                    }
                    else if (grid->baseTree().isType<openvdb::FloatTree>())
                    {
                        openvdb::FloatGrid::ConstPtr floatGrid = openvdb::gridConstPtrCast<openvdb::FloatGrid>(grid);
                        openvdb::FloatGrid::ConstAccessor accessor = floatGrid->getAccessor();
                        float avarageValue = 0.0f;
                        int sampleCount = 0;
                        for (int dx = -10; dx <= 10; ++dx)
                        {
                            for (int dy = -10; dy <= 10; ++dy)
                            {
                                for (int dz = -10; dz <= 10; ++dz)
                                {
                                    openvdb::Coord currentCoord =
                                        openvdb::Coord(sampleCoord[0] + dx, sampleCoord[1] + dy, sampleCoord[2] + dz);
                                    avarageValue += accessor.getValue(currentCoord);
                                    ++sampleCount;
                                }
                            }
                        }
                        avarageValue /= sampleCount;
                        message += std::to_string(avarageValue);
                        constructedMessage += message + "\n";
                    }
                    else
                    {
                        constructedMessage += "Can't sample this grid type\n";
                    }
                }
            }
        }

        std::string vdbCountMessage = "Number of VDB prims parsed: "s + std::to_string(primCount);
        addMessage(ROP_MESSAGE, constructedMessage.c_str());

        if (error() < UT_ERROR_ABORT)
        {
            executePostFrameScript(time);
        }

        return ROP_CONTINUE_RENDER;
    }

    ROP_RENDER_CODE ROP_InspectVolume::endRender()
    {
        if (error() < UT_ERROR_ABORT)
        {
            executePostRenderScript(m_EndTime);
        }

        return error() < UT_ERROR_ABORT ? ROP_CONTINUE_RENDER : ROP_ABORT_RENDER;
    }

    void ROP_InspectVolume::getOutputFile(UT_String& filename)
    {
        filename = "";
    }

} // namespace Zibra::InspectVolume

#endif
