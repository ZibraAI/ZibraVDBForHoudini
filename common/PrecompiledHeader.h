#pragma once

// Standard library includes
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
#include <winhttp.h>
#include <winuser.h>
#undef ERROR
#undef OUT
#elif ZIB_TARGET_OS_LINUX
#include <dlfcn.h>
#include <curl/curl.h>
#elif ZIB_TARGET_OS_MAC
#define SSE2NEON_H
#include <dlfcn.h>
#include <curl/curl.h>
#include <sys/xattr.h>
#else
#error Unexpected OS
#endif

// Houdini includes (common to all components)
#include <UT/UT_String.h>
#include <UT/UT_Assert.h>
#include <UT/UT_Exit.h>
#include <UT/UT_IOTable.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_OFStream.h>
#include <UT/UT_StringHolder.h>

// Additional Houdini includes for main project
#include <CH/CH_LocalVariable.h>
#include <CMD/CMD_Manager.h>
#include <GA/GA_Attribute.h>
#include <GA/GA_AttributeType.h>
#include <GA/GA_GBMacros.h>
#include <GEO/GEO_PrimPoly.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVDB.h>
#include <OP/OP_AutoLockInputs.h>
#include <OP/OP_Director.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <PRM/PRM_Conditional.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_Parm.h>
#include <PRM/PRM_ParmList.h>
#include <PRM/PRM_SpareData.h>
#include <PRM/PRM_TemplateBuilder.h>
#include <ROP/ROP_Error.h>
#include <ROP/ROP_Node.h>
#include <ROP/ROP_Templates.h>
#include <SOP/SOP_Node.h>
#include <SYS/SYS_Math.h>

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

// Project includes
#include "Globals.h"