#include "PrecompiledHeader.h"

#include "Helpers.h"
#include <regex>

namespace Zibra::Helpers
{
    std::vector<std::string> GetHoudiniEnvironmentVariable(UT_StrControl envVarEnum, const char* envVarName)
    {
        std::vector<std::string> result;
        const char* envVarHoudini = nullptr;
        if (envVarEnum != ENV_MAX_STR_CONTROLS)
        {
            envVarHoudini = UT_EnvControl::getString(envVarEnum);
            if (envVarHoudini != nullptr)
            {
                result.push_back(envVarHoudini);
            }
        }

        const char* envVarSTL = std::getenv(envVarName);
        if (envVarSTL != nullptr)
        {
            if (envVarHoudini == nullptr || strcmp(envVarHoudini, envVarSTL) != 0)
            {
                result.push_back(envVarSTL);
            }
        }
        return result;
    }

    void AppendToPath(std::vector<std::string>& pathsToModify, const std::string& relativePath)
    {
        for (std::string& path : pathsToModify)
        {
            std::filesystem::path fsPath(path);
            fsPath /= relativePath;
            path = fsPath.string();
        }
    }

    void OpenInBrowser(std::string url)
    {
        // Opens the URL in the browser via Python
        PYrunPythonStatementsAndExpectNoErrors(("import webbrowser\n"
                                                "webbrowser.open('" +
                                                url + "')")
                                                   .c_str(),
                                               "Failed to open URL in browser");
    }

    void OpenInFileExplorer(std::string path)
    {
#if ZIB_TARGET_OS_LINUX
        // Have to use xdg-open for Linux
        // Python codepath opens folder in default browser on Linux
        std::system(("xdg-open \"" + path + "\"").c_str());
#else
        // Opens path in default file explorer via Python
        PYrunPythonStatementsAndExpectNoErrors(("import pathlib\n"
                                                "import webbrowser\n"
                                                "path = pathlib.Path(\"" +
                                                path +
                                                "\").resolve()\n"
                                                "webbrowser.open(path.as_uri())")
                                                   .c_str(),
                                               "Failed to open folder in file explorer");
#endif
    }

    Zibra::RHI::GFXAPI SelectGFXAPI()
    {
        // Selects the graphics API based on the environment variable "ZIBRAVDB_FOR_HOUDINI_FORCE_GRAPHICS_API"
        // And limits the selection to the available graphics APIs

        // Available APIs based on OS:
        // Windows\/ Linux\/ Mac\/
        // DX12   +       -     -
        // Vulkan +       +     -
        // Metal  -       -     +

        std::vector<std::string> envVar = GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "ZIBRAVDB_FOR_HOUDINI_FORCE_GRAPHICS_API");
        if (envVar.empty())
        {
            // Auto means automatic selection for the OS
            // Windows = DX12, Linux = Vulkan, Mac = Metal
            return Zibra::RHI::GFXAPI::Auto;
        }

        std::string envVarValueUpper = envVar[0];
        std::transform(envVarValueUpper.begin(), envVarValueUpper.end(), envVarValueUpper.begin(), ::toupper);

