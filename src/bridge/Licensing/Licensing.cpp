    #include "Licensing.h"
    
namespace Zibra::LicenseManager
{
    static std::string GetKeyViaEnv()
    {
        const char* licenseKeyPath = std::getenv("ZIBRAVDB_LICENSE_KEY");
        if (licenseKeyPath == nullptr)
        {
            return "";
        }

        // Remove whitespace and " characters from the beginning and the end of the licenseKeyPath
        std::string licenseKeyPathStripped(licenseKeyPath);
        licenseKeyPathStripped.erase(0, licenseKeyPathStripped.find_first_not_of(" \n\r\t\""));
        licenseKeyPathStripped.erase(licenseKeyPathStripped.find_last_not_of(" \n\r\t\"") + 1);

        std::ifstream file(licenseKeyPathStripped);
        if (!file.is_open())
        {
            return "";
        }

        std::string licenseKey((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        // Remove whitespace characters from the beginning and the end of the licenseKey
        licenseKey.erase(0, licenseKey.find_first_not_of(" \n\r\t"));
        licenseKey.erase(licenseKey.find_last_not_of(" \n\r\t") + 1);

        return licenseKey;
    }

    static std::string GetKeyViaUSER_PREF_DIR()
    {
        const char* userPrefDir = std::getenv("HOUDINI_USER_PREF_DIR");
        if (userPrefDir == nullptr)
        {
            return "";
        }
        const std::string licenseKeyPath = std::string(userPrefDir) + "/zibravdb_license_key.txt";
        std::ifstream file(licenseKeyPath);
        if (!file.is_open())
        {
            return "";
        }

        std::string licenseKey((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return licenseKey;
    }

    static std::string GetKeyViaHSITE()
    {
        const char* hSiteDir = std::getenv("HSITE");
        if (hSiteDir == nullptr)
        {
            return "";
        }
        const std::string licenseKeyPath = std::string(hSiteDir) + "/zibravdb_license_key.txt";
        std::ifstream file(licenseKeyPath);
        if (!file.is_open())
        {
            return "";
        }

        std::string licenseKey((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return licenseKey;
    }

    std::string GetKey()
    {
        std::string result = GetKeyViaEnv();
        if (!result.empty())
        {
            return result;
        }
        result = GetKeyViaUSER_PREF_DIR();
        if (!result.empty())
        {
            return result;
        }
        result = GetKeyViaHSITE();
        return result;
    }

    static std::string GetOfflineLicenseViaEnv()
    {
        const char* offlineLicensePath = std::getenv("ZIBRAVDB_OFFLINE_LICENSE");
        if (offlineLicensePath == nullptr)
        {
            return "";
        }
        // Remove whitespace and " characters from the beginning and the end of the licenseKeyPath
        std::string offlineLicensePathStripped(offlineLicensePath);
        offlineLicensePathStripped.erase(0, offlineLicensePathStripped.find_first_not_of(" \n\r\t\""));
        offlineLicensePathStripped.erase(offlineLicensePathStripped.find_last_not_of(" \n\r\t\"") + 1);

        std::ifstream file(offlineLicensePathStripped);
        if (!file.is_open())
        {
            return "";
        }

        std::string offlineLicense((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return offlineLicense;
    }

    static std::string GetOfflineLicenseViaUSER_PREF_DIR()
    {
        const char* userPrefDir = std::getenv("HOUDINI_USER_PREF_DIR");
        if (userPrefDir == nullptr)
        {
            return "";
        }
        const std::string offlineLicensePath = std::string(userPrefDir) + "/zibravdb_offline_license.txt";
        std::ifstream file(offlineLicensePath);
        if (!file.is_open())
        {
            return "";
        }
        std::string offlineLicense((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return offlineLicense;
    }

    static std::string GetOfflineLicenseViaHSITE()
    {
        const char* hSiteDir = std::getenv("HSITE");
        if (hSiteDir == nullptr)
        {
            return "";
        }
        const std::string offlineLicensePath = std::string(hSiteDir) + "/zibravdb_offline_license.txt";
        std::ifstream file(offlineLicensePath);
        if (!file.is_open())
        {
            return "";
        }
        std::string offlineLicense((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return offlineLicense;
    }

    std::string GetOfflineLicense()
    {
        std::string result = GetOfflineLicenseViaEnv();
        if (!result.empty())
        {
            return result;
        }
        result = GetOfflineLicenseViaUSER_PREF_DIR();
        if (!result.empty())
        {
            return result;
        }
        result = GetOfflineLicenseViaHSITE();
        return result;
    }

} // namespace Zibra::LicenseManager