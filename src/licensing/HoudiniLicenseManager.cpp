#include <filesystem>
#include <regex>

#include "HoudiniLicenseManager.h"
#include "bridge/LibraryUtils.h"
#include "ui/MessageBox.h"
#include "utils/Helpers.h"

namespace Zibra
{
    const char* const HoudiniLicenseManager::ms_DefaultLicenseKeyFileName = "zibravdb_license_key.txt";
    const char* const HoudiniLicenseManager::ms_DefaultOfflineLicenseFileName = "zibravdb_offline_license.txt";
    const char* const HoudiniLicenseManager::ms_DefaultLicenseServerFileName = "zibravdb_license_server.txt";

    HoudiniLicenseManager& HoudiniLicenseManager::GetInstance()
    {
        static HoudiniLicenseManager instance;
        return instance;
    }

    bool HoudiniLicenseManager::IsAnyLicenseValid() const
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

    HoudiniLicenseManager::Status HoudiniLicenseManager::GetLicenseStatus(Product product) const
    {
        return m_Status[size_t(product)];
    }

    int HoudiniLicenseManager::GetLicenseTier(Product product) const
    {
        if (!IsLicenseManagerLoaded())
        {
            return - 1;
        }

        if (m_Status[size_t(product)] == Status::OK)
        {
            return m_Manager->GetProductLicenseTier(CE::Licensing::ProductType(product));
        }
        return -1;
    }

    const char* HoudiniLicenseManager::GetLicenseType(Product product) const
    {
        if (!IsLicenseManagerLoaded())
        {
            return "Licensing is not initialized";
        }

        if (m_Status[size_t(product)] == Status::OK)
        {
            return m_Manager->GetProductLicenseType(CE::Licensing::ProductType(product));
        }

        return "None";
    }

    HoudiniLicenseManager::ActivationType HoudiniLicenseManager::GetActivationType() const
    {
        return m_Type;
    }

    HoudiniLicenseManager::LicensePathType HoudiniLicenseManager::GetLicensePathType() const
    {
        return m_LicensePathType;
    }

    const std::string& HoudiniLicenseManager::GetLicensePath() const
    {
        return m_LicensePath;
    }

    void HoudiniLicenseManager::CheckoutLicense()
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

    void HoudiniLicenseManager::RemoveLicense()
    {
        if (!LoadLicenseManager())
        {
            return;
        }

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
            std::string licenseServerAddressPath = GetLicenseServerAddressPath(i);
            if (std::filesystem::is_regular_file(licenseServerAddressPath))
            {
                std::filesystem::remove(licenseServerAddressPath);
            }
        }

