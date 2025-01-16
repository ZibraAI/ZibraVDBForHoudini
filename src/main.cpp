#include "PrecompiledHeader.h"

#include "GUI/GUI_ZibraVDBRender.h"
#include "ROP/ROP_ZibraVDBCompressor.h"
#include "SOP/SOP_ZibraVDBDecompressor.h"
#include "bridge/CompressionEngine.h"

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

extern "C" SYS_VISIBILITY_EXPORT void newRenderHook(DM_RenderTable* dm_table)
{
    constexpr int priority = 0;

    GA_PrimitiveTypeId type{GA_PRIMNONE};
    dm_table->registerGEOHook(new Zibra::GUI_ZibraVDBRenderHook{}, type, priority, GUI_HOOK_FLAG_NONE);

    // Optional: install a custom display option (or options) for the hook
    dm_table->installGeometryOption("unique_option_name", "Framework Option");
}
