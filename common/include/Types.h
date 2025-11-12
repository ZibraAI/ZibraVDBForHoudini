#pragma once

enum class MetaAttributesLoadStatus
{
    FATAL_ERROR_INVALID_METADATA,
    ERROR_PARTIALLY_INVALID_METADATA,
    SUCCESS,
};

struct URI {
    std::string scheme;
    std::string path;
    std::map<std::string, std::string> queryParams;
    bool isValid = false;

    URI() = delete;
    explicit URI(const std::string& URIString);
};
