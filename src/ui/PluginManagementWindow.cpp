#include "PluginManagementWindow.h"

#include <SI/AP_Interface.h>
#include <UI/UI_Value.h>

#include "MessageBox.h"
#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
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
        void HandleSetLicenseKey(UI_Event* event);
        void HandleSetOfflineLicense(UI_Event* event);
        void HandleRetryLicenseCheck(UI_Event* event);
        void HandleRemoveLicense(UI_Event* event);
        void HandleCopyLicenseToHSITE(UI_Event* event);
        static void HandleCopyLicenseToHSITECalback(UI::MessageBox::Result);
        void HandleCopyLicenseToHQROOT(UI_Event* event);
        void HandleDownloadLibraryToHSITE(UI_Event* event);
        void HandleDownloadLibraryToHQROOT(UI_Event* event);
        void UpdateUI();
        void SetStringField(const char* fieldName, const char* value);
    };

    bool PluginManagementWindowImpl::m_IsParsed = false;

    class CopyLicenseToHQROOTWindow : public AP_Interface
    {
    public:
        const char* className() const final
        {
            return "ZibraVDBCopyLicenseToHQROOT";
        }

        void Show();

    private:
        static constexpr const char* UI_FILE = "ZibraVDBCopyLicenseToHQROOT.ui";

        static bool m_IsParsed;

        bool ParseUIFile();
        void SetDefaultPath();

        void HandleClick(UI_Event* event);
        void CloseWindow();
    };

    bool CopyLicenseToHQROOTWindow::m_IsParsed = false;

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
        getValueSymbol("download_library_to_hsite.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleDownloadLibraryToHSITE));
        getValueSymbol("download_library_to_hqroot.val")
            ->addInterest(this, static_cast<UI_EventMethod>(&PluginManagementWindowImpl::HandleDownloadLibraryToHQROOT));

        m_IsParsed = true;

        return true;
    }

    void PluginManagementWindowImpl::HandleDownloadLibrary(UI_Event* event)
    {
        // DONT SUBMIT
        // URL is placeholder
        Helpers::OpenInBrowser("https://zibra.ai/download");
        UpdateUI();
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
        static CopyLicenseToHQROOTWindow dialog;
        dialog.Show();
    }

    void PluginManagementWindowImpl::HandleDownloadLibraryToHSITE(UI_Event* event)
    {
        // DONT SUBMIT
        // TODO
    }

    void PluginManagementWindowImpl::HandleDownloadLibraryToHQROOT(UI_Event* event)
    {
        // DONT SUBMIT
        // TODO
    }

    void PluginManagementWindowImpl::UpdateUI()
    {
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
            case LicenseManager::Status::NetworkError:
                activationStatus = "Network Error";
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
            std::string licenseFilePath = LicenseManager::GetInstance().GetLicensePath();
            SetStringField("license_file_path.val", licenseFilePath.c_str());
        }
    }

    void PluginManagementWindowImpl::SetStringField(const char* fieldName, const char* value)
    {
        (*getValueSymbol(fieldName)) = value;
        getValueSymbol(fieldName)->changed(this);
    }

    void CopyLicenseToHQROOTWindow::Show()
    {
        if (!ParseUIFile())
        {
            return;
        }

        SetDefaultPath();

        (*getValueSymbol("dialog.val")) = true;
        getValueSymbol("dialog.val")->changed(this);
    }

    bool CopyLicenseToHQROOTWindow::ParseUIFile()
    {
        if (m_IsParsed)
        {
            return true;
        }

        if (!readUIFile(UI_FILE))
        {
            return false;
        }

        getValueSymbol("result.val")->addInterest(this, static_cast<UI_EventMethod>(&CopyLicenseToHQROOTWindow::HandleClick));

        m_IsParsed = true;

        return true;
    }

    void CopyLicenseToHQROOTWindow::SetDefaultPath()
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

    void CopyLicenseToHQROOTWindow::HandleClick(UI_Event* event)
    {
        int32 result = (*getValueSymbol("result.val"));
        if (result == 1)
        {
            CloseWindow();
            return;
        }

        const char* path = getValueSymbol("path.val")->getString();

        if (path == nullptr)
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Please enter valid path.");
            return;
        }

        CloseWindow();

        LicenseManager::GetInstance().CopyLicenseFile(path);
    }

    void CopyLicenseToHQROOTWindow::CloseWindow()
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
