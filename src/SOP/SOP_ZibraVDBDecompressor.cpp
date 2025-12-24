#include "PrecompiledHeader.h"

#include "SOP_ZibraVDBDecompressor.h"

#include "bridge/LibraryUtils.h"
#include "licensing/HoudiniLicenseManager.h"
#include "ui/PluginManagementWindow.h"
#include "utils/GAAttributesDump.h"

#ifdef _DEBUG
#define DBG_NAME(expression) expression
#else
#define DBG_NAME(expression) ""
#endif

namespace Zibra::ZibraVDBDecompressor
{
    using namespace std::literals;
    using namespace CE::Decompression;

    class StreamAutorelease
    {
    public:
        explicit StreamAutorelease(std::ifstream& stream) noexcept
            : m_ManagedStream(stream)
        {
        }
        ~StreamAutorelease() noexcept
        {
            m_ManagedStream.close();
        }

    private:
        std::ifstream& m_ManagedStream;
    };

    OP_Node* SOP_ZibraVDBDecompressor::Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new SOP_ZibraVDBDecompressor{net, name, op};
    }

    PRM_Template* SOP_ZibraVDBDecompressor::GetTemplateList() noexcept
    {
        static PRM_Name theFileName(FILENAME_PARAM_NAME, "Input File");
        static PRM_Default theFileDefault(0, "$HIP/vol/$HIPNAME.$OS.zibravdb");

        static PRM_Name theFrameName(FRAME_PARAM_NAME, "Sequence Frame");
        static PRM_Default theFrameDefault(0, "$F");

        static PRM_Name theReloadCacheName(REFRESH_CALLBACK_PARAM_NAME, "Reload Cache");
        static PRM_Callback theReloadCallback{[](void* node, int index, fpreal64 time, const PRM_Template* tplate) -> int {
            auto self = static_cast<SOP_ZibraVDBDecompressor*>(node);
            self->deleteCookedData();
            self->refreshGdp();
            return 1;
        }};

        static PRM_Name theOpenPluginManagementButtonName(OPEN_PLUGIN_MANAGEMENT_BUTTON_NAME, "Open Plugin Management");

        static PRM_Template templateList[] = {
            PRM_Template(PRM_FILE, 1, &theFileName, &theFileDefault), PRM_Template(PRM_INT, 1, &theFrameName, &theFrameDefault),
            PRM_Template(PRM_CALLBACK, 1, &theReloadCacheName, nullptr, nullptr, nullptr, theReloadCallback),
            PRM_Template(PRM_CALLBACK, 1, &theOpenPluginManagementButtonName, nullptr, nullptr, nullptr,
                         &SOP_ZibraVDBDecompressor::OpenManagementWindow),
            PRM_Template()};
        return templateList;
    }

    SOP_ZibraVDBDecompressor::SOP_ZibraVDBDecompressor(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : SOP_Node(net, name, entry)
    {
    }

    SOP_ZibraVDBDecompressor::~SOP_ZibraVDBDecompressor() noexcept
    {
        if (!LibraryUtils::IsLibraryLoaded())
        {
            return;
        }
        m_DecompressorManager.Release();
    }

    OP_ERROR SOP_ZibraVDBDecompressor::cookMySop(OP_Context& context)
    {
        gdp->clearAndDestroy();

        if (!LibraryUtils::IsPlatformSupported())
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED);
            return error(context);
        }

        if (!LibraryUtils::TryLoadLibrary())
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING);
            return error(context);
        }

        // License may or may not be required depending on .zibravdb file
        // So we need to trigger license check, but if it fails we proceed with decompression
        HoudiniLicenseManager::GetInstance().CheckLicense(HoudiniLicenseManager::Product::Decompression);

        UT_String filename = "";
        evalString(filename, "filename", nullptr, 0, context.getTime());
        if (filename == "")
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_NO_FILE_SELECTED);
            return error(context);
        }

        if (!std::filesystem::exists(filename.toStdString()))
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_FILE_NOT_FOUND);
            return error(context);
        }

        m_DecompressorManager.Initialize();

        Result res = m_DecompressorManager.RegisterDecompressor(filename);
        if (ZIB_FAILED(res))
        {
            std::string errorMessage = "Failed to initialize decompressor: " + LibraryUtils::ErrorCodeToString(res);
            addError(SOP_MESSAGE, errorMessage.c_str());
            return error(context);
        }

        const exint frameIndex = evalInt(FRAME_PARAM_NAME, 0, context.getTime());

        auto [frameMemory, frameProxy] = m_DecompressorManager.FetchFrame(frameIndex);
        if (frameMemory.size() == 0)
        {
            addWarning(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_FRAME_INDEX_OUT_OF_RANGE);
            return error(context);
        }
        ZIB_ON_SCOPE_EXIT([&]() { frameProxy->Release(); free(frameMemory.data()); });

        if (frameProxy->GetInfo().spatialBlockCount == 0)
        {
            return error(context);
        }

        auto gridShuffle = DeserializeGridShuffleInfo(frameProxy);

        openvdb::GridPtrVec vdbGrids = {};
        res = m_DecompressorManager.DecompressFrame(frameMemory, frameProxy, gridShuffle, &vdbGrids);
        ReleaseGridShuffleInfo(gridShuffle);
        if (ZIB_FAILED(res))
        {
            std::string errorMessage = "Failed to decompress frame: " + LibraryUtils::ErrorCodeToString(res);
            addError(SOP_MESSAGE, errorMessage.c_str());
            return error(context);
        }

        gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "name", 1);
        GA_RWHandleS nameAttr{gdp->findPrimitiveAttribute("name")};
        for (size_t i = 0; i < vdbGrids.size(); ++i)
        {
            const openvdb::GridBase::Ptr grid = vdbGrids[i];

            if (!grid)
            {
                addError(SOP_MESSAGE, ("Failed to decompress channel: "s + frameProxy->GetInfo().channels[i].name).c_str());
                continue;
            }

            GU_PrimVDB* vdbPrim = GU_PrimVDB::buildFromGrid(*gdp, grid);
            nameAttr.set(vdbPrim->getMapOffset(), grid->getName());
            ApplyGridMetadata(vdbPrim, frameProxy);
        }

        ApplyDetailMetadata(gdp, frameProxy);

        return error(context);
    }

    int SOP_ZibraVDBDecompressor::OpenManagementWindow(void* data, int index, fpreal32 time, const PRM_Template* tplate)
    {
        PluginManagementWindow::ShowWindow();
        return 0;
    }

    void SOP_ZibraVDBDecompressor::ApplyGridMetadata(GU_PrimVDB* vdbPrim, FrameProxy* const frameProxy)
    {
        ApplyGridAttributeMetadata(vdbPrim, frameProxy);
        ApplyGridVisualizationMetadata(vdbPrim, frameProxy);
    }

    void SOP_ZibraVDBDecompressor::ApplyGridAttributeMetadata(GU_PrimVDB* vdbPrim, FrameProxy* const frameProxy)
    {
        {
            const std::string attributeMetadataNameV2 = "houdiniPrimitiveAttributesV2_"s + vdbPrim->getGridName();

            const char* metadataEntryV2 = frameProxy->GetMetadataByKey(attributeMetadataNameV2.c_str());

            if (metadataEntryV2)
            {
                auto primAttribMeta = nlohmann::json::parse(metadataEntryV2);
                Utils::LoadAttributesV2(gdp, GA_ATTRIB_PRIMITIVE, vdbPrim->getMapOffset(), primAttribMeta);
                return;
            }
        }

        {
            const std::string attributeMetadataNameV1 = "houdiniPrimitiveAttributes_"s + vdbPrim->getGridName();

            const char* metadataEntryV1 = frameProxy->GetMetadataByKey(attributeMetadataNameV1.c_str());

            if (metadataEntryV1)
            {
                auto primAttribMeta = nlohmann::json::parse(metadataEntryV1);
                Utils::LoadAttributesV1(gdp, GA_ATTRIB_PRIMITIVE, vdbPrim->getMapOffset(), primAttribMeta);
                return;
            }
        }
    }

    void SOP_ZibraVDBDecompressor::ApplyGridVisualizationMetadata(GU_PrimVDB* vdbPrim, FrameProxy* const frameProxy)
    {
        const std::string keyPrefix = "houdiniVisualizationAttributes_"s + vdbPrim->getGridName();

        const std::string keyVisMode = keyPrefix + "_mode";
        const char* visModeMetadata = frameProxy->GetMetadataByKey(keyVisMode.c_str());

        const std::string keyVisIso = keyPrefix + "_iso";
        const char* visIsoMetadata = frameProxy->GetMetadataByKey(keyVisIso.c_str());

        const std::string keyVisDensity = keyPrefix + "_density";
        const char* visDensityMetadata = frameProxy->GetMetadataByKey(keyVisDensity.c_str());

        const std::string keyVisLod = keyPrefix + "_lod";
        const char* visLodMetadata = frameProxy->GetMetadataByKey(keyVisLod.c_str());

        if (visModeMetadata && visIsoMetadata && visDensityMetadata && visLodMetadata)
        {
            GEO_VolumeOptions visOptions{};
            visOptions.myMode = static_cast<GEO_VolumeVis>(std::stoi(visModeMetadata));
            visOptions.myIso = std::stof(visIsoMetadata);
            visOptions.myDensity = std::stof(visDensityMetadata);
            visOptions.myLod = static_cast<GEO_VolumeVisLod>(std::stoi(visLodMetadata));
            vdbPrim->setVisOptions(visOptions);
        }
    }

    void SOP_ZibraVDBDecompressor::ApplyDetailMetadata(GU_Detail* gdp, FrameProxy* const frameProxy)
    {
        {
            const char* detailMetadataV2 = frameProxy->GetMetadataByKey("houdiniDetailAttributesV2");

            if (detailMetadataV2)
            {
                auto detailAttribMeta = nlohmann::json::parse(detailMetadataV2);
                Utils::LoadAttributesV2(gdp, GA_ATTRIB_DETAIL, 0, detailAttribMeta);
                return;
            }
        }

        {
            const char* detailMetadataV1 = frameProxy->GetMetadataByKey("houdiniDetailAttributes");

            if (detailMetadataV1)
            {
                auto detailAttribMeta = nlohmann::json::parse(detailMetadataV1);
                Utils::LoadAttributesV1(gdp, GA_ATTRIB_DETAIL, 0, detailAttribMeta);
                return;
            }
        }
    }

    inline char* TransferStr(const std::string& src) noexcept
    {
        char* dst = new char[src.length() + 1];
        strcpy(dst, src.c_str());
        return dst;
    }

    std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> SOP_ZibraVDBDecompressor::DeserializeGridShuffleInfo(FrameProxy* frameProxy) noexcept
    {
        static std::map<std::string, CE::Addons::OpenVDBUtils::GridVoxelType> strToVoxelType = {
            {"Float1", CE::Addons::OpenVDBUtils::GridVoxelType::Float1}, {"Float3", CE::Addons::OpenVDBUtils::GridVoxelType::Float3}};

        const char* meta = frameProxy->GetMetadataByKey("chShuffle");
        if (!meta)
            return {};

        auto serialized = nlohmann::json::parse(meta);
        if (!serialized.is_array())
        {
            addWarning(SOP_MESSAGE, "Corrupted metadata for grid reconstruction. Applying direct mapping.");
            return {};
        }
        std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> result{};
        for (const auto& serializedDesc : serialized)
        {
            CE::Addons::OpenVDBUtils::VDBGridDesc gridDesc{};
            if (!serializedDesc.is_object())
            {
                addWarning(SOP_MESSAGE, "Partially corrupted metadata for grid reconstruction. Skipping corrupted grids.");
                continue;
            }
            gridDesc.gridName = TransferStr(serializedDesc["gridName"]);
            gridDesc.voxelType = strToVoxelType.at(serializedDesc["voxelType"]);

            for (size_t i = 0; i < std::size(gridDesc.chSource); ++i)
            {
                auto key = std::string{"chSource"} + std::to_string(i);
                if (serializedDesc.contains(key) && serializedDesc[key].is_string())
                {
                    gridDesc.chSource[i] = TransferStr(serializedDesc[key]);
                }
            }
            result.emplace_back(gridDesc);
        }
        return result;
    }

    void SOP_ZibraVDBDecompressor::ReleaseGridShuffleInfo(std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc>& gridDescs) noexcept
    {
        for (const CE::Addons::OpenVDBUtils::VDBGridDesc& desc : gridDescs)
        {
            delete desc.gridName;
            for (size_t i = 0; i < std::size(desc.chSource); ++i)
            {
                delete desc.chSource[i];
            }
        }
        gridDescs.clear();
    }

} // namespace Zibra::ZibraVDBDecompressor
