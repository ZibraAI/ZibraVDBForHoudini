#include "PrecompiledHeader.h"

#include "LOP_ZibraVDBImport.h"

#include <HUSD/HUSD_DataHandle.h>
#include <HUSD/HUSD_FindPrims.h>
#include <HUSD/XUSD_Data.h>
#include <pxr/usd/usdVol/openVDBAsset.h>

#include <bridge/LibraryUtils.h>
#include <licensing/LicenseManager.h>
#include <ui/PluginManagementWindow.h>
#include <utils/Helpers.h>

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
        if (LibraryUtils::IsSDKLibraryLoaded())
        {
            m_DecompressorManager.Initialize();
        }
    }

    LOP_ZibraVDBImport::~LOP_ZibraVDBImport() noexcept
    {
        if (LibraryUtils::IsSDKLibraryLoaded())
        {
            m_DecompressorManager.Release();
        }
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
        
        for (const auto& gridName : node->m_AvailableGrids)
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
        flags().setTimeDep(true);

        if (cookModifyInput(context) >= UT_ERROR_FATAL)
            return error(context);

        fpreal t = context.getTime();
        int frameIndex = context.getFrame();
        std::string filePath = GetFilePath(t);
        std::string fullPrimPath = GetPrimitivePath(t);
        std::string parentPrimType = GetParentPrimType(t);
        std::string fields = GetFields(t);

        if (filePath.empty())
        {
            addError(LOP_MESSAGE, "No ZibraVDB file specified");
            return error(context);
        }

        updateParmsFlags();
        if (!m_IsFileValid)
        {
            addError(LOP_MESSAGE, "Invalid or missing ZibraVDB file");
            return error(context);
        }

        if (!LibraryUtils::IsSDKLibraryLoaded())
        {
            addError(LOP_MESSAGE, "ZibraVDB library not loaded");
            return error(context);
        }

        // License may or may not be required depending on .zibravdb file
        // So we need to trigger license check, but if it fails we proceed with decompression
        LicenseManager::GetInstance().CheckLicense(Zibra::LicenseManager::Product::Decompression);

        if (m_LastFilePath != filePath)
        {
            if (!std::filesystem::exists(filePath))
            {
                addError(LOP_MESSAGE, "ZibraVDB file does not exist");
                return error(context);
            }
            auto status = m_DecompressorManager.RegisterDecompressor(UT_String(filePath));
            if (status != CE::ZCE_SUCCESS)
            {
                std::string errorMessage = "Failed to initialize decompressor: " + LibraryUtils::ErrorCodeToString(status);
                addError(LOP_MESSAGE, errorMessage.c_str());
                return error(context);
            }

            ParseAvailableGrids();
            m_LastFilePath = filePath;
        }

        auto frameRange = m_DecompressorManager.GetFrameRange();
        int currentFrame = static_cast<int>(std::round(context.getFrame()));
        if (currentFrame < frameRange.start || currentFrame > frameRange.end)
        {
            addWarning(LOP_MESSAGE, ("No volume data available for frame " + std::to_string(currentFrame) +
                      " (ZibraVDB range: " + std::to_string(frameRange.start) + "-" + std::to_string(frameRange.end) + ")").c_str());
            return error(context);
        }

        auto [primPath, primName] = ParsePrimitivePath(fullPrimPath, filePath);

        std::set<std::string> selectedFields = ParseSelectedFields(fields, m_AvailableGrids);
        if (selectedFields.empty())
        {
            addWarning(LOP_MESSAGE, "No valid fields selected");
        }

        HUSD_AutoWriteLock writelock(editableDataHandle());
        HUSD_AutoLayerLock layerlock(writelock);
        auto stage = writelock.data()->stage();

        if (!stage)
        {
            addError(LOP_MESSAGE, "Failed to get USD stage");
            return error(context);
        }

        CreateVolumeStructure(stage, primPath, primName, selectedFields, parentPrimType, t, frameIndex);
        return UT_ERROR_NONE;
    }

    bool LOP_ZibraVDBImport::updateParmsFlags()
    {
        bool changed = LOP_Node::updateParmsFlags();
        flags().setTimeDep(true);

        std::string filePath = GetFilePath(0);
        bool isValidFile = false;

        if (!filePath.empty())
        {
            if (std::filesystem::exists(filePath))
            {
                std::unordered_map<std::string, std::string> parseResult;
                if (Helpers::ParseZibraVDBURI(filePath, parseResult))
                {
                    isValidFile = true;
                }
            }
        }

        if (m_IsFileValid != isValidFile)
        {
            m_IsFileValid = isValidFile;
            changed = true;
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
        std::string sanitized = fieldName;
        
        if (sanitized.empty())
        {
            return "field";
        }
        
        for (char& c : sanitized)
        {
            if (!std::isalnum(c) && c != '_')
            {
                c = '_';
            }
        }
        
        while (!sanitized.empty() && sanitized[0] == '_')
        {
            sanitized.erase(0, 1);
        }
        while (!sanitized.empty() && sanitized.back() == '_')
        {
            sanitized.pop_back();
        }
        
        if (!sanitized.empty() && std::isdigit(sanitized[0]))
        {
            sanitized = "field_" + sanitized;
        }
        
        if (sanitized.empty())
        {
            sanitized = "field";
        }
        
        return sanitized;
    }

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

    void LOP_ZibraVDBImport::ParseAvailableGrids()
    {
        if (!LibraryUtils::IsSDKLibraryLoaded())
        {
            addError(LOP_MESSAGE, "ZibraVDB library not loaded");
            return;
        }

        m_AvailableGrids.clear();

        auto frameRange = m_DecompressorManager.GetFrameRange();
        if (frameRange.start > frameRange.end)
        {
            return;
        }

        auto frameContainer = m_DecompressorManager.FetchFrame(frameRange.start);
        if (!frameContainer)
        {
            return;
        }

        auto gridShuffle = m_DecompressorManager.DeserializeGridShuffleInfo(frameContainer);
        if (!gridShuffle.empty())
        {
            for (const auto& gridDesc : gridShuffle)
            {
                if (gridDesc.gridName && strlen(gridDesc.gridName) > 0)
                {
                    m_AvailableGrids.insert(gridDesc.gridName);
                }
            }
        }

        m_DecompressorManager.ReleaseGridShuffleInfo(gridShuffle);
        frameContainer->Release();
    }

    std::pair<std::string, std::string> LOP_ZibraVDBImport::ParsePrimitivePath(const std::string& fullPrimPath, const std::string& filePath)
    {
        std::string primPath;
        std::string primName;
        
        if (fullPrimPath.empty())
        {
            primPath = "";
            primName = "ZibraVolume";
        }
        else
        {
            size_t lastSlash = fullPrimPath.find_last_of('/');
            if (lastSlash == std::string::npos)
            {
                primPath = "";
                primName = fullPrimPath;
            }
            else if (lastSlash == 0)
            {
                primPath = "";
                primName = fullPrimPath.substr(1);
            }
            else
            {
                primPath = fullPrimPath.substr(0, lastSlash);
                primName = fullPrimPath.substr(lastSlash + 1);
            }
        }
        
        if (primName == "ZibraVolume" && !filePath.empty())
        {
            std::filesystem::path path(filePath);
            std::string filename = path.stem().string();
            primName = filename;
        }
        
        return std::make_pair(primPath, primName);
    }

    void LOP_ZibraVDBImport::CreateVolumeStructure(UsdStageRefPtr stage, const std::string& primPath, const std::string& primName,
                                                   const std::set<std::string>& selectedFields, const std::string& parentPrimType,
                                                   fpreal time, int frameIndex)
    {
        if (!stage)
        {
            return;
        }

        std::string filePath = GetFilePath(time);
        if (filePath.empty())
        {
            return;
        }

        if (selectedFields.empty())
        {
            return;
        }

        if (primPath != "/" && !primPath.empty() && parentPrimType != "none")
        {
            std::string parentPath = primPath;
            size_t pos = 0;
            while ((pos = parentPath.find('/', pos + 1)) != std::string::npos)
            {
                std::string currentPath = parentPath.substr(0, pos);
                if (!currentPath.empty() && currentPath != "/")
                {
                    SdfPath currentSdfPath(currentPath);
                    if (!stage->GetPrimAtPath(currentSdfPath))
                    {
                        // Use the specified parent primitive type for intermediate prims
                        TfToken primType = (parentPrimType == "scope") ? TfToken("Scope") : TfToken("Xform");
                        stage->DefinePrim(currentSdfPath, primType);
                    }
                }
            }
            
            SdfPath parentSdfPath(primPath);
            if (!stage->GetPrimAtPath(parentSdfPath))
            {
                TfToken primType = (parentPrimType == "scope") ? TfToken("Scope") : TfToken("Xform");
                stage->DefinePrim(parentSdfPath, primType);
            }
        }

        // Normalize primitive path: ensure leading slash, remove trailing slash
        std::string cleanPrimPath = primPath;
        if (!cleanPrimPath.empty())
        {
            if (cleanPrimPath[0] != '/') cleanPrimPath = "/" + cleanPrimPath;
            if (cleanPrimPath.length() > 1 && cleanPrimPath.back() == '/') cleanPrimPath.pop_back();
        }
        
        std::string safePrimName = primName.empty() ? "volume" : primName;
        std::string volumePath = cleanPrimPath + "/" + safePrimName;
        
        if (volumePath.empty() || volumePath[0] != '/')
        {
            const auto error = "Invalid volume path: '" + volumePath + "'";
            addError(LOP_MESSAGE, error.c_str());
            return;
        }
        
        SdfPath volumeSdfPath = SdfPath(volumePath);
        if (!volumeSdfPath.IsAbsolutePath() || volumeSdfPath.IsEmpty())
        {
            const auto error = "Invalid SdfPath for volume: " + volumePath;
            addError(LOP_MESSAGE, error.c_str());
            return;
        }

        UsdVolVolume volumePrim = UsdVolVolume::Define(stage, volumeSdfPath);
        if (!volumePrim)
        {
            const auto error = "Failed to create Volume prim: " + volumePath;
            addError(LOP_MESSAGE, error.c_str());
            return;
        }

        for (const std::string& fieldName : selectedFields)
        {
            std::string sanitizedFieldName = SanitizeFieldNameForUSD(fieldName);
            CreateOpenVDBAssetPrim(stage, volumePath, fieldName, sanitizedFieldName, filePath, frameIndex);
            CreateFieldRelationship(volumePrim, sanitizedFieldName, volumePath + "/" + sanitizedFieldName);
        }

        UT_StringArray primPaths;
        primPaths.append(volumePrim.GetPath().GetString().c_str());
        setLastModifiedPrims(primPaths);
    }

    void LOP_ZibraVDBImport::CreateOpenVDBAssetPrim(UsdStageRefPtr stage, const std::string& volumePath, const std::string& fieldName,
                                                    const std::string& sanitizedFieldName, const std::string& filePath, int frameIndex)
    {
        if (!stage)
            return;

        std::string assetPath = volumePath + "/" + sanitizedFieldName;
        if (assetPath.empty() || assetPath[0] != '/')
        {
            addError(LOP_MESSAGE, ("Invalid OpenVDBAsset path (not absolute): " + assetPath).c_str());
            return;
        }
        
        if (assetPath.find("//") != std::string::npos)
        {
            addError(LOP_MESSAGE, ("Invalid OpenVDBAsset path (double slashes): " + assetPath).c_str());
            return;
        }
        
        SdfPath assetSdfPath;
        try
        {
            assetSdfPath = SdfPath(assetPath);
        }
        catch (const std::exception& e)
        {
            addError(LOP_MESSAGE, ("Failed to create SdfPath for OpenVDBAsset: " + std::string(e.what())).c_str());
            return;
        }
        
        if (!assetSdfPath.IsAbsolutePath() || assetSdfPath.IsEmpty())
        {
            addError(LOP_MESSAGE, ("Invalid SdfPath for OpenVDBAsset: " + assetPath).c_str());
            return;
        }
        
        UsdVolOpenVDBAsset openVDBAsset = UsdVolOpenVDBAsset::Define(stage, assetSdfPath);
        if (!openVDBAsset)
        {
            addError(LOP_MESSAGE, ("Failed to create OpenVDBAsset prim for field: " + fieldName).c_str());
            return;
        }

        std::string zibraURL = GenerateZibraVDBURL(filePath, fieldName, frameIndex);

        UsdAttribute filePathAttr = openVDBAsset.GetFilePathAttr();
        if (filePathAttr)
        {
            UsdTimeCode timeCode = UsdTimeCode(frameIndex);
            filePathAttr.Set(SdfAssetPath(zibraURL), timeCode);
        }

        UsdAttribute fieldIndexAttr = openVDBAsset.GetFieldIndexAttr();
        if (fieldIndexAttr)
        {
            UsdTimeCode timeCode = UsdTimeCode(frameIndex);
            fieldIndexAttr.Set(0, timeCode);
        }

        UsdAttribute fieldNameAttr = openVDBAsset.GetFieldNameAttr();
        if (fieldNameAttr)
        {
            UsdTimeCode timeCode = UsdTimeCode(frameIndex);
            fieldNameAttr.Set(TfToken(fieldName), timeCode);
        }
    }

    void LOP_ZibraVDBImport::CreateFieldRelationship(UsdVolVolume& volumePrim, const std::string& fieldName, const std::string& assetPath)
    {
        if (!volumePrim)
            return;

        std::string relationshipName = "field:" + fieldName;

        UsdPrim prim = volumePrim.GetPrim();
        if (!prim)
        {
            return;
        }
        
        UsdRelationship fieldRel = prim.CreateRelationship(TfToken(relationshipName), true);
        
        if (fieldRel)
        {
            SdfPath targetPath(assetPath);
            fieldRel.SetTargets({targetPath});
        }
    }

    std::string LOP_ZibraVDBImport::GenerateZibraVDBURL(const std::string& filePath, const std::string& fieldName, int frameNumber) const
    {
        std::string url = filePath + "?frame=" + std::to_string(frameNumber);
        return url;
    }

} // namespace Zibra::ZibraVDBImport