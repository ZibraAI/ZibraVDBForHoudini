#include <UT/UT_EnvControl.h>
#include <Zibra/RHI.h>

#include "Types.h"

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
    std::map<std::string, std::string> ParseQueryParamsString(const std::string& queryString);
    inline bool IsZibraVDBURI(const URI& assetURI)
    {
        return assetURI.path.has_extension() && assetURI.path.extension() == ZIB_ZIBRAVDB_EXT;
    }
    
    inline bool IsZibraVDBURI(const std::string& assetPath)
    {
        const URI assetURI(assetPath);
        return assetURI.isValid ? IsZibraVDBURI(assetURI) : false;
    }

    // Number parsing
    bool TryParseInt(const std::string& str, int& result);

    // UUID formatting
    std::string FormatUUIDString(uint64_t uuid[2]);
} // namespace Zibra::Helpers