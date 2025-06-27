#pragma once

#include <OP/OP_Node.h>
#include <SOP/SOP_Node.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVDB.h>
#include <GA/GA_Iterator.h>
#include <LOP/LOP_Node.h>
#include <UT/UT_StringHolder.h>
#include <vector>
#include <set>
#include <string>
#include <functional>

namespace Zibra::Utils
{
    class ZibraUSDUtils
    {
    public:
        static void DetectCompressionMarkerNodes(OP_Node* lop_node, OP_Node* config_node, std::vector<OP_Node*>& markerNodes);
        static void SearchUpstreamForSOPCreate(OP_Node* markerNode, fpreal t, std::function<void(SOP_Node*, fpreal)> extractVDBCallback);
        static void TraverseSOPCreateNodes(OP_Node* lop_node, fpreal t, std::function<void(SOP_Node*, fpreal)> extractVDBCallback);

    private:
        static void searchForMarkerNodesRecursive(OP_Node* node, std::vector<OP_Node*>& markerNodes, std::set<OP_Node*>& visitedNodes);
        static void searchUpstreamForSOPCreateRecursive(OP_Node* node, fpreal t, std::set<OP_Node*>& visitedNodes, std::function<void(SOP_Node*, fpreal)> extractVDBCallback);
    };

} // namespace Zibra::Utils