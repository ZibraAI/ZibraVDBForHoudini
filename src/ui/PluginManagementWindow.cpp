#include "PluginManagementWindow.h"

#include <SI/AP_Interface.h>
#include <UI/UI_Value.h>

#include "MessageBox.h"
#include "bridge/LibraryUtils.h"
#include "bridge/UpdateCheck.h"
#include "licensing/LicenseManager.h"
#include "licensing/TrialManager.h"
#include "utils/Helpers.h"

namespace Zibra
{
    class PluginManagementWindowImpl : public AP_Interface
    {
    public:
        const char* className() const final
        {
            return "ZibraVDBPluginManagement";
        }

        void Show();

    private:
        static constexpr const char* UI_FILE = "ZibraVDBPluginManagement.ui";

        static bool m_IsParsed;

        bool ParseUIFile();
        void HandleDownloadLibrary(UI_Event* event);
        void HandleLoadLibrary(UI_Event* event);
        void HandleUpdateLibrary(UI_Event* event);
        static void HandleUpdateLibraryCalback(UI::MessageBox::Result result);
        void HandleSetLicenseKey(UI_Event* event);
        void HandleSetOfflineLicense(UI_Event* event);
        void HandleRetryLicenseCheck(UI_Event* event);
        void HandleRemoveLicense(UI_Event* event);
        void HandleCopyLicenseToHSITE(UI_Event* event);
        static void HandleCopyLicenseToHSITECalback(UI::MessageBox::Result result);
        void HandleCopyLicenseToHQROOT(UI_Event* event);
        static void HandleCopyLicenseToHQROOTCallback(const char* path);
        void HandleCopyLibraryToHSITE(UI_Event* event);
        void HandleCopyLibraryToHQROOT(UI_Event* event);
        static void HandleCopyLibraryToHQROOTCallback(const char* path);
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
        if (!ParseUIFile())
        {
            return;
        }

        LicenseManager::GetInstance().CheckoutLicense();
        UpdateUI();

        (*getValueSymbol("window.val")) = true;
        getValueSymbol("window.val")->changed(this);
    }

