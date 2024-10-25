#pragma once

namespace Zibra::ZibraVDBCompressor
{
    constexpr const char* NODE_NAME = "labs::zibravdb_compress::0.1";
    constexpr const char* NODE_LABEL = "Labs ZibraVDB Compress (Alpha)";

    class ROP_ZibraVDBCompressor final: public ROP_Node
    {
    private:
        static constexpr const char* QUALITY_PARAM_NAME = "quality";
        static constexpr const char* USE_PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME = "usePerChannelCompressionSettings";
        static constexpr const char* PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME = "perChannelCompressionSettings";
        static constexpr const char* PER_CHANNEL_COMPRESSION_SETTINGS_CHANNEL_NAME_PARAM_NAME =
            "perChannelCompressionSettings__ChannelName";
        static constexpr const char* PER_CHANNEL_COMPRESSION_SETTINGS_QUALITY_PARAM_NAME = "perChannelCompressionSettings__Quality";
        static constexpr const char* FILENAME_PARAM_NAME = "filename";
        static constexpr const char* DOWNLOAD_LIBRARY_BUTTON_NAME = "download_library";

    public:
        static OP_Node* Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept;
        static PRM_Template* GetTemplateList() noexcept;
        static OP_TemplatePair* GetTemplatePairs() noexcept;
        static OP_VariablePair* GetVariablePair() noexcept;

    public:
        ROP_ZibraVDBCompressor(OP_Network* net, const char* name, OP_Operator* entry) noexcept;
        ~ROP_ZibraVDBCompressor() noexcept final;

    public:
        int startRender(int nframes, fpreal tStart, fpreal tEnd) final;
        ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss) final;
        ROP_RENDER_CODE endRender() final;
        void getOutputFile(UT_String& filename) final;

    private:
        std::vector<std::pair<std::string, std::string>> DumpAttributes(const GU_Detail* gdp) noexcept;
        static int DownloadLibrary(void* data, int index, fpreal32 time, const PRM_Template* tplate);
        uint32_t CreateCompressor(const UT_String& filename, fpreal tStart);

    private:
        fpreal m_EndTime = 0;
        fpreal m_StartTime = 0;
        SOP_Node* m_InputSOP = nullptr;

        uint32_t CompressorInstanceID = uint32_t(-1);
        std::vector<std::string> m_OrderedChannelNames{};
    };

    class ROP_ZibraVDBCompressor_Operator : public OP_Operator
    {
        using OP_Operator::OP_Operator;

    public:
        explicit ROP_ZibraVDBCompressor_Operator() noexcept
            : OP_Operator{NODE_NAME, NODE_LABEL, ROP_ZibraVDBCompressor::Constructor,       ROP_ZibraVDBCompressor::GetTemplatePairs(),
                          1,         1,          ROP_ZibraVDBCompressor::GetVariablePair(), OP_FLAG_GENERATOR,
                          nullptr,   0}
        {
            setIconName(ZIBRAVDB_ICON_PATH);
            setOpTabSubMenuPath(ZIBRAVDB_NODES_TAB_NAME);
        }

        const UT_StringHolder& getDefaultShape() const final
        {
            static UT_StringHolder shape{"trapezoid_down"};
            return shape;
        }

        UT_Color getDefaultColor() const final
        {
            return UT_Color{UT_RGB, 0.1, 0.1, 0.1};
        }
    };
} // namespace Zibra::ZibraVDBCompressor