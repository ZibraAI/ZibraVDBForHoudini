#pragma once

namespace Zibra::Analytics
{
    using json = nlohmann::json;

    class AnalyticsPromptWindow;

    struct CompressionPerChannelSettings
    {
        std::string channelName;
        float quality;
    };

    struct CompressionEventData
    {
        uint32_t frameCount;
        std::vector<std::string> channels;
        std::array<uint32_t, 3> resolution;
        float quality;
        bool usingPerChannelCompressionSettings;
        std::vector<CompressionPerChannelSettings> perChannelCompressionSettings;
        float compressionTime;
    };

    class AnalyticsManager
    {
    public:
        bool IsNeedShowPrompt();
        bool IsAnalyticsEnabled();

        bool IsAnalyticsAllowedByLicense();
        bool IsLicenseKeyAvailable();

        void ShowAnalyticsPrompt();

        static AnalyticsManager& GetInstance();

        // Event methods check whether analytics are enabled internally
        void SendEventUsage();
        void SendEventCompression(const CompressionEventData& eventData);

    private:
        static const char* const ms_SettingsFileName;
        static const int ms_FreeLicenseTier;

        AnalyticsManager();

        void SaveSettings(bool isEnabled);
        void LoadSettings();
        bool IsSettingsPresent();

        json ConstructCommonData();
        json ConstructEventData(std::string_view eventName, const json& eventData);

        void ScheduleEvent(std::string_view eventName, json&& eventData);
        void SendEvent(std::string_view eventName, const json& eventData);

        std::string GetHoudiniVersionString();

        void WriteSettingsFile(std::string_view dataToWrite);
        std::pair<bool, std::string> ReadSettingsFile();
        std::filesystem::path GetSettingsFilePath();

        void SendThreadEntrypoint();

        bool m_IsSettingsPresent = false;
        bool m_IsAnalyticsEnabledInSettings = false;

        std::chrono::system_clock::time_point m_LastUsageEventSendTime;

        std::queue<std::pair<std::string, json>> m_EventQueue;
        std::mutex m_QueueMutex;
        std::condition_variable m_QueueSignal;
        std::thread m_SendThread;
        bool m_ThreadStarted = false;

        friend AnalyticsPromptWindow;
    };
} // namespace Zibra::Analytics
