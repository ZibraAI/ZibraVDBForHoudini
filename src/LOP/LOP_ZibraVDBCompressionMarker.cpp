#include "PrecompiledHeader.h"
#include "LOP_ZibraVDBCompressionMarker.h"
#include <HUSD/HUSD_DataHandle.h>
#include <LOP/LOP_Error.h>

namespace Zibra::ZibraVDBCompressionMarker
{
    OP_Node* LOP_ZibraVDBCompressionMarker::Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new LOP_ZibraVDBCompressionMarker{net, name, op};
    }

    PRM_Template* LOP_ZibraVDBCompressionMarker::GetTemplateList() noexcept
    {
        static PRM_Template templateList[] = {
            PRM_Template()  // Empty template list - no parameters
        };
        return templateList;
    }

    LOP_ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : LOP_Node(net, name, entry)
    {
    }

    OP_ERROR LOP_ZibraVDBCompressionMarker::cookMyLop(OP_Context& context)
    {
        // Make this node time-dependent so it cooks every frame
        flags().setTimeDep(true);
        
        // Simple marker node - pass through input stage unchanged
        // The output processor will detect this node in the chain
        if (nInputs() > 0)
        {
            HUSD_DataHandle inputHandle = lockedInputData(context, 0);
            if (inputHandle.hasData())
            {
                // Pass through input stage unchanged
                HUSD_DataHandle &dataHandle = editableDataHandle();
                dataHandle = inputHandle;
            }
            else
            {
                addError(LOP_MESSAGE, "Input data handle is empty. Cannot mark for compression.");
                return error();
            }
        }
        else
        {
            addError(LOP_MESSAGE, "ZibraVDBCompressionMarker node requires at least one input");
            return error();
        }
        
        return error();
    }

    bool LOP_ZibraVDBCompressionMarker::updateParmsFlags()
    {
        bool changed = LOP_Node::updateParmsFlags();
        flags().setTimeDep(true);
        return changed;
    }

} // namespace Zibra::ZibraVDBCompressionMarker