#include "PrecompiledHeader.h"

#include "ROP/ROP_ZibraVDBCompressor.h"
#include "SOP/SOP_ZibraVDBDecompressor.h"

extern "C" SYS_VISIBILITY_EXPORT void newSopOperator(OP_OperatorTable* table)
{
    using namespace Zibra;

    table->addOperator(new ZibraVDBCompressor::ROP_ZibraVDBCompressor_Operator(ContextType::SOP));
    table->addOperator(new ZibraVDBDecompressor::SOP_ZibraVDBDecompressor_Operator());
}

extern "C" SYS_VISIBILITY_EXPORT void newDriverOperator(OP_OperatorTable* table)
{
    using namespace Zibra;

    table->addOperator(new ZibraVDBCompressor::ROP_ZibraVDBCompressor_Operator(ContextType::OUT));
}
