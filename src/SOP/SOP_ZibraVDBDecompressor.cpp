#include "PrecompiledHeader.h"

#include "SOP_ZibraVDBDecompressor.h"

namespace Zibra::ZibraVDBDecompressor
{
    using namespace std::literals;
    using namespace CE::Decompression;

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
        LibraryUtils::LoadSDKLibrary();
    }

    SOP_ZibraVDBDecompressor::~SOP_ZibraVDBDecompressor() noexcept
    {
        if (!LibraryUtils::IsSDKLibraryLoaded())
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

        LibraryUtils::LoadSDKLibrary();
        if (!LibraryUtils::IsSDKLibraryLoaded())
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING);
            return error(context);
        }

        // License may or may not be required depending on .zibravdb file
        // So we need to trigger license check, but if it fails we proceed with decompression
        LicenseManager::GetInstance().CheckLicense(LicenseManager::Product::Decompression);

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
            std::string errorMessage = "Failed to initialize decompressor: " + LibraryUtils::ErrorCodeToString(status);
            addError(SOP_MESSAGE, errorMessage.c_str());
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

        auto gridShuffle = m_DecompressorManager.DeserializeGridShuffleInfo(frameContainer);

        openvdb::GridPtrVec vdbGrids = {};
        status = m_DecompressorManager.DecompressFrame(frameContainer, gridShuffle, &vdbGrids);
        m_DecompressorManager.ReleaseGridShuffleInfo(gridShuffle);
        if (status != CE::ZCE_SUCCESS)
        {
            frameContainer->Release();
            addError(SOP_MESSAGE, "Error when trying to decompress frame.");
            return error(context);
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

            auto gridContext = std::make_pair(gdp, vdbPrim);
            switch (Utils::MetadataHelper::ApplyGridMetadata(gridContext, frameContainer))
            {
            case Utils::MetaAttributesLoadStatus::FATAL_ERROR_INVALID_METADATA:
                addWarning(SOP_MESSAGE, "Corrupted metadata for channel. Canceling attributes transfer.");
                break;
            case Utils::MetaAttributesLoadStatus::ERROR_PARTIALLY_INVALID_METADATA:
                addWarning(SOP_MESSAGE, "Partially corrupted metadata for channel. Skipping invalid attributes.");
                break;
            default:
                break;
            }
        }

        switch (Utils::MetadataHelper::ApplyDetailMetadata(gdp, frameContainer))
        {
        case Utils::MetaAttributesLoadStatus::FATAL_ERROR_INVALID_METADATA:
            addWarning(SOP_MESSAGE, "Corrupted metadata for channel. Canceling attributes transfer.");
            break;
        case Utils::MetaAttributesLoadStatus::ERROR_PARTIALLY_INVALID_METADATA:
            addWarning(SOP_MESSAGE, "Partially corrupted metadata for channel. Skipping invalid attributes.");
            break;
        default:
            break;
        }

        frameContainer->Release();

        return error(context);
    }

    int SOP_ZibraVDBDecompressor::OpenManagementWindow(void* data, int index, fpreal32 time, const PRM_Template* tplate)
    {
        PluginManagementWindow::ShowWindow();
        return 0;
    }

} // namespace Zibra::ZibraVDBDecompressor
