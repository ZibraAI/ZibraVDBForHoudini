#include "PrecompiledHeader.h"
#include "LOP_ZibraVDBImport.h"
#include <HUSD/HUSD_DataHandle.h>
#include <HUSD/XUSD_Data.h>
#include <UT/UT_StringHolder.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usdVol/volume.h>
#include <pxr/usd/usdVol/openVDBAsset.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usd/timeCode.h>
#include <HUSD/HUSD_FindPrims.h>
#include <LOP/LOP_PRMShared.h>
#include <PRM/PRM_SpareData.h>
#include <OP/OP_Director.h>
#include <CH/CH_LocalVariable.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <set>
#include <Zibra/CE/Decompression.h>
#include "SOP/DecompressorManager/DecompressorManager.h"
#include "bridge/LibraryUtils.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::ZibraVDBImport
{
    static PRM_Name PRMfilePath("file", "ZibraVDB File");
    static PRM_Default PRMfilePathDefault(0.0f, "$HIP/vol/$HIPNAME.zibravdb");
    
    static PRM_Name PRMprimitivePath("primpath", "Primitive Path");
    static PRM_Default PRMprimitivePathDefault(0.0f, "/$OS");
    
    static PRM_Name PRMparentPrimType("parentprimtype", "Parent Primitive Type");
    static PRM_Default PRMparentPrimTypeDefault(1, "xform");
    
    static PRM_Name PRMfields("fields", "Fields");
    static PRM_Default PRMfieldsDefault(0.0f, "*");
    
    static PRM_Template::PRM_Export PRMfieldsExport = PRM_Template::PRM_EXPORT_TBX;
    static const char* PRMfieldsHelp = "Select fields to import. Use '*' for all channels, or space-separated names like 'density temperature'.";
    
    static PRM_Name PRMfileValid("__file_valid", "__file_valid");
    static PRM_Default PRMfileValidDefault(0);
    
    static PRM_Name fieldsChoiceNames[32];
    static bool fieldsChoiceNamesInitialized = false;
    
    static void initializeFieldsChoiceNames()
    {
        if (fieldsChoiceNamesInitialized) return;
        
        fieldsChoiceNames[0].setToken("*");
        fieldsChoiceNames[0].setLabel("* (All Channels)");
        fieldsChoiceNames[1].setToken(nullptr);
        fieldsChoiceNames[1].setLabel(nullptr);
        
        fieldsChoiceNamesInitialized = true;
    }
    
    static PRM_ChoiceList PRMfieldsChoiceList(PRM_CHOICELIST_REPLACE, fieldsChoiceNames);
    static PRM_Name parentPrimTypeChoices[] = {
        PRM_Name("none", "None"),
        PRM_Name("xform", "Xform"),
        PRM_Name("scope", "Scope"),
        PRM_Name(0, 0)
    };
    static PRM_ChoiceList PRMparentPrimTypeChoiceList(PRM_CHOICELIST_SINGLE, parentPrimTypeChoices);
    
    OP_Node* LOP_ZibraVDBImport::Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new LOP_ZibraVDBImport{net, name, op};
    }

    PRM_Template* LOP_ZibraVDBImport::GetTemplateList() noexcept
    {
        initializeFieldsChoiceNames();
        
        static PRM_Template templateList[] = {
            PRM_Template(PRM_FILE, 1, &PRMfilePath, &PRMfilePathDefault),
            PRM_Template(PRM_TOGGLE | PRM_TYPE_INVISIBLE, 1, &PRMfileValid, &PRMfileValidDefault),
            PRM_Template(PRM_STRING, 1, &PRMprimitivePath, &PRMprimitivePathDefault),
            PRM_Template(PRM_ORD, 1, &PRMparentPrimType, &PRMparentPrimTypeDefault, &PRMparentPrimTypeChoiceList),
            PRM_Template(PRM_STRING, 1, &PRMfields, &PRMfieldsDefault, &PRMfieldsChoiceList),
            PRM_Template()
        };
        return templateList;
    }

    LOP_ZibraVDBImport::LOP_ZibraVDBImport(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : LOP_Node(net, name, entry)
    {
        setInt("__file_valid", 0, 0, 0);
        
        LibraryUtils::LoadZibSDKLibrary();
        if (LibraryUtils::IsLibraryLoaded())
        {
            m_DecompressorManager.Initialize();
        }
    }

    LOP_ZibraVDBImport::~LOP_ZibraVDBImport() noexcept
    {
        if (LibraryUtils::IsLibraryLoaded())
        {
            m_DecompressorManager.Release();
        }
    }

    OP_ERROR LOP_ZibraVDBImport::cookMyLop(OP_Context &context)
    {
        flags().setTimeDep(true);

        if (cookModifyInput(context) >= UT_ERROR_FATAL)
            return error();

        fpreal t = context.getTime();
        int frameIndex = context.getFrame();
        std::string filePath = getFilePath(t);
        std::string fullPrimPath = getPrimitivePath(t);
        std::string parentPrimType = getParentPrimType(t);
        std::string fields = getFields(t);

        if (filePath.empty())
        {
            addError(LOP_MESSAGE, "No ZibraVDB file specified");
            return error();
        }

        if (evalInt("__file_valid", 0, t) == 0)
        {
            addError(LOP_MESSAGE, "Invalid or missing ZibraVDB file");
            return error();
        }

        if (!LibraryUtils::IsLibraryLoaded())
        {
            addError(LOP_MESSAGE, "ZibraVDB library not loaded");
            return error();
        }

        if (m_LastFilePath != filePath)
        {
            if (std::filesystem::exists(filePath))
            {
                auto status = m_DecompressorManager.RegisterDecompressor(UT_String(filePath));
                if (status != CE::ZCE_SUCCESS)
                {
                    addError(LOP_MESSAGE, "Failed to register ZibraVDB decompressor");
                    return error();
                }
                
                parseAvailableGrids();
                updateFieldsChoiceList();
                m_LastFilePath = filePath;
            }
            else
            {
                addError(LOP_MESSAGE, "ZibraVDB file does not exist");
                return error();
            }
        }

        auto frameRange = m_DecompressorManager.GetFrameRange();
        int currentFrame = static_cast<int>(std::round(context.getFrame()));
        if (currentFrame < frameRange.start || currentFrame > frameRange.end)
        {
            addWarning(LOP_MESSAGE, ("No volume data available for frame " + std::to_string(currentFrame) +
                      " (ZibraVDB range: " + std::to_string(frameRange.start) + "-" + std::to_string(frameRange.end) + ")").c_str());
            return error();
        }

        // TODO refine this logic
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

        std::vector<std::string> selectedFields = parseSelectedFields(fields, m_AvailableGrids);
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
            return error();
        }

        createVolumeStructure(stage, primPath, primName, selectedFields, parentPrimType, t, frameIndex);
        return error();
    }

    bool LOP_ZibraVDBImport::updateParmsFlags()
    {
        bool changed = LOP_Node::updateParmsFlags();
        flags().setTimeDep(true);

        std::string filePath = getFilePath(0);
        bool isValidFile = false;
        
        if (!filePath.empty())
        {
            if (std::filesystem::exists(filePath))
            {
                if (filePath.length() > 9 && filePath.substr(filePath.length() - 9) == ".zibravdb")
                {
                    isValidFile = true;
                }
            }
        }
        
        int currentValid = evalInt("__file_valid", 0, 0);
        int newValid = isValidFile ? 1 : 0;
        
        if (currentValid != newValid)
        {
            setInt("__file_valid", 0, 0, newValid);
            changed = true;
            if (!isValidFile)
            {
                for (int i = 1; i < 32; ++i)
                {
                    fieldsChoiceNames[i].setToken(nullptr);
                    fieldsChoiceNames[i].setLabel(nullptr);
                }
            }
        }
        
        bool enableParams = isValidFile;
        enableParm("primpath", enableParams);
        enableParm("parentprimtype", enableParams);
        enableParm("fields", enableParams);
        
        if (!enableParams)
        {
            changed = true;
        }
        
        return changed;
    }
    
    std::string LOP_ZibraVDBImport::getFilePath(fpreal t) const
    {
        UT_String filePath;
        evalString(filePath, "file", 0, t);
        return filePath.toStdString();
    }
    
    std::string LOP_ZibraVDBImport::getPrimitivePath(fpreal t) const
    {
        UT_String primPath;
        evalString(primPath, "primpath", 0, t);
        return primPath.toStdString();
    }
    
    std::string LOP_ZibraVDBImport::getParentPrimType(fpreal t) const
    {
        UT_String parentPrimType;
        evalString(parentPrimType, "parentprimtype", 0, t);
        return parentPrimType.toStdString();
    }
    
    std::string LOP_ZibraVDBImport::getFields(fpreal t) const
    {
        UT_String fields;
        evalString(fields, "fields", 0, t);
        return fields.toStdString();
    }
    
    void LOP_ZibraVDBImport::updateFieldsChoiceList()
    {
        static std::vector<std::string> staticGridNames;
        staticGridNames.clear();
        
        for (int i = 1; i < 32; ++i)
        {
            fieldsChoiceNames[i].setToken(nullptr);
            fieldsChoiceNames[i].setLabel(nullptr);
        }
        
        int choiceIndex = 1;
        for (const auto& gridName : m_AvailableGrids)
        {
            if (choiceIndex >= 31) break; // Leave room for null terminator
            
            staticGridNames.push_back(gridName);
            fieldsChoiceNames[choiceIndex].setToken(staticGridNames.back().c_str());
            fieldsChoiceNames[choiceIndex].setLabel(staticGridNames.back().c_str());
            choiceIndex++;
        }
        
        if (choiceIndex < 32)
        {
            fieldsChoiceNames[choiceIndex].setToken(nullptr);
            fieldsChoiceNames[choiceIndex].setLabel(nullptr);
        }
    }

    std::string LOP_ZibraVDBImport::sanitizeFieldNameForUSD(const std::string& fieldName)
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

    std::vector<std::string> LOP_ZibraVDBImport::parseSelectedFields(const std::string& fieldsStr, const std::vector<std::string>& availableGrids)
    {
        std::vector<std::string> selectedFields;
        
        if (fieldsStr.empty())
        {
            return selectedFields;
        }
        
        if (fieldsStr == "*")
        {
            selectedFields = availableGrids;
            return selectedFields;
        }
        
        std::istringstream iss(fieldsStr);
        std::string field;
        while (iss >> field)
        {
            auto it = std::find(availableGrids.begin(), availableGrids.end(), field);
            if (it != availableGrids.end())
            {
                selectedFields.push_back(field);
            }
        }
        
        if (!selectedFields.empty())
        {
            addError(LOP_MESSAGE, "No fields selected.");
        }
        
        return selectedFields;
    }

    void LOP_ZibraVDBImport::parseAvailableGrids()
    {
        if (!LibraryUtils::IsLibraryLoaded())
        {
            addError(LOP_MESSAGE, "ZibraVDB library not loaded");
            return;
        }

        m_AvailableGrids.clear();

        auto frameRange = m_DecompressorManager.GetFrameRange();
        if (frameRange.start <= frameRange.end)
        {
            CE::Decompression::CompressedFrameContainer* frameContainer = m_DecompressorManager.FetchFrame(frameRange.start);
            if (frameContainer)
            {
                auto frameInfo = frameContainer->GetInfo();
                for (size_t i = 0; i < frameInfo.channelsCount; ++i)
                {
                    if (frameInfo.channels[i].name && strlen(frameInfo.channels[i].name) > 0)
                    {
                        m_AvailableGrids.push_back(frameInfo.channels[i].name);
                    }
                }
                frameContainer->Release();
            }
        }
    }

    void LOP_ZibraVDBImport::createVolumeStructure(UsdStageRefPtr stage, const std::string& primPath, const std::string& primName,
                                                   const std::vector<std::string>& selectedFields, const std::string& parentPrimType,
                                                   fpreal time, int frameIndex)
    {
        if (!stage)
        {
            return;
        }

        std::string filePath = getFilePath(time);
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

        std::string cleanPrimPath = primPath;
        if (cleanPrimPath.empty())
        {
            cleanPrimPath = "";
        }
        else
        {
            if (cleanPrimPath[0] != '/')
            {
                cleanPrimPath = "/" + cleanPrimPath;
            }
            if (cleanPrimPath.length() > 1 && cleanPrimPath.back() == '/')
            {
                cleanPrimPath.pop_back();
            }
        }
        
        std::string safePrimName = primName;
        if (safePrimName.empty())
        {
            safePrimName = "volume";
        }
        
        std::string volumePath;
        if (cleanPrimPath.empty())
        {
            volumePath = "/" + safePrimName;
        }
        else
        {
            volumePath = cleanPrimPath + "/" + safePrimName;
        }
        
        if (volumePath.empty() || volumePath[0] != '/')
        {
            addError(LOP_MESSAGE, ("Invalid volume path: '" + volumePath + "'").c_str());
            return;
        }
        
        SdfPath volumeSdfPath = SdfPath(volumePath);
        if (!volumeSdfPath.IsAbsolutePath() || volumeSdfPath.IsEmpty())
        {
            addError(LOP_MESSAGE, ("Invalid SdfPath for volume: " + volumePath).c_str());
            return;
        }

        UsdVolVolume volumePrim = UsdVolVolume::Define(stage, volumeSdfPath);
        if (!volumePrim)
        {
            addError(LOP_MESSAGE, ("Failed to create Volume prim: " + volumePath).c_str());
            return;
        }

        // TODO currently Transform node effects this volume only in "All geometry" mode. This might potentially be used for default mode.
//        UsdGeomXformable xformable(volumePrim.GetPrim());
//        if (xformable) {
//            UsdGeomXformOp transformOp = xformable.AddTransformOp(UsdGeomXformOp::PrecisionDouble);
//            if (transformOp) {
//                GfMatrix4d identityMatrix(1.0);
//                transformOp.Set(identityMatrix);
//            }
//        }

        for (const std::string& fieldName : selectedFields)
        {
            std::string sanitizedFieldName = sanitizeFieldNameForUSD(fieldName);
            createOpenVDBAssetPrim(stage, volumePath, fieldName, sanitizedFieldName, filePath, frameIndex);
            createFieldRelationship(volumePrim, sanitizedFieldName, volumePath + "/" + sanitizedFieldName);
        }
    }

    void LOP_ZibraVDBImport::createOpenVDBAssetPrim(UsdStageRefPtr stage, const std::string& volumePath, const std::string& fieldName,
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

        std::string zibraURL = generateZibraVDBURL(filePath, fieldName, frameIndex);
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

    void LOP_ZibraVDBImport::createFieldRelationship(UsdVolVolume& volumePrim, const std::string& fieldName, const std::string& assetPath)
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

    std::string LOP_ZibraVDBImport::generateZibraVDBURL(const std::string& filePath, const std::string& fieldName, int frameNumber) const
    {
        std::string url = filePath + "?frame=" + std::to_string(frameNumber);
        return url;
    }

} // namespace Zibra::ZibraVDBImport