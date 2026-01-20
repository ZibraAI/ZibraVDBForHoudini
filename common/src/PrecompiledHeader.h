#pragma once

// Standard library
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if ZIB_TARGET_OS_WIN
#include <codecvt>
#include <locale>
#endif

// Houdini includes
#include <GA/GA_Attribute.h>
#include <GA/GA_AttributeType.h>
#include <GA/GA_Handle.h>
#include <GA/GA_Iterator.h>
#include <GA/GA_Types.h>
#include <GEO/GEO_Primitive.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVDB.h>
#include <LOP/LOP_Node.h>
#include <OP/OP_Context.h>
#include <OP/OP_Node.h>
#include <PY/PY_Python.h>
#include <ROP/ROP_Node.h>
#include <SI/AP_Interface.h>
#include <SOP/SOP_Node.h>
#include <SYS/SYS_Types.h>
#include <UI/UI_Value.h>
#include <UT/UT_EnvControl.h>
#include <UT/UT_Error.h>
#include <UT/UT_IStream.h>
#include <UT/UT_JSONHandle.h>
#include <UT/UT_JSONParser.h>
#include <UT/UT_JSONValueMap.h>
#include <UT/UT_StringHolder.h>
#include <tools/henv.h>

// OpenVDB includes
#include <openvdb/io/Stream.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/GridTransformer.h>
#include <openvdb/tools/Interpolation.h>

// 3rd party includes
#include <json.hpp>

// Platform specific includes
#if ZIB_TARGET_OS_WIN
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
#include <winhttp.h>
#include <winuser.h>
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

#define ZRHI_NO_STATIC_API_DECL
#include <Zibra/RHI.h>

#define ZCE_NO_STATIC_API_DECL
#include <Zibra/CE/Compression.h>
#include <Zibra/CE/Decompression.h>
#include <Zibra/CE/Licensing.h>
#include <Zibra/CE/Literals.h>

// Project code
#include "Globals.h"