        Zibra::RHI::GFXAPI result = Zibra::RHI::GFXAPI::Auto;
        if (envVarValueUpper == "D3D12" || envVarValueUpper == "DX12")
        {
            result = Zibra::RHI::GFXAPI::D3D12;
        }
        else if (envVarValueUpper == "VULKAN")
        {
            result = Zibra::RHI::GFXAPI::Vulkan;
        }
        else if (envVarValueUpper == "METAL")
        {
            result = Zibra::RHI::GFXAPI::Metal;
        }

// Filter result based on the available graphics APIs
#if ZIB_TARGET_OS_WIN
        if (result != Zibra::RHI::GFXAPI::D3D12 && result != Zibra::RHI::GFXAPI::Vulkan)
        {
            return Zibra::RHI::GFXAPI::Auto;
        }
#elif ZIB_TARGET_OS_LINUX
        if (result != Zibra::RHI::GFXAPI::Vulkan)
        {
            return Zibra::RHI::GFXAPI::Auto;
        }
#elif ZIB_TARGET_OS_MAC
        if (result != Zibra::RHI::GFXAPI::Metal)
        {
            return Zibra::RHI::GFXAPI::Auto;
        }
#else
#error Unexpected OS
#endif
        return result;
    }

    bool NeedForceSoftwareDevice()
    {
        std::vector<std::string> envVar = GetHoudiniEnvironmentVariable(ENV_MAX_STR_CONTROLS, "ZIBRAVDB_FOR_HOUDINI_FORCE_SOFTWARE_DEVICE");
        if (envVar.empty())
        {
            return false;
        }
        
        std::string envVarValueUpper = envVar[0];
        std::transform(envVarValueUpper.begin(), envVarValueUpper.end(), envVarValueUpper.begin(), ::toupper);
        if (envVarValueUpper == "ON" || envVarValueUpper == "TRUE" || envVarValueUpper == "1")
        {
            return true;
        }
        return false;
    }

    bool ParseZibraVDBURI(const std::string& uri, std::map<std::string, std::string>& keyValuePairs)
    {
        keyValuePairs.clear();

        size_t questionMarkCount = 0;
        size_t questionMarkPos = std::string::npos;
        for (size_t i = 0; i < uri.length(); i++)
        {
            if (uri[i] == '?')
            {
                questionMarkCount++;
                questionMarkPos = i;
                if (questionMarkCount > 1)
                {
                    return false;
                }
            }
        }

        std::string filePath = (questionMarkCount == 0) ? uri : uri.substr(0, questionMarkPos);
        size_t lastDotPos = filePath.find_last_of('.');
        if (lastDotPos == std::string::npos)
        {
            return false;
        }

        std::string extension = filePath.substr(lastDotPos);
        if (extension != ZIB_ZIBRAVDB_EXT)
        {
            return false;
        }

        std::filesystem::path filepath(filePath);
        keyValuePairs["path"] = filepath.parent_path().string();
        keyValuePairs["name"] = filepath.filename().string();

        if (questionMarkCount == 1 && questionMarkPos + 1 < uri.length())
        {
            std::string queryString = uri.substr(questionMarkPos + 1);

            size_t start = 0;
            size_t ampPos;
            do
            {
                ampPos = queryString.find('&', start);
                std::string param = (ampPos == std::string::npos) ?
                    queryString.substr(start) :
                    queryString.substr(start, ampPos - start);

                size_t equalPos = param.find('=');
                if (equalPos != std::string::npos && equalPos > 0 && equalPos + 1 < param.length())
                {
                    const std::string key = param.substr(0, equalPos);
                    const std::string value = param.substr(equalPos + 1);
                    keyValuePairs[key] = value;
                }

                start = ampPos + 1;
            } while (ampPos != std::string::npos);
        }

        return true;
    }

    bool ParseRelSOPNodeParams(const std::string& pathStr, double& t, std::string& extractedPath)
    {
        if (pathStr.compare(0, 4, "op:/") != 0)
        {
            return false;
        }

        // Extract the SOP path from op:/ URL (e.g., "op:/stage/cloud/sopnet/OUT.sop.volumes" -> "/stage/cloud/sopnet")
        extractedPath.clear();
        size_t pathStart = 3; // Skip "op:"
        size_t firstColon = pathStr.find(':', pathStart);
        
        std::string fullPath;
        if (firstColon != std::string::npos) {
            fullPath = pathStr.substr(pathStart, firstColon - pathStart);
        } else {
            fullPath = pathStr.substr(pathStart);
        }
        
        // Remove the last component to get the parent path
        size_t lastSlash = fullPath.find_last_of('/');
        if (lastSlash != std::string::npos) {
            extractedPath = fullPath.substr(0, lastSlash);
        } else {
            extractedPath = fullPath;
        }
        std::regex t_regex(R"([?&]t=([0-9]+(?:\.[0-9]+)?)(?:[&]|$))");
        std::smatch match;
        if (!std::regex_search(pathStr, match, t_regex))
        {
            return false;
        }

        try
        {
            t = std::stod(match[1].str());
        }
        catch (const std::exception&)
        {
            return false;
        }

        return true;
    }

    bool TryParseInt(const std::string& str, int& result)
    {
        if (str.empty())
        {
            return false;
        }

        try
        {
            size_t pos;
            result = std::stoi(str, &pos);
            return pos == str.length();
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    std::string FormatUUIDString(uint64_t uuid[2])
    {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << uuid[0] << std::setw(16) << uuid[1];
        return ss.str();
    }

} // namespace Zibra::Helpers
