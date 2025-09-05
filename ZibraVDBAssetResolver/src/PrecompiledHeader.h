#pragma once

// Standard library includes needed for resolver
#include <cassert>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <execution>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if __cplusplus >= 202002L
#include <bit>
#endif

// Platform specific includes
#if ZIB_TARGET_OS_WIN
#include <codecvt>
#include <locale>
#define WIN32_LEAN_AND_MEAN
#define NOATOM
#define NOGDI
#define NOGDICAPMASKS
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NORASTEROPS
#define NOSCROLL
#define NOSOUND
#define NOSYSMETRICS
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOCRYPT
#define NOMCX
#undef ERROR
#undef OUT
#elif ZIB_TARGET_OS_LINUX
#include <dlfcn.h>
#include <curl/curl.h>
#elif ZIB_TARGET_OS_MAC
#include <dlfcn.h>
#include <curl/curl.h>
#include <sys/xattr.h>
#else
#error Unexpected OS
#endif

// Houdini includes
#include <UT/UT_String.h>

// OpenVDB includes
#include <openvdb/io/Stream.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/GridTransformer.h>
#include <openvdb/tools/Interpolation.h>

// 3rd party includes
#include <json.hpp>

// Zibra SDK includes
#define ZRHI_NO_STATIC_API_DECL
#include <Zibra/RHI.h>

#define ZCE_NO_STATIC_API_DECL
#include <Zibra/CE/Compression.h>
#include <Zibra/CE/Decompression.h>
#include <Zibra/CE/Licensing.h>

// Project includes (from parent project)
#include "Globals.h"