        SetStatusForAllProducts(Status::Uninitialized);
        m_Type = ActivationType::None;
        m_LicensePathType = LicensePathType::None;
        m_LicensePath = "";
        m_LicenseKey = "";
        m_OfflineLicense = "";
        m_Manager->ReleaseLicense();
    }

    std::string HoudiniLicenseManager::GetLicenseKey() const
    {
        return m_LicenseKey;
    }

    std::string HoudiniLicenseManager::GetOfflineLicense() const
    {
        return m_OfflineLicense;
    }

    std::string HoudiniLicenseManager::GetLicenseServerAddress() const
    {
        return m_LicenseServerAddress;
    }

    void HoudiniLicenseManager::SetLicenseKey(const char* key)
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

    void HoudiniLicenseManager::SetOfflineLicense(const char* offlineLicense)
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

    void HoudiniLicenseManager::SetLicenseServer(const char* licenseServerAddress)
    {
        if (licenseServerAddress == nullptr)
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Please enter license server address.");
            return;
        }

        std::string licenseServerAddressTrimmed(licenseServerAddress);
        licenseServerAddressTrimmed.erase(0, licenseServerAddressTrimmed.find_first_not_of(" \r\n\t_"));
        licenseServerAddressTrimmed.erase(licenseServerAddressTrimmed.find_last_not_of(" \r\n\t_") + 1);

        if (licenseServerAddressTrimmed.empty())
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Invalid license server address format. Please enter a valid address.");
            return;
        }

        std::string targetFile;

        std::vector<std::string> envPath = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "ZIBRAVDB_LICENSE_SERVER");
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
                targetFile = (std::filesystem::path(userPrefDir[0]) / ms_DefaultLicenseServerFileName).string();
            }
        }

        assert(!targetFile.empty());
        if (!targetFile.empty())
        {
            std::ofstream file(targetFile);
            if (file.is_open())
            {
                file << licenseServerAddressTrimmed;
            }
        }
    }

    void HoudiniLicenseManager::CopyLicenseFile(const std::string& destFolder)
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

    bool HoudiniLicenseManager::CheckLicense(Product product)
    {
        if (!IsLicenseValid(product))
        {
            CheckoutLicense();
        }
        return IsLicenseValid(product);
    }

    const std::string& HoudiniLicenseManager::GetActivationError() const
    {
        return m_ActivationError;
    }

    bool HoudiniLicenseManager::LoadLicenseManager()
    {
        if (m_Manager != nullptr)
        {
            return true;
        }

        Zibra::LibraryUtils::LoadLibrary();
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            return false;
        }

        m_Manager = CE::Licensing::GetLicenseManager();

        return m_Manager != nullptr;
    }

    bool HoudiniLicenseManager::IsLicenseManagerLoaded() const
    {
        return m_Manager != nullptr;
    }

    std::string HoudiniLicenseManager::SanitizePath(const std::string& path)
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

    std::string HoudiniLicenseManager::SanitizeKey(const std::string& key)
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

    std::string HoudiniLicenseManager::SanitizeOfflineLicense(const std::string& offlineLicense)
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

    std::string HoudiniLicenseManager::GetKeyPath(LicensePathType pathType)
    {
        std::vector<std::string> potentialPaths;

        switch (pathType)
        {
        case LicensePathType::EnvVar:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "ZIBRAVDB_LICENSE_KEY");
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

    std::string HoudiniLicenseManager::GetOfflineLicensePath(LicensePathType pathType)
    {
        std::vector<std::string> potentialPaths;

        switch (pathType)
        {
        case LicensePathType::EnvVar:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "ZIBRAVDB_OFFLINE_LICENSE");
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

    std::string HoudiniLicenseManager::GetLicenseServerAddressPath(LicensePathType pathType)
    {
        std::vector<std::string> potentialPaths;

        switch (pathType)
        {
        case LicensePathType::EnvVar:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "ZIBRAVDB_LICENSE_SERVER");
            break;
        case LicensePathType::UserPrefDir:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_HOUDINI_USER_PREF_DIR, "HOUDINI_USER_PREF_DIR");
            Helpers::AppendToPath(potentialPaths, ms_DefaultLicenseServerFileName);
            break;
        case LicensePathType::HSite:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_HSITE, "HSITE");
            Helpers::AppendToPath(potentialPaths, ms_DefaultLicenseServerFileName);
            break;
        case LicensePathType::HQRoot:
            potentialPaths = Helpers::GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "HQROOT");
            Helpers::AppendToPath(potentialPaths, ms_DefaultLicenseServerFileName);
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

    std::string HoudiniLicenseManager::ReadKeyFromFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return "";
        }

        std::string licenseKey((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return SanitizeKey(licenseKey);
    }

    std::string HoudiniLicenseManager::ReadOfflineLicenseFromFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return "";
        }

        std::string offlineLicense((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return SanitizeOfflineLicense(offlineLicense);
    }

    std::string HoudiniLicenseManager::ReadLicenseServerAddressFromFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return "";
        }

        std::string licenseServerAddress((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        licenseServerAddress.erase(0, licenseServerAddress.find_first_not_of(" \r\n\t_"));
        licenseServerAddress.erase(licenseServerAddress.find_last_not_of(" \r\n\t_") + 1);
        return licenseServerAddress;
    }

    bool HoudiniLicenseManager::IsLicenseValid(Product product) const
    {
        return m_Status[size_t(product)] == Status::OK;
    }

    HoudiniLicenseManager::Status HoudiniLicenseManager::TryCheckoutLicense(ActivationType type, LicensePathType pathType)
    {
        if (!LoadLicenseManager())
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

            m_LicenseKey = "";
            m_OfflineLicense = offlineLicense;
            m_LicenseServerAddress = "";

            m_Manager->CheckoutLicenseOffline(offlineLicense.c_str(), static_cast<int>(offlineLicense.size()));

            if (m_Manager->IsLicenseValidated(CE::Licensing::ProductType::Compression) ||
                m_Manager->IsLicenseValidated(CE::Licensing::ProductType::Decompression))
            {
                SetStatusFromZibraVDBRuntime();
                m_Type = ActivationType::Offline;
                m_LicensePathType = pathType;
                m_LicensePath = offlineLicensePath;
                return Status::OK;
            }

            m_ActivationError = m_Manager->GetLicenseError();

            return Status::ValidationError;
        }
        case ActivationType::LicenseServer: {
            std::string licenseServerAddressPath = GetLicenseServerAddressPath(pathType);
            if (licenseServerAddressPath.empty())
            {
                return Status::NoLicense;
            }
            std::string licenseServerAddress = ReadLicenseServerAddressFromFile(licenseServerAddressPath);
            if (licenseServerAddress.empty())
            {
                return Status::NoLicense;
            }

            m_LicenseKey = "";
            m_OfflineLicense = "";
            m_LicenseServerAddress = licenseServerAddress;

            m_Manager->CheckoutLicenseLicenseServer(licenseServerAddress.c_str());

            if (m_Manager->IsLicenseValidated(CE::Licensing::ProductType::Compression) ||
                m_Manager->IsLicenseValidated(CE::Licensing::ProductType::Decompression))
            {
                SetStatusFromZibraVDBRuntime();
                m_Type = ActivationType::LicenseServer;
                m_LicensePathType = pathType;
                m_LicensePath = licenseServerAddressPath;
                return Status::OK;
            }

            m_ActivationError = m_Manager->GetLicenseError();

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

            m_LicenseKey = licenseKey;
            m_OfflineLicense = "";
            m_LicenseServerAddress = "";

            m_Manager->CheckoutLicenseWithKey(licenseKey.c_str());
            if (m_Manager->IsLicenseValidated(CE::Licensing::ProductType::Compression) ||
                m_Manager->IsLicenseValidated(CE::Licensing::ProductType::Decompression))
            {
                SetStatusFromZibraVDBRuntime();
                m_Type = ActivationType::Online;
                m_LicensePathType = pathType;
                m_LicensePath = licenseKeyPath;
                return Status::OK;
            }

            m_ActivationError = m_Manager->GetLicenseError();

            return Status::ValidationError;
        }
        default:
            assert(0);
            return Status::NoLicense;
        }
    }

    void HoudiniLicenseManager::SetStatusFromZibraVDBRuntime()
    {
        if (!LoadLicenseManager())
        {
            return;
        }

        for (size_t i = 0; i < size_t(Product::Count); ++i)
        {
            m_Status[i] = m_Manager->IsLicenseValidated(CE::Licensing::ProductType(i)) ? Status::OK : Status::ValidationError;
        }
    }

    void HoudiniLicenseManager::SetStatusForAllProducts(Status status)
    {
        for (size_t i = 0; i < size_t(Product::Count); ++i)
        {
            m_Status[i] = status;
        }
    }

} // namespace Zibra