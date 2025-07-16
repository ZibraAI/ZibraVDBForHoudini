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
#include <vector>
#include <iostream>
#include "utils/ZibraUSDUtils.h"

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
        flags().setTimeDep(true);

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

        findAndWireVDBConvertNodes(t);

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
    
    float LOP_ZibraVDBCompressionMarker::getCompressionQuality() const
    {
        return static_cast<float>(evalFloat("quality", 0, 0));
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

    void LOP_ZibraVDBCompressionMarker::findAndWireVDBConvertNodes(fpreal t)
    {
        std::cout << "[ZibraVDB] Starting findAndWireVDBConvertNodes for node: " << getName() << std::endl;
        
        Zibra::Utils::USD::SearchUpstreamForSOPNode(this, t, [this](SOP_Node* sopNode, fpreal t) {
            if (!sopNode) {
                std::cout << "[ZibraVDB] No SOP node found" << std::endl;
                return;
            }
            
            std::cout << "[ZibraVDB] Found SOP node: " << sopNode->getName() << std::endl;
            
            auto* sopNetwork = dynamic_cast<OP_Network*>(sopNode->getParent());
            if (!sopNetwork) {
                std::cout << "[ZibraVDB] SOP node has no parent network" << std::endl;
                return;
            }
            
            std::cout << "[ZibraVDB] SOP network: " << sopNetwork->getName() << std::endl;
            
            std::vector<OP_Node*> vdbConvertNodes;
            findVDBConvertNodesInNetwork(sopNetwork, vdbConvertNodes);
            
            std::cout << "[ZibraVDB] Found " << vdbConvertNodes.size() << " VDBConvert nodes" << std::endl;
            
            for (OP_Node* vdbConvertNode : vdbConvertNodes) {
                std::cout << "[ZibraVDB] Processing VDBConvert node: " << vdbConvertNode->getName() << std::endl;
                OP_Node* nodeToWire = this;
                wireNodeBeforeVDBConvert(vdbConvertNode, nodeToWire);
            }
        });
        
        std::cout << "[ZibraVDB] Finished findAndWireVDBConvertNodes" << std::endl;
    }

    void LOP_ZibraVDBCompressionMarker::findVDBConvertNodesInNetwork(OP_Network* network, std::vector<OP_Node*>& vdbConvertNodes)
    {
        if (!network) {
            std::cout << "[ZibraVDB] Network is null" << std::endl;
            return;
        }
        
        std::cout << "[ZibraVDB] Searching network: " << network->getName() << " with " << network->getNchildren() << " children" << std::endl;
        
        for (int i = 0; i < network->getNchildren(); i++) {
            OP_Node* child = network->getChild(i);
            if (child) {
                std::cout << "[ZibraVDB] Checking child node: " << child->getName() << " (type: " << child->getOperator()->getName() << ")" << std::endl;
                
                if (child->getOperator()->getName().equal("convertvdb")) {
                    std::cout << "[ZibraVDB] Found VDBConvert node: " << child->getName() << std::endl;
                    vdbConvertNodes.push_back(child);
                }
                
                if (child->isNetwork()) {
                    std::cout << "[ZibraVDB] Recursing into network: " << child->getName() << std::endl;
                    findVDBConvertNodesInNetwork(static_cast<OP_Network*>(child), vdbConvertNodes);
                }
            }
        }
    }

    void LOP_ZibraVDBCompressionMarker::wireNodeBeforeVDBConvert(OP_Node* vdbConvertNode, OP_Node* nodeToWire)
    {
        if (!vdbConvertNode) {
            std::cout << "[ZibraVDB] VDBConvert node is null" << std::endl;
            return;
        }
        
        std::cout << "[ZibraVDB] Wiring node before VDBConvert: " << vdbConvertNode->getName() << std::endl;
        
        auto* sopNetwork = dynamic_cast<OP_Network*>(vdbConvertNode->getParent());
        if (!sopNetwork) {
            std::cout << "[ZibraVDB] VDBConvert node has no parent network" << std::endl;
            return;
        }
        
        std::cout << "[ZibraVDB] Parent network: " << sopNetwork->getName() << std::endl;
        
        OP_Node* vdbConvertInput = vdbConvertNode->getInput(0);
        if (vdbConvertInput) {
            std::cout << "[ZibraVDB] VDBConvert input node: " << vdbConvertInput->getName() << std::endl;
            
            UT_String nodeName;
            nodeName.sprintf("%s_zibramarker", vdbConvertNode->getName().c_str());
            
            std::cout << "[ZibraVDB] Creating passthrough node with name: " << nodeName << std::endl;
            
            OP_Node* existingNode = sopNetwork->findNode(nodeName);
            if (existingNode) {
                std::cout << "[ZibraVDB] Node already exists, skipping: " << nodeName << std::endl;
                return;
            }
            
            std::cout << "[ZibraVDB] Attempting to create attribcreate node in network: " << sopNetwork->getFullPath() << std::endl;
            
            OP_Node* passthroughNode = sopNetwork->createNode("attribcreate", nodeName);
            if (passthroughNode) {
                std::cout << "[ZibraVDB] Successfully created attribcreate node: " << passthroughNode->getName() << std::endl;
            } else {
                std::cout << "[ZibraVDB] Failed to create attribcreate node" << std::endl;
            }
            
            if (passthroughNode) {
                std::cout << "[ZibraVDB] Successfully created passthrough node: " << passthroughNode->getName() << std::endl;
                
                passthroughNode->setInput(0, vdbConvertInput);
                vdbConvertNode->setInput(0, passthroughNode);
                
                std::cout << "[ZibraVDB] Connected: " << vdbConvertInput->getName() << " -> " << passthroughNode->getName() << " -> " << vdbConvertNode->getName() << std::endl;
                
                passthroughNode->flags().setDisplay(false);
                passthroughNode->flags().setRender(false);
                passthroughNode->flags().setBypass(false);
                
                // Set up attribcreate node parameters
                if (passthroughNode->hasParm("name1")) {
                    passthroughNode->setString("usdvolumesavepath", CH_STRING_LITERAL, "name1", 0,  0);
                    std::cout << "[ZibraVDB] Set attribute name to 'usdvolumesavepath'" << std::endl;
                }
//                if (passthroughNode->hasParm("class1")) {
//                    passthroughNode->setString("primitive", CH_STRING_LITERAL, "class1", 0, 0);
//                    std::cout << "[ZibraVDB] Set attribute class to 'primitive'" << std::endl;
//                }
//                if (passthroughNode->hasParm("type1")) {
//                    passthroughNode->setString("string", CH_STRING_LITERAL, "type1", 0, 0);
//                    std::cout << "[ZibraVDB] Set attribute type to 'string'" << std::endl;
//                }
//                if (passthroughNode->hasParm("string1")) {
//                    passthroughNode->setString("$HIP/zibra/test_zibra.$F4.vdb", CH_STRING_LITERAL, "string1", 0, 0);
//                    std::cout << "[ZibraVDB] Set string value to '$HIP/zibra/test_zibra.$F4.vdb'" << std::endl;
//                }
                
                std::cout << "[ZibraVDB] Node wiring completed successfully" << std::endl;
            } else {
                std::cout << "[ZibraVDB] Failed to create any passthrough node type" << std::endl;
                std::cout << "[ZibraVDB] Network path: " << sopNetwork->getFullPath() << std::endl;
                std::cout << "[ZibraVDB] Network type: " << sopNetwork->getOperator()->getName() << std::endl;
            }
        } else {
            std::cout << "[ZibraVDB] VDBConvert node has no input" << std::endl;
        }
    }

} // namespace Zibra::ZibraVDBCompressionMarker