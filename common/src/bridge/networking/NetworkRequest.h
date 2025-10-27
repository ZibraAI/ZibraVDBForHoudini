#pragma once

namespace Zibra::NetworkRequest
{
    // Performs GET request
    // Returns response as string
    std::string Get(const std::string& url);
    
    // Performs GET request
    // Saves response to file
    // creating folder structure if necessary
    bool DownloadFile(const std::string& url, const std::string& filepath);
} // namespace Zibra::NetworkRequest
