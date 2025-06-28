#include "PrecompiledHeader.h"
#include "ZibraUSDUtils.h"
#include <iostream>

namespace Zibra::Utils
{
    void ZibraUSDUtils::DetectCompressionMarkerNodes(OP_Node* lop_node, OP_Node* config_node, std::vector<OP_Node*>& markerNodes)
    {
        std::cout << "[ZibraVDB] Detecting compression marker nodes..." << std::endl;
        
        // Start from the USD ROP input chain
        OP_Node* startNode = lop_node ? lop_node : config_node;
        if (!startNode)
        {
            std::cout << "[ZibraVDB] No starting node available" << std::endl;
            return;
        }
        
        std::cout << "[ZibraVDB] Starting detection from: " << startNode->getName().toStdString() << std::endl;
        
        // Recursively search through the input chain for marker nodes
        std::set<OP_Node*> visitedNodes;
        searchForMarkerNodesRecursive(startNode, markerNodes, visitedNodes);
        
        std::cout << "[ZibraVDB] Detection complete. Found " << markerNodes.size() << " marker nodes:" << std::endl;
        for (size_t i = 0; i < markerNodes.size(); ++i)
        {
            std::cout << "[ZibraVDB]   " << (i+1) << ". " << markerNodes[i]->getName().toStdString() 
                      << " (" << markerNodes[i]->getOperator()->getName().toStdString() << ")" << std::endl;
        }
    }

    void ZibraUSDUtils::SearchUpstreamForSOPCreate(OP_Node* markerNode, fpreal t, std::function<void(SOP_Node*, fpreal)> extractVDBCallback)
    {
        if (!markerNode)
            return;

        std::cout << "[ZibraVDB] ========== Searching upstream from marker: " << markerNode->getName().toStdString() << " ==========" << std::endl;

        std::set<OP_Node*> visitedNodes;
        searchUpstreamForSOPCreateRecursive(markerNode, t, visitedNodes, extractVDBCallback);
    }

    void ZibraUSDUtils::TraverseSOPCreateNodes(OP_Node* lop_node, fpreal t, std::function<void(SOP_Node*, fpreal)> extractVDBCallback)
    {
        if (!lop_node)
            return;
        if (lop_node->getOperator()->getName().equal("sopcreate"))
        {
            UT_String sopPath;
            std::vector<std::string> paramNames = {
                "soppath", "source", "geometry", "input", "primpattern", "inputgeometry",
                "soppath1", "soppath0", "pathpattern", "sop_path", "objectpath",
                "sourcepath", "rootpath"
            };

            for (const auto& paramName : paramNames)
            {
                if (lop_node->hasParm(paramName.c_str()))
                {
                    lop_node->evalString(sopPath, paramName.c_str(), 0, t);
                    if (sopPath.length() > 0)
                        break;
                }
            }

            if (sopPath.length() > 0)
            {
                OP_Node* referencedNode = lop_node->findNode(sopPath);
                if (referencedNode)
                {
                    SOP_Node* sopNode = nullptr;
                    if (referencedNode->getOpTypeID() == SOP_OPTYPE_ID)
                    {
                        sopNode = static_cast<SOP_Node*>(referencedNode);
                    }
                    else if (referencedNode->isNetwork())
                    {
                        auto* sopNetwork = static_cast<OP_Network*>(referencedNode);
                        OP_Node* outputNode = sopNetwork->getDisplayNodePtr();
                        if (outputNode && outputNode->getOpTypeID() == SOP_OPTYPE_ID)
                        {
                            sopNode = static_cast<SOP_Node*>(outputNode);
                        }
                    }

                    if (sopNode)
                    {
                        extractVDBCallback(sopNode, t);
                        return;
                    }
                }
                else
                {
                    assert(false && "Referenced SOP node not found");
                }
            }
            else
            {
                for (int input_idx = 0; input_idx < lop_node->nInputs(); ++input_idx)
                {
                    OP_Node* inputNode = lop_node->getInput(input_idx);
                    if (inputNode)
                    {
                        if (inputNode->getOpTypeID() == SOP_OPTYPE_ID)
                        {
                            extractVDBCallback(static_cast<SOP_Node*>(inputNode), t);
                            return;
                        }
                        else if (inputNode->isNetwork())
                        {
                            OP_Network* inputNetwork = static_cast<OP_Network*>(inputNode);
                            OP_Node* displayNode = inputNetwork->getDisplayNodePtr();
                            if (displayNode && displayNode->getOpTypeID() == SOP_OPTYPE_ID)
                            {
                                extractVDBCallback(static_cast<SOP_Node*>(displayNode), t);
                                return;
                            }
                            OP_Node* renderNode = inputNetwork->getRenderNodePtr();
                            if (renderNode && renderNode->getOpTypeID() == SOP_OPTYPE_ID)
                            {
                                extractVDBCallback(static_cast<SOP_Node*>(renderNode), t);
                                return;
                            }
                        }
                    }
                }

                if (lop_node->isNetwork())
                {
                    auto* sopCreateNetwork = static_cast<OP_Network*>(lop_node);
                    for (int i = 0; i < sopCreateNetwork->getNchildren(); ++i)
                    {
                        OP_Node* child = sopCreateNetwork->getChild(i);
                        if (child)
                        {
                            if (child->getOpTypeID() == SOP_OPTYPE_ID)
                            {
                                auto* sopChild = static_cast<SOP_Node*>(child);
                                OP_Context context(t);
                                const GU_Detail* testGdp = sopChild->getCookedGeo(context);
                                if (testGdp)
                                {
                                    bool hasVDB = false;
                                    for (GA_Iterator it(testGdp->getPrimitiveRange()); !it.atEnd(); ++it)
                                    {
                                        const GA_Primitive* prim = testGdp->getPrimitive(*it);
                                        if (prim->getTypeId() == GEO_PRIMVDB)
                                        {
                                            hasVDB = true;
                                            break;
                                        }
                                    }

                                    if (hasVDB)
                                    {
                                        extractVDBCallback(sopChild, t);
                                        return;
                                    }
                                }
                            }
                            else if (child->isNetwork())
                            {
                                auto* childNetwork = static_cast<OP_Network*>(child);
                                OP_Node* displayNode = childNetwork->getDisplayNodePtr();
                                if (displayNode && displayNode->getOpTypeID() == SOP_OPTYPE_ID)
                                {
                                    extractVDBCallback(static_cast<SOP_Node*>(displayNode), t);
                                    return;
                                }
                            }
                        }
                    }
                }

                assert(false && "Could not find VDB data in SOP Create node");
            }
        }

        if (!lop_node->getOperator()->getName().equal("sopcreate") && lop_node->isNetwork())
        {
            auto* network = static_cast<OP_Network*>(lop_node);
            for (int i = 0; i < network->getNchildren(); ++i)
            {
                OP_Node* child = network->getChild(i);
                TraverseSOPCreateNodes(child, t, extractVDBCallback);
            }
        }
    }

