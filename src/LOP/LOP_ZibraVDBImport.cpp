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
#include <CH/CH_Manager.h>
#include <algorithm>
#include <cctype>
#include <vector>
#include <iostream>
#include <sstream>
#include <filesystem>
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
    
    // Add help for the fields parameter
    static PRM_Template::PRM_Export PRMfieldsExport = PRM_Template::PRM_EXPORT_TBX;
    static const char* PRMfieldsHelp = "Select fields to import. Use '*' for all channels, or space-separated names like 'density temperature'.";
    
    // Hidden parameter to track file validity
    static PRM_Name PRMfileValid("__file_valid", "__file_valid");
    static PRM_Default PRMfileValidDefault(0);
    
    // Static choice list for fields dropdown - will be populated dynamically
    static PRM_Name fieldsChoiceNames[32]; // Max 32 choices
    static bool fieldsChoiceNamesInitialized = false;
    
    static void initializeFieldsChoiceNames()
    {
        if (fieldsChoiceNamesInitialized) return;
        
        // Initialize with default values
        fieldsChoiceNames[0].setToken("*");
        fieldsChoiceNames[0].setLabel("* (All Channels)");
        fieldsChoiceNames[1].setToken(nullptr);
        fieldsChoiceNames[1].setLabel(nullptr);
        
        fieldsChoiceNamesInitialized = true;
    }
    
    static PRM_ChoiceList PRMfieldsChoiceList(PRM_CHOICELIST_REPLACE, fieldsChoiceNames);
    
    // Parent primitive type choices
    static PRM_Name parentPrimTypeChoices[] = {
        PRM_Name("none", "None"),
        PRM_Name("xform", "Xform"),
        PRM_Name("scope", "Scope"),
        PRM_Name(0, 0)
    };
    static PRM_ChoiceList PRMparentPrimTypeChoiceList(PRM_CHOICELIST_SINGLE, parentPrimTypeChoices);
    
    // TODO: Re-enable these parameters later
    // static PRM_Name PRMprimitiveKind("primkind", "Primitive Kind");
    // static PRM_Default PRMprimitiveKindDefault(0.0f, "");
    
    // static PRM_Name PRMpurpose("purpose", "Purpose");
    // static PRM_Default PRMpurposeDefault(0.0f, "default");
    
    // static PRM_Name PRMcreateParent("createparent", "Create Parent Prims");
    // static PRM_Default PRMcreateParentDefault(1);
    
    // static PRM_Name PRMcreateViewportLOD("createviewportlod", "Create Viewport LOD");
    // static PRM_Default PRMcreateViewportLODDefault(0);
    
    // static PRM_Name PRMframe("frame", "Frame");
    // static PRM_Default PRMframeDefault(0.0f, "$F");

    OP_Node* LOP_ZibraVDBImport::Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new LOP_ZibraVDBImport{net, name, op};
    }

    PRM_Template* LOP_ZibraVDBImport::GetTemplateList() noexcept
    {
        // Initialize choice names
        initializeFieldsChoiceNames();
        
        static PRM_Template templateList[] = {
            PRM_Template(PRM_FILE, 1, &PRMfilePath, &PRMfilePathDefault),
            PRM_Template(PRM_TOGGLE | PRM_TYPE_INVISIBLE, 1, &PRMfileValid, &PRMfileValidDefault),
            PRM_Template(PRM_STRING, 1, &PRMprimitivePath, &PRMprimitivePathDefault),
            PRM_Template(PRM_ORD, 1, &PRMparentPrimType, &PRMparentPrimTypeDefault, &PRMparentPrimTypeChoiceList),
            PRM_Template(PRM_STRING, 1, &PRMfields, &PRMfieldsDefault, &PRMfieldsChoiceList),
            // TODO: Re-enable these parameters later
            // PRM_Template(PRM_STRING, 1, &PRMprimitiveKind, &PRMprimitiveKindDefault),
            // PRM_Template(PRM_STRING, 1, &PRMpurpose, &PRMpurposeDefault),
            // PRM_Template(PRM_TOGGLE, 1, &PRMcreateParent, &PRMcreateParentDefault),
            // PRM_Template(PRM_TOGGLE, 1, &PRMcreateViewportLOD, &PRMcreateViewportLODDefault),
            // PRM_Template(PRM_FLT, 1, &PRMframe, &PRMframeDefault),
            PRM_Template()
        };
        return templateList;
    }

    LOP_ZibraVDBImport::LOP_ZibraVDBImport(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : LOP_Node(net, name, entry)
    {
        // Initialize file validity to 0 (invalid)
        setInt("__file_valid", 0, 0, 0);
    }

    OP_ERROR LOP_ZibraVDBImport::cookMyLop(OP_Context &context)
    {
        flags().setTimeDep(true);

        if (cookModifyInput(context) >= UT_ERROR_FATAL)
            return error();

        fpreal t = context.getTime();
        std::string filePath = getFilePath(t);
        std::string fullPrimPath = getPrimitivePath(t);
        std::string parentPrimType = getParentPrimType(t);
        std::string fields = getFields(t);
        // TODO: Re-enable these parameters later
        // std::string primKind = getPrimitiveKind(t);
        // std::string purpose = getPurpose(t);
        // bool createParent = getCreateParent(t);
        // bool createViewportLOD = getCreateViewportLOD(t);
        // float frame = getFrame(t);

        if (filePath.empty())
        {
            addError(LOP_MESSAGE, "No ZibraVDB file specified");
            return error();
        }

        // Check if file is valid before processing
        if (evalInt("__file_valid", 0, t) == 0)
        {
            addError(LOP_MESSAGE, "Invalid or missing ZibraVDB file");
            return error();
        }

        // Extract primitive path and name from full path
        std::string primPath;
        std::string primName;
        
        if (fullPrimPath.empty())
        {
            primPath = "";  // No parent path for root level
            primName = "volume1";
        }
        else
        {
            // Find the last '/' to split path and name
            size_t lastSlash = fullPrimPath.find_last_of('/');
            if (lastSlash == std::string::npos)
            {
                // No slash found, treat as name only at root level
                primPath = "";
                primName = fullPrimPath;
            }
            else if (lastSlash == 0)
            {
                // Only one slash at the beginning (e.g., "/$OS")
                primPath = "";  // No parent path for root level
                primName = fullPrimPath.substr(1);
            }
            else
            {
                primPath = fullPrimPath.substr(0, lastSlash);
                primName = fullPrimPath.substr(lastSlash + 1);
            }
        }
        
        // Set volume name from filename if primName is default "volume1"
        if (primName == "volume1" && !filePath.empty())
        {
            std::filesystem::path path(filePath);
            std::string filename = path.stem().string(); // Get filename without extension
            primName = filename;
            std::cout << "Auto-set volume name to: " << primName << std::endl;
        }

        // Read and display ZibraVDB file metadata and collect available grids
        std::vector<std::string> availableGrids;
        readAndDisplayZibraVDBMetadata(filePath, availableGrids);

        // Update the fields choice list with available grids
        updateFieldsChoiceList(availableGrids);

        // Parse selected fields
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

        // Create volume structure for selected fields
        createVolumeStructure(stage, primPath, primName, selectedFields, parentPrimType, t);

        return error();
    }

    bool LOP_ZibraVDBImport::updateParmsFlags()
    {
        bool changed = LOP_Node::updateParmsFlags();
        flags().setTimeDep(true);
        
        // Check if file is valid and update the hidden parameter
        std::string filePath = getFilePath(0);
        bool isValidFile = false;
        
        // Only valid if file path is not empty, exists, and has correct extension
        if (!filePath.empty())
        {
            if (std::filesystem::exists(filePath))
            {
                // Quick validation - check if it's a .zibravdb file
                if (filePath.length() > 9 && filePath.substr(filePath.length() - 9) == ".zibravdb")
                {
                    isValidFile = true;
                }
            }
        }
        
        std::cout << "File validity check: '" << filePath << "' -> " << (isValidFile ? "VALID" : "INVALID") << std::endl;
        
        // Update the hidden file validity parameter
        int currentValid = evalInt("__file_valid", 0, 0);
        int newValid = isValidFile ? 1 : 0;
        
        if (currentValid != newValid)
        {
            setInt("__file_valid", 0, 0, newValid);
            changed = true;
            
            std::cout << "File validity changed: " << currentValid << " -> " << newValid << std::endl;
            
            if (isValidFile)
            {
                std::cout << "Valid ZibraVDB file detected - enabling parameters" << std::endl;
            }
            else
            {
                std::cout << "No valid ZibraVDB file - disabling parameters" << std::endl;
                
                // Clear choice list when file becomes invalid
                for (int i = 1; i < 32; ++i)
                {
                    fieldsChoiceNames[i].setToken(nullptr);
                    fieldsChoiceNames[i].setLabel(nullptr);
                }
            }
        }
        else
        {
            std::cout << "File validity unchanged: " << currentValid << std::endl;
        }
        
        // Enable/disable parameters based on file validity
        bool enableParams = isValidFile;
        
        // Enable/disable primitive path
        enableParm("primpath", enableParams);
        
        // Enable/disable parent primitive type
        enableParm("parentprimtype", enableParams);
        
        // Enable/disable fields
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
    
    std::string LOP_ZibraVDBImport::getPrimitiveKind(fpreal t) const
    {
        UT_String primKind;
        evalString(primKind, "primkind", 0, t);
        return primKind.toStdString();
    }
    
    std::string LOP_ZibraVDBImport::getPurpose(fpreal t) const
    {
        UT_String purpose;
        evalString(purpose, "purpose", 0, t);
        return purpose.toStdString();
    }
    
    bool LOP_ZibraVDBImport::getCreateParent(fpreal t) const
    {
        return evalInt("createparent", 0, t) != 0;
    }
    
    bool LOP_ZibraVDBImport::getCreateViewportLOD(fpreal t) const
    {
        return evalInt("createviewportlod", 0, t) != 0;
    }
    
    float LOP_ZibraVDBImport::getFrame(fpreal t) const
    {
        return static_cast<float>(evalFloat("frame", 0, t));
    }

    void LOP_ZibraVDBImport::updateFieldsChoiceList(const std::vector<std::string>& availableGrids)
    {
        // Store grid names in static storage to keep them alive
        static std::vector<std::string> staticGridNames;
        staticGridNames.clear();
        
        // Clear existing choices (skip first entry which is always "*")
        for (int i = 1; i < 32; ++i)
        {
            fieldsChoiceNames[i].setToken(nullptr);
            fieldsChoiceNames[i].setLabel(nullptr);
        }
        
        // Add available grids to choice list
        int choiceIndex = 1; // Start after "*" option
        for (const auto& gridName : availableGrids)
        {
            if (choiceIndex >= 31) break; // Leave room for null terminator
            
            staticGridNames.push_back(gridName);
            fieldsChoiceNames[choiceIndex].setToken(staticGridNames.back().c_str());
            fieldsChoiceNames[choiceIndex].setLabel(staticGridNames.back().c_str());
            choiceIndex++;
        }
        
        // Ensure list is properly terminated
        if (choiceIndex < 32)
        {
            fieldsChoiceNames[choiceIndex].setToken(nullptr);
            fieldsChoiceNames[choiceIndex].setLabel(nullptr);
        }
        
        std::cout << "Updated fields choice list with " << availableGrids.size() << " grids" << std::endl;
    }

    std::string LOP_ZibraVDBImport::sanitizeFieldNameForUSD(const std::string& fieldName)
    {
        std::string sanitized = fieldName;
        
        // Handle empty field name
        if (sanitized.empty())
        {
            return "field";
        }
        
        // Remove or replace invalid characters for USD paths
        for (char& c : sanitized)
        {
            if (!std::isalnum(c) && c != '_')
            {
                c = '_';
            }
        }
        
        // Remove any leading or trailing underscores that might have been created
        while (!sanitized.empty() && sanitized[0] == '_')
        {
            sanitized.erase(0, 1);
        }
        while (!sanitized.empty() && sanitized.back() == '_')
        {
            sanitized.pop_back();
        }
        
        // Ensure it doesn't start with a number
        if (!sanitized.empty() && std::isdigit(sanitized[0]))
        {
            sanitized = "field_" + sanitized;
        }
        
        // Ensure it's not empty after cleanup
        if (sanitized.empty())
        {
            sanitized = "field";
        }
        
        // Ensure it's not too long (USD has practical limits)
        if (sanitized.length() > 64)
        {
            sanitized = sanitized.substr(0, 64);
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
        
        // If "*" is specified, return all available grids
        if (fieldsStr == "*")
        {
            selectedFields = availableGrids;
            std::cout << "Selected all " << selectedFields.size() << " channels: ";
            for (size_t i = 0; i < selectedFields.size(); ++i)
            {
                if (i > 0) std::cout << ", ";
                std::cout << selectedFields[i];
            }
            std::cout << std::endl;
            return selectedFields;
        }
        
        // Parse space-separated field names
        std::istringstream iss(fieldsStr);
        std::string field;
        while (iss >> field)
        {
            // Check if this field exists in available grids
            auto it = std::find(availableGrids.begin(), availableGrids.end(), field);
            if (it != availableGrids.end())
            {
                selectedFields.push_back(field);
            }
            else
            {
                std::cout << "Warning: Field '" << field << "' not found in available grids" << std::endl;
            }
        }
        
        if (!selectedFields.empty())
        {
            std::cout << "Selected " << selectedFields.size() << " channel(s): ";
            for (size_t i = 0; i < selectedFields.size(); ++i)
            {
                if (i > 0) std::cout << ", ";
                std::cout << selectedFields[i];
            }
            std::cout << std::endl;
        }
        
        return selectedFields;
    }

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
            std::cout << "ERROR: USD stage is null" << std::endl;
            return;
        }

        std::string filePath = getFilePath(t);
        if (filePath.empty())
        {
            std::cout << "ERROR: File path is empty" << std::endl;
            return;
        }

        std::cout << "=== Creating Volume Structure ===" << std::endl;
        std::cout << "Stage: " << (stage ? "Valid" : "Null") << std::endl;
        std::cout << "Prim Path: '" << primPath << "' (length: " << primPath.length() << ")" << std::endl;
        std::cout << "Prim Name: '" << primName << "' (length: " << primName.length() << ")" << std::endl;
        std::cout << "Selected Fields Count: " << selectedFields.size() << std::endl;
        for (size_t i = 0; i < selectedFields.size(); ++i)
        {
            std::cout << "  Field " << i << ": '" << selectedFields[i] << "'" << std::endl;
        }
        
        // Debug parameter validation
        if (primPath.empty())
        {
            std::cout << "WARNING: primPath is empty!" << std::endl;
        }
        if (primName.empty())
        {
            std::cout << "WARNING: primName is empty!" << std::endl;
        }

        if (selectedFields.empty())
        {
            std::cout << "ERROR: No fields selected" << std::endl;
            return;
        }

        // Create parent directory structure (skip if primPath is root)
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
            
            // Create the parent directory prim
            SdfPath parentSdfPath(primPath);
            if (!stage->GetPrimAtPath(parentSdfPath))
            {
                TfToken primType = (parentPrimType == "scope") ? TfToken("Scope") : TfToken("Xform");
                stage->DefinePrim(parentSdfPath, primType);
                std::cout << "Created parent prim: " << primPath << " with type: " << primType << std::endl;
            }
        }

        // Clean and validate primPath
        std::string cleanPrimPath = primPath;
        
        // Handle empty primPath (root level volume)
        if (cleanPrimPath.empty())
        {
            cleanPrimPath = "";  // Keep empty for root level
        }
        else
        {
            // Ensure it starts with '/'
            if (cleanPrimPath[0] != '/')
            {
                cleanPrimPath = "/" + cleanPrimPath;
            }
            
            // Remove trailing '/' unless it's the root path
            if (cleanPrimPath.length() > 1 && cleanPrimPath.back() == '/')
            {
                cleanPrimPath.pop_back();
            }
        }
        
        // Create volume name
        std::string safePrimName = primName;
        if (safePrimName.empty())
        {
            safePrimName = "volume";
        }
        
        // Construct volume path
        std::string volumePath;
        if (cleanPrimPath.empty())
        {
            // Root level volume (e.g., for /$OS)
            volumePath = "/" + safePrimName;
        }
        else
        {
            volumePath = cleanPrimPath + "/" + safePrimName;
        }
        
        std::cout << "Creating single Volume prim with path: '" << volumePath << "'" << std::endl;
        std::cout << "  Original primPath: '" << primPath << "'" << std::endl;
        std::cout << "  cleanPrimPath: '" << cleanPrimPath << "'" << std::endl;
        std::cout << "  safePrimName: '" << safePrimName << "'" << std::endl;
        
        // Validate the constructed path
        if (volumePath.empty() || volumePath[0] != '/')
        {
            addError(LOP_MESSAGE, ("Invalid volume path: '" + volumePath + "'").c_str());
            return;
        }
        
        // Create the single Volume prim
        SdfPath volumeSdfPath;
        try
        {
            volumeSdfPath = SdfPath(volumePath);
        }
        catch (const std::exception& e)
        {
            addError(LOP_MESSAGE, ("Failed to create SdfPath for volume: " + std::string(e.what())).c_str());
            return;
        }
        
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

        std::cout << "Created Volume prim: " << volumePath << std::endl;

        // Create OpenVDBAsset prims for each field and set up field relationships
        for (const std::string& fieldName : selectedFields)
        {
            // Sanitize field name for USD path
            std::string sanitizedFieldName = sanitizeFieldNameForUSD(fieldName);
            
            // Create OpenVDBAsset prim for this field
            createOpenVDBAssetPrim(stage, volumePath, fieldName, sanitizedFieldName, filePath, t);
            
            // Create field relationship
            createFieldRelationship(volumePrim, fieldName, volumePath + "/" + sanitizedFieldName);
        }
    }

    void LOP_ZibraVDBImport::createOpenVDBAssetPrim(UsdStageRefPtr stage, const std::string& volumePath,
                                                   const std::string& fieldName, const std::string& sanitizedFieldName, const std::string& filePath, fpreal t)
    {
        if (!stage)
            return;

        // Construct asset path using the sanitized field name
        std::string assetPath = volumePath + "/" + sanitizedFieldName;
        
        std::cout << "Creating OpenVDBAsset prim with path: '" << assetPath << "'" << std::endl;
        
        // Validate path before creating SdfPath
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

        // Generate the zibravdb:// URL for this specific field
        std::string zibraURL = generateZibraVDBURL(filePath, fieldName);
        
        // Convert time to frame number to match Volume node behavior
        // Get the current frame from the global time manager
        fpreal frameRate = OPgetDirector()->getChannelManager()->getSamplesPerSec();
        fpreal currentFrame = t * frameRate + 1; // Add 1 to match Volume node frame numbering
        
        // Set the filePath attribute with time samples using frame number
        UsdAttribute filePathAttr = openVDBAsset.GetFilePathAttr();
        if (filePathAttr)
        {
            UsdTimeCode timeCode = UsdTimeCode(currentFrame);
            filePathAttr.Set(SdfAssetPath(zibraURL), timeCode);
        }

        // Set fieldIndex (assume 0 for now)
        UsdAttribute fieldIndexAttr = openVDBAsset.GetFieldIndexAttr();
        if (fieldIndexAttr)
        {
            UsdTimeCode timeCode = UsdTimeCode(currentFrame);
            fieldIndexAttr.Set(0, timeCode);
        }

        // Set fieldName to the specific field we're referencing
        UsdAttribute fieldNameAttr = openVDBAsset.GetFieldNameAttr();
        if (fieldNameAttr)
        {
            UsdTimeCode timeCode = UsdTimeCode(currentFrame);
            fieldNameAttr.Set(TfToken(fieldName), timeCode);
        }

        std::cout << "Created OpenVDBAsset prim: " << assetPath << " -> " << zibraURL << " (field: " << fieldName << ")" << std::endl;
    }

    void LOP_ZibraVDBImport::createFieldRelationship(UsdVolVolume& volumePrim, const std::string& fieldName, const std::string& assetPath)
    {
        if (!volumePrim)
            return;

        // Create the field relationship name (field:fieldName)
        std::string relationshipName = "field:" + fieldName;
        
        std::cout << "Creating field relationship: " << relationshipName << " -> " << assetPath << std::endl;
        
        // Get the underlying UsdPrim to create the relationship
        UsdPrim prim = volumePrim.GetPrim();
        if (!prim)
        {
            std::cout << "Failed to get UsdPrim from volume" << std::endl;
            return;
        }
        
        // Create the relationship
        UsdRelationship fieldRel = prim.CreateRelationship(TfToken(relationshipName), /* custom */ true);
        
        if (fieldRel)
        {
            SdfPath targetPath(assetPath);
            fieldRel.SetTargets({targetPath});
            std::cout << "Created field relationship: " << relationshipName << " pointing to " << assetPath << std::endl;
        }
        else
        {
            std::cout << "Failed to create field relationship: " << relationshipName << std::endl;
        }
    }

    std::string LOP_ZibraVDBImport::generateZibraVDBURL(const std::string& filePath, const std::string& fieldName) const
    {
        // Remove the redundant field parameter - the fieldName is set in the OpenVDBAsset
        // For now, we'll use frame=0 which can be enhanced later to support time-varying sequences
        std::string url = "zibravdb://" + filePath + "?frame=0";
        return url;
    }

} // namespace Zibra::ZibraVDBImport