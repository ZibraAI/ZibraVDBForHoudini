#pragma once
#include <LOP/LOP_Node.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usd/common.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdVol/volume.h>

#include "Globals.h"
#include "SOP/DecompressorManager/DecompressorManager.h"
#include "utils/VDBCacheManager.h"

namespace Zibra::ZibraVDBDecompressor
{
    using namespace CE::Decompression;
    PXR_NAMESPACE_USING_DIRECTIVE

    constexpr const char* LOP_NODE_NAME = "labs::zibravdb_decompress_lop::" ZIB_ZIBRAVDB_VERSION_SHORT;
    constexpr const char* LOP_NODE_LABEL = "Labs ZibraVDB Decompress LOP";

    class LOP_ZibraVDBDecompressor final : public LOP_Node
    {
    private:
        static constexpr const char* FILENAME_PARAM_NAME = "filename";
        static constexpr const char* FRAME_PARAM_NAME = "frame";
        static constexpr const char* REFRESH_CALLBACK_PARAM_NAME = "reload";
        static constexpr const char* OPEN_PLUGIN_MANAGEMENT_BUTTON_NAME = "openmanagement";

    public:
        static OP_Node* Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept;
        static PRM_Template* GetTemplateList() noexcept;

    public:
        LOP_ZibraVDBDecompressor(OP_Network* net, const char* name, OP_Operator* entry) noexcept;
        ~LOP_ZibraVDBDecompressor() noexcept final;

    public:
        OP_ERROR cookMyLop(OP_Context& context) final;

    private:
        static int OpenManagementWindow(void* data, int index, fpreal32 time, const PRM_Template* tplate);
        
        void CreateZibraVDBVolumePrimitive(PXR_NS::UsdStageRefPtr stage, const PXR_NS::SdfPath& primPath, 
                                          const std::string& binaryFilePath, int frameIndex);
        void InjectOpenVDBVolume(PXR_NS::UsdStageRefPtr stage, const PXR_NS::SdfAssetPath& filePath, const openvdb::GridPtrVec& vdbGrids);
        void ApplyVolumeVisualizationMetadata(PXR_NS::UsdStageRefPtr stage, const openvdb::GridPtrVec& vdbGrids, CompressedFrameContainer* const frameContainer);
        void ApplyGridVisualizationMetadata(PXR_NS::UsdVolVolume& volume, const std::string& gridName, CompressedFrameContainer* const frameContainer);

    private:
        Helpers::DecompressorManager m_DecompressorManager;
    };

    class LOP_ZibraVDBDecompressor_Operator final : public OP_Operator
    {
        using OP_Operator::OP_Operator;

    public:
        explicit LOP_ZibraVDBDecompressor_Operator() noexcept
            : OP_Operator{
                  LOP_NODE_NAME, LOP_NODE_LABEL, LOP_ZibraVDBDecompressor::Constructor, 
                  LOP_ZibraVDBDecompressor::GetTemplateList(), 0, 0,
                  nullptr, 0}
        {
            setIconName(ZIBRAVDB_ICON_PATH);
            setOpTabSubMenuPath(ZIBRAVDB_NODES_TAB_NAME);
        }

        const UT_StringHolder& getDefaultShape() const final
        {
            static UT_StringHolder shape{"tabbed_left"};
            return shape;
        }

        UT_Color getDefaultColor() const final
        {
            return UT_Color{UT_RGB, 0.75, 0.6, 0.85};
        }
    };
} // namespace Zibra::ZibraVDBDecompressor