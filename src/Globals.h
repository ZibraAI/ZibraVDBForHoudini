#pragma once

namespace Zibra
{

#define ZIB_STRINGIFY_INTERNAL(x) #x
#define ZIB_STRINGIFY(x) ZIB_STRINGIFY_INTERNAL(x)

#define ZIB_ZIBRAVDB_VERSION_SHORT "0.2"

    constexpr auto* ZIBRAVDB_NODES_TAB_NAME = "ZibraVDB, Labs/FX/Pyro";
    constexpr auto* ZIBRAVDB_ICON_PATH = "zibravdb.svg";

    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED = "Platform is not supported. Currently ZibraVDB "
                                                                    "is only supported on Windows and Linux.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING = "The ZibraVDB core library is not installed. Please first download "
                                                                        "the core library via the Download Library button on any of the "
                                                                        "ZibraVDB nodes. (See the node help page for more information.)";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_LIBRARY_NOT_INITIALIZED =
        "Initialization of ZibraVDB failed. Make sure you have supported hardware or software implementation of graphics API installed.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_LICENSE_ERROR = "License is not verified. Visit "
                                                           "'https://effects.zibra.ai/zibravdbhoudini' for activation.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_NO_FILE_SELECTED = "No file was selected to decompress.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_COULDNT_OPEN_FILE = "File couldn't be open. Please check the file and make sure "
                                                               "it is valid .zibravdb compressed effect.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_COULDNT_DECOMPRESS_FRAME = "Couldn't decompress frame. Please verify "
                                                                      "decompression settings.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_FRAME_INDEX_OUT_OF_RANGE = "Trying to decompress frame out of range of "
                                                                      "input sequence.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_NO_LICENSE_AFTER_DOWNLOAD = "Library downloaded successfully, but no valid license found. Visit "
                                                                       "'https://effects.zibra.ai/zibravdbhoudini', set up your license "
                                                                       "and restart Houdini.";

    constexpr auto* ZVDB_MSG_FAILED_TO_DOWNLOAD_LIBRARY = "Failed to download ZibraVDB library.";
    constexpr auto* ZVDB_MSG_LIB_DOWNLOADED_SUCCESSFULLY_INITIALIZATION_FAILED =
        "Library downloaded successfully to $HOUDINI_USER_PREF_DIR/zibra/, but initialization failed. Please make sure you have supported "
        "hardware or software implementation of graphics API installed";
    constexpr auto* ZVDB_MSG_LIB_DOWNLOADED_SUCCESSFULLY_WITH_NO_LICENSE = "Library downloaded successfully to "
                                                                           "$HOUDINI_USER_PREF_DIR/zibra/, "
                                                                           "but no valid license found. Visit "
                                                                           "http://effects.zibra.ai/zibravdbhoudini, set up your license, "
                                                                           "and then restart Houdini. (See the node help page for more "
                                                                           "information.)";
    constexpr auto* ZVDB_MSG_LIB_DOWNLOADED_SUCCESSFULLY_WITH_LICENSE = "Library downloaded successfully to "
                                                                        "$HOUDINI_USER_PREF_DIR/zibra/. "
                                                                        "(See the node help page for more information.)";

    enum class ContextType
    {
        SOP,
        OUT
    };

} // namespace Zibra
