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

#include "../USD/ZibraVDBVolumeSchema/src/zibraVDBVolume.h"

#include <openvdb/io/File.h>

#include <filesystem>

namespace Zibra::ZibraVDBDecompressor
{
    using namespace std::literals;
    using namespace CE::Decompression;
    PXR_NAMESPACE_USING_DIRECTIVE

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

        static PRM_Name thePrimPathName(PRIMPATH_PARAM_NAME, "Primitive Path");
        static PRM_Default thePrimPathDefault(0, "/ZibraVDB");

        static PRM_Name theCreateVDBPreviewName(CREATE_VDB_PREVIEW_PARAM_NAME, "Create VDB Preview");
        static PRM_Default theCreateVDBPreviewDefault(1);

        static PRM_Name theVDBOutputPathName(VDB_OUTPUT_PATH_PARAM_NAME, "VDB Output Path");
        static PRM_Default theVDBOutputPathDefault(0, "$TEMP/zibravdb_preview_$OS_$F.vdb");

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
            PRM_Template(PRM_STRING, 1, &thePrimPathName, &thePrimPathDefault),
            PRM_Template(PRM_TOGGLE, 1, &theCreateVDBPreviewName, &theCreateVDBPreviewDefault),
            PRM_Template(PRM_FILE, 1, &theVDBOutputPathName, &theVDBOutputPathDefault),
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
    }

    LOP_ZibraVDBDecompressor::~LOP_ZibraVDBDecompressor() noexcept
    {
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

        UT_String primPath = "";
        evalString(primPath, PRIMPATH_PARAM_NAME, 0, t);
        if (primPath == "")
        {
            addError(LOP_MESSAGE, "Primitive path cannot be empty");
            return error();
        }

        const exint frameIndex = evalInt(FRAME_PARAM_NAME, 0, t);
        const bool createVDBPreview = evalInt(CREATE_VDB_PREVIEW_PARAM_NAME, 0, t);

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

        CompressedFrameContainer* frameContainer = m_DecompressorManager.FetchFrame(frameIndex);
        if (!frameContainer || frameContainer->GetInfo().spatialBlockCount == 0)
        {
            if (frameContainer)
                frameContainer->Release();
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
            frameContainer->Release();
            return error();
        }
        UsdStageRefPtr stage = writelock.data()->stage();

        SdfPath sdfPath(HUSDgetSdfPath(primPath));
        try {
            CreateZibraVDBVolumePrimitive(stage, sdfPath, filename.toStdString(), frameIndex);
            addMessage(LOP_MESSAGE, ("Created ZibraVDBVolume primitive at " + sdfPath.GetString()).c_str());
        }
        catch (const std::exception& e) {
            addError(LOP_MESSAGE, ("Failed to create ZibraVDBVolume primitive: " + std::string(e.what())).c_str());
        }

        if (createVDBPreview)
        {
            auto gridShuffle = m_DecompressorManager.DeserializeGridShuffleInfo(frameContainer);
            openvdb::GridPtrVec vdbGrids = {};
            status = m_DecompressorManager.DecompressFrame(frameContainer, gridShuffle, &vdbGrids);
            m_DecompressorManager.ReleaseGridShuffleInfo(gridShuffle);

            if (status == CE::ZCE_SUCCESS && !vdbGrids.empty())
            {
                std::filesystem::path p(filename.toStdString());
                auto folder = p.parent_path().string();
                auto name = p.stem().string();
                auto vdbOutputPath = UT_String(folder + "/" + name + "_frame_" + std::to_string(frameIndex) + ".vdb");

//                UT_String vdbOutputPath = "";
//                evalString(vdbOutputPath, VDB_OUTPUT_PATH_PARAM_NAME, 0, t);

                std::string tempVDBPath = WriteTemporaryVDBFile(vdbGrids, vdbOutputPath.toStdString());
                if (!tempVDBPath.empty())
                {
                    InjectOpenVDBVolume(stage, SdfAssetPath(tempVDBPath));
                }
                else
                {
                    addWarning(LOP_MESSAGE, "Failed to write temporary VDB file");
                }
            }
            else
            {
                addWarning(LOP_MESSAGE, "VDB decompression failed or no grids found");
            }
        }

        frameContainer->Release();
        flags().setTimeDep(true);
        setLastModifiedPrims(primPath);

        return error();
    }

    int LOP_ZibraVDBDecompressor::OpenManagementWindow(void* data, int index, fpreal32 time, const PRM_Template* tplate)
    {
        PluginManagementWindow::ShowWindow();
        return 0;
    }

    void LOP_ZibraVDBDecompressor::CreateZibraVDBVolumePrimitive(UsdStageRefPtr stage, const SdfPath& primPath,
                                                                 const std::string& binaryFilePath, int frameIndex)
    {
        // Create the ZibraVDBVolume primitive using our custom schema
        ZibraVDBZibraVDBVolume zibraVolume = ZibraVDBZibraVDBVolume::Define(stage, primPath);

        // Set the file path attribute
        SdfAssetPath assetPath(binaryFilePath);
        zibraVolume.CreateFilePathAttr().Set(assetPath);

        // Add frame index as custom data instead of metadata to avoid warnings
        zibraVolume.GetPrim().SetCustomDataByKey(TfToken("frameIndex"), VtValue(frameIndex));
    }

    void LOP_ZibraVDBDecompressor::InjectOpenVDBVolume(UsdStageRefPtr stage, const SdfAssetPath& filePath)
    {
        SdfPath volPath("/openVDBVolume");
        UsdPrim volPrim = stage->DefinePrim(volPath, TfToken("Volume"));
        UsdVolVolume volume(volPrim);

        SdfPath assetPath = volPath.AppendChild(TfToken("density"));
        UsdPrim assetPrim = stage->DefinePrim(assetPath, TfToken("OpenVDBAsset"));
        UsdVolOpenVDBAsset vdbAsset(assetPrim);

        vdbAsset.GetFilePathAttr().Set(filePath);
        vdbAsset.GetFieldNameAttr().Set(TfToken("density"));
        vdbAsset.GetFieldIndexAttr().Set(0);

        volume.CreateFieldRelationship(TfToken("density"), assetPrim.GetPath());
    }

    std::string LOP_ZibraVDBDecompressor::WriteTemporaryVDBFile(const openvdb::GridPtrVec& vdbGrids, const std::string& basePath)
    {
        if (vdbGrids.empty())
            return "";

        try
        {
            std::filesystem::path outputPath(basePath);
            std::filesystem::create_directories(outputPath.parent_path());

            openvdb::io::File file(basePath);
            file.write(vdbGrids);
            file.close();

            if (std::filesystem::exists(basePath) && std::filesystem::file_size(basePath) > 0)
            {
                return basePath;
            }
            else
            {
                addWarning(LOP_MESSAGE, "VDB file was not written correctly");
                return "";
            }
        }
        catch (const std::exception& e)
        {
            addWarning(LOP_MESSAGE, ("Failed to write temporary VDB file: "s + e.what()).c_str());
            return "";
        }
    }

} // namespace Zibra::ZibraVDBDecompressor