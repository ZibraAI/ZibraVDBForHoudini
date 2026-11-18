#include "PrecompiledHeader.h"

#include <HUSD/HUSD_OutputProcessor.h>
// This header must be included exactly once in the plugin!
#include <UT/UT_DSOVersion.h>

#include "LOP/LOP_ZibraVDBImport.h"
#include "LOP/ZibraVDBOutputProcessor.h"
#include "ROP/ROP_ZibraVDBCompressor.h"
#include "SOP/SOP_ZibraVDBDecompressor.h"

extern "C"
{
    //NOLINTBEGIN
    SYS_VISIBILITY_EXPORT void newSopOperator(OP_OperatorTable* table)
    {
        using namespace Zibra;

        table->addOperator(new ZibraVDBCompressor::ROP_ZibraVDBCompressor_Operator(ContextType::SOP));
        table->addOperator(new ZibraVDBDecompressor::SOP_ZibraVDBDecompressor_Operator());
    }

    SYS_VISIBILITY_EXPORT void newLopOperator(OP_OperatorTable* table)
    {
        using namespace Zibra;

        table->addOperator(new ZibraVDBImport::LOP_ZibraVDBImport_Operator());

        HUSD_OutputProcessorRegistry::get().registerOutputProcessor(
            ZibraVDBOutputProcessor::OUTPUT_PROCESSOR_INNER_NAME,
            ZibraVDBOutputProcessor::createZibraVDBOutputProcessor);
    }

    SYS_VISIBILITY_EXPORT void newDriverOperator(OP_OperatorTable* table)
    {
        using namespace Zibra;

        table->addOperator(new ZibraVDBCompressor::ROP_ZibraVDBCompressor_Operator(ContextType::OUT));
    }
    //NOLINTEND
}