    bool PluginManagementWindowImpl::ParseUIFile()
    {
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
        getValueSymbol("load_library.val")->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleLoadLibrary));
        getValueSymbol("update_library.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleUpdateLibrary));
        getValueSymbol("set_license_key.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleSetLicenseKey));
        getValueSymbol("set_offline_license.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleSetOfflineLicense));
        getValueSymbol("retry_license_check.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleRetryLicenseCheck));
        getValueSymbol("remove_license.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleRemoveLicense));
        getValueSymbol("copy_license_to_hsite.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleCopyLicenseToHSITE));
        getValueSymbol("copy_license_to_hqroot.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleCopyLicenseToHQROOT));
        getValueSymbol("copy_library_to_hsite.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleCopyLibraryToHSITE));
        getValueSymbol("copy_library_to_hqroot.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleCopyLibraryToHQROOT));

        m_IsParsed = true;

        return true;
    }

    void PluginManagementWindowImpl::HandleDownloadLibrary(UI_Event* event)
    {
        if (LibraryUtils::IsLibraryLoaded())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Library is already downloaded.");
            return;
        }

        Helpers::OpenInBrowser(LIBRARY_DOWNLOAD_URL);
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleLoadLibrary(UI_Event* event)
    {
        if (LibraryUtils::IsLibraryLoaded())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Library is already loaded.");
            return;
        }

        LibraryUtils::LoadLibrary();
        UpdateUI();

        if (!LibraryUtils::IsLibraryLoaded())
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
                             "You will be directed to download page. To update ZibraVDB Library, first close Houdini, then proceed with "
                             "normal installation flow and when prompted overwrite old version files.",
                             &PluginManagementWindowImpl::HandleUpdateLibraryCalback);
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleUpdateLibraryCalback(UI::MessageBox::Result result)
    {
        Helpers::OpenInBrowser(LIBRARY_DOWNLOAD_URL);
    }

    void PluginManagementWindowImpl::HandleSetLicenseKey(UI_Event* event)
    {
        auto key = getValueSymbol("license_key.val")->getString();
        LicenseManager::GetInstance().SetLicenseKey(key);
        LicenseManager::GetInstance().CheckoutLicense();
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleSetOfflineLicense(UI_Event* event)
    {
        auto offlineLicense = getValueSymbol("offline_license.val")->getString();
        LicenseManager::GetInstance().SetOfflineLicense(offlineLicense);
        LicenseManager::GetInstance().CheckoutLicense();
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleRetryLicenseCheck(UI_Event* event)
    {
        LicenseManager::GetInstance().CheckoutLicense();
        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleRemoveLicense(UI_Event* event)
    {
        LicenseManager::GetInstance().RemoveLicense();
        LicenseManager::GetInstance().CheckoutLicense();

        for (size_t i = 0; i < size_t(LicenseManager::Product::Count); ++i)
        {
            if (LicenseManager::GetInstance().GetLicenseStatus(LicenseManager::Product(i)) == LicenseManager::Status::OK)
            {
                UI::MessageBox::Show(UI::MessageBox::Type::OK, "Can not automatically remove license. If you wish to remove license from "
                                                               "HSITE or HQROOT please remove the file manually.");
            }
        }

        UpdateUI();
    }

    void PluginManagementWindowImpl::HandleCopyLicenseToHSITE(UI_Event* event)
    {
        if (!LicenseManager::GetInstance().CheckLicense(LicenseManager::Product::Compression))
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

        UI::MessageBox::Show(UI::MessageBox::Type::YesNo,
                             "This will copy your license key or your offline license to HSITE. This is intended for site-wide "
                             "installation of the license. Note that \"Remove License\" button can not remove license from HSITE. In case "
                             "you'll want to remove it, please manually remove the file. Do you wish to proceed?",
                             &PluginManagementWindowImpl::HandleCopyLicenseToHSITECalback);
    }

    void PluginManagementWindowImpl::HandleCopyLicenseToHSITECalback(UI::MessageBox::Result result)
    {
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

        LicenseManager::GetInstance().CopyLicenseFile(hsitePath[0]);
    }

    void PluginManagementWindowImpl::HandleCopyLicenseToHQROOT(UI_Event* event)
    {
        static EnterHQROOTPathWindow dialog(&HandleCopyLicenseToHQROOTCallback);

        if (!LicenseManager::GetInstance().CheckLicense(LicenseManager::Product::Compression))
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "No license found to copy.");
            return;
        }

        dialog.Show();
    }

    void PluginManagementWindowImpl::HandleCopyLicenseToHQROOTCallback(const char* path)
    {
        if (path == nullptr)
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Please enter valid path.");
            return;
        }

        LicenseManager::GetInstance().CopyLicenseFile(path);
    }

    void PluginManagementWindowImpl::HandleCopyLibraryToHSITE(UI_Event* event)
    {
        std::vector<std::string> userPrefDirPaths =
            Helpers::GetHoudiniEnvironmentVariable(ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR");
        if (userPrefDirPaths.empty())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Could not find User Preference Directory path.");
            return;
        }
        std::filesystem::path source = userPrefDirPaths[0];
        source /= ZIB_LIBRARY_FOLDER;

        std::vector<std::string> hsitePathVector = Helpers::GetHoudiniEnvironmentVariable(ENV_HSITE, "HSITE");
        if (hsitePathVector.empty())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "HSITE environment variable is not set.");
            return;
        }
        std::filesystem::path destination = hsitePathVector[0];
        destination = destination / ZIB_LIBRARY_FOLDER;

        std::error_code ec;
        std::filesystem::create_directories(destination, ec);
        if (ec)
        {
            std::string message = "Failed to create target directory. Error: ";
            message += ec.message();
            UI::MessageBox::Show(UI::MessageBox::Type::OK, message);
        }

        std::filesystem::copy(source, destination, std::filesystem::copy_options::recursive, ec);
        if (ec)
        {
            std::string message = "Failed to copy library to HSITE. Error: ";
            message += ec.message();
            UI::MessageBox::Show(UI::MessageBox::Type::OK, message);
        }
        else
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Library copied to HSITE.");
        }
    }

    void PluginManagementWindowImpl::HandleCopyLibraryToHQROOT(UI_Event* event)
    {
        static EnterHQROOTPathWindow dialog(&HandleCopyLibraryToHQROOTCallback);
        dialog.Show();
    }

    void PluginManagementWindowImpl::HandleCopyLibraryToHQROOTCallback(const char* path)
    {
        if (path == nullptr)
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Please enter valid path.");
            return;
        }

        std::vector<std::string> userPrefDirPaths =
            Helpers::GetHoudiniEnvironmentVariable(ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR");
        if (userPrefDirPaths.empty())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Could not find User Preference Directory path.");
            return;
        }
        std::filesystem::path source = userPrefDirPaths[0];
        source /= ZIB_LIBRARY_FOLDER;

        std::filesystem::path destination = path;
        destination /= ZIB_LIBRARY_FOLDER;

        std::error_code ec;
        std::filesystem::create_directories(destination, ec);
        if (ec)
        {
            std::string message = "Failed to create target directory. Error: ";
            message += ec.message();
            UI::MessageBox::Show(UI::MessageBox::Type::OK, message);
            return;
        }

        std::filesystem::copy(source, destination, std::filesystem::copy_options::recursive, ec);
        if (ec)
        {
            std::string message = "Failed to copy library to HQROOT. Error: ";
            message += ec.message();
            UI::MessageBox::Show(UI::MessageBox::Type::OK, message);
        }
        else
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Library copied to HQROOT.");
        }
    }

    void PluginManagementWindowImpl::UpdateUI()
    {
        LibraryUtils::LoadLibrary();
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
            std::string trialStatus;

            if (!LibraryUtils::IsLibraryLoaded())
            {
                trialStatus = "Download library to see trial status.";
            }
            else if (LicenseManager::GetInstance().GetLicenseStatus(LicenseManager::Product::Compression) == LicenseManager::Status::OK)
            {
                trialStatus = "License activated, no trial required.";
            }
            else
            {
                int remainingTrialCompressions = TrialManager::GetRemainingTrialCompressions();
                if (remainingTrialCompressions == -1)
                {
                    trialStatus = "Failed to get trial status.";
                }
                else
                {
                    trialStatus = std::to_string(remainingTrialCompressions) + " trial compressions remaining.";
                }
            }

            SetStringField("trial_status.val", trialStatus.c_str());
        }
        {
            std::string activationStatus;
            LicenseManager::Status status = LicenseManager::Status::Uninitialized;
            for (size_t i = 0; i < size_t(LicenseManager::Product::Count); ++i)
            {
                auto productStatus = LicenseManager::GetInstance().GetLicenseStatus(LicenseManager::Product(i));
                if (productStatus < status)
                {
                    status = productStatus;
                }
            }
            switch (status)
            {
            case LicenseManager::Status::OK:
                activationStatus = "Activated";
                break;
            case LicenseManager::Status::ValidationError:
                activationStatus = "Validation Error";
                break;
            case LicenseManager::Status::InvalidKeyFormat:
                activationStatus = "Invalid Key Format";
                break;
            case LicenseManager::Status::NoLicense:
                activationStatus = "License not found";
                break;
            case LicenseManager::Status::LibraryError:
                activationStatus = "Library is required";
                break;
            case LicenseManager::Status::Uninitialized:
                activationStatus = "Uninitialized";
                break;
            default:
                assert(0);
                break;
            }

            SetStringField("activation_status.val", activationStatus.c_str());
        }
        {
            std::string licenseType;
            auto type = LicenseManager::GetInstance().GetLicenceType();
            switch (type)
            {
            case LicenseManager::ActivationType::Offline:
                licenseType = "Offline";
                break;
            case LicenseManager::ActivationType::Online:
                licenseType = "Online";
                break;
            case LicenseManager::ActivationType::None:
                licenseType = "None";
                break;
            default:
                assert(0);
                break;
            }

            SetStringField("license_type.val", licenseType.c_str());
        }
        {
            std::string licensePathType;
            auto type = LicenseManager::GetInstance().GetLicensePathType();
            switch (type)
            {
            case LicenseManager::LicensePathType::EnvVar:
                licensePathType = "Environment Variable";
                break;
            case LicenseManager::LicensePathType::UserPrefDir:
                licensePathType = "User Preference Directory";
                break;
            case LicenseManager::LicensePathType::HSite:
                licensePathType = "HSITE";
                break;
            case LicenseManager::LicensePathType::HQRoot:
                licensePathType = "HQROOT";
                break;
            case LicenseManager::LicensePathType::None:
                licensePathType = "None";
                break;
            default:
                assert(0);
                break;
            }

            SetStringField("license_file_loaded_via.val", licensePathType.c_str());
        }
        {
            const std::string& licenseFilePath = LicenseManager::GetInstance().GetLicensePath();
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
        (*getValueSymbol(fieldName)) = value;
        getValueSymbol(fieldName)->changed(this);
    }

    EnterHQROOTPathWindow::EnterHQROOTPathWindow(void (*callback)(const char*))
        : m_Callback(callback)
    {
    }

    void EnterHQROOTPathWindow::Show()
    {
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
        std::string defaultPath = "H:";

        std::vector<std::string> hqrootPath = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "HQROOT");
        if (!hqrootPath.empty())
        {
            defaultPath = hqrootPath[0];
        }

        (*getValueSymbol("path.val")) = defaultPath.c_str();
        getValueSymbol("path.val")->changed(this);
    }

    void EnterHQROOTPathWindow::HandleClick(UI_Event* event)
    {
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
        (*getValueSymbol("dialog.val")) = false;
        getValueSymbol("dialog.val")->changed(this);
    }

    void PluginManagementWindow::ShowWindow()
    {
        static PluginManagementWindowImpl impl;
        impl.Show();
    }
} // namespace Zibra