    void ZibraUSDUtils::searchForMarkerNodesRecursive(OP_Node* node, std::vector<OP_Node*>& markerNodes, std::set<OP_Node*>& visitedNodes)
    {
        if (!node)
            return;
            
        // Check if we've already visited this node to prevent infinite loops
        if (visitedNodes.find(node) != visitedNodes.end())
        {
            return;
        }
        visitedNodes.insert(node);
            
        // Check if this node is a compression marker
        if (node->getOperator()->getName().contains("zibravdb_mark_for_compression"))
        {
            std::cout << "[ZibraVDB] Found marker node: " << node->getName().toStdString() 
                      << " (op: " << node->getOperator()->getName().toStdString() << ")" << std::endl;
            markerNodes.push_back(node);
        }
        
        // ONLY traverse the input chain - don't search through all children of networks
        // This ensures we only find nodes that are actually connected to the USD ROP
        for (int i = 0; i < node->nInputs(); ++i)
        {
            OP_Node* input = node->getInput(i);
            if (input)
            {
                searchForMarkerNodesRecursive(input, markerNodes, visitedNodes);
            }
        }
        
        // Do NOT search through network children - that would find unconnected nodes
        // Only traverse the actual input connections
    }

    void ZibraUSDUtils::searchUpstreamForSOPCreateRecursive(OP_Node* node, fpreal t, std::set<OP_Node*>& visitedNodes, std::function<void(SOP_Node*, fpreal)> extractVDBCallback)
    {
        if (!node || visitedNodes.find(node) != visitedNodes.end())
            return;
            
        visitedNodes.insert(node);
        
        // Check if this node is a SOP Create node
        if (node->getOperator()->getName().equal("sopcreate"))
        {
            std::cout << "[ZibraVDB] Found SOP Create node upstream: " << node->getName().toStdString() << std::endl;
            TraverseSOPCreateNodes(node, t, extractVDBCallback);
            return; // Found what we're looking for, no need to go further upstream
        }
        
        // Search through all inputs of this node
        for (int i = 0; i < node->nInputs(); ++i)
        {
            OP_Node* input = node->getInput(i);
            if (input)
            {
                searchUpstreamForSOPCreateRecursive(input, t, visitedNodes, extractVDBCallback);
            }
        }
    }

} // namespace Zibra::Utils