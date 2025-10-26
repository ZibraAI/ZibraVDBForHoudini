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
    struct ParsedZibraURI {
        std::filesystem::path filepath;
        std::string configurationNode;
        int frame = -1;
        bool isZibraVDB = false;
        bool isValid = false;
    };
    
    std::map<std::string, std::string> ParseQueryParamsString(const std::string& queryString);
    ParsedZibraURI ParseZibraVDBURI(const std::string& uri);

    // Number parsing
    bool TryParseInt(const std::string& str, int& result);

    // UUID formatting
    std::string FormatUUIDString(uint64_t uuid[2]);
} // namespace Zibra::Helpers