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

    bool IsZibraVDBExtension(const std::string& uri)
    {
        const size_t queryPos = uri.find('?');
        std::string pathPart = (queryPos == std::string::npos) ? uri : uri.substr(0, queryPos);
        
        try
        {
            std::filesystem::path path(pathPart);
            return path.extension() == ZIB_ZIBRAVDB_EXT;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    std::unordered_map<std::string, std::string> ParseQueryString(const std::string& queryString)
    {
        std::unordered_map<std::string, std::string> result;

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

    // Parses a ZibraVDB URI with optional query parameters
    // Format: path/file.zibravdb?param1=value1&param2=value2
    //
    // Returns empty map if URI is invalid (wrong extension, multiple '?' chars, or invalid path)
    //
    // Special parameters added automatically:
    // - URI_PATH_PARAM: directory portion of the URI path (before filename)
    // - URI_NAME_PARAM: filename portion of the URI path
    //
    // Note: If duplicate parameters exist in query string, later values override earlier ones
    std::unordered_map<std::string, std::string> ParseZibraVDBPath(const std::string& uri)
    {
        if (!IsZibraVDBExtension(uri))
        {
            return {};
        }

        std::unordered_map<std::string, std::string> result;

        const size_t questionMarkPos = uri.find('?');
        if (uri.find('?', questionMarkPos + 1) != std::string::npos)
        {
            // More than one '?' char in uri
            return {};
        }
        // If found some query params
        if (questionMarkPos != std::string::npos && questionMarkPos + 1 < uri.length())
        {
            std::string queryString = uri.substr(questionMarkPos + 1);
            auto queryParams = ParseQueryString(queryString);
            result.insert(queryParams.begin(), queryParams.end());
        }

        try
        {
            const std::string uriPath = (questionMarkPos == std::string::npos) ? uri : uri.substr(0, questionMarkPos);
            const std::filesystem::path filepath(uriPath);
            result.insert({URI_PATH_PARAM, filepath.parent_path().string()});
            result.insert({URI_NAME_PARAM, filepath.filename().string()});
        }
        catch (const std::exception&)
        {
            return {};
        }

        return result;
    }

    // Parses a Houdini SOP node URI with parameters from "op:/" format
    // Format: op:/path/to/node.sop.volumes:FORMAT_ARGS:param1=value1&param2=value2&t=0.123
    // Example:
    // op:/obj/geo1/zibravdb_configure_usd1.sop.volumes:SDF_FORMAT_ARGS:authormaterialpath=0&globalauthortimesamples=1&pathprefix=/sopimport1&savepath=/home/sskaplun/src/houdini/usd/test/explosion.usda&savepathtimedep=0&setmissingwidths=0.010000&t=0.4166666666666667
    //
    // Returns empty map if URI is invalid (doesn't start with "op:/" or has no parameters after last colon)
    //
    // Special parameters added automatically:
    // - URI_PATH_PARAM: parent path of the SOP node (e.g., "/obj/geo1" from "/obj/geo1/node")
    //
    // Note: Parameters are extracted from the part after the last colon
    // If duplicate parameters exist, later values override earlier ones
    std::unordered_map<std::string, std::string> ParseRelSOPNodeParams(const std::string& pathStr)
    {
        if (pathStr.compare(0, 4, "op:/") != 0)
        {
            return {};
        }

        std::unordered_map<std::string, std::string> result;

        // Find the last colon to get the query string part
        const size_t lastColonPos = pathStr.find_last_of(':');
        if (lastColonPos == std::string::npos || lastColonPos + 1 >= pathStr.length())
        {
            return {};
        }

        const std::string queryString = pathStr.substr(lastColonPos + 1);
        auto queryParams = ParseQueryString(queryString);
        result.insert(queryParams.begin(), queryParams.end());

        // Extract the SOP path (everything between "op:/" and the last colon)
        std::string fullPath = pathStr.substr(3, lastColonPos - 3);
        // Remove the last component to get the parent path
        const size_t lastSlash = fullPath.find_last_of('/');
        result.insert({URI_PATH_PARAM, lastSlash != std::string::npos ? fullPath.substr(0, lastSlash) : fullPath});

        return result;
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
