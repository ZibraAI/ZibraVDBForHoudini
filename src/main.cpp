#include "PrecompiledHeader.h"

// This header must be included exactly once in the plugin!
#include <UT/UT_DSOVersion.h>

#include "LOP/LOP_ZibraVDBDecompressor.h"
#include "ROP/ROP_ZibraVDBCompressor.h"
#include "SOP/SOP_ZibraVDBDecompressor.h"

extern "C" {
    SYS_VISIBILITY_EXPORT void newSopOperator(OP_OperatorTable* table)
    {
        using namespace Zibra;

        table->addOperator(new ZibraVDBCompressor::ROP_ZibraVDBCompressor_Operator(ContextType::SOP));
        table->addOperator(new ZibraVDBDecompressor::SOP_ZibraVDBDecompressor_Operator());
    }

    SYS_VISIBILITY_EXPORT void newLopOperator(OP_OperatorTable* table)
    {
        using namespace Zibra;

        table->addOperator(new ZibraVDBDecompressor::LOP_ZibraVDBDecompressor_Operator());

    }

    SYS_VISIBILITY_EXPORT void newDriverOperator(OP_OperatorTable* table)
    {
        using namespace Zibra;

        table->addOperator(new ZibraVDBCompressor::ROP_ZibraVDBCompressor_Operator(ContextType::OUT));
    }

    /*SYS_VISIBILITY_EXPORT void newGeometryPrim(OP_OperatorTable* table)
    {
        using namespace Zibra;

        ZibraVDBPluginRegistry::GetInstance().Initialize();
    }*/
}