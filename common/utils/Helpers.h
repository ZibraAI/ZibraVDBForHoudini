#include <string>
#include <vector>
#include <unordered_map>
#include <UT/UT_EnvControl.h>
#include <Zibra/RHI.h>

namespace Zibra::Helpers
{
    // Querying environment variables
    std::vector<std::string> GetHoudiniEnvironmentVariable(UT_StrControl envVarEnum, const char* envVarName);    
    void AppendToPath(std::vector<std::string>& pathsToModify, const std::string& relativePath);

    // Actions to perform
    void OpenInBrowser(std::string url);
    void OpenInFileExplorer(std::string path);

    // Querying options set via environment variables
    Zibra::RHI::GFXAPI SelectGFXAPI();
    bool NeedForceSoftwareDevice();

    // URI parsing
    bool ParseZibraVDBURI(const std::string& uri, std::unordered_map<std::string, std::string>& keyValuePairs);
}