#pragma once

namespace Zibra
{

#define ZIB_STRINGIFY_INTERNAL(x) #x
#define ZIB_STRINGIFY(x) ZIB_STRINGIFY_INTERNAL(x)

#define ZIB_ZIBRAVDB_VERSION_SHORT "0.2"

    static constexpr const char* ZIBRAVDB_NODES_TAB_NAME = "ZibraVDB, Labs/FX/Pyro";
    static constexpr const char* ZIBRAVDB_ICON_PATH = "zibravdb.svg";

    static constexpr const char* ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED = "Platform is not supported. Currently ZibraVDB "
                                                                                 "is only supported on Windows.";
    static constexpr const char* ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING = "The ZibraVDB core library is not installed. Please "
                                                                                     "first download the core library via the Download "
                                                                                     "Library button on any of the ZibraVDB nodes.";
    static constexpr const char* ZIBRAVDB_ERROR_MESSAGE_LICENSE_ERROR = "License is not verified. Visit "
                                                                        "'https://effects.zibra.ai/zibravdbhoudini' for activation.";
    static constexpr const char* ZIBRAVDB_ERROR_MESSAGE_NO_FILE_SELECTED = "No file was selected to decompress.";
    static constexpr const char* ZIBRAVDB_ERROR_MESSAGE_COULDNT_OPEN_FILE = "File couldn't be open. Please check the file and make sure "
                                                                            "it is valid .zibravdb compressed effect.";
    static constexpr const char* ZIBRAVDB_ERROR_MESSAGE_COULDNT_DECOMPRESS_FRAME = "Couldn't decompress frame. Please verify "
                                                                                   "decompression settings.";
    static constexpr const char* ZIBRAVDB_ERROR_MESSAGE_FRAME_INDEX_OUT_OF_RANGE = "Trying to decompress frame out of range of "
                                                                                   "input sequence.";

    enum class ContextType
    {
        SOP,
        OUT
    };

} // namespace Zibra
