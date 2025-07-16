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
    }

    OP_ERROR LOP_ZibraVDBImport::cookMyLop(OP_Context &context)
    {
        //TODO dont output anything out of zibravdb file sequence range
        flags().setTimeDep(true);

        if (cookModifyInput(context) >= UT_ERROR_FATAL)
            return error();

        fpreal t = context.getTime();
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

        std::string primPath;
        std::string primName;
        
        if (fullPrimPath.empty())
        {
            // TODO refine this logic
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
        
        // TODO do we need this?
        if (primName == "ZibraVolume" && !filePath.empty())
        {
            std::filesystem::path path(filePath);
            std::string filename = path.stem().string();
            primName = filename;
        }

        std::vector<std::string> availableGrids;
        readAndDisplayZibraVDBMetadata(filePath, availableGrids);
        updateFieldsChoiceList(availableGrids);

        std::vector<std::string> selectedFields = parseSelectedFields(fields, availableGrids);
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

        createVolumeStructure(stage, primPath, primName, selectedFields, parentPrimType, t);
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
    
    void LOP_ZibraVDBImport::updateFieldsChoiceList(const std::vector<std::string>& availableGrids)
    {
        static std::vector<std::string> staticGridNames;
        staticGridNames.clear();
        
        for (int i = 1; i < 32; ++i)
        {
            fieldsChoiceNames[i].setToken(nullptr);
            fieldsChoiceNames[i].setLabel(nullptr);
        }
        
        int choiceIndex = 1;
        for (const auto& gridName : availableGrids)
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

    //TODO this will be further removed/reduced
    void LOP_ZibraVDBImport::readAndDisplayZibraVDBMetadata(const std::string& filePath, std::vector<std::string>& availableGrids)
    {
        std::cout << "\n=== ZibraVDB File Analysis ===" << std::endl;
        std::cout << "File path: " << filePath << std::endl;

        // Check if file exists
        if (!std::filesystem::exists(filePath))
        {
            std::cout << "ERROR: File does not exist!" << std::endl;
            addError(LOP_MESSAGE, "ZibraVDB file does not exist");
            return;
        }

        // Get file size
        auto fileSize = std::filesystem::file_size(filePath);
        std::cout << "File size: " << fileSize << " bytes (" << (fileSize / 1024.0 / 1024.0) << " MB)" << std::endl;

        // Load ZibraVDB library
        LibraryUtils::LoadLibrary();
        if (!LibraryUtils::IsLibraryLoaded())
        {
            std::cout << "ERROR: ZibraVDB library not loaded!" << std::endl;
            addError(LOP_MESSAGE, "ZibraVDB library not loaded");
            return;
        }

        // Initialize decompressor manager
        Zibra::Helpers::DecompressorManager decompressorManager;
        decompressorManager.Initialize();

        // Register the decompressor
        auto status = decompressorManager.RegisterDecompressor(UT_String(filePath));
        if (status != CE::ZCE_SUCCESS)
        {
            std::cout << "ERROR: Failed to register decompressor, status: " << static_cast<int>(status) << std::endl;
            addError(LOP_MESSAGE, "Failed to register ZibraVDB decompressor");
            return;
        }

        // Get frame range
        auto frameRange = decompressorManager.GetFrameRange();
        std::cout << "Frame range: " << frameRange.start << " - " << frameRange.end << " (total: " << (frameRange.end - frameRange.start + 1) << " frames)" << std::endl;

        // Try to fetch the first frame to get metadata
        if (frameRange.start <= frameRange.end)
        {
            std::cout << "\n=== Frame " << frameRange.start << " Analysis ===" << std::endl;
            
            CE::Decompression::CompressedFrameContainer* frameContainer = decompressorManager.FetchFrame(frameRange.start);
            if (frameContainer)
            {
                auto frameInfo = frameContainer->GetInfo();
                std::cout << "Spatial block count: " << frameInfo.spatialBlockCount << std::endl;
                std::cout << "Channel block count: " << frameInfo.channelBlockCount << std::endl;
                std::cout << "Channels count: " << frameInfo.channelsCount << std::endl;
                std::cout << "AABB size: [" << frameInfo.AABBSize[0] << ", " << frameInfo.AABBSize[1] << ", " << frameInfo.AABBSize[2] << "]" << std::endl;

                // Display channel information and collect grid names
                std::cout << "\n=== Channels ===" << std::endl;
                availableGrids.clear();
                for (size_t i = 0; i < frameInfo.channelsCount; ++i)
                {
                    std::cout << "Channel " << i << ":" << std::endl;
                    std::cout << "  Name: " << frameInfo.channels[i].name << std::endl;
                    std::cout << "  Min grid value: " << frameInfo.channels[i].minGridValue << std::endl;
                    std::cout << "  Max grid value: " << frameInfo.channels[i].maxGridValue << std::endl;
                    std::cout << "  Grid transform (4x4 matrix): " << std::endl;
                    
                    // Collect grid name for dropdown
                    if (frameInfo.channels[i].name && strlen(frameInfo.channels[i].name) > 0)
                    {
                        availableGrids.push_back(frameInfo.channels[i].name);
                    }
                    
                    // Display the transformation matrix as 4x4
                    auto& transform = frameInfo.channels[i].gridTransform;
                    for (int row = 0; row < 4; ++row)
                    {
                        std::cout << "    [";
                        for (int col = 0; col < 4; ++col)
                        {
                            std::cout << transform.raw[row * 4 + col];
                            if (col < 3) std::cout << ", ";
                        }
                        std::cout << "]" << std::endl;
                    }
                }

                // Display metadata (get from frame container directly)
                std::cout << "\n=== Metadata ===" << std::endl;
                // Try to get common metadata keys
                const char* keys[] = {
                    "houdiniDetailAttributes",
                    "houdiniPrimitiveAttributes",
                    "houdiniVisualizationAttributes",
                    "compression_quality",
                    "frame_index",
                    "timestamp"
                };
                
                for (const char* key : keys)
                {
                    const char* value = frameContainer->GetMetadataByKey(key);
                    if (value)
                    {
                        std::cout << "  " << key << ": " << value << std::endl;
                    }
                }

                // Try to get grid shuffle info
                auto gridShuffle = decompressorManager.DeserializeGridShuffleInfo(frameContainer);
                if (!gridShuffle.empty())
                {
                    std::cout << "\n=== Grid Shuffle Info ===" << std::endl;
                    std::cout << "Grid shuffle info available" << std::endl;
                    
                    // Try to decompress the frame to get more grid information
                    openvdb::GridPtrVec vdbGrids;
                    auto decompressStatus = decompressorManager.DecompressFrame(frameContainer, gridShuffle, &vdbGrids);
                    if (decompressStatus == CE::ZCE_SUCCESS)
                    {
                        std::cout << "\n=== VDB Grids ===" << std::endl;
                        std::cout << "Successfully decompressed " << vdbGrids.size() << " grids:" << std::endl;
                        
                        for (size_t i = 0; i < vdbGrids.size(); ++i)
                        {
                            if (vdbGrids[i])
                            {
                                std::cout << "Grid " << i << ":" << std::endl;
                                std::cout << "  Name: " << vdbGrids[i]->getName() << std::endl;
                                std::cout << "  Type: " << vdbGrids[i]->valueType() << std::endl;
                                std::cout << "  Class: " << vdbGrids[i]->getGridClass() << std::endl;
                                std::cout << "  Vector type: " << vdbGrids[i]->getVectorType() << std::endl;
                                std::cout << "  Active voxel count: " << vdbGrids[i]->activeVoxelCount() << std::endl;
                                
                                // Get bounding box
                                auto bbox = vdbGrids[i]->evalActiveVoxelBoundingBox();
                                std::cout << "  Bounding box: (" << bbox.min().x() << ", " << bbox.min().y() << ", " << bbox.min().z() << ") to (" 
                                         << bbox.max().x() << ", " << bbox.max().y() << ", " << bbox.max().z() << ")" << std::endl;
                            }
                        }
                    }
                    else
                    {
                        std::cout << "ERROR: Failed to decompress frame, status: " << static_cast<int>(decompressStatus) << std::endl;
                    }
                    
                    decompressorManager.ReleaseGridShuffleInfo(gridShuffle);
                }
                else
                {
                    std::cout << "ERROR: Failed to get grid shuffle info" << std::endl;
                }

                frameContainer->Release();
            }
            else
            {
                std::cout << "ERROR: Failed to fetch frame " << frameRange.start << std::endl;
            }
        }
        else
        {
            std::cout << "ERROR: Invalid frame range" << std::endl;
        }

        std::cout << "\n=== Available Grids ===" << std::endl;
        if (!availableGrids.empty())
        {
            std::cout << "Found " << availableGrids.size() << " grid(s):" << std::endl;
            for (size_t i = 0; i < availableGrids.size(); ++i)
            {
                std::cout << "  " << i << ": " << availableGrids[i] << std::endl;
            }
            
            // Fields parameter now defaults to "*" so no auto-setting needed
            std::cout << "Fields parameter: " << getFields(0) << std::endl;
        }
        else
        {
            std::cout << "No grids found in file!" << std::endl;
        }
        
        std::cout << "\n=== End Analysis ===" << std::endl;
    }

    void LOP_ZibraVDBImport::createVolumeStructure(UsdStageRefPtr stage, const std::string& primPath,
                                                  const std::string& primName, const std::vector<std::string>& selectedFields, const std::string& parentPrimType, fpreal t)
    {
        if (!stage)
        {
            return;
        }

        std::string filePath = getFilePath(t);
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
            createOpenVDBAssetPrim(stage, volumePath, fieldName, sanitizedFieldName, filePath, t);
            createFieldRelationship(volumePrim, sanitizedFieldName, volumePath + "/" + sanitizedFieldName);
        }
    }

    void LOP_ZibraVDBImport::createOpenVDBAssetPrim(UsdStageRefPtr stage, const std::string& volumePath,
                                                   const std::string& fieldName, const std::string& sanitizedFieldName, const std::string& filePath, fpreal t)
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

        fpreal frameRate = OPgetDirector()->getChannelManager()->getSamplesPerSec();
        fpreal currentFrame = t * frameRate + 1;
        
        int roundedFrame = static_cast<int>(std::round(currentFrame));
        fpreal startFrame = evalFloat("fstart", 0, t);
        int roundedStartFrame = static_cast<int>(std::round(startFrame));
        int relativeFrame = roundedFrame - roundedStartFrame;
        
        if (relativeFrame < 0) {
            relativeFrame = 0;
        }
        
        std::string zibraURL = generateZibraVDBURL(filePath, fieldName, relativeFrame);
        UsdAttribute filePathAttr = openVDBAsset.GetFilePathAttr();
        if (filePathAttr)
        {
            UsdTimeCode timeCode = UsdTimeCode(roundedFrame);
            filePathAttr.Set(SdfAssetPath(zibraURL), timeCode);
        }

        UsdAttribute fieldIndexAttr = openVDBAsset.GetFieldIndexAttr();
        if (fieldIndexAttr)
        {
            UsdTimeCode timeCode = UsdTimeCode(roundedFrame);
            fieldIndexAttr.Set(0, timeCode);
        }

        UsdAttribute fieldNameAttr = openVDBAsset.GetFieldNameAttr();
        if (fieldNameAttr)
        {
            UsdTimeCode timeCode = UsdTimeCode(roundedFrame);
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
        std::string url = ZIB_VDB_SCHEMA + filePath + "?frame=" + std::to_string(frameNumber);
        return url;
    }

} // namespace Zibra::ZibraVDBImport