#include "LicenseManager.h"

#include <filesystem>

#include "bridge/LibraryUtils.h"
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

    LicenseManager::Status LicenseManager::GetLicenseStatus() const
    {
        return m_Status;
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
        m_Status = status;
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

        m_Status = Status::Uninitialized;
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

    void LicenseManager::SetLicenseKey(const std::string& key)
    {
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
                file << key;
            }
        }
    }

    void LicenseManager::SetOfflineLicense(const std::string& offlineLicense)
    {
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
                file << offlineLicense;
            }
        }
    }

    bool LicenseManager::CheckLicense()
    {
        if (!IsLicenseValid())
        {
            CheckoutLicense();
        }
        return IsLicenseValid();
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

    bool LicenseManager::IsLicenseValid() const
    {
        return m_Status == Status::OK;
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
            if (CE::Licensing::CAPI::CheckoutLicenseOffline(offlineLicense.c_str()))
            {
                m_Status = Status::OK;
                m_Type = ActivationType::Offline;
                m_LicensePathType = pathType;
                m_LicensePath = offlineLicensePath;
                m_LicenseKey = "";
                m_OfflineLicense = offlineLicense;
                return Status::OK;
            }
            return Status::ActivationError;
            break;
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
            if (CE::Licensing::CAPI::CheckoutLicenseWithKey(licenseKey.c_str()))
            {
                m_Status = Status::OK;
                m_Type = ActivationType::Online;
                m_LicensePathType = pathType;
                m_LicensePath = licenseKeyPath;
                m_LicenseKey = licenseKey;
                m_OfflineLicense = "";
                return Status::OK;
            }
            return Status::ActivationError;
            break;
        }
        default:
            assert(0);
            return Status::NoLicense;
        }
    }

} // namespace Zibra