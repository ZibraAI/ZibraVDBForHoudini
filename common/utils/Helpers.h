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
    constexpr const char* URI_PATH_PARAM = "__uri_path";
    constexpr const char* URI_NAME_PARAM = "__uri_name";
    constexpr const char* URI_NODE_PARAM = "node";
    constexpr const char* URI_FRAME_PARAM = "frame";
    
    bool IsZibraVDBExtension(const std::string& uri);
    std::unordered_map<std::string, std::string> ParseZibraVDBPath(const std::string& uri);
    std::unordered_map<std::string, std::string> ParseRelSOPNodeParams(const std::string& pathStr/*, double& t, std::string& extractedPath*/);

    // Number parsing
    bool TryParseInt(const std::string& str, int& result);

    // UUID formatting
    std::string FormatUUIDString(uint64_t uuid[2]);
} // namespace Zibra::Helpers