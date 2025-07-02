#include "PrecompiledHeader.h"
#include "LOP_ZibraVDBCompressionMarker.h"
#include <HUSD/HUSD_DataHandle.h>
#include <HUSD/XUSD_Data.h>
#include <UT/UT_StringHolder.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/references.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/layer.h>
#include <HUSD/XUSD_Utils.h>
#include <HUSD/HUSD_FindPrims.h>
#include <LOP/LOP_PRMShared.h>
#include <PRM/PRM_Conditional.h>
#include <algorithm>
#include <cctype>

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::ZibraVDBCompressionMarker
{
    static PRM_Name PRMoutputDirName("output_dir", "Output Directory");
    static PRM_Default PRMoutputDirDefault(0.0f, "$HIP/ZibraAI");
    
    static PRM_Name PRMoutputFilenameName("output_filename", "Output Filename");
    static PRM_Default PRMoutputFilenameDefault(0.0f, "$OS");
    
    static PRM_Name PRMremoveOriginalName("remove_original", "Remove Original Files");
    static PRM_Default PRMremoveOriginalDefault(0);
    
    static PRM_Name PRMqualityName("quality", "Compression Quality");
    static PRM_Default PRMqualityDefault(0.6f);  // Default quality of 0.6
    static PRM_Range PRMqualityRange(PRM_RANGE_RESTRICTED, 0.0f, PRM_RANGE_RESTRICTED, 1.0f);

    static PRM_Name prm_label_primitives_enable("enable_primitives", "");
    static PRM_Default prm_default_enable(1);

    OP_Node* LOP_ZibraVDBCompressionMarker::Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new LOP_ZibraVDBCompressionMarker{net, name, op};
    }

    PRM_Template* LOP_ZibraVDBCompressionMarker::GetTemplateList() noexcept
    {
        static PRM_Template templateList[] = {
            PRM_Template(PRM_TOGGLE, 1, &prm_label_primitives_enable, &prm_default_enable),
            PRM_Template(PRM_STRING, 1, new PRM_Name("soppath", "SOP Path"), nullptr, nullptr, nullptr, 0, &PRM_SpareData::sopPath),
            PRM_Template(PRM_STRING, 1, &PRMoutputDirName, &PRMoutputDirDefault),
            PRM_Template(PRM_STRING, 1, &PRMoutputFilenameName, &PRMoutputFilenameDefault),
            PRM_Template(PRM_FLT_J, 1, &PRMqualityName, &PRMqualityDefault, 0, &PRMqualityRange),
            PRM_Template(PRM_TOGGLE, 1, &PRMremoveOriginalName, &PRMremoveOriginalDefault),
            PRM_Template()
        };
        return templateList;
    }

    LOP_ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : LOP_Node(net, name, entry)
    {
    }

    OP_ERROR LOP_ZibraVDBCompressionMarker::cookMyLop(OP_Context &context)
    {
        if (cookModifyInput(context) >= UT_ERROR_FATAL)
            return error();

        fpreal t = context.getTime();
        std::string outputDir = getOutputDirectory(t);
        std::string outputFilename = getOutputFilename(t);
        std::string layerName = getName().toStdString();
        std::transform(layerName.begin(), layerName.end(), layerName.begin(), ::tolower);
        std::string updatedFilename = layerName + ".usda";
        setString(updatedFilename.c_str(), CH_STRING_LITERAL, PRMoutputFilenameName.getToken(), 0, t);
        std::string layerPath = outputDir + "/" + updatedFilename;
        std::string layerPathRaw = getOutputDirectoryRaw(t) + "/" + updatedFilename;

        HUSD_AutoWriteLock writelock(editableDataHandle());
        HUSD_AutoLayerLock layerlock(writelock);
        auto stage = writelock.data()->stage();

        auto lopNetwork = dynamic_cast<OP_Network*>(getParent());
        if (lopNetwork) {
            for (int i = 0; i < lopNetwork->getNchildren(); i++) {
                OP_Node* child = lopNetwork->getChild(i);
                if (child && (child->getOperator()->getName() == "sopcreate" || child->getOperator()->getName() == "sopimport")) {
                    if (isNodeUpstream(child)) {
                        PRM_Parm* enableSavePathParm = child->getParmPtr("enable_savepath");
                        if (enableSavePathParm) {
                            child->setInt("enable_savepath", 0, 0, 1);
                        }
                        PRM_Parm* savePathParm = child->getParmPtr("savepath");
                        if (savePathParm) {
                            child->setString(UT_String(layerPathRaw), CH_STRING_LITERAL, "savepath", 0, 0);
                        }
                        break;
                    }
                }
            }
        }

//        HUSD_ConfigureLayer configure_layer(writelock);
//        configure_layer.setSavePath(layerPath.c_str(), false);

        return error();
    }


    bool LOP_ZibraVDBCompressionMarker::updateParmsFlags()
    {
        bool changed = LOP_Node::updateParmsFlags();
        flags().setTimeDep(true);
        return changed;
    }
    
    std::string LOP_ZibraVDBCompressionMarker::getOutputDirectory(fpreal t) const
    {
        UT_String outputDir;
        evalString(outputDir, "output_dir", 0, t);
        return outputDir.toStdString();
    }

    std::string LOP_ZibraVDBCompressionMarker::getOutputDirectoryRaw(fpreal t) const
    {
        UT_String outputDir;
        evalStringRaw(outputDir, "output_dir", 0, t);
        return outputDir.toStdString();
    }
    
    std::string LOP_ZibraVDBCompressionMarker::getOutputFilename(fpreal t) const
    {
        UT_String outputFilename;
        evalString(outputFilename, "output_filename", 0, t);
        return outputFilename.toStdString();
    }
    
    float LOP_ZibraVDBCompressionMarker::getCompressionQuality(fpreal t) const
    {
        return static_cast<float>(evalFloat("quality", 0, t));
    }
    
    bool LOP_ZibraVDBCompressionMarker::getRemoveOriginalFiles(fpreal t) const
    {
        return evalInt("remove_original", 0, t) != 0;
    }
    
    bool LOP_ZibraVDBCompressionMarker::isNodeUpstream(OP_Node* node) const
    {
        //TODO redo
        if (!node) return false;
        
        std::function<bool(OP_Node*, OP_Node*, int)> checkUpstream = [&](OP_Node* current, OP_Node* target, int depth) -> bool {
            if (!current || depth > 100) return false;
            if (current == target) return true;
            
            for (unsigned int i = 0; i < current->nInputs(); i++) {
                OP_Node* input = current->getInput(i);
                if (input && checkUpstream(input, target, depth + 1)) {
                    return true;
                }
            }
            return false;
        };
        
        return checkUpstream(const_cast<LOP_ZibraVDBCompressionMarker*>(this), node, 0);
    }

} // namespace Zibra::ZibraVDBCompressionMarker