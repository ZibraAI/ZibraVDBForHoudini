#include "PrecompiledHeader.h"

#include "utils/Helpers.h"

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

    std::map<std::string, std::string> ParseQueryParamsString(const std::string& queryString)
    {
        std::map<std::string, std::string> result;

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
                result.insert({key, value});
            }

            start = ampPos + 1;
        } while (ampPos != std::string::npos);
        
        return result;
    }

    bool IsZibraVDBURI(const URI& assetURI)
    {
        return assetURI.path.has_extension() && assetURI.path.extension() == ZIB_ZIBRAVDB_EXT;
    }

    bool IsZibraVDBURI(const std::string& assetPath)
    {
        const URI assetURI(assetPath);
        return assetURI.isValid ? IsZibraVDBURI(assetURI) : false;
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

URI::URI(const std::string& URIString)
{
    const size_t questionMarkPos = URIString.find('?');
    if (questionMarkPos != std::string::npos && URIString.find('?', questionMarkPos + 1) != std::string::npos)
    {
        // Valid URI can't have more than one '?' character
        return;
    }

    const std::string pathPart = questionMarkPos == std::string::npos ? URIString : URIString.substr(0, questionMarkPos);
    const size_t colonPos = pathPart.find(':');
    
    if (colonPos != std::string::npos)
    {
        scheme = pathPart.substr(0, colonPos);
        path = std::filesystem::path(pathPart.substr(colonPos + 1));
    }
    else
    {
        path = std::filesystem::path(pathPart);
    }

    if (questionMarkPos != std::string::npos && questionMarkPos + 1 < URIString.length())
    {
        std::string queryString = URIString.substr(questionMarkPos + 1);
        queryParams = Zibra::Helpers::ParseQueryParamsString(queryString);
    }

    isValid = true;
}