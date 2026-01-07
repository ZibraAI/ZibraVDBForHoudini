#include "PrecompiledHeader.h"

#include "OSInfo.h"

namespace Zibra::Analytics
{

#ifdef ZIB_TARGET_OS_WIN
    std::string GetWindowsVersion()
    {
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (!ntdll)
        {
            return "";
        }

        typedef int32_t(WINAPI * PFN_RtlGetVersion)(OSVERSIONINFOEX*);
        PFN_RtlGetVersion RtlGetVersion = (PFN_RtlGetVersion)GetProcAddress(ntdll, "RtlGetVersion");
        if (!RtlGetVersion)
        {
            return "";
        }

        OSVERSIONINFOEX versionInfo = {sizeof(versionInfo)};
        // Documentation says it always returns success.
        RtlGetVersion(&versionInfo);

        int requiredSize =
            snprintf(nullptr, 0, "%d.%d.%d", versionInfo.dwMajorVersion, versionInfo.dwMinorVersion, versionInfo.dwBuildNumber);
        std::string version(requiredSize, '\0');
        snprintf(version.data(), version.size() + 1, "%d.%d.%d", versionInfo.dwMajorVersion, versionInfo.dwMinorVersion,
                 versionInfo.dwBuildNumber);
        return version;
    }
#endif

#ifdef ZIB_TARGET_OS_LINUX
    std::string StringString(const std::string& str)
    {
        size_t start = str.find_first_not_of(" \t\n\v\f\r\"");
        size_t end = str.find_last_not_of(" \t\n\v\f\r\"");
        if (start == std::string::npos || end == std::string::npos)
        {
            return "";
        }
        return str.substr(start, end - start + 1);
    }

    std::string GetLinuxVersion()
    {
        std::ifstream file("/etc/os-release");
        if (!file.is_open())
        {
            return "";
        }

        std::string name;
        std::string version;
        std::string line;

        while (std::getline(file, line))
        {
            if (line.find("NAME=") == 0)
            {
                name = StringString(line.substr(6));
            }
            else if (line.find("VERSION=") == 0)
            {
                version = StringString(line.substr(9));
            }
        }

        if (name.empty())
        {
            return "";
        }

        if (version.empty())
        {
            return name;
        }

        return name + " " + version;
    }
#endif

#ifdef ZIB_TARGET_OS_MAC
    std::string GetMacOSVersion()
    {
        std::string result;
        size_t resultSize = 0;
        int res = sysctlbyname("kern.osproductversion", nullptr, &resultSize, nullptr, 0);
        if (res != 0)
        {
            return "";
        }

        result.resize(resultSize);
        res = sysctlbyname("kern.osproductversion", &result[0], &resultSize, nullptr, 0);
        if (res != 0)
        {
            return "";
        }
        // Remove the trailing null character
        result.resize(resultSize - 1);
        return result;
    }
#endif

    OSInfo GetOSInfo()
    {
        OSInfo result;
#if ZIB_TARGET_OS_WIN
        result.Name = "Windows";
        result.Version = GetWindowsVersion();
#elif ZIB_TARGET_OS_LINUX
        result.Name = "Linux";
        result.Version = GetLinuxVersion();
#elif ZIB_TARGET_OS_MAC
        result.Name = "macOS";
        result.Version = GetMacOSVersion();
#else
#error "Unsupported OS"
#endif
        return result;
    }

} // namespace Zibra::Analytics
