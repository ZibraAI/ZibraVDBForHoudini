#pragma once
#include <utils/DecompressorManager.h>
#include <LOP/LOP_Node.h>
#include <pxr/usd/usd/common.h>
#include <pxr/usd/usdVol/volume.h>

#include "Globals.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::ZibraVDBImport
{
    constexpr const char* LOP_NODE_NAME = "labs::zibravdb_import::" ZIB_ZIBRAVDB_VERSION_SHORT;
    constexpr const char* LOP_NODE_LABEL = "Labs ZibraVDB Import";

    class LOP_ZibraVDBImport final : public LOP_Node
    {
    public:
        static OP_Node* Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept;
        static PRM_Template* GetTemplateList() noexcept;

        LOP_ZibraVDBImport(OP_Network* net, const char* name, OP_Operator* entry) noexcept;
        ~LOP_ZibraVDBImport() noexcept final;

        OP_ERROR cookMyLop(OP_Context& context) final;
        bool updateParmsFlags() final;
        
    private:
        std::string GetFilePath(fpreal t) const;
        std::string GetPrimitivePath(fpreal t) const;
        std::string GetParentPrimType(fpreal t) const;
        std::string GetFields(fpreal t) const;

    private:
        std::string SanitizeFieldNameForUSD(const std::string& fieldName);
        std::vector<std::string> ParseSelectedFields(const std::string& fieldsStr, const std::set<std::string>& availableGrids);
        void ParseAvailableGrids();
        void UpdateFieldsChoiceList();
        void CreateVolumeStructure(UsdStageRefPtr stage, const std::string& primPath, const std::string& primName,
                                   const std::vector<std::string>& selectedFields, const std::string& parentPrimType, fpreal t,
                                   int frameIndex);
        void CreateOpenVDBAssetPrim(UsdStageRefPtr stage, const std::string& volumePath, const std::string& fieldName,
                                    const std::string& sanitizedFieldName, const std::string& filePath, int frameIndex);
        void CreateFieldRelationship(UsdVolVolume& volumePrim, const std::string& fieldName, const std::string& assetPath);
        std::string GenerateZibraVDBURL(const std::string& filePath, const std::string& fieldName, int frameNumber) const;

        static int OpenManagementWindow(void* data, int index, fpreal32 time, const PRM_Template* tplate);

    private:
        Helpers::DecompressorManager m_DecompressorManager;
        std::string m_LastFilePath;
        std::set<std::string> m_AvailableGrids;
    };

    class LOP_ZibraVDBImport_Operator final : public OP_Operator
    {
        using OP_Operator::OP_Operator;

    public:
        explicit LOP_ZibraVDBImport_Operator() noexcept
            : OP_Operator(LOP_NODE_NAME, LOP_NODE_LABEL, LOP_ZibraVDBImport::Constructor,
                          LOP_ZibraVDBImport::GetTemplateList(), 0, 1, 0, OP_FLAG_GENERATOR, 0, 1)
        {
            setIconName(ZIBRAVDB_ICON_PATH);
            setOpTabSubMenuPath(ZIBRAVDB_NODES_TAB_NAME);
        }
    };

} // namespace Zibra::ZibraVDBImport