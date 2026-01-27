#pragma once

struct URI {
    std::string scheme;
    std::string path;
    std::map<std::string, std::string> queryParams;
    bool isValid = false;

    URI() = delete;
    explicit URI(const std::string& URIString);

    std::string ToString() const;
};
