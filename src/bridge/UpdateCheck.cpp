#include "UpdateCheck.h"

#include <UT/UT_IStream.h>
#include <UT/UT_JSONHandle.h>
#include <UT/UT_JSONParser.h>

#include "LibraryUtils.h"
#include "networking/NetworkRequest.h"

namespace Zibra::UpdateCheck
{
    class ZibraJsonVersionHandle : public UT_JSONHandleNull
    {
    public:
        bool jsonKey(UT_JSONParser& p, const char* v, int64 len) final
        {
            m_Key.assign(v, len);
            return true;
        };
        bool jsonString(UT_JSONParser& p, const char* v, int64 len) final
        {
            m_Value.assign(v, len);
            return true;
        };

        std::string GetVersion()
        {
            if (m_Key == "version")
            {
                return m_Value;
            }
            return "";
        }

    private:
        std::string m_Key;
        std::string m_Value;
    };

    std::optional<LibraryUtils::Version> ParseStringVersion(const std::string& versionString)
    {
        std::vector<uint32_t> versionNumbers;
        versionNumbers.push_back(0);
        for (char c : versionString)
        {
            if (c == '.')
            {
                versionNumbers.push_back(0);
            }
            else
            {
                versionNumbers.back() = versionNumbers.back() * 10 + (c - '0');
            }
        }

        if (versionNumbers.size() != 3)
        {
            return std::nullopt;
        }

        return LibraryUtils::Version{versionNumbers[0], versionNumbers[1], versionNumbers[2], 0};
    }

    bool IsVersionLatest(const LibraryUtils::Version& currentVersion, const LibraryUtils::Version& latestVersion)
    {
        if (latestVersion.major > currentVersion.major)
        {
            return false;
        }
        if (latestVersion.major < currentVersion.major)
        {
            return true;
        }
        if (latestVersion.minor > currentVersion.minor)
        {
            return false;
        }
        if (latestVersion.minor < currentVersion.minor)
        {
            return true;
        }
        if (latestVersion.patch > currentVersion.patch)
        {
            return false;
        }
        return true;
    }

    Status RunReal() noexcept
    {
        // Get the current version of the application
        LibraryUtils::Version currentVersion = LibraryUtils::GetZibSDKVersion();
        // Get the latest version via web request
        std::string latestVersionJson = NetworkRequest::Get(
            "https://generation.zibra.ai/api/pluginVersion?effect=zibravdb_" ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING
            "&engine=houdini&sku=pro");

        if (latestVersionJson.empty())
        {
            return Status::UpdateCheckFailed;
        }

        // Parse latestVersion as JSON
        UT_IStream jsonStream(latestVersionJson.data(), latestVersionJson.size(), UT_ISTREAM_ASCII);
        ZibraJsonVersionHandle latestVersionJsonHandler;

        UT_JSONParser parser;
        parser.parseObject(latestVersionJsonHandler, &jsonStream);

        std::string latestVersionString = latestVersionJsonHandler.GetVersion();
        auto latestVersionOpt = ParseStringVersion(latestVersionString);
        if (!latestVersionOpt)
        {
            return Status::UpdateCheckFailed;
        }

        LibraryUtils::Version latestVersion = *latestVersionOpt;
        return IsVersionLatest(currentVersion, latestVersion) ? Status::Latest : Status::UpdateAvailable;
    }

    Status Run() noexcept
    {
        using clock_t = std::chrono::system_clock;
        using time_point_t = std::chrono::time_point<clock_t>;

        static Status cachedStatus = Status::Count;
        static time_point_t lastCheckTime;

        LibraryUtils::LoadZibSDKLibrary();
        if (!LibraryUtils::IsZibSDKLoaded())
        {
            return Status::NotInstalled;
        }

        // Throttle update check request to max once every 5 minutes
        time_point_t now = clock_t::now();
        if (cachedStatus != Status::Count && now - lastCheckTime < std::chrono::minutes(5))
        {
            return cachedStatus;
        }

        cachedStatus = RunReal();
        lastCheckTime = now;
        return cachedStatus;
    }

} // namespace Zibra::UpdateCheck
