#include <string>
#include <vector>
#include <UT/UT_EnvControl.h>
#include <Zibra/RHI.h>

namespace Zibra::Helpers
{
    std::vector<std::string> GetHoudiniEnvironmentVariable(UT_StrControl envVarEnum, const char* envVarName);
    void AppendToPath(std::vector<std::string>& pathsToModify, const std::string& relativePath);
    void OpenInBrowser(std::string url);
    void OpenInFileExplorer(std::string path);
    Zibra::RHI::GFXAPI SelectGFXAPI();
}