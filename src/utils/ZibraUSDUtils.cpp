#include "PrecompiledHeader.h"
#include "ZibraUSDUtils.h"
#include <iostream>

namespace Zibra::Utils::USD
{
    void SearchUpstreamForSOPNode(OP_Node* markerNode, fpreal t, std::function<void(SOP_Node*, fpreal)> extractVDBCallback)
    {
        if (!markerNode)
            return;

        std::set<OP_Node*> visitedNodes;
        SearchUpstreamForSOPNodesRecursive(markerNode, t, visitedNodes, extractVDBCallback);
    }

    void TraverseSOPNodes(OP_Node* lop_node, fpreal t, std::function<void(SOP_Node*, fpreal)> extractVDBCallback)
    {
        if (!lop_node)
            return;
        if (lop_node->getOperator()->getName().equal("sopcreate") ||
            lop_node->getOperator()->getName().equal("sopimport"))
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

        if (!lop_node->getOperator()->getName().equal("sopcreate") &&
            !lop_node->getOperator()->getName().equal("sopimport") &&
            lop_node->isNetwork())
        {
            auto* network = static_cast<OP_Network*>(lop_node);
            for (int i = 0; i < network->getNchildren(); ++i)
            {
                OP_Node* child = network->getChild(i);
                TraverseSOPNodes(child, t, extractVDBCallback);
            }
        }
    }

    void SearchUpstreamForSOPNodesRecursive(OP_Node* node, fpreal t, std::set<OP_Node*>& visitedNodes, std::function<void(SOP_Node*, fpreal)> extractVDBCallback)
    {
        if (!node || visitedNodes.find(node) != visitedNodes.end())
            return;
            
        visitedNodes.insert(node);
        
        // Check if this node is a SOP Create node
        if (node->getOperator()->getName().equal("sopcreate") ||
            node->getOperator()->getName().equal("sopimport"))
        {
            std::cout << "[ZibraVDB] Found SOP Create node upstream: " << node->getName().toStdString() << std::endl;
            TraverseSOPNodes(node, t, extractVDBCallback);
            return; // Found what we're looking for, no need to go further upstream
        }
        
        // Search through all inputs of this node
        for (int i = 0; i < node->nInputs(); ++i)
        {
            OP_Node* input = node->getInput(i);
            if (input)
            {
                SearchUpstreamForSOPNodesRecursive(input, t, visitedNodes, extractVDBCallback);
            }
        }
    }

    ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker* findMarkerNodeByName(const std::string& nodeName)
    {
        OP_Node* rootNode = OPgetDirector()->findNode("/stage");
        if (!rootNode || !rootNode->isNetwork())
        {
            return nullptr;
        }

        auto stageNetwork = static_cast<OP_Network*>(rootNode);
        OP_Node* node = stageNetwork->findNode(nodeName.c_str());
        if (node && dynamic_cast<ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker*>(node))
        {
            return static_cast<ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker*>(node);
        }

        return nullptr;
    }

} // namespace Zibra::Utils::USD