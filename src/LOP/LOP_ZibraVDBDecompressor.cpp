#include "PrecompiledHeader.h"

#include "LOP_ZibraVDBDecompressor.h"

#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
#include "ui/PluginManagementWindow.h"
#include <HUSD/HUSD_Constants.h>
#include <HUSD/HUSD_DataHandle.h>
#include <HUSD/XUSD_Data.h>
#include <HUSD/XUSD_Utils.h>
#include <LOP/LOP_Error.h>
#include <SOP/SOP_Node.h>
#include <PRM/PRM_Include.h>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdVol/volume.h>
#include <pxr/usd/usdVol/openVDBAsset.h>
#include <pxr/usd/sdf/assetPath.h>

#include <filesystem>

namespace Zibra::ZibraVDBDecompressor
{
    using namespace std::literals;

    OP_Node* LOP_ZibraVDBDecompressor::Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new LOP_ZibraVDBDecompressor{net, name, op};
    }

    PRM_Template* LOP_ZibraVDBDecompressor::GetTemplateList() noexcept
    {
        static PRM_Name theFileName(FILENAME_PARAM_NAME, "Input File");
        static PRM_Default theFileDefault(0, "$HIP/vol/$HIPNAME.$OS.zibravdb");

        static PRM_Name theFrameName(FRAME_PARAM_NAME, "Sequence Frame");
        static PRM_Default theFrameDefault(0, "$F");

        static PRM_Name theReloadCacheName(REFRESH_CALLBACK_PARAM_NAME, "Reload Cache");
        static PRM_Callback theReloadCallback{[](void* node, int index, fpreal64 time, const PRM_Template* tplate) -> int {
            auto self = static_cast<LOP_ZibraVDBDecompressor*>(node);
            self->forceRecook();
            return 1;
        }};

        static PRM_Name theOpenPluginManagementButtonName(OPEN_PLUGIN_MANAGEMENT_BUTTON_NAME, "Open Plugin Management");

        static PRM_Template templateList[] = {
            PRM_Template(PRM_FILE, 1, &theFileName, &theFileDefault),
            PRM_Template(PRM_INT, 1, &theFrameName, &theFrameDefault),
            PRM_Template(PRM_CALLBACK, 1, &theReloadCacheName, nullptr, nullptr, nullptr, theReloadCallback),
            PRM_Template(PRM_CALLBACK, 1, &theOpenPluginManagementButtonName, nullptr, nullptr, nullptr,
                         &LOP_ZibraVDBDecompressor::OpenManagementWindow),
            PRM_Template()
        };
        return templateList;
    }

    LOP_ZibraVDBDecompressor::LOP_ZibraVDBDecompressor(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : LOP_Node(net, name, entry)
    {
        LibraryUtils::LoadLibrary();
        if (!LibraryUtils::IsLibraryLoaded())
        {
            return;
        }
        Utils::VDBCacheManager::GetInstance().RegisterNode(this, getName().toStdString());
    }

    LOP_ZibraVDBDecompressor::~LOP_ZibraVDBDecompressor() noexcept
    {
        Utils::VDBCacheManager::GetInstance().UnregisterNode(this);
        
        if (!LibraryUtils::IsLibraryLoaded())
        {
            return;
        }
        m_DecompressorManager.Release();
    }

    OP_ERROR LOP_ZibraVDBDecompressor::cookMyLop(OP_Context& context)
    {
        if (!LibraryUtils::IsPlatformSupported())
        {
            addError(LOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED);
            return error();
        }

        openvdb::initialize();
        LibraryUtils::LoadLibrary();
        if (!LibraryUtils::IsLibraryLoaded())
        {
            addError(LOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING);
            return error();
        }

        fpreal t = context.getTime();

        UT_String filename = "";
        evalString(filename, FILENAME_PARAM_NAME, 0, t);
        if (filename == "")
        {
            addError(LOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_NO_FILE_SELECTED);
            return error();
        }

        if (!std::filesystem::exists(filename.toStdString()))
        {
            addError(LOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_FILE_NOT_FOUND);
            return error();
        }

        const exint frameIndex = evalInt(FRAME_PARAM_NAME, 0, t);

        m_DecompressorManager.Initialize();
        auto status = m_DecompressorManager.RegisterDecompressor(filename);
        if (status != CE::ZCE_SUCCESS)
        {
            addError(LOP_MESSAGE, "Failed to create a decompressor.");
            return error();
        }

        FrameRange frameRange = m_DecompressorManager.GetFrameRange();
        if (frameIndex < frameRange.start || frameIndex > frameRange.end)
        {
            addWarning(LOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_FRAME_INDEX_OUT_OF_RANGE);
            return error();
        }

        HUSD_DataHandle &dataHandle = editableDataHandle();
        if (!dataHandle.hasData())
        {
            dataHandle.createNewData();
        }
        HUSD_AutoWriteLock writelock(dataHandle);
        if (!writelock.data() || !writelock.data()->stage())
        {
            addError(LOP_MESSAGE, "Could not get USD stage from input");
            return error();
        }
        UsdStageRefPtr stage = writelock.data()->stage();

        // Use binary filename as primitive name
        std::filesystem::path filePath(filename.toStdString());
        std::string primName = filePath.stem().string();
        SdfPath sdfPath(HUSDgetSdfPath("/" + primName));
        try {
            CreateZibraVDBVolumePrimitive(stage, sdfPath, filename.toStdString(), frameIndex, writelock);
            addMessage(LOP_MESSAGE, ("Created USD Volume primitive at " + sdfPath.GetString()).c_str());
        }
        catch (const std::exception& e) {
            addError(LOP_MESSAGE, ("Failed to create USD Volume primitive: " + std::string(e.what())).c_str());
        }

        // Asset resolver will handle decompression automatically when USD resolves the asset path
        // Update frame tracking for cache management
        Utils::VDBCacheManager::GetInstance().UpdateNodeFrame(this, filename.toStdString(), frameIndex);

        flags().setTimeDep(true);
        setLastModifiedPrims("/" + primName);

        return error();
    }

    int LOP_ZibraVDBDecompressor::OpenManagementWindow(void* data, int index, fpreal32 time, const PRM_Template* tplate)
    {
        PluginManagementWindow::ShowWindow();
        return 0;
    }

    void LOP_ZibraVDBDecompressor::CreateZibraVDBVolumePrimitive(UsdStageRefPtr stage, const SdfPath& primPath,
                                                                 const std::string& binaryFilePath, int frameIndex,
                                                                 HUSD_AutoWriteLock& writelock)
    {
        // Create standard USD Volume prim
        UsdPrim volPrim = stage->DefinePrim(primPath, TfToken("Volume"));
        UsdVolVolume volume(volPrim);

        // Get the output file path from the USD stage
        std::string outputPath;
        if (stage->GetRootLayer()) {
            std::string rootLayerPath = stage->GetRootLayer()->GetRealPath();
            if (!rootLayerPath.empty()) {
                outputPath = rootLayerPath;
            }
        }
        
        // Create relative path from USD save location to .zibravdb file
        std::string relativePath;
        if (!outputPath.empty()) {
            std::filesystem::path usdPath(outputPath);
            std::filesystem::path vdbPath(binaryFilePath);
            relativePath = std::filesystem::relative(vdbPath, usdPath.parent_path()).string();
        } else {
            // Fallback to filename only if no output path available
            std::filesystem::path filePath(binaryFilePath);
            relativePath = filePath.filename().string();
        }
        
        std::string zibraVDBURI = "zibravdb://" + relativePath;
        SdfAssetPath assetPath(zibraVDBURI);
        
        // Create OpenVDBAsset named "density"
        SdfPath vdbAssetPath = primPath.AppendChild(TfToken("density"));
        UsdPrim vdbAssetPrim = stage->DefinePrim(vdbAssetPath, TfToken("OpenVDBAsset"));
        UsdVolOpenVDBAsset vdbAsset(vdbAssetPrim);
        
        // Set the required attributes
        vdbAsset.GetFilePathAttr().Set(assetPath);
        vdbAsset.GetFieldIndexAttr().Set(0);
        vdbAsset.GetFieldNameAttr().Set(TfToken("density"));
        
        // Create field relationship for density field
        volume.CreateFieldRelationship(TfToken("density"), vdbAssetPrim.GetPath());
        
        // Store metadata for reference
        volPrim.SetCustomDataByKey(TfToken("zibraVDBSource"), VtValue(binaryFilePath));
        volPrim.SetCustomDataByKey(TfToken("frameIndex"), VtValue(frameIndex));
    }

} // namespace Zibra::ZibraVDBDecompressor