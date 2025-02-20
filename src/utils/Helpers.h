#include <string>
#include <vector>
#include <UT/UT_EnvControl.h>

namespace Zibra::Helpers
{
    std::vector<std::string> GetHoudiniEnvironmentVariable(UT_StrControl envVarEnum, const char* envVarName);
    void AppendToPath(std::vector<std::string>& pathsToModify, const std::string& relativePath);
}