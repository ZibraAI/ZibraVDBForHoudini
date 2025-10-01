#include "PrecompiledHeader.h"

#include "LOP_ZibraVDBImport.h"

#include <HUSD/HUSD_DataHandle.h>
#include <HUSD/HUSD_FindPrims.h>
#include <HUSD/XUSD_Data.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usdVol/openVDBAsset.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::ZibraVDBImport
{
    
    OP_Node* LOP_ZibraVDBImport::Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new LOP_ZibraVDBImport{net, name, op};
    }

    PRM_Template* LOP_ZibraVDBImport::GetTemplateList() noexcept
    {
        static PRM_Name theFileName(FILE_PARAM_NAME, "ZibraVDB File");
        static PRM_Default theFileDefault(0.0f, "$HIP/vol/$HIPNAME.zibravdb");

        static PRM_Name thePrimitivePath(PRIMPATH_PARAM_NAME, "Primitive Path");
        static PRM_Default thePrimitivePathDefault(0.0f, "/$OS");

        static PRM_Name theParentPrimType(PARENTPRIMTYPE_PARAM_NAME, "Parent Primitive Type");
        static PRM_Default theParentPrimTypeDefault(1, "xform");

        static PRM_Name theFields(FIELDS_PARAM_NAME, "Fields");
        static PRM_Default theFieldsDefault(0.0f, "*");
        static PRM_ChoiceList fieldsChoiceList(PRM_CHOICELIST_REPLACE, &LOP_ZibraVDBImport::BuildFieldsChoiceList);

        static PRM_Name parentPrimTypeChoices[] = {
            PRM_Name("none", "None"),
            PRM_Name("xform", "Xform"),
            PRM_Name("scope", "Scope"),
            PRM_Name(0, 0)
        };
        static PRM_ChoiceList PRMparentPrimTypeChoiceList(PRM_CHOICELIST_SINGLE, parentPrimTypeChoices);

        static PRM_Name theOpenPluginManagement(OPEN_PLUGIN_MANAGEMENT_PARAM_NAME, "Open Plugin Management");

        static PRM_Template templateList[] = {
            PRM_Template(PRM_FILE, 1, &theFileName, &theFileDefault),
            PRM_Template(PRM_STRING, 1, &thePrimitivePath, &thePrimitivePathDefault),
            PRM_Template(PRM_ORD, 1, &theParentPrimType, &theParentPrimTypeDefault, &PRMparentPrimTypeChoiceList),
            PRM_Template(PRM_STRING, 1, &theFields, &theFieldsDefault, &fieldsChoiceList),
            PRM_Template(PRM_CALLBACK, 1, &theOpenPluginManagement, nullptr, nullptr, nullptr, &LOP_ZibraVDBImport::OpenManagementWindow),
            PRM_Template()
        };
        return templateList;
    }

    LOP_ZibraVDBImport::LOP_ZibraVDBImport(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : LOP_Node(net, name, entry)
    {
        LibraryUtils::LoadSDKLibrary();
        flags().setTimeDep(true);
    }

    void LOP_ZibraVDBImport::BuildFieldsChoiceList(void* data, PRM_Name* choiceNames, int maxListSize, const PRM_SpareData*, const PRM_Parm*)
    {
        if (!choiceNames || maxListSize <= 0)
        {
            return;
        }

        auto node = static_cast<LOP_ZibraVDBImport*>(data);
        if (!node)
        {
            return;
        }

        int choiceIndex = 0;
        
        if (choiceIndex < maxListSize - 1)
        {
            choiceNames[choiceIndex].setToken("*");
            choiceNames[choiceIndex].setLabel("All Fields");
            choiceIndex++;
        }
        
        for (const auto& gridName : node->m_CachedFileInfo.availableGrids)
        {
            if (choiceIndex >= maxListSize - 1)
                break;
                
            choiceNames[choiceIndex].setToken(gridName.c_str());
            choiceNames[choiceIndex].setLabel(gridName.c_str());
            choiceIndex++;
        }
        
        if (choiceIndex < maxListSize)
        {
            choiceNames[choiceIndex].setToken(nullptr);
            choiceNames[choiceIndex].setLabel(nullptr);
        }
    }

    OP_ERROR LOP_ZibraVDBImport::cookMyLop(OP_Context &context)
    {
#define SHOW_ERROR_AND_RETURN(message) \
    addError(LOP_MESSAGE, message);    \
    return error(context);

        if (cookModifyInput(context) >= UT_ERROR_FATAL)
            return error(context);

        const fpreal t = context.getTime();
        const int currentFrame = static_cast<int>(context.getFrame());
        // const std::string parentPrimType = GetParentPrimType(t);
        const std::string fields = GetFields(t);

        if (GetFilePath(t).empty())
        {
            SHOW_ERROR_AND_RETURN("No ZibraVDB file specified")
        }

        auto volumePrimPath = std::filesystem::path(GetPrimitivePath(t));
        if (volumePrimPath.empty() || !volumePrimPath.has_filename())
        {
            SHOW_ERROR_AND_RETURN("No valid USD primitive name specified")
        }

        updateParmsFlags();
        if (!m_CachedFileInfo.error.empty())
        {
            SHOW_ERROR_AND_RETURN(m_CachedFileInfo.error.c_str())
        }

        if (m_CachedFileInfo.uuid.empty())
        {
            SHOW_ERROR_AND_RETURN("No valid ZibraVDB file loaded")
        }

        if (currentFrame < m_CachedFileInfo.frameStart || currentFrame > m_CachedFileInfo.frameEnd)
        {
            addWarning(LOP_MESSAGE, ("No volume data available for frame " + std::to_string(currentFrame) +
                      " (ZibraVDB range: " + std::to_string(m_CachedFileInfo.frameStart) + "-" + std::to_string(m_CachedFileInfo.frameEnd) + ")").c_str());
            return error(context);
        }

        const std::set<std::string> selectedFields = ParseSelectedFields(fields, m_CachedFileInfo.availableGrids);
        if (selectedFields.empty())
        {
            SHOW_ERROR_AND_RETURN("No valid fields selected")
        }

        const HUSD_AutoWriteLock writeLock(editableDataHandle());
        HUSD_AutoLayerLock layerLock(writeLock);
        const auto stage = writeLock.data()->stage();
        if (!stage)
        {
            SHOW_ERROR_AND_RETURN("Failed to get USD stage")
        }

        volumePrimPath = volumePrimPath.parent_path() / SanitizeFieldNameForUSD(volumePrimPath.filename());
        WriteZibraVolumeToStage(stage, volumePrimPath, selectedFields, currentFrame);
        return error(context);
    }

    bool LOP_ZibraVDBImport::updateParmsFlags()
    {
        bool changed = LOP_Node::updateParmsFlags();
        flags().setTimeDep(true);
        
        // Load file info when file parameter changes
        std::string currentFilePath = GetFilePath(0.0);
        if (m_CachedFileInfo.filePath != currentFilePath)
        {
            if (!currentFilePath.empty())
            {
                FileInfo newFileInfo = LoadFileInfo(currentFilePath);
                m_CachedFileInfo = std::move(newFileInfo);
                changed = true;
            }
            else
            {
                m_CachedFileInfo = FileInfo{};
                changed = true;
            }
        }
        
        return changed;
    }
    
    int LOP_ZibraVDBImport::OpenManagementWindow(void* data, int index, fpreal32 time, const PRM_Template* tplate)
    {
        PluginManagementWindow::ShowWindow();
        return 0;
    }
    
    std::string LOP_ZibraVDBImport::GetFilePath(fpreal t) const
    {
        UT_String filePath;
        evalString(filePath, FILE_PARAM_NAME, 0, t);
        return filePath.toStdString();
    }
    
    std::string LOP_ZibraVDBImport::GetPrimitivePath(fpreal t) const
    {
        UT_String primPath;
        evalString(primPath, PRIMPATH_PARAM_NAME, 0, t);
        return primPath.toStdString();
    }
    
    std::string LOP_ZibraVDBImport::GetParentPrimType(fpreal t) const
    {
        UT_String parentPrimType;
        evalString(parentPrimType, PARENTPRIMTYPE_PARAM_NAME, 0, t);
        return parentPrimType.toStdString();
    }
    
    std::string LOP_ZibraVDBImport::GetFields(fpreal t) const
    {
        UT_String fields;
        evalString(fields, FIELDS_PARAM_NAME, 0, t);
        return fields.toStdString();
    }
    
    std::string LOP_ZibraVDBImport::SanitizeFieldNameForUSD(const std::string& fieldName)
    {
        return SdfPath::IsValidIdentifier(fieldName) ? fieldName : TfMakeValidIdentifier(fieldName);
    }

    // Parses field selection string. Expected formats:
    // "*" - selects all available fields
    // "field1 field2 field3" - space-separated field names (no support for fields with spaces in names)
    std::set<std::string> LOP_ZibraVDBImport::ParseSelectedFields(const std::string& fieldsStr, const std::unordered_set<std::string>& availableGrids)
    {
        std::set<std::string> selectedFields;
        
        if (fieldsStr.empty())
        {
            return selectedFields;
        }
        
        if (fieldsStr == "*")
        {
            selectedFields.insert(availableGrids.begin(), availableGrids.end());
            return selectedFields;
        }
        
        std::istringstream iss(fieldsStr);
        std::string field;
        while (iss >> field)
        {
            if (availableGrids.find(field) != availableGrids.end())
            {
                selectedFields.insert(field);
            }
        }
        
        return selectedFields;
    }

    LOP_ZibraVDBImport::FileInfo LOP_ZibraVDBImport::LoadFileInfo(const std::string& filePath)
    {
        FileInfo info;
        info.filePath = filePath;
        
        if (!LibraryUtils::IsSDKLibraryLoaded())
        {
            info.error = "ZibraVDB library not loaded";
            return info;
        }

        if (!std::filesystem::exists(filePath))
        {
            info.error = "ZibraVDB file does not exist";
            return info;
        }

        std::unordered_map<std::string, std::string> parseResult;
        if (!Helpers::ParseZibraVDBURI(filePath, parseResult))
        {
            info.error = "No valid .zibravdb file found";
            return info;
        }

        // License may or may not be required depending on .zibravdb file
        // So we need to trigger license check, but if it fails we proceed with decompression
        LicenseManager::GetInstance().CheckLicense(Zibra::LicenseManager::Product::Decompression);

        Helpers::DecompressorManager decompressor;
        auto result = decompressor.Initialize();
        if (result != CE::ZCE_SUCCESS)
        {
            info.error = "Failed to initialize ZibraVDB decompressor";
            return info;
        }

        result = decompressor.RegisterDecompressor(UT_String(filePath));
        if (result != CE::ZCE_SUCCESS)
        {
            decompressor.Release();
            info.error = "Failed to register ZibraVDB file with decompressor";
            return info;
        }

        auto sequenceInfo = decompressor.GetSequenceInfo();
        info.uuid = Helpers::FormatUUID(sequenceInfo.fileUUID);
        
        auto frameRange = decompressor.GetFrameRange();
        info.frameStart = frameRange.start;
        info.frameEnd = frameRange.end;
        
        if (frameRange.start <= frameRange.end)
        {
            auto frameContainer = decompressor.FetchFrame(frameRange.start);
            if (frameContainer)
            {
                auto gridShuffle = decompressor.DeserializeGridShuffleInfo(frameContainer);
                if (!gridShuffle.empty())
                {
                    for (const auto& gridDesc : gridShuffle)
                    {
                        info.availableGrids.insert(gridDesc.gridName);
                    }
                }
                
                decompressor.ReleaseGridShuffleInfo(gridShuffle);
                frameContainer->Release();
            }
        }
        
        decompressor.Release();
        
        if (info.uuid.empty())
        {
            info.error = "Invalid ZibraVDB file - no UUID found";
        }
        
        return info;
    }

    void LOP_ZibraVDBImport::WriteZibraVolumeToStage(const UsdStageRefPtr& stage, const std::filesystem::path& volumePrimPath,
                                                     const std::set<std::string>& selectedFields, int frameIndex)
    {
        if (volumePrimPath.has_parent_path())
        {
            WriteParentPrimHierarchyToStage(stage, volumePrimPath);
        }

        const auto volumeSdfPath = SdfPath(volumePrimPath);
        if (!volumeSdfPath.IsAbsolutePath() || volumeSdfPath.IsEmpty())
        {
            const auto error = "Invalid SdfPath for volume: " + volumePrimPath.string();
            addError(LOP_MESSAGE, error.c_str());
            return;
        }

        const UsdVolVolume volumePrim = UsdVolVolume::Define(stage, volumeSdfPath);
        if (!volumePrim)
        {
            const auto error = "Failed to create Volume prim: " + volumePrimPath.string();
            addError(LOP_MESSAGE, error.c_str());
            return;
        }

        for (const std::string& fieldName : selectedFields)
        {
            const auto vdbPrimPath = std::filesystem::path(volumePrimPath) / SanitizeFieldNameForUSD(fieldName);
            WriteOpenVDBAssetPrimToStage(stage, vdbPrimPath, frameIndex);
            WriteVolumeFieldRelationshipsToStage(volumePrim, vdbPrimPath);
        }

        UT_StringArray primPaths;
        primPaths.append(volumePrim.GetPath().GetString().c_str());
        setLastModifiedPrims(primPaths);
    }

    // Creates the USD parent prim hierarchy for proper scene graph organization.
    // For "/world/volumes/sequence01", this creates "/world", "/world/volumes", etc.
    // Uses the specified parentPrimType (Xform or Scope) for all intermediate prims.
    void LOP_ZibraVDBImport::WriteParentPrimHierarchyToStage(const UsdStageRefPtr& stage, const std::filesystem::path& primPath)
    {
        auto parentPrimType = GetParentPrimType(0);
        if (parentPrimType == "none")
        {
            return;
        }

        TfToken primType = (parentPrimType == "scope") ? TfToken("Scope") : TfToken("Xform");
        
        std::filesystem::path currentPath = primPath;
        while (currentPath.has_parent_path() && currentPath != currentPath.parent_path())
        {
            currentPath = currentPath.parent_path();
            if (currentPath != "/" && !currentPath.empty())
            {
                if (SdfPath sdfPath(currentPath.string()); !stage->GetPrimAtPath(sdfPath))
                {
                    stage->DefinePrim(sdfPath, primType);
                }
            }
        }
        
        if (const SdfPath finalPath(primPath.string()); !stage->GetPrimAtPath(finalPath))
        {
            stage->DefinePrim(finalPath, primType);
        }
    }

    void LOP_ZibraVDBImport::WriteOpenVDBAssetPrimToStage(const UsdStageRefPtr& stage, const std::filesystem::path& assetPath, int frameIndex)
    {
        const auto sdfPath = SdfPath(assetPath);
        if (!sdfPath.IsAbsolutePath() || sdfPath.IsEmpty())
        {
            addError(LOP_MESSAGE, "Empty SdfPath for OpenVDBAsset");
            return;
        }
        const UsdVolOpenVDBAsset openVDBAsset = UsdVolOpenVDBAsset::Define(stage, sdfPath);
        if (!openVDBAsset)
        {
            addError(LOP_MESSAGE, ("Failed to create OpenVDBAsset prim for field: " + assetPath.filename().string()).c_str());
            return;
        }

        const std::string zibraURL = GetFilePath(0) + "?frame=" + std::to_string(frameIndex);
        const auto timeCode = UsdTimeCode(frameIndex);

        if (const auto filePathAttr = openVDBAsset.GetFilePathAttr())
        {
            filePathAttr.Set(SdfAssetPath(zibraURL), timeCode);
        }
        if (const auto fieldNameAttr = openVDBAsset.GetFieldNameAttr())
        {
            fieldNameAttr.Set(TfToken(assetPath.filename().string()), timeCode);
        }
        if (const auto fieldIndexAttr = openVDBAsset.GetFieldIndexAttr())
        {
            fieldIndexAttr.Set(0, timeCode);
        }
    }

    void LOP_ZibraVDBImport::WriteVolumeFieldRelationshipsToStage(const UsdVolVolume& volumePrim, const std::filesystem::path& primPath)
    {
        const std::string relationshipName = "field:" + primPath.filename().string();
        const UsdPrim prim = volumePrim.GetPrim();
        if (!prim)
        {
            return;
        }
        if (const auto fieldRel = prim.CreateRelationship(TfToken(relationshipName), true))
        {
            fieldRel.SetTargets({SdfPath(primPath.string())});
        }
    }

} // namespace Zibra::ZibraVDBImport