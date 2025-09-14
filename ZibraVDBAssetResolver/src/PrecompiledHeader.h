#pragma once

// Standard library
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

// Houdini includes (needed by DecompressorManager.h)
#include <SYS/SYS_Types.h>
#include <UT/UT_StringHolder.h>

// OpenVDB includes (needed by asset resolver)
#include <openvdb/openvdb.h>

// USD includes
#include "pxr/base/tf/debug.h"
#include "pxr/base/tf/token.h"
#include "pxr/pxr.h"
#include "pxr/usd/ar/resolver.h"

// Platform specific includes
#if ZIB_TARGET_OS_WIN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef ERROR
#elif ZIB_TARGET_OS_LINUX
#include <dlfcn.h>
#elif ZIB_TARGET_OS_MAC
#include <dlfcn.h>
#else
#error Unexpected OS
#endif

#define ZCE_NO_STATIC_API_DECL
#include <Zibra/CE/Decompression.h>
#include <Zibra/CE/Addons/OpenVDBCommon.h>

#include "Globals.h"