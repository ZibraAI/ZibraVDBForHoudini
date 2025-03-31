#include "PrecompiledHeader.h"

#include "SOP_ZibraVDBDecompressor.h"

#include <Zibra/CE/Addons/OpenVDBEncoder.h>

#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
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
        Zibra::LibraryUtils::LoadLibrary();
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            return;
        }
    }

    SOP_ZibraVDBDecompressor::~SOP_ZibraVDBDecompressor() noexcept
    {
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            return;
        }
        m_DecompressorManager.Release();
    }

    OP_ERROR SOP_ZibraVDBDecompressor::cookMySop(OP_Context& context)
    {
        gdp->clearAndDestroy();

        if (!Zibra::LibraryUtils::IsPlatformSupported())
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED);
            return error(context);
        }

        Zibra::LibraryUtils::LoadLibrary();
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING);
            return error(context);
        }

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

        auto status = m_DecompressorManager.RegisterDecompressor(filename);
        if (status != CE::ZCE_SUCCESS)
        {
            addError(SOP_MESSAGE, "Failed to create a decompressor.");
            return error(context);
        }

        const exint frameIndex = evalInt(FRAME_PARAM_NAME, 0, context.getTime());

        CompressedFrameContainer* frameContainer = nullptr;
        FrameRange frameRange = m_DecompressorManager.GetFrameRange();

        if (frameIndex < frameRange.start || frameIndex > frameRange.end)
        {
            addWarning(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_FRAME_INDEX_OUT_OF_RANGE);
            return error(context);
        }

        frameContainer = m_DecompressorManager.FetchFrame(frameIndex);
        if (frameContainer->GetInfo().spatialBlockCount == 0)
        {
            frameContainer->Release();
            return error(context);
        }

        if (frameContainer == nullptr)
        {
            addError(SOP_MESSAGE, "Error when trying to fetch frame.");
            return error(context);
        }

        openvdb::GridPtrVec vdbGrids = {};
        status = m_DecompressorManager.DecompressFrame(frameContainer, &vdbGrids);
        if (status != CE::ZCE_SUCCESS)
        {
            frameContainer->Release();
            addError(SOP_MESSAGE, "Error when trying to decompress frame.");
            return error(context);
        }

        CE::Addons::OpenVDBUtils::OpenVDBReader::Feedback encodeMetadata = ReadFeedback(frameContainer);
        for (const auto& grid : vdbGrids)
        {
            const openvdb::math::Vec3d translationFromMetadata(-encodeMetadata.offsetX, -encodeMetadata.offsetY, -encodeMetadata.offsetZ);
            // transform3x3 will apply only 3x3 part of matrix, without translation.
            const openvdb::math::Vec3d frameTranslationInFrameCoordinateSystem =
                grid->transform().baseMap()->getAffineMap()->getMat4().transform3x3(translationFromMetadata);
            grid->transform().postTranslate(frameTranslationInFrameCoordinateSystem);
        }

        gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "name", 1);
        GA_RWHandleS nameAttr{gdp->findPrimitiveAttribute("name")};
        for (size_t i = 0; i < vdbGrids.size(); ++i)
        {
            const openvdb::GridBase::Ptr grid = vdbGrids[i];

            if (!grid)
            {
                addError(SOP_MESSAGE, ("Failed to decompress channel: "s + frameContainer->GetInfo().channels[i].name).c_str());
                continue;
            }

            GU_PrimVDB* vdbPrim = GU_PrimVDB::build(gdp);
            nameAttr.set(vdbPrim->getMapOffset(), grid->getName());
            vdbPrim->setGrid(*grid);

            ApplyGridMetadata(vdbPrim, frameContainer);
        }

        ApplyDetailMetadata(gdp, frameContainer);

        frameContainer->Release();

        return error(context);
    }

    int SOP_ZibraVDBDecompressor::OpenManagementWindow(void* data, int index, fpreal32 time, const PRM_Template* tplate)
    {
        Zibra::PluginManagementWindow::ShowWindow();
        return 0;
    }

    void SOP_ZibraVDBDecompressor::ApplyGridMetadata(GU_PrimVDB* vdbPrim, CompressedFrameContainer* const frameContainer)
    {
        ApplyGridAttributeMetadata(vdbPrim, frameContainer);
        ApplyGridVisualizationMetadata(vdbPrim, frameContainer);
    }

    void SOP_ZibraVDBDecompressor::ApplyGridAttributeMetadata(GU_PrimVDB* vdbPrim, CompressedFrameContainer* const frameContainer)
    {
        const std::string attributeMetadataName = "houdiniPrimitiveAttributes_"s + vdbPrim->getGridName();

        const char* metadataEntry = frameContainer->GetMetadataByKey(attributeMetadataName.c_str());

        if (metadataEntry)
        {
            auto primAttribMeta = nlohmann::json::parse(metadataEntry);
            switch (Utils::LoadEntityAttributesFromMeta(gdp, GA_ATTRIB_PRIMITIVE, vdbPrim->getMapOffset(), primAttribMeta))
            {
            case Utils::MetaAttributesLoadStatus::SUCCESS:
                break;
            case Utils::MetaAttributesLoadStatus::FATAL_ERROR_INVALID_METADATA:
                addWarning(SOP_MESSAGE, "Corrupted metadata for channel. Canceling attributes transfer.");
                break;
            case Utils::MetaAttributesLoadStatus::ERROR_PARTIALLY_INVALID_METADATA:
                addWarning(SOP_MESSAGE, "Partially corrupted metadata for channel. Skipping invalid attributes.");
                break;
            }
        }
    }

    void SOP_ZibraVDBDecompressor::ApplyGridVisualizationMetadata(GU_PrimVDB* vdbPrim, CompressedFrameContainer* const frameContainer)
    {
        const std::string keyPrefix = "houdiniVisualizationAttributes_"s + vdbPrim->getGridName();

        const std::string keyVisMode = keyPrefix + "_mode";
        const char* visModeMetadata = frameContainer->GetMetadataByKey(keyVisMode.c_str());

        const std::string keyVisIso = keyPrefix + "_iso";
        const char* visIsoMetadata = frameContainer->GetMetadataByKey(keyVisIso.c_str());

        const std::string keyVisDensity = keyPrefix + "_density";
        const char* visDensityMetadata = frameContainer->GetMetadataByKey(keyVisDensity.c_str());

        const std::string keyVisLod = keyPrefix + "_lod";
        const char* visLodMetadata = frameContainer->GetMetadataByKey(keyVisLod.c_str());

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

    void SOP_ZibraVDBDecompressor::ApplyDetailMetadata(GU_Detail* gdp, CompressedFrameContainer* const frameContainer)
    {
        const char* detailMetadata = frameContainer->GetMetadataByKey("houdiniDetailAttributes");

        if (!detailMetadata)
        {
            return;
        }

        auto detailAttribMeta = nlohmann::json::parse(detailMetadata);
        switch (Utils::LoadEntityAttributesFromMeta(gdp, GA_ATTRIB_DETAIL, 0, detailAttribMeta))
        {
        case Utils::MetaAttributesLoadStatus::SUCCESS:
            break;
        case Utils::MetaAttributesLoadStatus::FATAL_ERROR_INVALID_METADATA:
            addWarning(SOP_MESSAGE, "Corrupted metadata for channel. Canceling attributes transfer.");
            break;
        case Utils::MetaAttributesLoadStatus::ERROR_PARTIALLY_INVALID_METADATA:
            addWarning(SOP_MESSAGE, "Partially corrupted metadata for channel. Skipping invalid attributes.");
            break;
        }
    }

    CE::Addons::OpenVDBUtils::OpenVDBReader::Feedback SOP_ZibraVDBDecompressor::ReadFeedback(const CompressedFrameContainer* frameContainer)
    {
        const char* metadataKey = "houdiniDecodeMetadata";
        const char* metadataValue = frameContainer->GetMetadataByKey(metadataKey);
        if (!metadataValue)
        {
            return {};
        }
        CE::Addons::OpenVDBUtils::OpenVDBReader::Feedback feedback{};
        std::istringstream metadataStream(metadataValue);
        metadataStream >> feedback.offsetX >> feedback.offsetY >> feedback.offsetZ;
        return feedback;
    }

} // namespace Zibra::ZibraVDBDecompressor
