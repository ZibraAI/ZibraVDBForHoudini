#pragma once

#include <string>
#include <utility>
#include <vector>

namespace Zibra::NetworkRequest
{
    enum class Method
    {
        GET,
        POST
    };

    struct Request
    {
        Method method;
        std::string URL;
        std::string userAgent;
        std::string acceptTypes;
        // Only used for POST/PUT requests
        std::string contentType;
        std::vector<std::pair<std::string, std::string>> additionalHeaders;
        // Only used for POST/PUT requests
        std::vector<char> data;
    };

    struct Response
    {
        bool success = false;
        std::string errorMessage;
        int statusCode = 0;
        std::vector<std::pair<std::string, std::string>> headers;
        std::vector<char> data;
    };

    Response Perform(const Request& request);
} // namespace Zibra::NetworkRequest
