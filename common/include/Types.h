#pragma once

enum class MetaAttributesLoadStatus
{
    FATAL_ERROR_INVALID_METADATA,
    ERROR_PARTIALLY_INVALID_METADATA,
    SUCCESS,
};

struct ParsedZibraURI {
    std::filesystem::path filepath;
    std::string configurationNode;
    int frame = -1;
    bool isZibraVDB = false;
    bool isValid = false;
};