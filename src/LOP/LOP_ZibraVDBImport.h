#pragma once

#include <LOP/LOP_Node.h>
#include <pxr/usd/usd/common.h>
#include <pxr/usd/usdVol/volume.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::ZibraVDBImport
{
    constexpr const char* LOP_NODE_NAME = "labs::zibravdb_import::" ZIB_ZIBRAVDB_VERSION_SHORT;
    constexpr const char* LOP_NODE_LABEL = "Labs ZibraVDB Import";

    class LOP_ZibraVDBImport final : public LOP_Node
    {
    private:
        struct FileInfo
        {
            std::string filePath;
            std::string uuid;
            std::set<std::string> availableGrids;
            int frameStart = 0;
            int frameEnd = 0;
            std::string error;
        };

    private:
        static constexpr const char* FILE_PARAM_NAME = "file";
        static constexpr const char* FRAME_INDEX_PARAM_NAME = "frame";
        static constexpr const char* PRIMPATH_PARAM_NAME = "primpath";
        static constexpr const char* PARENTPRIMTYPE_PARAM_NAME = "parentprimtype";
        static constexpr const char* CHANNELS_PARAM_NAME = "channels";
        static constexpr const char* OPEN_PLUGIN_MANAGEMENT_PARAM_NAME = "openmanagement";

    public:
        static OP_Node* Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept;
        static PRM_Template* GetTemplateList() noexcept;

        LOP_ZibraVDBImport(OP_Network* net, const char* name, OP_Operator* entry) noexcept;

        OP_ERROR cookMyLop(OP_Context& context) final;
        bool updateParmsFlags() final;

    private:
        std::string GetFilePath(fpreal t) const;
        int GetFrameIndex(fpreal t) const;
        std::string GetPrimitivePath(fpreal t) const;
        std::string GetParentPrimType(fpreal t) const;
        std::string GetChannels(fpreal t) const;

        inline std::string SanitizeFieldNameForUSD(const std::string& fieldName);
        std::set<std::string> ParseSelectedChannels(const std::string& fieldsStr, std::set<std::string>& invalidGridNames);
        FileInfo LoadFileInfo(const std::string& filePath);

        void WriteZibraVolumeToStage(const UsdStageRefPtr& stage, const SdfPath& volumePrimPath,
                                     const std::set<std::string>& selectedChannels, int frameIndex);
        void WriteParentPrimHierarchyToStage(const UsdStageRefPtr& stage, const SdfPath& primPath);
        void WriteOpenVDBAssetPrimToStage(const UsdStageRefPtr& stage, const SdfPath& assetPath, int frameIndex);
        void WriteVolumeChannelRelationshipsToStage(const UsdVolVolume& volumePrim, const SdfPath& primPath);

        static int OpenManagementWindow(void* data, int index, fpreal32 time, const PRM_Template* tplate);
        static void BuildChannelsChoiceList(void* data, PRM_Name* choiceNames, int maxListSize, const PRM_SpareData*, const PRM_Parm*);

    private:
        FileInfo m_CachedFileInfo;
    };

    class LOP_ZibraVDBImport_Operator final : public OP_Operator
    {
        using OP_Operator::OP_Operator;

    public:
        explicit LOP_ZibraVDBImport_Operator() noexcept
            : OP_Operator(LOP_NODE_NAME, LOP_NODE_LABEL, LOP_ZibraVDBImport::Constructor, LOP_ZibraVDBImport::GetTemplateList(), 0, 1, 0,
                          OP_FLAG_GENERATOR, 0, 1)
        {
            setIconName(ZIBRAVDB_ICON_PATH);
            setOpTabSubMenuPath(ZIBRAVDB_NODES_TAB_NAME);
        }
    };

} // namespace Zibra::ZibraVDBImport