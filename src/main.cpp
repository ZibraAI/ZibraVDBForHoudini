#include "PrecompiledHeader.h"

// This header must be included exactly once in the plugin!
#include <HUSD/HUSD_OutputProcessor.h>
#include <UT/UT_DSOVersion.h>
#include <bridge/LibraryUtils.h>

#include "LOP/LOP_ZibraVDBCompressionMarker.h"
#include "LOP/ZibraVDBOutputProcessor.h"
#include "ROP/ROP_ZibraVDBCompressor.h"
#include "SOP/SOP_ZibraVDBDecompressor.h"
#include "SOP/SOP_ZibraVDBUSDExport.h"

extern "C" {
    SYS_VISIBILITY_EXPORT void newSopOperator(OP_OperatorTable* table)
    {
        using namespace Zibra;

        table->addOperator(new ZibraVDBCompressor::ROP_ZibraVDBCompressor_Operator(ContextType::SOP));
        table->addOperator(new ZibraVDBDecompressor::SOP_ZibraVDBDecompressor_Operator());
        table->addOperator(new ZibraVDBUSDExport::SOP_ZibraVDBUSDExport_Operator());
    }

    SYS_VISIBILITY_EXPORT void newLopOperator(OP_OperatorTable* table)
    {
        using namespace Zibra;

        //table->addOperator(new ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker_Operator());

        HUSD_OutputProcessorRegistry::get().registerOutputProcessor(ZibraVDBOutputProcessor::OUTPUT_PROCESSOR_NAME,
                                                                    ZibraVDBOutputProcessor::createZibraVDBOutputProcessor);
        Zibra::LibraryUtils::RegisterAssetResolver();
    }

    SYS_VISIBILITY_EXPORT void newDriverOperator(OP_OperatorTable* table)
    {
        using namespace Zibra;

        table->addOperator(new ZibraVDBCompressor::ROP_ZibraVDBCompressor_Operator(ContextType::OUT));
    }
}