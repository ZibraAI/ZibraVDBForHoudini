#include <string>
#include <vector>
#include <UT/UT_EnvControl.h>
#include <Zibra/RHI.h>

namespace Zibra::Helpers
{
    // Depending on houdini version and specific env var
    // Either way of querying environmnt variables can return incorrect value
    // But not both at the same time
    // So we return up to 2 values, so user can try both values
    std::vector<std::string> GetHoudiniEnvironmentVariable(UT_StrControl envVarEnum, const char* envVarName);
    // This method can be used for env vars that don't have corresponding UT_StrControl value
    std::optional<std::string> GetNormalEnvironmentVariable(const char* envVarName);

    void AppendToPath(std::vector<std::string>& pathsToModify, const std::string& relativePath);

    // Actions to perform
    void OpenInBrowser(const std::string& url);
    void OpenInFileExplorer(const std::filesystem::path& path);

    // Querying options set via environment variables
    Zibra::RHI::GFXAPI SelectGFXAPI();
    bool NeedForceSoftwareDevice();
}