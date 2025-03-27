#include "LicenseManager.h"

#include <filesystem>
#include <regex>

#include "bridge/LibraryUtils.h"
#include "ui/MessageBox.h"
#include "utils/Helpers.h"

namespace Zibra
{
    const char* const LicenseManager::ms_DefaultLicenseKeyFileName = "zibravdb_license_key.txt";
    const char* const LicenseManager::ms_DefaultOfflineLicenseFileName = "zibravdb_offline_license.txt";

    LicenseManager& LicenseManager::GetInstance()
    {
        static LicenseManager instance;
        return instance;
    }

    Zibra::LicenseManager::Status LicenseManager::GetLicenseStatus(Product product) const
    {
        return m_Status[size_t(product)];
    }

    LicenseManager::ActivationType LicenseManager::GetLicenceType() const
    {
        return m_Type;
    }

    LicenseManager::LicensePathType LicenseManager::GetLicensePathType() const
    {
        return m_LicensePathType;
    }

    const std::string& LicenseManager::GetLicensePath() const
    {
        return m_LicensePath;
    }

    void LicenseManager::CheckoutLicense()
    {
        if (IsAnyLicenseValid())
        {
            return;
        }

        Status status = Status::Uninitialized;
        for (ActivationType i = ActivationType::Offline; i < ActivationType::None; i = ActivationType(uint32_t(i) + 1))
        {
            for (LicensePathType j = LicensePathType::EnvVar; j < LicensePathType::None; j = LicensePathType(uint32_t(j) + 1))
            {
                Status newStatus = TryCheckoutLicense(i, j);
                if (newStatus == Status::OK)
                {
                    return;
                }

                if (newStatus < status)
                {
                    status = newStatus;
                }
            }
        }
        SetStatusForAllProducts(status);
    }

    void LicenseManager::RemoveLicense()
    {
        for (LicensePathType i = LicensePathType::EnvVar; i <= LicensePathType::UserPrefDir; i = LicensePathType(uint32_t(i) + 1))
        {
            std::string keyPath = GetKeyPath(i);
            if (std::filesystem::is_regular_file(keyPath))
            {
                std::filesystem::remove(keyPath);
            }
            std::string offlineLicensePath = GetOfflineLicensePath(i);
            if (std::filesystem::is_regular_file(offlineLicensePath))
            {
                std::filesystem::remove(offlineLicensePath);
            }
        }

        SetStatusForAllProducts(Status::Uninitialized);
        m_Type = ActivationType::None;
        m_LicensePathType = LicensePathType::None;
        m_LicensePath = "";
        m_LicenseKey = "";
        m_OfflineLicense = "";
        CE::Licensing::CAPI::ReleaseLicense();
    }

    std::string LicenseManager::GetLicenseKey() const
    {
        return m_LicenseKey;
    }

    std::string LicenseManager::GetOfflineLicense() const
    {
        return m_OfflineLicense;
    }

    void LicenseManager::SetLicenseKey(const char* key)
    {
        if (key == nullptr)
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Please enter license key.");
            return;
        }

        std::string keyTrimmed(key);
        keyTrimmed.erase(0, keyTrimmed.find_first_not_of('_'));
        keyTrimmed.erase(keyTrimmed.find_last_not_of('_') + 1);

        static std::regex keyFormat("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");

        if (!std::regex_match(keyTrimmed, keyFormat))
        {
            UI::MessageBox::Show(
                UI::MessageBox::Type::OK,
                "Invalid license key format. Please enter License key in format: \"01234567-89ab-cdef-0123-456789abcdef\".");
            return;
        }

        std::string targetFile;

