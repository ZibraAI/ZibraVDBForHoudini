#include "PrecompiledHeader.h"

#include "ROP_ZibraVDBCompressor.h"

#include "bridge/CompressionEngine.h"
#include "openvdb/OpenVDBDecoder.h"
#include "ui/MessageBox.h"
#include "utils/GAAttributesDump.h"

namespace Zibra::ZibraVDBCompressor
{
    using namespace std::literals;

    OP_Node* ROP_ZibraVDBCompressor::Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new ROP_ZibraVDBCompressor{net, name, op};
    }

    PRM_Template* ROP_ZibraVDBCompressor::GetTemplateList() noexcept
    {
        static PRM_Name theFileName(FILENAME_PARAM_NAME, "Out File");
        static PRM_Default theFileDefault(0, "$HIP/vol/$HIPNAME.$OS.zibravdb");

        static PRM_Name theQualityName(QUALITY_PARAM_NAME, "Quality");
        static PRM_Default theQualityDefault(0.6, nullptr);
        static PRM_Range theQualityRange(PRM_RANGE_RESTRICTED, 0.0f, PRM_RANGE_RESTRICTED, 1.0f);

        static PRM_Name theDownloadLibraryButtonName(DOWNLOAD_LIBRARY_BUTTON_NAME, "Download Library");

        static PRM_Template templateList[] = {PRM_Template(PRM_FILE, 1, &theFileName, &theFileDefault),
                                              PRM_Template(PRM_FLT, 1, &theQualityName, &theQualityDefault, nullptr, &theQualityRange),
                                              PRM_Template(PRM_CALLBACK, 1, &theDownloadLibraryButtonName, nullptr, nullptr, nullptr,
                                                           &ROP_ZibraVDBCompressor::DownloadLibrary),
                                              theRopTemplates[ROP_TPRERENDER_TPLATE],
                                              theRopTemplates[ROP_PRERENDER_TPLATE],
                                              theRopTemplates[ROP_LPRERENDER_TPLATE],
                                              theRopTemplates[ROP_TPREFRAME_TPLATE],
                                              theRopTemplates[ROP_PREFRAME_TPLATE],
                                              theRopTemplates[ROP_LPREFRAME_TPLATE],
                                              theRopTemplates[ROP_TPOSTFRAME_TPLATE],
                                              theRopTemplates[ROP_POSTFRAME_TPLATE],
                                              theRopTemplates[ROP_LPOSTFRAME_TPLATE],
                                              theRopTemplates[ROP_TPOSTRENDER_TPLATE],
                                              theRopTemplates[ROP_POSTRENDER_TPLATE],
                                              theRopTemplates[ROP_LPOSTRENDER_TPLATE],
                                              PRM_Template()};

        return templateList;
    }

    OP_TemplatePair* ROP_ZibraVDBCompressor::GetTemplatePairs() noexcept
    {
        static PRM_Template tmp[] = {theRopTemplates[ROP_RENDERBACKGROUND_TPLATE], PRM_Template()};

        static OP_TemplatePair base{GetTemplateList()};
        static OP_TemplatePair ropPair1{ROP_Node::getROPbaseTemplate(), &base};
        static OP_TemplatePair ropPair2{tmp, &ropPair1};
        return &ropPair2;
    }

    OP_VariablePair* ROP_ZibraVDBCompressor::GetVariablePair() noexcept
    {
        static OP_VariablePair pair{ROP_Node::myVariableList};
        return &pair;
    }

    ROP_ZibraVDBCompressor::ROP_ZibraVDBCompressor(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : ROP_Node{net, name, entry}
    {
    }

    ROP_ZibraVDBCompressor::~ROP_ZibraVDBCompressor() noexcept = default;

    int ROP_ZibraVDBCompressor::startRender(const int nFrames, const fpreal tStart, const fpreal tEnd)
    {
        if (!CompressionEngine::IsPlatformSupported())
        {
            addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED);
            return ROP_ABORT_RENDER;
        }

        CompressionEngine::LoadLibrary();

        if (!CompressionEngine::IsLibraryLoaded())
        {
            addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING);
            return ROP_ABORT_RENDER;
        }

        if (!CompressionEngine::IsLicenseValid(CompressionEngine::ZCE_Product::Compression))
        {
            addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_LICENSE_ERROR);
            return ROP_ABORT_RENDER;
        }

        m_EndTime = tEnd;
        m_StartTime = tStart;

        using namespace std::literals;
        OP_Context ctx(tStart);
        const int thread = ctx.getThread();

        OP_AutoLockInputs inputs{this};
        if (inputs.lock(ctx) >= UT_ERROR_ABORT)
            return false;

        // 0 index referring to the node's first input
        m_InputSOP = CAST_SOPNODE(getInputFollowingOutputs(0));
        if (!m_InputSOP)
        {
            addError(ROP_MESSAGE, "No inputs detected. First input must be an OpenVDB source.");
            return ROP_ABORT_RENDER;
        }
        const GU_Detail* gdp = m_InputSOP->getCookedGeoHandle(ctx, 0).gdp();
        if (!gdp)
        {
            addError(ROP_MESSAGE, "Failed to cook input SOP geometry");
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
            addError(ROP_MESSAGE, "No input VDB primitives found");
            return ROP_ABORT_RENDER;
        }

        UT_String filename = "";
        evalString(filename, "filename", nullptr, 0, tStart);
        std::filesystem::create_directories(std::filesystem::path{filename.c_str()}.parent_path());

        CompressionEngine::ZCE_CompressionSettings settings{};
        settings.outputFilePath = filename.c_str();
        settings.quality = static_cast<float>(evalFloat(QUALITY_PARAM_NAME, 0, tStart));

        CompressorInstanceID = CompressionEngine::CreateCompressorInstance(&settings);
        CompressionEngine::StartSequence(CompressorInstanceID);

        if (error() < UT_ERROR_ABORT)
            executePreRenderScript(tStart);

        return ROP_CONTINUE_RENDER;
    }

    ROP_RENDER_CODE ROP_ZibraVDBCompressor::renderFrame(const fpreal time, UT_Interrupt* boss)
    {
        using namespace std::literals;

        assert(CompressionEngine::IsLibraryLoaded());

        if (!CompressionEngine::IsLicenseValid(CompressionEngine::ZCE_Product::Compression))
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
                    std::string m = "Unsupported value type for '"s + gridName + "' prim. Only float grids suppored.";
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

        auto attrDump = DumpAttributes(gdp);
        std::vector<CompressionEngine::ZCE_MetadataEntry> metadata{};
        metadata.reserve(attrDump.size());
        for (auto& [key, val] : attrDump)
        {
            metadata.push_back(CompressionEngine::ZCE_MetadataEntry{key.c_str(), val.c_str()});
        }

        CompressionEngine::ZCE_FrameContainer frameData{};
        frameData.metadata = metadata.data();
        frameData.metadataCount = metadata.size();

        OpenVDBSupport::OpenVDBDecoder reader{volumes.data(), orderedChannelNames.data(), orderedChannelNames.size()};
        frameData.frameData = reader.DecodeFrame();

        CompressionEngine::CompressFrame(CompressorInstanceID, &frameData);

        reader.FreeFrame(frameData.frameData);

        if (error() < UT_ERROR_ABORT)
        {
            executePostFrameScript(time);
        }
        return ROP_CONTINUE_RENDER;
    }

    ROP_RENDER_CODE ROP_ZibraVDBCompressor::endRender()
    {
        if (!CompressionEngine::IsLibraryLoaded())
        {
            return ROP_ABORT_RENDER;
        }

        if (CompressorInstanceID != uint32_t(-1))
        {

            if (error() < UT_ERROR_ABORT)
            {
                CompressionEngine::FinishSequence(CompressorInstanceID);
            }
            else
            {
                CompressionEngine::AbortSequence(CompressorInstanceID);
            }

            CompressionEngine::ReleaseCompressorInstance(CompressorInstanceID);
        }

        CompressorInstanceID = uint32_t(-1);

        if (error() < UT_ERROR_ABORT)
        {
            executePostRenderScript(m_EndTime);
        }
        return error() < UT_ERROR_ABORT ? ROP_CONTINUE_RENDER : ROP_ABORT_RENDER;
    }

    void ROP_ZibraVDBCompressor::getOutputFile(UT_String& filename)
    {
        evalString(filename, "filename", nullptr, 0, m_StartTime);
    }

    std::vector<std::pair<std::string, std::string>> ROP_ZibraVDBCompressor::DumpAttributes(const GU_Detail* gdp) noexcept
    {
        std::vector<std::pair<std::string, std::string>> result{};

        const GEO_Primitive* prim;
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                auto vdbPrim = dynamic_cast<const GEO_PrimVDB*>(prim);
                nlohmann::json primAttrDump = Utils::DumpAttributesForSingleEntity(gdp, GA_ATTRIB_PRIMITIVE, prim->getMapOffset());
                std::string keyName = "houdiniPrimitiveAttributes_"s + vdbPrim->getGridName();
                result.emplace_back(keyName, primAttrDump.dump());
            }
        }

        nlohmann::json detailAttrDump = Utils::DumpAttributesForSingleEntity(gdp, GA_ATTRIB_DETAIL, 0);
        result.emplace_back("houdiniDetailAttributes", detailAttrDump.dump());
        return result;
    }

    int ROP_ZibraVDBCompressor::DownloadLibrary(void* data, int index, fpreal32 time, const PRM_Template* tplate)
    {
        using namespace Zibra::UI;

        auto node = static_cast<ROP_ZibraVDBCompressor*>(data);

        if (CompressionEngine::IsLibraryLoaded())
        {
            MessageBox::Result result = MessageBox::Show(MessageBox::Type::OK, "Library is already downloaded.", "ZibraVDB");
            return 0;
        }
        MessageBox::Result result = MessageBox::Show(MessageBox::Type::YesNo,
                                                     "By downloading ZibraVDB library you agree to ZibraVDB for Houdini Terms of Service - "
                                                     "https://effects.zibra.ai/vdb-terms-of-services-trial. Do you wish to proceed?",
                                                     "ZibraVDB");
        if (result == MessageBox::Result::No)
        {
            return 0;
        }
        CompressionEngine::DownloadLibrary();
        if (!CompressionEngine::IsLibraryLoaded())
        {
            node->addError(ROP_MESSAGE, "Failed to download ZibraVDB library.");
            return 0;
        }

        if (!CompressionEngine::IsLicenseValid(CompressionEngine::ZCE_Product::Compression))
        {
            node->addWarning(ROP_MESSAGE, "Library downloaded successfully, but no valid license found. Visit "
                                          "'https://effects.zibra.ai/zibravdbhoudini', set up your license and restart Houdini.");
            return 0;
        }
        MessageBox::Show(MessageBox::Type::OK, "Library downloaded successfully.", "ZibraVDB");
        return 0;
    }

} // namespace Zibra::ZibraVDBCompressor