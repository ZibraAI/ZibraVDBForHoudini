#pragma once

namespace Zibra::ZibraVDBDecompressor
{
    constexpr const char* NODE_NAME = "labs::zibravdb_decompress::0.1";
    constexpr const char* NODE_LABEL = "Labs ZibraVDB Decompress (Alpha)";

    class SOP_ZibraVDBDecompressor : public SOP_Node
    {
    private:
        static constexpr const char* FILENAME_PARAM_NAME = "filename";
        static constexpr const char* FRAME_PARAM_NAME = "frame";
        static constexpr const char* REFRESH_CALLBACK_PARAM_NAME = "refresh";
        static constexpr const char* DOWNLOAD_LIBRARY_BUTTON_NAME = "download_library";
        static constexpr const char* LIBRARY_VERSION_NAME = "library_version";

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
        void UpdateCompressionLibraryVersion();

        uint32_t m_DecompressorInstanceID = uint32_t(-1);
    };

    class SOP_ZibraVDBDecompressor_Operator : public OP_Operator
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
            return UT_Color{UT_RGB, 0.1, 0.1, 0.1};
        }
    };
} // namespace Zibra::ZibraVDBDecompressor