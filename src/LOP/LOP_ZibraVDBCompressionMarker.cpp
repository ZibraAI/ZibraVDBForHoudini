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
#include <HUSD/HUSD_ConfigureLayer.h>
#include <HUSD/HUSD_FindPrims.h>
#include <LOP/LOP_PRMShared.h>
#include <PRM/PRM_ConditionalType.h>
#include <PRM/PRM_Conditional.h>
#include <iostream>
#include <regex>
#include <filesystem>
#include <algorithm>
#include <cctype>

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::ZibraVDBCompressionMarker
{
    // Parameter names and defaults
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

        // Get the current time for other parameter evaluation
        fpreal t = context.getTime();
        
        // Build the save path using parameters
        std::string outputDir = getOutputDirectory(t);
        std::string outputFilename = getOutputFilename(t);
        
        std::string layerName = getName().toStdString();
        std::transform(layerName.begin(), layerName.end(), layerName.begin(), ::tolower);

        // Update the filename field to show nodename.usda
        std::string updatedFilename = layerName + ".usda";
        setString(updatedFilename.c_str(), CH_STRING_LITERAL, PRMoutputFilenameName.getToken(), 0, t);

        std::string layerPath = outputDir + "/" + updatedFilename;
        std::string layerPathRaw = getOutputDirectoryRaw(t) + "/" + updatedFilename;

        // Use editableDataHandle to get write access to our data handle
        HUSD_AutoWriteLock writelock(editableDataHandle());
        HUSD_AutoLayerLock layerlock(writelock);
        
        // Get all layers in the stage to check for upstream save paths
        auto stage = writelock.data()->stage();
        if (stage) {
            auto layerStack = stage->GetLayerStack();
            for (auto& layer : layerStack) {
                std::string identifier = layer->GetIdentifier();
                // Check if this layer has a save path set by upstream nodes
                if (!identifier.empty() && identifier != layer->GetRealPath()) {
                    std::cout << "[ZibraVDB] Found upstream layer with save path: " << identifier << std::endl;

                    // Check if this is a SOP import or SOP create node
                    if (identifier.find("op:/obj/") == 0 && identifier.find(".sop") != std::string::npos) {
                        // Extract the node path from op:/obj/network/node.sop
                        size_t start = 7; // Skip "op:/obj/"
                        size_t end = identifier.find(".sop");
                        if (end != std::string::npos) {
                            std::string nodePath = identifier.substr(start, end - start);

                            // Get the actual node to check its type
                            OP_Node* sopNode = OPgetDirector()->findNode(("/obj/" + nodePath).c_str());
                            if (sopNode) {
                                std::string nodeType = sopNode->getOperator()->getName().toStdString();
                                std::cout << "[ZibraVDB] Found SOP node type: " << nodeType << " at path: " << nodePath << std::endl;

                                // This is the SOP node being referenced, now find the LOP SOP Import node
                                std::cout << "[ZibraVDB] Found referenced SOP: " << nodeType << " at " << nodePath << std::endl;
                                
                                // Now find the LOP SOP Import node that's importing from this SOP
                                // We need to traverse the LOP network to find the SOP Import node
                                OP_Network* lopNetwork = dynamic_cast<OP_Network*>(getParent());
                                if (lopNetwork) {
                                    // Look through all nodes in the LOP network
                                    for (int i = 0; i < lopNetwork->getNchildren(); i++) {
                                        OP_Node* child = lopNetwork->getChild(i);
                                        if (child && child->getOperator()->getName() == "sopimport") {
                                            // Check if this SOP Import references our SOP
                                            UT_String sopPath;
                                            child->evalString(sopPath, "soppath", 0, 0);
                                            
                                            if (sopPath.toStdString().find(nodePath) != std::string::npos) {
                                                std::cout << "[ZibraVDB] Found LOP SOP Import node: " << child->getFullPath() << std::endl;

                                                PRM_Parm* enableSavePathParm = child->getParmPtr("enable_savepath");
                                                if (enableSavePathParm) {
                                                    child->setInt("enable_savepath", 0, 0, 1);
                                                    std::cout << "[ZibraVDB] Disabled enable_savepath on SOP Import" << std::endl;
                                                }
                                                PRM_Parm* savePathParm = child->getParmPtr("savepath");
                                                if (savePathParm) {
                                                    child->setString(UT_String(layerPathRaw), CH_STRING_LITERAL, "savepath", 0, 0);
                                                    std::cout << "[ZibraVDB] Cleared savepath on SOP Import" << std::endl;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // Also handle anonymous layers that might be from LOP nodes
                    else if (identifier.find("anon:") == 0) {
                        // Skip anonymous layers - these are typically system layers
                        std::cout << "[ZibraVDB] Skipping anonymous layer: " << identifier << std::endl;
                    }
                }
            }
        }
        
        // Also search for SOP Create nodes directly since they don't show up in layer identifiers
        OP_Network* lopNetwork = dynamic_cast<OP_Network*>(getParent());
        if (lopNetwork) {
            std::cout << "[ZibraVDB] Searching for SOP Create nodes directly..." << std::endl;
            for (int i = 0; i < lopNetwork->getNchildren(); i++) {
                OP_Node* child = lopNetwork->getChild(i);
                if (child && child->getOperator()->getName() == "sopcreate") {
                    std::cout << "[ZibraVDB] Found SOP Create node: " << child->getFullPath() << std::endl;
                    
                    PRM_Parm* enableSavePathParm = child->getParmPtr("enable_savepath");
                    if (enableSavePathParm) {
                        child->setInt("enable_savepath", 0, 0, 1);
                        std::cout << "[ZibraVDB] Disabled enable_savepath on SOP Import" << std::endl;
                    }
                    PRM_Parm* savePathParm = child->getParmPtr("savepath");
                    if (savePathParm) {
                        child->setString(UT_String(layerPathRaw), CH_STRING_LITERAL, "savepath", 0, 0);
                        std::cout << "[ZibraVDB] Cleared savepath on SOP Import" << std::endl;
                    }
                }
            }
        }

//        // Configure the layer for saving to the specified path
//        HUSD_ConfigureLayer configure_layer(writelock);
//
//        // Try to configure both active layer and root layer
//        configure_layer.setModifyRootLayer(true);
//        configure_layer.setSavePath(layerPath.c_str(), false); // false = don't flatten
//
//        // Also try to set save control to override upstream settings
//        configure_layer.setSaveControl("all");
//
//        configure_layer.setModifyRootLayer(false);
//        configure_layer.setSavePath(layerPath.c_str(), false);
//
//        std::cout << "[ZibraVDB] Configured layer save to: " << layerPath << std::endl;

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

} // namespace Zibra::ZibraVDBCompressionMarker