#pragma once

namespace Zibra
{

#define ZIB_STRINGIFY_INTERNAL(x) #x
#define ZIB_STRINGIFY(x) ZIB_STRINGIFY_INTERNAL(x)

#define ZIB_ZIBRAVDB_VERSION_SHORT "1.0"

#define ZIB_COMPRESSION_ENGINE_MAJOR_VERSION 0

#define ZIB_ZIBRAVDB_EXT ".zibravdb"
#define ZIB_TMP_FILES_FOLDER_NAME "zibravdb_houdini_usd_asset_resolver"
#define ZIB_MAX_CACHED_FRAMES_DEFAULT 2

#define ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING ZIB_STRINGIFY(ZIB_COMPRESSION_ENGINE_MAJOR_VERSION)

    constexpr auto* ZIBRAVDB_NODES_TAB_NAME = "ZibraVDB, Labs/FX/Pyro";
    constexpr auto* ZIBRAVDB_ICON_PATH = "zibravdb.svg";

    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED = "Platform is not supported. Currently ZibraVDB "
                                                                    "is only supported on Windows, Linux and MacOS.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING = "The ZibraVDB core library is not installed. Please first download "
                                                                        "the core library via the Download Library button on any of the "
                                                                        "ZibraVDB nodes. (See the node help page for more information.)";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_LIBRARY_NOT_INITIALIZED =
        "Initialization of ZibraVDB failed. Make sure you have supported hardware or software implementation of graphics API installed.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_LICENSE_ERROR = "License is not verified. Visit "
                                                           "'https://www.zibra.ai/zibravdb-pricing' for activation.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_LICENSE_NO_COMPRESSION = "Your license does not include Compression. Visit "
                                                                    "'https://www.zibra.ai/zibravdb-pricing' for activation.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_NO_FILE_SELECTED = "No file was selected to decompress.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_FILE_NOT_FOUND =
        "Specified file could not be found. Please make sure the file exists and that path is valid.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_COULDNT_DECOMPRESS_FRAME = "Couldn't decompress frame. Please verify "
                                                                      "decompression settings.";
    constexpr auto* ZIBRAVDB_ERROR_MESSAGE_FRAME_INDEX_OUT_OF_RANGE = "Trying to decompress frame out of range of "
                                                                      "input sequence.";

    // Output preprocessor error messages
    constexpr auto* ZIBRAVDB_ERROR_MISSING_NODE_PARAM = "Missing required node parameter in .zibravdb path";
    constexpr auto* ZIBRAVDB_ERROR_EMPTY_NODE_PARAM = "Empty node parameter in .zibravdb path";
    constexpr auto* ZIBRAVDB_ERROR_MISSING_FRAME_PARAM = "Missing required frame parameter in .zibravdb path";
    constexpr auto* ZIBRAVDB_ERROR_EMPTY_FRAME_PARAM = "Empty frame parameter in .zibravdb path";
    constexpr auto* ZIBRAVDB_ERROR_NODE_NOT_FOUND_TEMPLATE = "Could not find SOP node at path: ";
    constexpr auto* ZIBRAVDB_ERROR_WRONG_NODE_TYPE_TEMPLATE = "Node is not a ZibraVDB USD Export node: ";
    constexpr auto* ZIBRAVDB_ERROR_PARSE_FRAME_INDEX_TEMPLATE = "Can't parse frame index from string: ";
    constexpr auto* ZIBRAVDB_ERROR_START_COMPRESSION_TEMPLATE = "Failed to start compression sequence. Status: ";
    constexpr auto* ZIBRAVDB_ERROR_FINISH_COMPRESSION_TEMPLATE = "Failed to finish compression sequence for in-memory VDBs. Status: ";
    constexpr auto* ZIBRAVDB_ERROR_NULL_GEOMETRY = "SOP node returned null geometry";
    constexpr auto* ZIBRAVDB_ERROR_NO_VDB_GRIDS = "No VDB grids found in SOP node";
    constexpr auto* ZIBRAVDB_ERROR_NO_GRIDS_TO_COMPRESS = "No grids to compress";
    constexpr auto* ZIBRAVDB_ERROR_LOAD_FRAME_FAILED = "Failed to load frame from memory grids";
    constexpr auto* ZIBRAVDB_ERROR_COMPRESS_FRAME_FAILED_TEMPLATE = "Frame compressopn failed. Status: ";
    constexpr auto* ZIBRAVDB_ERROR_FRAME_MANAGER_NULL = "Frame compression returned invalid FrameManager";
    constexpr auto* ZIBRAVDB_ERROR_FINISH_FRAME_MANAGER_TEMPLATE = "Failed to finish frame manager. Status: ";
    constexpr auto* ZIBRAVDB_ERROR_CREATE_OUTPUT_DIRECTORY_TEMPLATE = "Failed to create output directory: ";

    constexpr auto* LIBRARY_DOWNLOAD_URL =
        "https://zibra.ai/zibravdb-for-houdini-library-download?version=" ZIB_COMPRESSION_ENGINE_BRIDGE_VERSION_STRING;

    enum class ContextType
    {
        SOP,
        OUT
    };

} // namespace Zibra
