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
        static PRM_Name fileName(FILE_PARAM_NAME, "ZibraVDB File");
        static PRM_Default fileDefault(0.0f, "");

        static PRM_Name frameIndexName(FRAME_INDEX_PARAM_NAME, "Frame Index");
        static PRM_Default frameIndexDefault(0.0f, "$F");
        static PRM_Range frameIndexRange(PRM_RANGE_UI, 0, PRM_RANGE_UI, 100);

        static PRM_Name primitivePath(PRIMPATH_PARAM_NAME, "Primitive Path");
        static PRM_Default primitivePathDefault(0.0f, "/$OS");

        static PRM_Name parentPrimType(PARENTPRIMTYPE_PARAM_NAME, "Parent Primitive Type");
        static PRM_Default parentPrimTypeDefault(1, "xform");

        static PRM_Name channels(CHANNELS_PARAM_NAME, "Channels");
        static PRM_Default channelsDefault(0.0f, "*");
        static PRM_ChoiceList channelsChoiceList(PRM_CHOICELIST_REPLACE, &LOP_ZibraVDBImport::BuildChannelsChoiceList);

        static PRM_Name parentPrimTypeChoices[] = {
            PRM_Name("none", "None"),
            PRM_Name("xform", "Xform"),
            PRM_Name("scope", "Scope"),
            PRM_Name(0, 0)
        };
        static PRM_ChoiceList PRMparentPrimTypeChoiceList(PRM_CHOICELIST_SINGLE, parentPrimTypeChoices);

        static PRM_Name openPluginManagement(OPEN_PLUGIN_MANAGEMENT_PARAM_NAME, "Open Plugin Management");

        static PRM_Template templateList[] = {
            PRM_Template(PRM_FILE, 1, &fileName, &fileDefault),
            PRM_Template(PRM_INT, 1, &frameIndexName, &frameIndexDefault, 0, &frameIndexRange),
            PRM_Template(PRM_STRING, 1, &primitivePath, &primitivePathDefault),
            PRM_Template(PRM_ORD, 1, &parentPrimType, &parentPrimTypeDefault, &PRMparentPrimTypeChoiceList),
            PRM_Template(PRM_STRING, 1, &channels, &channelsDefault, &channelsChoiceList),
            PRM_Template(PRM_CALLBACK, 1, &openPluginManagement, nullptr, nullptr, nullptr, &LOP_ZibraVDBImport::OpenManagementWindow),
            PRM_Template()
        };
        return templateList;
    }

    LOP_ZibraVDBImport::LOP_ZibraVDBImport(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : LOP_Node(net, name, entry)
    {
        LibraryUtils::LoadSDKLibrary();
    }

    void LOP_ZibraVDBImport::BuildChannelsChoiceList(void* data, PRM_Name* choiceNames, int maxListSize, const PRM_SpareData*, const PRM_Parm*)
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
            choiceNames[choiceIndex].setLabel("All Channels");
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

        updateParmsFlags();

        const fpreal t = context.getTime();
        const int currentFrameIndex = GetFrameIndex(t);
        const std::string channels = GetChannels(t);

        if (GetFilePath(t).empty())
        {
            SHOW_ERROR_AND_RETURN("No ZibraVDB file specified")
        }

        const std::string rawPrimPath = GetPrimitivePath(t);
        SdfPath volumePrimPath(rawPrimPath);
        if (volumePrimPath.IsEmpty() || !volumePrimPath.IsAbsolutePath())
        {
            SHOW_ERROR_AND_RETURN("No valid USD primitive name specified")
        }

        if (!m_CachedFileInfo.error.empty())
        {
            SHOW_ERROR_AND_RETURN(m_CachedFileInfo.error.c_str())
        }

        if (m_CachedFileInfo.uuid.empty())
        {
            SHOW_ERROR_AND_RETURN("No valid ZibraVDB file loaded")
        }

        if (currentFrameIndex < m_CachedFileInfo.frameStart || currentFrameIndex > m_CachedFileInfo.frameEnd)
        {
            addWarning(LOP_MESSAGE, ("No volume data available for frame " + std::to_string(currentFrameIndex) +
                      " (ZibraVDB range: " + std::to_string(m_CachedFileInfo.frameStart) + "-" + std::to_string(m_CachedFileInfo.frameEnd) + ")").c_str());
            return error(context);
        }

        std::set<std::string> invalidChannelNames;
        const std::set<std::string> selectedChannels = ParseSelectedChannels(channels, invalidChannelNames);
        if (selectedChannels.empty())
        {
            SHOW_ERROR_AND_RETURN("No valid channels selected")
        }
        if (!invalidChannelNames.empty())
        {
            std::string invalidNamesList;
            for (const auto& channel : invalidChannelNames)
            {
                if (!invalidNamesList.empty()) invalidNamesList += ", ";
                invalidNamesList += channel;
            }
            addWarning(LOP_MESSAGE, ("Unknown channel names specified: " + invalidNamesList).c_str());
        }

        const HUSD_AutoWriteLock writeLock(editableDataHandle());
        HUSD_AutoLayerLock layerLock(writeLock);
        const auto stage = writeLock.data()->stage();
        if (!stage)
        {
            SHOW_ERROR_AND_RETURN("Failed to get USD stage")
        }

        const std::string sanitizedName = SanitizeFieldNameForUSD(volumePrimPath.GetName());
        volumePrimPath = volumePrimPath.GetParentPath().AppendChild(TfToken(sanitizedName));
        WriteZibraVolumeToStage(stage, volumePrimPath, selectedChannels, currentFrameIndex);
        return error(context);
#undef SHOW_ERROR_AND_RETURN
    }

    bool LOP_ZibraVDBImport::updateParmsFlags()
    {
        bool changed = LOP_Node::updateParmsFlags();

        std::string currentFilePath = GetFilePath(CHgetEvalTime());
        if (m_CachedFileInfo.filePath != currentFilePath)
        {
            if (!currentFilePath.empty())
            {
                FileInfo newFileInfo = LoadFileInfo(currentFilePath);
                m_CachedFileInfo = std::move(newFileInfo);
            }
            else
            {
                m_CachedFileInfo = FileInfo{};
            }
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

    int LOP_ZibraVDBImport::GetFrameIndex(fpreal t) const
    {
        return evalInt(FRAME_INDEX_PARAM_NAME, 0, t);
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
    
    std::string LOP_ZibraVDBImport::GetChannels(fpreal t) const
    {
        UT_String channels;
        evalString(channels, CHANNELS_PARAM_NAME, 0, t);
        return channels.toStdString();
    }
    
    std::string LOP_ZibraVDBImport::SanitizeFieldNameForUSD(const std::string& fieldName)
    {
        return SdfPath::IsValidIdentifier(fieldName) ? fieldName : TfMakeValidIdentifier(fieldName);
    }

    // Parses channel selection string. Expected formats:
    // "*" - selects all available channels
    // "channel1 channel2 channel3" - space-separated channel names (no support for channels with spaces in names)
    std::set<std::string> LOP_ZibraVDBImport::ParseSelectedChannels(const std::string& channelsStr, std::set<std::string>& invalidChannelNames)
    {
        std::set<std::string> selectedChannels;
        
        if (channelsStr.empty())
        {
            return selectedChannels;
        }
        
        if (channelsStr == "*")
        {
            selectedChannels.insert(m_CachedFileInfo.availableGrids.begin(), m_CachedFileInfo.availableGrids.end());
            return selectedChannels;
        }
        
        std::istringstream iss(channelsStr);
        std::string channel;
        while (iss >> channel)
        {
            if (m_CachedFileInfo.availableGrids.find(channel) != m_CachedFileInfo.availableGrids.end())
            {
                selectedChannels.insert(channel);
            }
            else
            {
                invalidChannelNames.insert(channel);
            }
        }
        
        return selectedChannels;
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

        if (Helpers::GetExtension(URI(filePath)) != ZIB_ZIBRAVDB_EXT || !std::filesystem::exists(filePath))
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
            info.error = "Failed to register ZibraVDB file with decompressor: " + LibraryUtils::ErrorCodeToString(result);
            return info;
        }

        auto sequenceInfo = decompressor.GetSequenceInfo();
        info.uuid = Helpers::FormatUUIDString(sequenceInfo.fileUUID);
        
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

        return info;
    }

    void LOP_ZibraVDBImport::WriteZibraVolumeToStage(const UsdStageRefPtr& stage, const SdfPath& volumePrimPath,
                                                     const std::set<std::string>& selectedChannels, int frameIndex)
    {
        if (!volumePrimPath.IsRootPrimPath())
        {
            WriteParentPrimHierarchyToStage(stage, volumePrimPath);
        }

        if (volumePrimPath.IsEmpty() || !volumePrimPath.IsAbsolutePath())
        {
            const auto error = "Invalid SdfPath for volume: " + volumePrimPath.GetString();
            addError(LOP_MESSAGE, error.c_str());
            return;
        }

        const UsdVolVolume volumePrim = UsdVolVolume::Define(stage, volumePrimPath);
        if (!volumePrim)
        {
            const auto error = "Failed to create Volume prim: " + volumePrimPath.GetString();
            addError(LOP_MESSAGE, error.c_str());
            return;
        }

        for (const std::string& channelName : selectedChannels)
        {
            const std::string sanitizedChannelName = SanitizeFieldNameForUSD(channelName);
            const SdfPath vdbPrimPath = volumePrimPath.AppendChild(TfToken(sanitizedChannelName));
            WriteOpenVDBAssetPrimToStage(stage, vdbPrimPath, frameIndex);
            WriteVolumeChannelRelationshipsToStage(volumePrim, vdbPrimPath);
        }

        UT_StringArray primPaths;
        primPaths.append(volumePrim.GetPath().GetString().c_str());
        setLastModifiedPrims(primPaths);
    }

    // Creates the USD parent prim hierarchy for proper scene graph organization.
    // For "/world/volumes/sequence01", this creates "/world", "/world/volumes", etc.
    // Uses the specified parentPrimType (Xform or Scope) for all intermediate prims.
    void LOP_ZibraVDBImport::WriteParentPrimHierarchyToStage(const UsdStageRefPtr& stage, const SdfPath& primPath)
    {
        auto parentPrimType = GetParentPrimType(0);
        if (parentPrimType == "none")
        {
            return;
        }

        TfToken primType = (parentPrimType == "scope") ? TfToken("Scope") : TfToken("Xform");

        SdfPath currentPath = primPath;
        while (!currentPath.IsRootPrimPath())
        {
            currentPath = currentPath.GetParentPath();
            if (!currentPath.IsAbsoluteRootPath())
            {
                if (!stage->GetPrimAtPath(currentPath))
                {
                    stage->DefinePrim(currentPath, primType);
                }
            }
        }
        
        if (!stage->GetPrimAtPath(primPath))
        {
            stage->DefinePrim(primPath, primType);
        }
    }

    void LOP_ZibraVDBImport::WriteOpenVDBAssetPrimToStage(const UsdStageRefPtr& stage, const SdfPath& assetPath, int frameIndex)
    {
        if (!assetPath.IsAbsolutePath() || assetPath.IsEmpty())
        {
            addError(LOP_MESSAGE, "Empty SdfPath for OpenVDBAsset");
            return;
        }
        const UsdVolOpenVDBAsset openVDBAsset = UsdVolOpenVDBAsset::Define(stage, assetPath);
        if (!openVDBAsset)
        {
            addError(LOP_MESSAGE, ("Failed to create OpenVDBAsset prim for field: " + assetPath.GetName()).c_str());
            return;
        }

        const std::string zibraURL = GetFilePath(0) + "?frame=" + std::to_string(frameIndex);
        const auto timeCode = UsdTimeCode(frameIndex);

        auto filePathAttr = openVDBAsset.GetFilePathAttr();
        if (!filePathAttr)
        {
            filePathAttr = openVDBAsset.CreateFilePathAttr();
        }
        filePathAttr.Set(SdfAssetPath(zibraURL), timeCode);

        auto fieldNameAttr = openVDBAsset.GetFieldNameAttr();
        if (!fieldNameAttr)
        {
            fieldNameAttr = openVDBAsset.CreateFieldNameAttr();
        }
        fieldNameAttr.Set(TfToken(assetPath.GetName()), timeCode);
    }

    void LOP_ZibraVDBImport::WriteVolumeChannelRelationshipsToStage(const UsdVolVolume& volumePrim, const SdfPath& primPath)
    {
        const std::string relationshipName = "field:" + primPath.GetName();
        const UsdPrim prim = volumePrim.GetPrim();
        if (!prim)
        {
            return;
        }
        if (const auto fieldRel = prim.CreateRelationship(TfToken(relationshipName), true))
        {
            fieldRel.SetTargets({primPath});
        }
    }

} // namespace Zibra::ZibraVDBImport