        std::vector<std::string> envPath = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "ZIBRAVDB_LICENSE_KEY");
        assert(envPath.size() <= 1);
        if (!envPath.empty() && std::filesystem::is_regular_file(envPath[0]))
        {
            targetFile = envPath[0];
        }
        else
        {
            std::vector<std::string> userPrefDir =
                Helpers::GetHoudiniEnvironmentVariable(ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR");
            assert(userPrefDir.size() >= 1);
            if (!userPrefDir.empty())
            {
                targetFile = (std::filesystem::path(userPrefDir[0]) / ms_DefaultLicenseKeyFileName).string();
            }
        }

        assert(!targetFile.empty());
        if (!targetFile.empty())
        {
            std::ofstream file(targetFile);
            if (file.is_open())
            {
                file << keyTrimmed;
            }
        }
    }

    void LicenseManager::SetOfflineLicense(const char* offlineLicense)
    {
        if (offlineLicense == nullptr)
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Please enter offline license.");
            return;
        }

        std::string offlineLicenseTrimmed(offlineLicense);
        offlineLicenseTrimmed.erase(0, offlineLicenseTrimmed.find_first_not_of(" \r\n\t_"));
        offlineLicenseTrimmed.erase(offlineLicenseTrimmed.find_last_not_of(" \r\n\t_") + 1);

        // Check that first and last characters are '{' and '}' respectively
        if (offlineLicenseTrimmed.empty() || offlineLicenseTrimmed.front() != '{' || offlineLicenseTrimmed.back() != '}')
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK,
                                 "Invalid offline license format. Please enter offline license in format: \"{ \"license_info\": ... }\".");
            return;
        }

        std::string targetFile;

        std::vector<std::string> envPath = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "ZIBRAVDB_OFFLINE_LICENSE");
        assert(envPath.size() <= 1);
        if (!envPath.empty() && std::filesystem::is_regular_file(envPath[0]))
        {
            targetFile = envPath[0];
        }
        else
        {
            std::vector<std::string> userPrefDir =
                Helpers::GetHoudiniEnvironmentVariable(ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR");
            assert(userPrefDir.size() >= 1);
            if (!userPrefDir.empty())
            {
                targetFile = (std::filesystem::path(userPrefDir[0]) / ms_DefaultOfflineLicenseFileName).string();
            }
        }

        assert(!targetFile.empty());
        if (!targetFile.empty())
        {
            std::ofstream file(targetFile);
            if (file.is_open())
            {
                file << offlineLicenseTrimmed;
            }
        }
    }

    void LicenseManager::CopyLicenseFile(const std::string& destFolder)
    {
        if (!IsAnyLicenseValid())
        {
            assert(0);
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "No license file found to copy.");
            return;
        }

        std::string licensePath = GetLicensePath();
        if (licensePath.empty())
        {
            assert(0);
            return;
        }
        std::filesystem::path sourcePath(licensePath);
        std::filesystem::path destPath(destFolder);
        destPath /= sourcePath.filename();
        std::error_code ec;
        std::filesystem::copy_file(sourcePath, destPath, ec);
        if (ec)
        {
            std::string message = "Failed to copy license file to \"";
            message += destPath.string();
            message += "\". Please make sure that target folder exists and does not already contain license file. If you want to overwrite "
                       "old license file, please manually delete old one.";
            UI::MessageBox::Show(UI::MessageBox::Type::OK, message);
        }
        else
        {
            std::string message = "Successfully copied license file to \"";
            message += destPath.string();
            message += "\".";
            UI::MessageBox::Show(UI::MessageBox::Type::OK, message);
        }
    }

    bool LicenseManager::CheckLicense(Product product)
    {
        if (!IsLicenseValid(product))
        {
            CheckoutLicense();
        }
        return IsLicenseValid(product);
    }

    std::string LicenseManager::SanitizePath(const std::string& path)
    {
        std::string result = path;
        const char* charsToStrip = " \n\r\t\"'";

        size_t startPos = result.find_first_not_of(charsToStrip);
        if (startPos == std::string::npos)
        {
            return "";
        }
        result.erase(0, startPos);

        size_t endPos = result.find_last_not_of(charsToStrip);
        if (endPos == std::string::npos)
        {
            assert(0);
            return "";
        }
        result.erase(endPos + 1);
        return result;
    }

    std::string LicenseManager::SanitizeKey(const std::string& key)
    {
        std::string result = key;
        const char* keyChars = "0123456789abcdef-";

        size_t startPos = result.find_first_of(keyChars);
        if (startPos == std::string::npos)
        {
            return "";
        }
        result.erase(0, startPos);

        size_t endPos = result.find_last_of(keyChars);
        if (endPos == std::string::npos)
        {
            assert(0);
            return "";
        }
        result.erase(endPos + 1);
        return result;
    }

    std::string LicenseManager::SanitizeOfflineLicense(const std::string& offlineLicense)
    {
        std::string result = offlineLicense;

        size_t startPos = result.find_first_of('{');
        if (startPos == std::string::npos)
        {
            return "";
        }
        result.erase(0, startPos);

        size_t endPos = result.find_last_of('}');
        if (endPos == std::string::npos)
        {
            return "";
        }
        result.erase(endPos + 1);
        return result;
    }

    std::string LicenseManager::GetKeyPath(LicensePathType pathType)
    {
        std::vector<std::string> potentialPaths;

        switch (pathType)
        {
        case LicensePathType::EnvVar:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "ZIBRAVDB_OFFLINE_LICENSE");
            break;
        case LicensePathType::UserPrefDir:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR");
            Helpers::AppendToPath(potentialPaths, ms_DefaultLicenseKeyFileName);
            break;
        case LicensePathType::HSite:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_HSITE, "HSITE");
            Helpers::AppendToPath(potentialPaths, ms_DefaultLicenseKeyFileName);
            break;
        case LicensePathType::HQRoot:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "HQROOT");
            Helpers::AppendToPath(potentialPaths, ms_DefaultLicenseKeyFileName);
            break;
        default:
            assert(0);
            return "";
        }

        for (const std::string& path : potentialPaths)
        {
            std::string sanitizedPath = SanitizePath(path);
            if (std::filesystem::is_regular_file(sanitizedPath))
            {
                return sanitizedPath;
            }
        }

        return "";
    }

    std::string LicenseManager::GetOfflineLicensePath(LicensePathType pathType)
    {
        std::vector<std::string> potentialPaths;

        switch (pathType)
        {
        case LicensePathType::EnvVar:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "ZIBRAVDB_LICENSE_KEY");
            break;
        case LicensePathType::UserPrefDir:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR");
            Helpers::AppendToPath(potentialPaths, ms_DefaultOfflineLicenseFileName);
            break;
        case LicensePathType::HSite:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_HSITE, "HSITE");
            Helpers::AppendToPath(potentialPaths, ms_DefaultOfflineLicenseFileName);
            break;
        case LicensePathType::HQRoot:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "HQROOT");
            Helpers::AppendToPath(potentialPaths, ms_DefaultOfflineLicenseFileName);
            break;
        default:
            assert(0);
            return "";
        }

        for (const std::string& path : potentialPaths)
        {
            std::string sanitizedPath = SanitizePath(path);
            if (std::filesystem::is_regular_file(sanitizedPath))
            {
                return sanitizedPath;
            }
        }

        return "";
    }

    std::string LicenseManager::ReadKeyFromFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return "";
        }

        std::string licenseKey((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return SanitizeKey(licenseKey);
    }

    std::string LicenseManager::ReadOfflineLicenseFromFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return "";
        }

        std::string offlineLicense((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return SanitizeOfflineLicense(offlineLicense);
    }

    bool LicenseManager::IsLicenseValid(Product product) const
    {
        return m_Status[size_t(product)] == Status::OK;
    }

    bool LicenseManager::IsAnyLicenseValid() const
    {
        for (size_t i = 0; i < size_t(Product::Count); ++i)
        {
            if (m_Status[i] == Status::OK)
            {
                return true;
            }
        }
        return false;
    }

    LicenseManager::Status LicenseManager::TryCheckoutLicense(ActivationType type, LicensePathType pathType)
    {
        Zibra::LibraryUtils::LoadLibrary();
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            return Status::LibraryError;
        }

        switch (type)
        {
        case ActivationType::Offline: {
            std::string offlineLicensePath = GetOfflineLicensePath(pathType);
            if (offlineLicensePath.empty())
            {
                return Status::NoLicense;
            }
            std::string offlineLicense = ReadOfflineLicenseFromFile(offlineLicensePath);
            if (offlineLicense.empty())
            {
                return Status::NoLicense;
            }
            CE::Licensing::CAPI::CheckoutLicenseOffline(offlineLicense.c_str(), offlineLicense.size());

            if (CE::Licensing::CAPI::IsLicenseValidated(CE::Licensing::ProductType::Compression))
            {
                SetStatusFromZibraVDBRuntime();
                m_Type = ActivationType::Offline;
                m_LicensePathType = pathType;
                m_LicensePath = offlineLicensePath;
                m_LicenseKey = "";
                m_OfflineLicense = offlineLicense;
                return Status::OK;
            }

            return Status::ValidationError;
        }
        case ActivationType::Online: {
            std::string licenseKeyPath = GetKeyPath(pathType);
            if (licenseKeyPath.empty())
            {
                return Status::NoLicense;
            }
            std::string licenseKey = ReadKeyFromFile(licenseKeyPath);
            if (licenseKey.empty())
            {
                return Status::NoLicense;
            }
            CE::Licensing::CAPI::CheckoutLicenseWithKey(licenseKey.c_str());
            if (CE::Licensing::CAPI::IsLicenseValidated(CE::Licensing::ProductType::Compression))
            {
                SetStatusFromZibraVDBRuntime();
                m_Type = ActivationType::Online;
                m_LicensePathType = pathType;
                m_LicensePath = licenseKeyPath;
                m_LicenseKey = licenseKey;
                m_OfflineLicense = "";
                return Status::OK;
            }

            return Status::ValidationError;
        }
        default:
            assert(0);
            return Status::NoLicense;
        }
    }

    void LicenseManager::SetStatusFromZibraVDBRuntime()
    {
        for (size_t i = 0; i < size_t(Product::Count); ++i)
        {
            m_Status[i] = CE::Licensing::CAPI::IsLicenseValidated(CE::Licensing::ProductType(i)) ? Status::OK : Status::ValidationError;
        }
    }

    void LicenseManager::SetStatusForAllProducts(Status status)
    {
        for (size_t i = 0; i < size_t(Product::Count); ++i)
        {
            m_Status[i] = status;
        }
    }

} // namespace Zibra