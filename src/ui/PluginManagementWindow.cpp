#include "PrecompiledHeader.h"

#include "PluginManagementWindow.h"

#include "MessageBox.h"
#include "bridge/LibraryUtils.h"
#include "bridge/UpdateCheck.h"
#include "licensing/HoudiniLicenseManager.h"
#include "utils/Helpers.h"
#include "analytics/Analytics.h"

namespace Zibra
{
    class PluginManagementWindowImpl : public AP_Interface
    {
    public:
        const char* className() const final
        {
            static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::className)>);

            return "ZibraVDBPluginManagement";
        }

        void Show();

    private:
        static constexpr const char* UI_FILE = "ZibraVDBPluginManagement.ui";

        static bool m_IsParsed;

        bool ParseUIFile();
        void InitializeLicenseFields();
        void HandleDownloadLibrary(UI_Event* event);
        void HandleOpenPackagesDirectory(UI_Event* event);
        void HandleLoadLibrary(UI_Event* event);
        void HandleUpdateLibrary(UI_Event* event);
        static void HandleUpdateLibraryCalback(UI::MessageBox::Result result);
        void HandleSetLicenseKey(UI_Event* event);
        void HandleSetOfflineLicense(UI_Event* event);
        void HandleSetLicenseServer(UI_Event* event);
        void HandleOpenAnalyticsSettings(UI_Event* event);
        void HandleRetryLicenseCheck(UI_Event* event);
        void HandleRemoveLicense(UI_Event* event);
        void HandleCopyLicenseToHSITE(UI_Event* event);
        static void HandleCopyLicenseToHSITECalback(UI::MessageBox::Result result);
        void HandleCopyLicenseToHQROOT(UI_Event* event);
        static void HandleCopyLicenseToHQROOTCallback(const char* path);
        void UpdateUI();
        void SetStringField(const char* fieldName, const char* value);
    };

    bool PluginManagementWindowImpl::m_IsParsed = false;

    class EnterHQROOTPathWindow : public AP_Interface
    {
    public:
        EnterHQROOTPathWindow(void (*callback)(const char*));
        const char* className() const final
        {
            static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&EnterHQROOTPathWindow::className)>);

            return "ZibraVDBEnterHQROOTPath";
        }

        void Show();

    private:
        static constexpr const char* UI_FILE = "ZibraVDBEnterHQROOTPath.ui";

        bool ParseUIFile();
        void SetDefaultPath();

        void HandleClick(UI_Event* event);
        void CloseWindow();

        bool m_IsParsed = false;
        void (*m_Callback)(const char*);
    };

    void PluginManagementWindowImpl::Show()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::Show)>);

        if (!ParseUIFile())
        {
            return;
        }

        HoudiniLicenseManager::GetInstance().CheckoutLicense();
        InitializeLicenseFields();
        UpdateUI();

        (*getValueSymbol("window.val")) = true;
        getValueSymbol("window.val")->changed(this);
    }

    bool PluginManagementWindowImpl::ParseUIFile()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::ParseUIFile)>);

        if (m_IsParsed)
        {
            return true;
        }

        if (!readUIFile(UI_FILE))
        {
            return false;
        }

        getValueSymbol("download_library.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleDownloadLibrary));
        getValueSymbol("open_packages_directory.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleOpenPackagesDirectory));
        getValueSymbol("load_library.val")->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleLoadLibrary));
        getValueSymbol("update_library.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleUpdateLibrary));
        getValueSymbol("set_license_key.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleSetLicenseKey));
        getValueSymbol("set_offline_license.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleSetOfflineLicense));
        getValueSymbol("set_license_server.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleSetLicenseServer));
        getValueSymbol("open_analytics_settings.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleOpenAnalyticsSettings));
        getValueSymbol("retry_license_check.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleRetryLicenseCheck));
        getValueSymbol("remove_license.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleRemoveLicense));
        getValueSymbol("copy_license_to_hsite.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleCopyLicenseToHSITE));
        getValueSymbol("copy_license_to_hqroot.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleCopyLicenseToHQROOT));

        m_IsParsed = true;

        return true;
    }

    void PluginManagementWindowImpl::InitializeLicenseFields()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::InitializeLicenseFields)>);

        SetStringField("license_key.val", HoudiniLicenseManager::GetInstance().GetLicenseKey().c_str());
        SetStringField("offline_license.val", HoudiniLicenseManager::GetInstance().GetOfflineLicense().c_str());
        SetStringField("license_server.val", HoudiniLicenseManager::GetInstance().GetLicenseServerAddress().c_str());
    }

    void PluginManagementWindowImpl::HandleDownloadLibrary(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleDownloadLibrary)>);

        if (LibraryUtils::IsLibraryLoaded())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Library is already downloaded.");
            return;
        }

        Helpers::OpenInBrowser(LIBRARY_DOWNLOAD_URL);
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleOpenPackagesDirectory(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleOpenPackagesDirectory)>);

        std::vector<std::string> userPrefDirPath =
            Helpers::GetHoudiniEnvironmentVariable(ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR");
        if (userPrefDirPath.empty())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Could not find User Preference Directory path.");
            return;
        }

        std::error_code errorCode;

        std::filesystem::path userPrefDirStdPath;
        try
        {
            userPrefDirStdPath = std::filesystem::path(userPrefDirPath[0]);
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            const std::string errorMessage =
                std::string("Failed to parse path specified by USER_PREF_DIR_PATH due to error :\"") + e.what() + "\"";
            UI::MessageBox::Show(UI::MessageBox::Type::OK, errorMessage);
            return;
        }

        // MSVC seem to not implement std::filesystem::is_directory/status correctly, and sets errorCode when path does not exist
        // We first need to check whether something exists at packages directory path
        // And only then we can use std::filesystem::status
        bool userPrefDirExists = std::filesystem::exists(userPrefDirStdPath, errorCode);

        if (errorCode)
        {
            const std::string errorMessage =
                "Failed to access path specified by USER_PREF_DIR_PATH due to error :\"" + errorCode.message() + "\"";
            UI::MessageBox::Show(UI::MessageBox::Type::OK, errorMessage);
            return;
        }

        if (!userPrefDirExists)
        {
            const std::string errorMessage = "Directory pointer to by USER_PREF_DIR_PATH - \"" + userPrefDirPath[0] + "\" does not exist";
            UI::MessageBox::Show(UI::MessageBox::Type::OK, errorMessage.c_str());
            return;
        }

        bool isUserPrefDirPathADirectory = std::filesystem::is_directory(userPrefDirStdPath, errorCode);

        if (errorCode)
        {
            const std::string errorMessage =
                "Failed to access path specified by USER_PREF_DIR_PATH due to error :\"" + errorCode.message() + "\"";
            UI::MessageBox::Show(UI::MessageBox::Type::OK, errorMessage);
            return;
        }

        if (!isUserPrefDirPathADirectory)
        {
            const std::string errorMessage =
                "Directory pointer to by USER_PREF_DIR_PATH - \"" + userPrefDirPath[0] + "\" points to file instead of a directory";
            UI::MessageBox::Show(UI::MessageBox::Type::OK, errorMessage.c_str());
            return;
        }

        std::filesystem::path packagesStdPath;
        try
        {
            packagesStdPath = userPrefDirStdPath / "packages";
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            const std::string errorMessage = std::string("Failed to parse packages directory path due to error :\"") + e.what() + "\"";
            UI::MessageBox::Show(UI::MessageBox::Type::OK, errorMessage);
            return;
        }

        bool packagesPathExists = std::filesystem::exists(packagesStdPath, errorCode);

        if (errorCode)
        {
            const std::string errorMessage = "Failed to access packages directory path due to error :\"" + errorCode.message() + "\"";
            UI::MessageBox::Show(UI::MessageBox::Type::OK, errorMessage);
            return;
        }

        if (packagesPathExists)
        {
            bool isPackagePathADirectory = std::filesystem::is_directory(packagesStdPath, errorCode);

            if (errorCode)
            {
                const std::string errorMessage = "Failed to access packages directory path due to error :\"" + errorCode.message() + "\"";
                UI::MessageBox::Show(UI::MessageBox::Type::OK, errorMessage);
                return;
            }

            if (!isPackagePathADirectory)
            {
                UI::MessageBox::Show(UI::MessageBox::Type::OK, "Packages directory path points to a file instead.");
                return;
            }
        }
        else
        {
            bool isCreated = std::filesystem::create_directory(packagesStdPath, errorCode);
            if (errorCode)
            {
                const std::string errorMessage =
                    "Packages directory does not exist, and creating one failed with an error :\"" + errorCode.message() + "\"";
                UI::MessageBox::Show(UI::MessageBox::Type::OK, errorMessage);
                return;
            }

            if (!isCreated)
            {
                UI::MessageBox::Show(UI::MessageBox::Type::OK, "Packages directory does not exist, and creating one failed");
                return;
            }
        }

        Helpers::OpenInFileExplorer(packagesStdPath);
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleLoadLibrary(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleLoadLibrary)>);

        if (LibraryUtils::IsLibraryLoaded())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Library is already loaded.");
            return;
        }

        std::vector<std::string> userPrefDirPath =
            Helpers::GetHoudiniEnvironmentVariable(ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR");
        if (userPrefDirPath.empty())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Could not find User Preference Directory path.");
            return;
        }

        std::string packagePath =
            userPrefDirPath[0] + "/packages/ZibraVDB_Library_" ZIB_STRINGIFY(ZIB_COMPRESSION_ENGINE_MAJOR_VERSION) ".json";

        UT_EnvControl::loadPackage(packagePath.c_str(), UT_EnvControl::packageLoader());

        bool isLoaded = LibraryUtils::TryLoadLibrary();

        if (isLoaded)
        {
            HoudiniLicenseManager::GetInstance().CheckoutLicense();
        }

        UpdateUI();

        if (!isLoaded)
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK,
                                 "Could not load library. Please make sure that you have copied library to the correct folder.");
            return;
        }
        else
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Library loaded successfully. You can now use ZibraVDB.");
        }
    }

    void PluginManagementWindowImpl::HandleUpdateLibrary(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleUpdateLibrary)>);

        auto updateStatus = UpdateCheck::Run();

        if (updateStatus == UpdateCheck::Status::Latest)
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Library is already up to date.");
            return;
        }
        else if (updateStatus == UpdateCheck::Status::NotInstalled)
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Library is not yet installed.");
            return;
        }

        UI::MessageBox::Show(UI::MessageBox::Type::OK,
                             "You will be directed to download page. To update ZibraVDB Library, open Packages Directory, close Houdini, "
                             "then proceed with normal installation flow and when prompted overwrite old version files.",
                             &PluginManagementWindowImpl::HandleUpdateLibraryCalback);
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleUpdateLibraryCalback(UI::MessageBox::Result result)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleUpdateLibraryCalback)>);

        Helpers::OpenInBrowser(LIBRARY_DOWNLOAD_URL);
    }

    void PluginManagementWindowImpl::HandleSetLicenseKey(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleSetLicenseKey)>);

        auto key = getValueSymbol("license_key.val")->getString();
        HoudiniLicenseManager::GetInstance().RemoveLicense();
        HoudiniLicenseManager::GetInstance().SetLicenseKey(key);
        HoudiniLicenseManager::GetInstance().CheckoutLicense();
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleSetOfflineLicense(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleSetOfflineLicense)>);

        auto offlineLicense = getValueSymbol("offline_license.val")->getString();
        HoudiniLicenseManager::GetInstance().RemoveLicense();
        HoudiniLicenseManager::GetInstance().SetOfflineLicense(offlineLicense);
        HoudiniLicenseManager::GetInstance().CheckoutLicense();
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleSetLicenseServer(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleSetLicenseServer)>);

        auto offlineLicense = getValueSymbol("license_server.val")->getString();
        HoudiniLicenseManager::GetInstance().RemoveLicense();
        HoudiniLicenseManager::GetInstance().SetLicenseServer(offlineLicense);
        HoudiniLicenseManager::GetInstance().CheckoutLicense();
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleOpenAnalyticsSettings(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleOpenAnalyticsSettings)>);

        auto& analyticsManager = Analytics::AnalyticsManager::GetInstance();

        if (!HoudiniLicenseManager::GetInstance().IsAnyLicenseValid())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Analytics can not be enabled without active license.");
            return;
        }

        if (!analyticsManager.IsLicenseKeyAvailable())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Analytics can not be enabled if activation method other than License Key was used.");
            return;
        }

        if (!analyticsManager.IsAnalyticsAllowedByLicense())
        {
            std::string errorMessage = std::string("Analytics can only be enabled when using Free license. Current license type is: ") +
                                       HoudiniLicenseManager::GetInstance().GetLicenseType() + ".";
            UI::MessageBox::Show(UI::MessageBox::Type::OK, errorMessage.c_str());
            return;
        }

        analyticsManager.ShowAnalyticsPrompt();
    }

    void PluginManagementWindowImpl::HandleRetryLicenseCheck(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleRetryLicenseCheck)>);

        HoudiniLicenseManager::GetInstance().CheckoutLicense();
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleRemoveLicense(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleRemoveLicense)>);

        HoudiniLicenseManager::GetInstance().RemoveLicense();
        HoudiniLicenseManager::GetInstance().CheckoutLicense();

        for (size_t i = 0; i < size_t(HoudiniLicenseManager::Product::Count); ++i)
        {
            if (HoudiniLicenseManager::GetInstance().GetLicenseStatus(HoudiniLicenseManager::Product(i)) ==
                HoudiniLicenseManager::Status::OK)
            {
                UI::MessageBox::Show(UI::MessageBox::Type::OK, "Can not automatically remove license. If you wish to remove license from "
                                                               "HSITE or HQROOT please remove the file manually.");
            }
        }

        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleCopyLicenseToHSITE(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleCopyLicenseToHSITE)>);

        if (!HoudiniLicenseManager::GetInstance().IsAnyLicenseValid())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "No license found to copy.");
            return;
        }

        std::vector<std::string> hsitePath = Helpers::GetHoudiniEnvironmentVariable(ENV_HSITE, "HSITE");
        if (hsitePath.empty())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "HSITE environment variable is not set.");
            return;
        }

        UI::MessageBox::Show(
            UI::MessageBox::Type::YesNo,
            "This will copy your license key, offline license or license server address to HSITE. This is intended for site-wide "
            "installation of the license. Note that \"Remove License\" button can not remove license from HSITE. In case "
            "you'll want to remove it, please manually remove the file. Do you wish to proceed?",
            &PluginManagementWindowImpl::HandleCopyLicenseToHSITECalback);
    }

    void PluginManagementWindowImpl::HandleCopyLicenseToHSITECalback(UI::MessageBox::Result result)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleCopyLicenseToHSITECalback)>);

        if (result == UI::MessageBox::Result::No)
        {
            return;
        }

        std::vector<std::string> hsitePath = Helpers::GetHoudiniEnvironmentVariable(ENV_HSITE, "HSITE");
        if (hsitePath.empty())
        {
            assert(0);
            return;
        }

        HoudiniLicenseManager::GetInstance().CopyLicenseFile(hsitePath[0]);
    }

    void PluginManagementWindowImpl::HandleCopyLicenseToHQROOT(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleCopyLicenseToHQROOT)>);

        static EnterHQROOTPathWindow dialog(&HandleCopyLicenseToHQROOTCallback);

        if (!HoudiniLicenseManager::GetInstance().IsAnyLicenseValid())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "No license found to copy.");
            return;
        }

        dialog.Show();
    }

    void PluginManagementWindowImpl::HandleCopyLicenseToHQROOTCallback(const char* path)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::HandleCopyLicenseToHQROOTCallback)>);

        if (path == nullptr)
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Please enter valid path.");
            return;
        }

        HoudiniLicenseManager::GetInstance().CopyLicenseFile(path);
    }

    void PluginManagementWindowImpl::UpdateUI()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::UpdateUI)>);

        std::ignore = LibraryUtils::TryLoadLibrary();
        const auto& licenseManager = HoudiniLicenseManager::GetInstance();
        {
            SetStringField("plugin_version.val", ZIBRAVDB_VERSION);
        }
        {
            std::string libraryStatus;
            if (!LibraryUtils::IsPlatformSupported())
            {
                libraryStatus = "Platform not supported";
            }
            else if (!LibraryUtils::IsLibraryLoaded())
            {
                libraryStatus = "Library not loaded";
            }
            else
            {
                libraryStatus = "Library loaded";
            }
            SetStringField("library_status.val", libraryStatus.c_str());
        }
        {
            std::string libraryVersion = LibraryUtils::GetLibraryVersionString();
            SetStringField("library_version.val", libraryVersion.c_str());
        }
        {
            std::string updateStatusString;
            auto updateStatus = UpdateCheck::Run();
            switch (updateStatus)
            {
            case UpdateCheck::Status::Latest:
                updateStatusString = "Up to date";
                break;
            case UpdateCheck::Status::UpdateAvailable:
                updateStatusString = "Update available";
                break;
            case UpdateCheck::Status::UpdateCheckFailed:
                updateStatusString = "Update check failed";
                break;
            case UpdateCheck::Status::NotInstalled:
                updateStatusString = "Library not installed";
                break;
            default:
                assert(0);
                break;
            }
            SetStringField("update_status.val", updateStatusString.c_str());
        }
        {
            std::string activationStatus;
            HoudiniLicenseManager::Status status = HoudiniLicenseManager::Status::Uninitialized;
            for (size_t i = 0; i < size_t(HoudiniLicenseManager::Product::Count); ++i)
            {
                auto productStatus = licenseManager.GetLicenseStatus(HoudiniLicenseManager::Product(i));
                if (productStatus < status)
                {
                    status = productStatus;
                }
            }

            switch (status)
            {
            case HoudiniLicenseManager::Status::OK:
                if (licenseManager.GetLicenseStatus(HoudiniLicenseManager::Product::Compression) == HoudiniLicenseManager::Status::OK)
                {
                    if (licenseManager.GetLicenseStatus(HoudiniLicenseManager::Product::Decompression) == HoudiniLicenseManager::Status::OK)
                    {
                        activationStatus = "Activated";
                    }
                    else
                    {
                        activationStatus = "Activated (Only Compression)";
                    }
                }
                else
                {
                    if (licenseManager.GetLicenseStatus(HoudiniLicenseManager::Product::Decompression) == HoudiniLicenseManager::Status::OK)
                    {
                        activationStatus = "Activated (Only Decompression)";
                    }
                    else
                    {
                        assert(0);
                        activationStatus = "You should never see this.";
                    }
                }
                break;
            case HoudiniLicenseManager::Status::ValidationError: {
                std::string activationError = licenseManager.GetActivationError();
                activationStatus = std::string("License validation failed") + (activationError.empty() ? "" : ": ") + activationError;
                break;
            }
            case HoudiniLicenseManager::Status::InvalidKeyFormat:
                activationStatus = "Invalid Key Format";
                break;
            case HoudiniLicenseManager::Status::NoLicense:
                activationStatus = "License not found";
                break;
            case HoudiniLicenseManager::Status::LibraryError:
                activationStatus = "Library is required";
                break;
            case HoudiniLicenseManager::Status::Uninitialized:
                activationStatus = "Uninitialized";
                break;
            default:
                assert(0);
                break;
            }

            SetStringField("activation_status.val", activationStatus.c_str());
        }
        {
            std::string licenseType = "None";
            for (size_t i = 0; i < size_t(HoudiniLicenseManager::Product::Count); ++i)
            {
                HoudiniLicenseManager::Product currentProduct = HoudiniLicenseManager::Product(i);
                if (licenseManager.GetLicenseStatus(currentProduct) == HoudiniLicenseManager::Status::OK)
                {
                    licenseType = licenseManager.GetLicenseType(currentProduct);
                    break;
                }
            }
            SetStringField("license_type.val", licenseType.c_str());
        }
        {
            std::string activationType;
            auto type = licenseManager.GetActivationType();
            switch (type)
            {
            case HoudiniLicenseManager::ActivationType::Offline:
                activationType = "Offline";
                break;
            case HoudiniLicenseManager::ActivationType::LicenseServer:
                activationType = "License Server";
                break;
            case HoudiniLicenseManager::ActivationType::Online:
                activationType = "Online";
                break;
            case HoudiniLicenseManager::ActivationType::None:
                activationType = "None";
                break;
            default:
                assert(0);
                break;
            }

            SetStringField("activation_type.val", activationType.c_str());
        }
        {
            std::string licensePathType;
            auto type = licenseManager.GetLicensePathType();
            switch (type)
            {
            case HoudiniLicenseManager::LicensePathType::EnvVar:
                licensePathType = "Environment Variable";
                break;
            case HoudiniLicenseManager::LicensePathType::UserPrefDir:
                licensePathType = "User Preference Directory";
                break;
            case HoudiniLicenseManager::LicensePathType::HSite:
                licensePathType = "HSITE";
                break;
            case HoudiniLicenseManager::LicensePathType::HQRoot:
                licensePathType = "HQROOT";
                break;
            case HoudiniLicenseManager::LicensePathType::None:
                licensePathType = "None";
                break;
            default:
                assert(0);
                break;
            }

            SetStringField("license_file_loaded_via.val", licensePathType.c_str());
        }
        {
            const std::string& licenseFilePath = licenseManager.GetLicensePath();
            if (licenseFilePath.empty())
            {
                SetStringField("license_file_path.val", "None");
            }
            else
            {
                SetStringField("license_file_path.val", licenseFilePath.c_str());
            }
        }
    }

    void PluginManagementWindowImpl::SetStringField(const char* fieldName, const char* value)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindowImpl::SetStringField)>);

        auto* symbol = getValueSymbol(fieldName);
        *symbol = value;
        symbol->changed(this);
    }

    EnterHQROOTPathWindow::EnterHQROOTPathWindow(void (*callback)(const char*))
        : m_Callback(callback)
    {
    }

    void EnterHQROOTPathWindow::Show()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&EnterHQROOTPathWindow::Show)>);

        if (!ParseUIFile())
        {
            return;
        }

        SetDefaultPath();

        (*getValueSymbol("dialog.val")) = true;
        getValueSymbol("dialog.val")->changed(this);
    }

    bool EnterHQROOTPathWindow::ParseUIFile()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&EnterHQROOTPathWindow::ParseUIFile)>);

        if (m_IsParsed)
        {
            return true;
        }

        if (!readUIFile(UI_FILE))
        {
            return false;
        }

        getValueSymbol("result.val")->addInterest(this, static_cast<UI_EventMethod>(&EnterHQROOTPathWindow::HandleClick));

        m_IsParsed = true;

        return true;
    }

    void EnterHQROOTPathWindow::SetDefaultPath()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&EnterHQROOTPathWindow::SetDefaultPath)>);

        std::string defaultPath = "H:";

        std::optional<std::string> hqrootPath = Helpers::GetNormalEnvironmentVariable("HQROOT");
        if (hqrootPath.has_value())
        {
            defaultPath = hqrootPath.value();
        }

        (*getValueSymbol("path.val")) = defaultPath.c_str();
        getValueSymbol("path.val")->changed(this);
    }

    void EnterHQROOTPathWindow::HandleClick(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&EnterHQROOTPathWindow::HandleClick)>);

        int32 result = (*getValueSymbol("result.val"));
        if (result == 1)
        {
            CloseWindow();
            return;
        }

        const char* path = getValueSymbol("path.val")->getString();

        CloseWindow();

        m_Callback(path);
    }

    void EnterHQROOTPathWindow::CloseWindow()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&EnterHQROOTPathWindow::CloseWindow)>);

        (*getValueSymbol("dialog.val")) = false;
        getValueSymbol("dialog.val")->changed(this);
    }

    void PluginManagementWindow::ShowWindow()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&PluginManagementWindow::ShowWindow)>);

        static PluginManagementWindowImpl impl;
        impl.Show();
    }
} // namespace Zibra
