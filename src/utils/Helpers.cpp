#include "Helpers.h"

#include <PY/PY_Python.h>
#include <cstdlib>
#include <filesystem>

#include "ui/MessageBox.h"

namespace Zibra::Helpers
{
    std::vector<std::string> GetHoudiniEnvironmentVariable(UT_StrControl envVarEnum, const char* envVarName)
    {
        std::vector<std::string> result;
        const char* envVarHoudini = nullptr;
        if (envVarEnum < ENV_MAX_STR_CONTROLS)
        {
            envVarHoudini = UT_EnvControl::getString(envVarEnum);
            if (envVarHoudini != nullptr)
            {
                result.push_back(envVarHoudini);
            }
        }

        const char* envVarSTL = HoudiniGetenv(envVarName);
        if (envVarSTL != nullptr)
        {
            if (envVarHoudini == nullptr || strcmp(envVarHoudini, envVarSTL) != 0)
            {
                result.push_back(envVarSTL);
            }
        }
        return result;
    }

    std::optional<std::string> GetNormalEnvironmentVariable(const char* envVarName)
    {
        const char* result = HoudiniGetenv(envVarName);
        if (result == nullptr)
        {
            return std::nullopt;
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

    void OpenInBrowser(const std::string& url)
    {
        std::string pythonCode = "import webbrowser\n"
                                 "webbrowser.open('" +
                                 url + "')";
        PYrunPythonStatementsInNewContextAndExpectNoErrors(pythonCode.c_str());
    }

    void OpenInFileExplorer(const std::filesystem::path& path)
    {
        std::string normalizedPath;
        try
        {
            normalizedPath = path.generic_string();
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            UI::MessageBox::Show(UI::MessageBox::Type::OK, "Failed to parse path to open in explorer");
            return;
        }

#if ZIB_TARGET_OS_LINUX
        // Have to use xdg-open for Linux
        // Python codepath opens folder in default browser on Linux
        std::string command = "xdg-open \"" + normalizedPath + "\"";
        std::system(command.c_str());
#else
        std::string pythonCode = "import pathlib\n"
                                 "import webbrowser\n"
                                 "path = pathlib.Path(\"" +
                                 normalizedPath +
                                 "\").resolve()\n"
                                 "webbrowser.open(path.as_uri())";
        // Opens path in default file explorer via Python
        PYrunPythonStatementsInNewContextAndExpectNoErrors(pythonCode.c_str());
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

        std::optional<std::string> envVar = GetNormalEnvironmentVariable("ZIBRAVDB_FOR_HOUDINI_FORCE_GRAPHICS_API");
        if (!envVar.has_value())
        {
            // Auto means automatic selection for the OS
            // Windows = DX12, Linux = Vulkan, Mac = Metal
            return Zibra::RHI::GFXAPI::Auto;
        }

        std::string envVarValueUpper = envVar.value();
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
        std::optional<std::string> envVar = GetNormalEnvironmentVariable("ZIBRAVDB_FOR_HOUDINI_FORCE_SOFTWARE_DEVICE");
        if (!envVar.has_value())
        {
            return false;
        }

        std::string envVarValueUpper = envVar.value();
        std::transform(envVarValueUpper.begin(), envVarValueUpper.end(), envVarValueUpper.begin(), ::toupper);
        if (envVarValueUpper == "ON" || envVarValueUpper == "TRUE" || envVarValueUpper == "1")
        {
            return true;
        }
        return false;
    }

} // namespace Zibra::Helpers
