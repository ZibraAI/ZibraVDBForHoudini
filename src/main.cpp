#include "PrecompiledHeader.h"

// If not Labs build
#ifndef LABS_BUILD
// And compiling with MSVC
#ifdef _MSC_VER
// And compiling with build tools version higher than 14.39 but lower than 15.0
#if _MSC_VER > 1939 && _MSC_VER < 2000
// We misreport build tools version to be 14.39 to trick older Houdini 20.5 versions into loading our DSO
#undef _MSC_VER
#define _MSC_VER 1939
#endif // _MSC_VER > 1939 && _MSC_VER < 2000
#endif // _MSC_VER
#endif // !LABS_BUILD

// This header must be included exactly once in the plugin!
#include <UT/UT_DSOVersion.h>

#include "ROP/ROP_ZibraVDBCompressor.h"
#include "SOP/SOP_ZibraVDBDecompressor.h"
#include "ROP/ROP_InspectVolume.h"

extern "C" SYS_VISIBILITY_EXPORT void newSopOperator(OP_OperatorTable* table)
{
    using namespace Zibra;

    table->addOperator(new ZibraVDBCompressor::ROP_ZibraVDBCompressor_Operator(ContextType::SOP));
    table->addOperator(new ZibraVDBDecompressor::SOP_ZibraVDBDecompressor_Operator());

#ifdef ZIB_ENABLE_DEBUG_NODES
    table->addOperator(new InspectVolume::ROP_InspectVolume_Operator(ContextType::SOP));
#endif
}

extern "C" SYS_VISIBILITY_EXPORT void newDriverOperator(OP_OperatorTable* table)
{
    using namespace Zibra;

    table->addOperator(new ZibraVDBCompressor::ROP_ZibraVDBCompressor_Operator(ContextType::OUT));

#ifdef ZIB_ENABLE_DEBUG_NODES
    table->addOperator(new InspectVolume::ROP_InspectVolume_Operator(ContextType::OUT));
#endif
}
