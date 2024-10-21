#pragma once

namespace Zibra::NetworkRequest
{
    // Downloads file to disk, creating folder structure if necessary
    bool DownloadFile(const std::string& url, const std::string& filepath);
} // namespace Zibra::NetworkRequest
