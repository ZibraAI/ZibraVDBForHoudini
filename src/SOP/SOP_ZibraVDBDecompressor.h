#pragma once
#include "Globals.h"
#include "openvdb/OpenVDBEncoder.h"

namespace Zibra::ZibraVDBDecompressor
{
    constexpr const char* NODE_NAME = "labs::zibravdb_decompress::" ZIB_ZIBRAVDB_VERSION_SHORT;
    constexpr const char* NODE_LABEL = "Labs ZibraVDB Decompress (Alpha)";

    class SOP_ZibraVDBDecompressor final : public SOP_Node
    {
    private:
        static constexpr const char* FILENAME_PARAM_NAME = "filename";
        static constexpr const char* FRAME_PARAM_NAME = "frame";
        static constexpr const char* REFRESH_CALLBACK_PARAM_NAME = "reload";
        static constexpr const char* DOWNLOAD_LIBRARY_BUTTON_NAME = "downloadlibrary";
        static constexpr const char* CORE_LIB_PATH_FIELD_NAME = "corelibpath";

    public:
        static OP_Node* Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept;
        static PRM_Template* GetTemplateList() noexcept;

    public:
        SOP_ZibraVDBDecompressor(OP_Network* net, const char* name, OP_Operator* entry) noexcept;
        ~SOP_ZibraVDBDecompressor() noexcept final;

    public:
        OP_ERROR cookMySop(OP_Context& context) final;

    private:
        static int DownloadLibrary(void* data, int index, fpreal32 time, const PRM_Template* tplate);

        void ApplyGridMetadata(GU_PrimVDB* vdbPrim, CompressionEngine::ZCE_MetadataEntry* metadataBegin,
                               CompressionEngine::ZCE_MetadataEntry* metadataEnd);
        void ApplyGridAttributeMetadata(GU_PrimVDB* vdbPrim, CompressionEngine::ZCE_MetadataEntry* metadataBegin,
                                        CompressionEngine::ZCE_MetadataEntry* metadataEnd);
        void ApplyGridVisualizationMetadata(GU_PrimVDB* vdbPrim, CompressionEngine::ZCE_MetadataEntry* metadataBegin,
                                            CompressionEngine::ZCE_MetadataEntry* metadataEnd);
        void ApplyDetailMetadata(GU_Detail* gdp, CompressionEngine::ZCE_MetadataEntry* metadataBegin,
                                 CompressionEngine::ZCE_MetadataEntry* metadataEnd);
        OpenVDBSupport::EncodeMetadata ReadEncodeMetadata(const CompressionEngine::ZCE_MetadataEntry* metadata, uint32_t metadataCount);

    private:
        CE::Decompression::DecompressorFactory* m_Factory = nullptr;
        CE::ZibraVDB::FileDecoder* m_Decoder = nullptr;
        CE::Decompression::Decompressor* m_Decompressor = nullptr;
    };

    class SOP_ZibraVDBDecompressor_Operator final : public OP_Operator
    {
        using OP_Operator::OP_Operator;

    public:
        explicit SOP_ZibraVDBDecompressor_Operator() noexcept
            : OP_Operator{
                  NODE_NAME, NODE_LABEL,       SOP_ZibraVDBDecompressor::Constructor, SOP_ZibraVDBDecompressor::GetTemplateList(), 0, 0,
                  nullptr,   OP_FLAG_GENERATOR}
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
            return UT_Color{UT_RGB, 0.9, 0.8, 0.55};
        }
    };
} // namespace Zibra::ZibraVDBDecompressor