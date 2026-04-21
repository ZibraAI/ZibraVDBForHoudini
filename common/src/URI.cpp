#include "PrecompiledHeader.h"

#include "URI.h"

#include "utils/Helpers.h"

namespace Zibra
{

    URI::URI(const std::string& URIString)
    {
        const size_t questionMarkPos = URIString.find('?');
        if (questionMarkPos != std::string::npos && URIString.find('?', questionMarkPos + 1) != std::string::npos)
        {
            // Valid URI can't have more than one '?' character
            return;
        }

        const std::string pathPart = questionMarkPos == std::string::npos ? URIString : URIString.substr(0, questionMarkPos);
        const size_t schemePos = pathPart.find("://");

        if (schemePos != std::string::npos)
        {
            scheme = pathPart.substr(0, schemePos);
            path = pathPart.substr(schemePos + 3);
        }
        else
        {
            path = pathPart;
        }

        if (path.empty())
        {
            return;
        }

        if (questionMarkPos != std::string::npos && questionMarkPos + 1 < URIString.length())
        {
            std::string queryString = URIString.substr(questionMarkPos + 1);
            queryParams = Zibra::Helpers::ParseQueryParamsString(queryString);
        }

        isValid = true;
    }

    std::string URI::ToString() const
    {
        std::string result;
        if (!scheme.empty())
        {
            result = scheme + "://";
        }
        result += path;

        if (!queryParams.empty())
        {
            result += "?";
            bool first = true;
            for (const auto& [key, value] : queryParams)
            {
                if (!first)
                {
                    result += "&";
                    first = false;
                }
                result += key + "=" + value;
            }
        }
        return result;
    }

} // namespace Zibra
