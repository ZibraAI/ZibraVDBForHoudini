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

    // Parses a ZibraVDB URI and returns structured information
    // Format: path/file.zibravdb?frame=N&node=configNode
    //
    // Returns ParsedZibraURI with:
    // - isZibraVDB: true if URI has .zibravdb extension
    // - isValid: true if URI is properly formatted (only if isZibraVDB=true)
    // - filepath: path to the .zibravdb file
    // - frame: frame number from query parameter (default -1)
    // - configurationNode: node parameter from query string
    //
    // Examples:
    // - "file.zibravdb" -> isZibraVDB=true, isValid=true, frame=-1
    // - "file.zibravdb?frame=5" -> isZibraVDB=true, isValid=true, frame=5
    // - "file.zibravdb?frame=abc" -> isZibraVDB=true, isValid=false (malformatted)
    // - "file.vdb" -> isZibraVDB=false, isValid=false
    ParsedZibraURI ParseZibraVDBURI(const std::string& uri)
    {
        ParsedZibraURI result;
        result.isZibraVDB = IsZibraVDBExtension(uri);
        
        if (!result.isZibraVDB)
        {
            return result;
        }

        const size_t questionMarkPos = uri.find('?');
        if (uri.find('?', questionMarkPos + 1) != std::string::npos)
        {
            // More than one '?' char in uri - malformatted
            return result;
        }
        
        try
        {
            const std::string uriPath = (questionMarkPos == std::string::npos) ? uri : uri.substr(0, questionMarkPos);
            result.filepath = std::filesystem::path(uriPath);
            
            // Parse query parameters if they exist
            if (questionMarkPos != std::string::npos && questionMarkPos + 1 < uri.length())
            {
                std::string queryString = uri.substr(questionMarkPos + 1);
                auto queryParams = ParseQueryParamsString(queryString);
                
                // Extract frame parameter - validate format
                auto frameIt = queryParams.find("frame");
                if (frameIt != queryParams.end())
                {
                    if (!TryParseInt(frameIt->second, result.frame))
                    {
                        // Invalid frame format - malformatted but still ZibraVDB
                        return result;
                    }
                }
                
                // Extract node parameter
                auto nodeIt = queryParams.find("node");
                if (nodeIt != queryParams.end())
                {
                    result.configurationNode = nodeIt->second;
                }
            }
            
            // If we get here, parsing was successful
            result.isValid = true;
        }
        catch (const std::exception&)
        {
            // Exception during parsing - malformatted but still ZibraVDB
            return result;
        }

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
