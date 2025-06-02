#include "PrecompiledHeader.h"

#include "LOP_ZibraVDBDecompressor.h"

#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
#include "ui/PluginManagementWindow.h"
#include <LOP/USD/zibraVDBVolume.h>

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

#include <openvdb/io/File.h>

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

        SdfPath sdfPath(HUSDgetSdfPath("/ZibraVDB"));
        try {
            CreateZibraVDBVolumePrimitive(stage, sdfPath, filename.toStdString(), frameIndex);
            addMessage(LOP_MESSAGE, ("Created ZibraVDBVolume primitive at " + sdfPath.GetString()).c_str());
        }
        catch (const std::exception& e) {
            addError(LOP_MESSAGE, ("Failed to create ZibraVDBVolume primitive: " + std::string(e.what())).c_str());
        }

        auto gridShuffle = m_DecompressorManager.DeserializeGridShuffleInfo(frameContainer);
        openvdb::GridPtrVec vdbGrids = {};
        status = m_DecompressorManager.DecompressFrame(frameContainer, gridShuffle, &vdbGrids);
        m_DecompressorManager.ReleaseGridShuffleInfo(gridShuffle);

        if (status == CE::ZCE_SUCCESS && !vdbGrids.empty())
        {
            // Update frame tracking and get cached VDB file
            Utils::VDBCacheManager::GetInstance().UpdateNodeFrame(this, filename.toStdString(), frameIndex);
            std::string tempVDBPath = Utils::VDBCacheManager::GetInstance().GetOrCreateVDBCache(filename.toStdString(), frameIndex, vdbGrids);
            
            if (!tempVDBPath.empty())
            {
                InjectOpenVDBVolume(stage, SdfAssetPath(tempVDBPath), vdbGrids);
//                    ApplyVolumeVisualizationMetadata(stage, vdbGrids, frameContainer);
            }
            else
            {
                addWarning(LOP_MESSAGE, "Failed to create or access VDB cache file");
            }
        }
        else
        {
            addWarning(LOP_MESSAGE, "VDB decompression failed or no grids found");
        }

        frameContainer->Release();
        flags().setTimeDep(true);
        setLastModifiedPrims("/ZibraVDB");

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
        ZibraVDBZibraVDBVolume zibraVolume = ZibraVDBZibraVDBVolume::Define(stage, primPath);

        SdfAssetPath assetPath(binaryFilePath);
        zibraVolume.CreateFilePathAttr().Set(assetPath);
        zibraVolume.GetPrim().SetCustomDataByKey(TfToken("frameIndex"), VtValue(frameIndex));
    }

    void LOP_ZibraVDBDecompressor::InjectOpenVDBVolume(UsdStageRefPtr stage, const SdfAssetPath& filePath, const openvdb::GridPtrVec& vdbGrids)
    {
        SdfPath volPath("/openVDBVolume");
        UsdPrim volPrim = stage->DefinePrim(volPath, TfToken("Volume"));
        UsdVolVolume volume(volPrim);

        for (size_t i = 0; i < vdbGrids.size(); ++i)
        {
            const openvdb::GridBase::Ptr grid = vdbGrids[i];
            if (!grid)
                continue;
                
            std::string gridName = grid->getName();
            
            SdfPath assetPath = volPath.AppendChild(TfToken(gridName));
            UsdPrim assetPrim = stage->DefinePrim(assetPath, TfToken("OpenVDBAsset"));
            UsdVolOpenVDBAsset vdbAsset(assetPrim);

            vdbAsset.GetFilePathAttr().Set(filePath);
            vdbAsset.GetFieldNameAttr().Set(TfToken(gridName));
            vdbAsset.GetFieldIndexAttr().Set(static_cast<int>(i));

            volume.CreateFieldRelationship(TfToken(gridName), assetPrim.GetPath());
        }
    }

    void LOP_ZibraVDBDecompressor::ApplyVolumeVisualizationMetadata(UsdStageRefPtr stage, const openvdb::GridPtrVec& vdbGrids, CompressedFrameContainer* const frameContainer)
    {
        const char* detailMetadata = frameContainer->GetMetadataByKey("houdiniDetailAttributes");
        if (detailMetadata)
        {
            auto detailAttribMeta = nlohmann::json::parse(detailMetadata);
            
            SdfPath volSettingsPath("/VolumeSettings");
            UsdPrim volSettingsPrim = stage->GetPrimAtPath(volSettingsPath);
            if (!volSettingsPrim)
            {
                volSettingsPrim = stage->DefinePrim(volSettingsPath, TfToken("Scope"));
            }
            
            for (auto& [key, value] : detailAttribMeta.items())
            {
                if (key.find("volvis_") == 0)
                {
                    std::string attrName = "houdini:" + key;
                    if (value["t"] == "string")
                    {
                        volSettingsPrim.CreateAttribute(TfToken(attrName), SdfValueTypeNames->String).Set(value["v"].get<std::string>());
                    }
                    else if (value["t"] == "float32")
                    {
                        if (value["v"].is_array() && !value["v"].empty())
                        {
                            volSettingsPrim.CreateAttribute(TfToken(attrName), SdfValueTypeNames->Float).Set(value["v"][0].get<float>());
                        }
                    }
                    else if (value["t"] == "int32")
                    {
                        if (value["v"].is_array() && !value["v"].empty())
                        {
                            volSettingsPrim.CreateAttribute(TfToken(attrName), SdfValueTypeNames->Int).Set(value["v"][0].get<int>());
                        }
                    }
                }
            }
        }

        SdfPath volPath("/openVDBVolume");
        UsdPrim volPrim = stage->GetPrimAtPath(volPath);
        if (volPrim)
        {
            UsdVolVolume volume(volPrim);
            for (const auto& grid : vdbGrids)
            {
                if (grid)
                {
                    ApplyGridVisualizationMetadata(volume, grid->getName(), frameContainer);
                }
            }
        }
    }

    void LOP_ZibraVDBDecompressor::ApplyGridVisualizationMetadata(UsdVolVolume& volume, const std::string& gridName, CompressedFrameContainer* const frameContainer)
    {
        const std::string keyPrefix = "houdiniVisualizationAttributes_"s + gridName;

        const std::string keyVisMode = keyPrefix + "_mode";
        const char* visModeMetadata = frameContainer->GetMetadataByKey(keyVisMode.c_str());

        const std::string keyVisIso = keyPrefix + "_iso";
        const char* visIsoMetadata = frameContainer->GetMetadataByKey(keyVisIso.c_str());

        const std::string keyVisDensity = keyPrefix + "_density";
        const char* visDensityMetadata = frameContainer->GetMetadataByKey(keyVisDensity.c_str());

        const std::string keyVisLod = keyPrefix + "_lod";
        const char* visLodMetadata = frameContainer->GetMetadataByKey(keyVisLod.c_str());

        std::vector<std::string> metadataKeys;
        int metadataCount = frameContainer->GetMetadataCount();
        std::cout << "Metadata count: " << metadataCount << std::endl;
        for (int i=0; i<metadataCount; i++)
        {
            MetadataEntry curEntry;
            frameContainer->GetMetadataByIndex(i, &curEntry);
            std::cout << "Metadata key: " << curEntry.key << ", value: " << curEntry.value << std::endl;
        }

        if (visModeMetadata && visIsoMetadata && visDensityMetadata && visLodMetadata)
        {
            UsdPrim volPrim = volume.GetPrim();
            
            float densityScale = std::stof(visDensityMetadata);
            
            volPrim.CreateAttribute(TfToken("houdini:vis_mode"), SdfValueTypeNames->Int).Set(std::stoi(visModeMetadata));
            volPrim.CreateAttribute(TfToken("houdini:vis_iso"), SdfValueTypeNames->Float).Set(std::stof(visIsoMetadata));
            volPrim.CreateAttribute(TfToken("houdini:vis_density"), SdfValueTypeNames->Float).Set(densityScale);
            volPrim.CreateAttribute(TfToken("houdini:vis_lod"), SdfValueTypeNames->Int).Set(std::stoi(visLodMetadata));
            
            if (densityScale != 1.0f)
            {
                volPrim.CreateAttribute(TfToken("primvars:density_scale"), SdfValueTypeNames->Float).Set(densityScale);
                volPrim.CreateAttribute(TfToken("houdini:density_multiplier"), SdfValueTypeNames->Float).Set(densityScale);
            }
        }
    }
} // namespace Zibra::ZibraVDBDecompressor