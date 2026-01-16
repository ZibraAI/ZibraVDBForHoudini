#include "PrecompiledHeader.h"

#include "Analytics.h"

#include "licensing/HoudiniLicenseManager.h"
#include "utils/Helpers.h"
#include "bridge/networking/NetworkRequest.h"
#include "OSInfo.h"

namespace Zibra::Analytics
{
    const char* const AnalyticsManager::ms_SettingsFileName = "zibravdb_analytics_settings.txt";
    const int AnalyticsManager::ms_FreeLicenseTier = 200;

    template <typename T>
    static json VectorToJson(const std::vector<T>& in)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(VectorToJson<T>)>);

        json result = json::array();

        for (const T& value : in)
        {
            result.emplace_back(value);
        }

        return result;
    }

    static json CompressionPerChannelSettingsToJson(const std::vector<CompressionPerChannelSettings>& in)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(CompressionPerChannelSettingsToJson)>);

        json result = json::object();

        for (const auto& setting : in)
        {
            result[setting.channelName] = json::object({{"Quality", setting.quality}});
        }

        return result;
    }

    class AnalyticsPromptWindow : public AP_Interface
    {
    public:
        const char* className() const final
        {
            static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsPromptWindow::className)>);

            return "ZibraVDBAnalyticsPromptWindow";
        }

        void Show();

    private:
        static constexpr const char* UI_FILE = "ZibraVDBAnalyticsPromptWindow.ui";
        static constexpr int32 ANALYTICS_ENABLED_VALUE = 0;
        static constexpr int32 ANALYTICS_DISABLED_VALUE = 1;

        static bool m_IsParsed;

        bool ParseUIFile();
        void InitializeValues();
        void HandleOK(UI_Event* event);
    };

    bool AnalyticsPromptWindow::m_IsParsed = false;

    void AnalyticsPromptWindow::Show()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsPromptWindow::Show)>);

        if (!ParseUIFile())
        {
            return;
        }

        InitializeValues();

        // Open window
        auto* windowSymbol = getValueSymbol("window.val");
        *windowSymbol = true;
        windowSymbol->changed(this);
    }

    bool AnalyticsPromptWindow::ParseUIFile()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsPromptWindow::ParseUIFile)>);

        if (m_IsParsed)
        {
            return true;
        }

        if (!readUIFile(UI_FILE))
        {
            return false;
        }

        getValueSymbol("ok_button.val")->addInterest(this, static_cast<UI_EventMethod>(&AnalyticsPromptWindow::HandleOK));

        m_IsParsed = true;

        return true;
    }

    void AnalyticsPromptWindow::InitializeValues()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsPromptWindow::InitializeValues)>);

        if (!AnalyticsManager::GetInstance().IsSettingsPresent())
        {
            return;
        }

        auto* symbol = getValueSymbol("analytics_enabled.val");
        *symbol = AnalyticsManager::GetInstance().IsAnalyticsEnabled() ? ANALYTICS_ENABLED_VALUE : ANALYTICS_DISABLED_VALUE;
        symbol->changed(this);
    }

    void AnalyticsPromptWindow::HandleOK(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsPromptWindow::HandleOK)>);

        auto* analyticsSymbol = getValueSymbol("analytics_enabled.val");
        int32 uiValue = 0;
        analyticsSymbol->getValue(&uiValue);
        bool isAnalyticsEnabled = uiValue == ANALYTICS_ENABLED_VALUE;

        AnalyticsManager::GetInstance().SaveSettings(isAnalyticsEnabled);

        // Close window
        auto* windowSymbol = getValueSymbol("window.val");
        *windowSymbol = false;
        windowSymbol->changed(this);
    }

    bool AnalyticsManager::IsNeedShowPrompt()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::IsNeedShowPrompt)>);

        return !m_IsSettingsPresent && IsLicenseKeyAvailable() && IsAnalyticsAllowedByLicense();
    }

    bool AnalyticsManager::IsAnalyticsEnabled()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::IsAnalyticsEnabled)>);

        return m_IsAnalyticsEnabledInSettings && IsLicenseKeyAvailable() && IsAnalyticsAllowedByLicense();
    }

    bool AnalyticsManager::IsAnalyticsAllowedByLicense()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::IsAnalyticsAllowedByLicense)>);

        // We only allow sending analytics if license is activated and license is Free one
        // If license is not activated, GetLicenseTier returns -1
        return HoudiniLicenseManager::GetInstance().GetLicenseTier() == ms_FreeLicenseTier;
    }

    bool AnalyticsManager::IsLicenseKeyAvailable()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::IsLicenseKeyAvailable)>);

        // We only send analytics if there's license key we can use for said analytics
        // Depending on activation method there may be no key available to query
        return !HoudiniLicenseManager::GetInstance().GetLicenseKey().empty();
    }

    void AnalyticsManager::ShowAnalyticsPrompt()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::ShowAnalyticsPrompt)>);

        static AnalyticsPromptWindow window;
        window.Show();
    }

    AnalyticsManager& AnalyticsManager::GetInstance()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::GetInstance)>);

        static AnalyticsManager instance;
        return instance;
    }

    void AnalyticsManager::SendEventUsage()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::SendEventUsage)>);

        if (!IsAnalyticsEnabled())
        {
            return;
        }

        // Don't send more often than once each 30 minutes
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        std::chrono::minutes duration = std::chrono::duration_cast<std::chrono::minutes>(now - m_LastUsageEventSendTime);
        if (duration.count() < 30)
        {
            return;
        }

        json encodedData;

        // m_LastUsageEventSendTime is default constructed to 0 time stamp
        encodedData["Startup"] = (m_LastUsageEventSendTime.time_since_epoch().count() == 0);

        m_LastUsageEventSendTime = now;

        ScheduleEvent("Usage", std::move(encodedData));
    }

    void AnalyticsManager::SendEventCompression(const CompressionEventData& eventData)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::SendEventCompression)>);

        if (!IsAnalyticsEnabled())
        {
            return;
        }

        json encodedData;

        encodedData["FrameNumber"] = eventData.frameCount;
        encodedData["ChannelNumber"] = eventData.channels.size();
        encodedData["AvailableChannels"] = VectorToJson(eventData.channels);
        encodedData["VDBResolution"] = json::array({eventData.resolution[0], eventData.resolution[1], eventData.resolution[2]});
        encodedData["OriginalVDBSize"] = 0;
        encodedData["CompressionRate"] = 0.0f;
        encodedData["Quality"] = eventData.quality;
        encodedData["UsePerChannelCompressionSettings"] = eventData.usingPerChannelCompressionSettings;
        encodedData["PerChannelCompressionSettings"] = CompressionPerChannelSettingsToJson(eventData.perChannelCompressionSettings);
        encodedData["CompressionTime"] = eventData.compressionTime;
        encodedData["UEAssetID"] = "";
        encodedData["AssetUUID"] = "";
        encodedData["Reimport"] = false;

        ScheduleEvent("Compression", std::move(encodedData));
    };

    AnalyticsManager::AnalyticsManager()
    {
        LoadSettings();
    }

    void AnalyticsManager::SaveSettings(bool isEnabled)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::SaveSettings)>);

        WriteSettingsFile(isEnabled ? "1" : "0");
        m_IsSettingsPresent = true;
        m_IsAnalyticsEnabledInSettings = isEnabled;
    }

    void AnalyticsManager::LoadSettings()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::LoadSettings)>);

        auto [readSuccessfully, fileData] = ReadSettingsFile();

        m_IsSettingsPresent = readSuccessfully;
        if (!readSuccessfully)
        {
            m_IsAnalyticsEnabledInSettings = false;
        }

        // Analytics enabled if and only if settings file is literally string "1"
        m_IsAnalyticsEnabledInSettings = fileData == "1";
    }

    bool AnalyticsManager::IsSettingsPresent()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::IsSettingsPresent)>);

        return m_IsSettingsPresent;
    }

    json AnalyticsManager::ConstructCommonData()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::ConstructCommonData)>);

        json result;

        result["Product"] = "zibravdb";
        result["HardwareId"] = HoudiniLicenseManager::GetInstance().GetHardwareID();
        result["LicenseKey"] = HoudiniLicenseManager::GetInstance().GetLicenseKey();

        const auto osInfo = GetOSInfo();

        result["OS"] = osInfo.Name;
        result["OSVersion"] = osInfo.Version;
        result["GPU"] = "";
        result["CurrentRHI"] = "";
        result["VRAMMB"] = "";
        result["PluginVersion"] = ZIBRAVDB_VERSION;
        result["Engine"] = "houdini";
        result["EngineVersion"] = GetHoudiniVersionString();

        return result;
    }

    json AnalyticsManager::ConstructEventData(std::string_view eventName, const json& eventData)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::ConstructEventData)>);

        json result = json::array();
        json& event = result.emplace_back(json::object());
        event["EventType"] = eventName;
        event["Data"] = eventData;

        return result;
    }

    void AnalyticsManager::ScheduleEvent(std::string_view eventName, json&& eventData)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::ScheduleEvent)>);

        {
            std::lock_guard<std::mutex> lockGuard(m_QueueMutex);
            m_EventQueue.emplace(eventName, std::move(eventData));
        }

        m_QueueSignal.notify_one();

        if (!m_ThreadStarted)
        {
            m_SendThread = std::thread(&AnalyticsManager::SendThreadEntrypoint, this);
            m_ThreadStarted = true;
        }
    }

    void AnalyticsManager::SendEvent(std::string_view eventName, const json& fields)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::SendEvent)>);

        json requestBody;

        requestBody["CommonData"] = ConstructCommonData();
        requestBody["Events"] = ConstructEventData(eventName, fields);
        requestBody["APIVersion"] = "2024.07.05";

        std::string requestBodyString = requestBody.dump();

        NetworkRequest::Request request;
        request.method = NetworkRequest::Method::POST;
        request.URL = "https://analytics.zibra.ai/api/usageAnalytics";
        request.userAgent = std::string("ZibraVDBForHoudini/") + ZIBRAVDB_VERSION;
        request.acceptTypes = "application/json";
        request.contentType = "application/json";
        request.data = std::vector<char>(requestBodyString.begin(), requestBodyString.end());

        NetworkRequest::Response response = NetworkRequest::Perform(request);
    }

    std::string AnalyticsManager::GetHoudiniVersionString()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::GetHoudiniVersionString)>);

        constexpr const char* COMPILE_TIME_VERSION = SYS_VERSION_MAJOR "." SYS_VERSION_MINOR;

        int major = 0;
        int minor = 0;
        int build = 0;

        HAPI_Result result = HAPI_GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_MAJOR, &major);
        if (result != HAPI_RESULT_SUCCESS)
        {
            return COMPILE_TIME_VERSION;
        }

        result = HAPI_GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_MINOR, &minor);
        if (result != HAPI_RESULT_SUCCESS)
        {
            return COMPILE_TIME_VERSION;
        }

        result = HAPI_GetEnvInt(HAPI_ENVINT_VERSION_HOUDINI_BUILD, &build);
        if (result != HAPI_RESULT_SUCCESS)
        {
            return COMPILE_TIME_VERSION;
        }

        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(build);
    }

    void AnalyticsManager::WriteSettingsFile(std::string_view dataToWrite)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::WriteSettingsFile)>);

        std::filesystem::path targetFile = GetSettingsFilePath();
        if (targetFile.empty())
        {
            return;
        }

        std::ofstream outStream(targetFile);
        if (outStream.is_open())
        {
            outStream << dataToWrite;
        }
    }

    std::pair<bool, std::string> AnalyticsManager::ReadSettingsFile()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::ReadSettingsFile)>);

        std::filesystem::path targetFile = GetSettingsFilePath();
        if (targetFile.empty())
        {
            return {false, {}};
        }

        std::ifstream inStream(targetFile);
        if (!inStream.is_open())
        {
            return {false, {}};
        }

        std::stringstream buffer;
        buffer << inStream.rdbuf();
        return {true, buffer.str()};
    }

    std::filesystem::path AnalyticsManager::GetSettingsFilePath()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::GetSettingsFilePath)>);

        std::vector<std::string> userPrefDir = Helpers::GetHoudiniEnvironmentVariable(ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR");

        if (userPrefDir.empty())
        {
            return {};
        }

        return std::filesystem::path(userPrefDir[0]) / ms_SettingsFileName;
    }

    void AnalyticsManager::SendThreadEntrypoint()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&AnalyticsManager::SendThreadEntrypoint)>);

        while (true)
        {
            std::pair<std::string, json> event;
            {
                std::unique_lock<std::mutex> uniqueLock(m_QueueMutex);
                while (true)
                {
                    if (!m_EventQueue.empty())
                    {
                        event = std::move(m_EventQueue.front());
                        m_EventQueue.pop();
                        break;
                    }
                    m_QueueSignal.wait(uniqueLock);
                }
            }
            SendEvent(event.first, event.second);
        }
    }

} // namespace Zibra::Analytics
