#pragma once

// Standard library
#include <csignal>
#include <filesystem>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Houdini includes
#include <SYS/SYS_Hash.h>
#include <SYS/SYS_Types.h>
#include <UT/UT_Exit.h>
#include <UT/UT_StringHolder.h>

// OpenVDB includes
#include <openvdb/openvdb.h>
// OpenVDB MUST BE INCLUED BEFORE USD/PXR!!!
// pxr/pxr.h define _DEBUG, which trips up selection of correct tbb library on windows

// USD/PXR includes
#include "pxr/base/tf/debug.h"
#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/tf/token.h"
#include "pxr/pxr.h"
#include "pxr/usd/ar/defaultResolver.h"
#include "pxr/usd/ar/defineResolver.h"
#include "pxr/usd/ar/defineResolverContext.h"
#include "pxr/usd/ar/filesystemAsset.h"
#include "pxr/usd/ar/filesystemWritableAsset.h"
#include "pxr/usd/ar/notice.h"
#include "pxr/usd/ar/resolver.h"

// Project code
#include "Globals.h"
