#include "PrecompiledHeader.h"

// This header must be included exactly once in the plugin!
#include <HUSD/HUSD_OutputProcessor.h>
#include <UT/UT_DSOVersion.h>
#include <bridge/LibraryUtils.h>

#include "LOP/LOP_ZibraVDBCompressionMarker.h"
#include "LOP/ZibraVDBOutputProcessor.h"
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

        table->addOperator(new ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker_Operator());

        // Register the output processor
        std::cout << "[ZibraVDB] Registering output processor: " << ZibraVDBOutputProcessor::OUTPUT_PROCESSOR_NAME << std::endl;
        HUSD_OutputProcessorRegistry::get().registerOutputProcessor(
            ZibraVDBOutputProcessor::OUTPUT_PROCESSOR_NAME,
            ZibraVDBOutputProcessor::createZibraVDBOutputProcessor
        );
        std::cout << "[ZibraVDB] Output processor registration complete" << std::endl;

        Zibra::LibraryUtils::RegisterAssetResolver();
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