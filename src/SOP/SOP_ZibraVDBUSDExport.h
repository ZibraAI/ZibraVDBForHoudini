#pragma once

namespace Zibra::ZibraVDBUSDExport
{
    constexpr const char* SOP_NODE_NAME = "labs::zibravdb_configure_usd::" ZIB_ZIBRAVDB_VERSION_SHORT;
    constexpr const char* SOP_NODE_LABEL = "Labs ZibraVDB Configure USD";

    class SOP_ZibraVDBUSDExport final : public SOP_Node
    {
    private:
        static constexpr const char* QUALITY_PARAM_NAME = "quality";
        static constexpr const char* USE_PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME = "useperchsettings";
        static constexpr const char* PER_CHANNEL_COMPRESSION_SETTINGS_PARAM_NAME = "perch_settings";
        static constexpr const char* PER_CHANNEL_COMPRESSION_SETTINGS_CHANNEL_NAME_PARAM_NAME = "perchname";
        static constexpr const char* PER_CHANNEL_COMPRESSION_SETTINGS_QUALITY_PARAM_NAME = "perchquality";
        static constexpr const char* FILENAME_PARAM_NAME = "filename";
        static constexpr const char* OPEN_PLUGIN_MANAGEMENT_BUTTON_NAME = "openmanagement";

    public:
        static OP_Node* Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept;
        static PRM_Template* GetTemplateList() noexcept;

        SOP_ZibraVDBUSDExport(OP_Network* net, const char* name, OP_Operator* entry) noexcept;
        ~SOP_ZibraVDBUSDExport() noexcept final = default;

        OP_ERROR cookMySop(OP_Context& context) final;

        bool UsePerChannelCompressionSettings() const noexcept;
        std::vector<std::pair<UT_String, float>> GetPerChannelCompressionSettings() const noexcept;
        float GetCompressionQuality() const noexcept;

    private:

        std::unordered_set<std::string> m_AvailableChannels{};
    };

    class SOP_ZibraVDBUSDExport_Operator final : public OP_Operator
    {
        using OP_Operator::OP_Operator;

    public:
        explicit SOP_ZibraVDBUSDExport_Operator() noexcept
            : OP_Operator(SOP_NODE_NAME, SOP_NODE_LABEL, SOP_ZibraVDBUSDExport::Constructor,
                          SOP_ZibraVDBUSDExport::GetTemplateList(), 1, 1, 0, OP_FLAG_GENERATOR, 0, 1)
        {
            setIconName(ZIBRAVDB_ICON_PATH);
            setOpTabSubMenuPath(ZIBRAVDB_NODES_TAB_NAME);
        }
    };

} // namespace Zibra::ZibraVDBUSDExport