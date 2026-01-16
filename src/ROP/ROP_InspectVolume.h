#pragma once

#ifdef ZIB_ENABLE_DEBUG_NODES

namespace Zibra::InspectVolume
{
    constexpr const char* NODE_NAME_SOP_CONTEXT = "debug::rop_inspect_volume::" ZIB_ZIBRAVDB_VERSION_SHORT;
    constexpr const char* NODE_NAME_OUT_CONTEXT = "debug::inspect_volume::" ZIB_ZIBRAVDB_VERSION_SHORT;
    constexpr const char* NODE_LABEL = "DEBUG Inspect Volume";

    class ROP_InspectVolume_Operator : public OP_Operator
    {
        using OP_Operator::OP_Operator;

    public:
        explicit ROP_InspectVolume_Operator(ContextType contextType) noexcept;

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

    class ROP_InspectVolume final : public ROP_Node
    {
    private:
        static constexpr const char* INPUT_SOP_PARAM_NAME = "soppath";
        static constexpr const char* SAMPLE_COORD_NAME = "sample_coord";

    public:
        static OP_Node* ConstructorSOPContext(OP_Network* net, const char* name, OP_Operator* op) noexcept;
        static OP_Node* ConstructorOUTContext(OP_Network* net, const char* name, OP_Operator* op) noexcept;
        static PRM_Template* GetTemplateList(ContextType contextType) noexcept;
        static OP_TemplatePair* GetTemplatePairs(ContextType contextType) noexcept;
        static OP_VariablePair* GetVariablePair(ContextType contextType) noexcept;

    public:
        ROP_InspectVolume(ContextType contextType, OP_Network* net, const char* name, OP_Operator* entry) noexcept;
        ~ROP_InspectVolume() noexcept final = default;

    public:
        int startRender(int nframes, fpreal tStart, fpreal tEnd) final;
        ROP_RENDER_CODE renderFrame(fpreal time, UT_Interrupt* boss) final;
        ROP_RENDER_CODE endRender() final;
        void getOutputFile(UT_String& filename) final;

    private:
        static std::vector<PRM_Template>& GetTemplateListContainer(ContextType contextType) noexcept;

    private:
        fpreal m_EndTime = 0;
        fpreal m_StartTime = 0;
        SOP_Node* m_InputSOP = nullptr;
        ContextType m_ContextType;
    };
} // namespace Zibra::ZibraVDBCompressor

#endif
