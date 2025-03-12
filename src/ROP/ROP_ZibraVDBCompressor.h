#pragma once
#include "Globals.h"
#include "ROP/CompressorManager/CompressorManager.h"

namespace Zibra::OpenVDBSupport
{
    struct DecodeMetadata;
}

namespace Zibra::ZibraVDBCompressor
{
    constexpr const char* NODE_NAME_SOP_CONTEXT = "labs::rop_zibravdb_compress::" ZIB_ZIBRAVDB_VERSION_SHORT;
    constexpr const char* NODE_NAME_OUT_CONTEXT = "labs::zibravdb_compress::" ZIB_ZIBRAVDB_VERSION_SHORT;
    constexpr const char* NODE_LABEL = "Labs ZibraVDB Compress (Alpha)";

    class ROP_ZibraVDBCompressor_Operator : public OP_Operator
    {
        using OP_Operator::OP_Operator;

    public:
        explicit ROP_ZibraVDBCompressor_Operator(ContextType contextType) noexcept;

        const UT_StringHolder& getDefaultShape() const final;
        UT_Color getDefaultColor() const final;

    private:
        static OP_Constructor GetConstructorForContext(ContextType contextType);
        static unsigned GetOperatorFlags(ContextType contextType);
        static unsigned GetMinSources(ContextType contextType);
        static unsigned GetMaxSources(ContextType contextType);
        static unsigned GetMaxOutputs(ContextType contextType);
        const char** GetSourceLabels(ContextType contextType);
        const char* GetNodeName(ContextType contextType);
        const char* GetNodeLabel(ContextType contextType);

        ContextType m_ContextType;
    };

    class ROP_ZibraVDBCompressor final : public ROP_Node
    {
    private:
        static constexpr const char* INPUT_SOP_PARAM_NAME = "soppath";
        static constexpr const char* QUALITY_PARAM_NAME = "quality";
        static constexpr const char* USE_PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME = "useperchsettings";
        static constexpr const char* PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME = "perch_settings";
        static constexpr const char* PER_CHANNEL_COMPRESSION_SETTINGS_CHANNEL_NAME_PARAM_NAME = "perchname";
        static constexpr const char* PER_CHANNEL_COMPRESSION_SETTINGS_QUALITY_PARAM_NAME = "perchquality";
        static constexpr const char* FILENAME_PARAM_NAME = "filename";
        static constexpr const char* OPEN_PLUGIN_MANAGEMENT_BUTTON_NAME = "openmanagement";
        static constexpr const char* CORE_LIB_PATH_FIELD_NAME = "corelibpath";

    public:
        static OP_Node* ConstructorSOPContext(OP_Network* net, const char* name, OP_Operator* op) noexcept;
        static OP_Node* ConstructorOUTContext(OP_Network* net, const char* name, OP_Operator* op) noexcept;
        static PRM_Template* GetTemplateList(ContextType contextType) noexcept;
        static OP_TemplatePair* GetTemplatePairs(ContextType contextType) noexcept;
        static OP_VariablePair* GetVariablePair(ContextType contextType) noexcept;

    public:
        ROP_ZibraVDBCompressor(ContextType contextType, OP_Network* net, const char* name, OP_Operator* entry) noexcept;
        ~ROP_ZibraVDBCompressor() noexcept final;

    public:
        int startRender(int nframes, fpreal tStart, fpreal tEnd) final;
        ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss) final;
        ROP_RENDER_CODE endRender() final;
        void getOutputFile(UT_String& filename) final;

    private:
        static std::vector<PRM_Template>& GetTemplateListContainer(ContextType contextType);

        std::vector<std::pair<std::string, std::string>> DumpAttributes(const GU_Detail* gdp,
                                                                        const OpenVDBSupport::DecodeMetadata& decodeMetadata) noexcept;
        void DumpVisualisationAttributes(std::vector<std::pair<std::string, std::string>>& attributes, const GEO_PrimVDB* vdbPrim);
        void DumpDecodeMetadata(std::vector<std::pair<std::string, std::string>>& result,
                                const OpenVDBSupport::DecodeMetadata& decodeMetadata);

        static int OpenManagementWindow(void* data, int index, fpreal32 time, const PRM_Template* tplate);

        ROP_RENDER_CODE CreateCompressor(fpreal tStart);

    private:
        fpreal m_EndTime = 0;
        fpreal m_StartTime = 0;
        SOP_Node* m_InputSOP = nullptr;

        std::vector<std::string> m_OrderedChannelNames{};

        ContextType m_ContextType;

        std::ofstream m_Ofstream;
        CE::Compression::CompressorManager m_CompressorManager;
    };
} // namespace Zibra::ZibraVDBCompressor