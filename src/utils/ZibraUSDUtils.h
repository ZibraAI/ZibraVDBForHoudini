#pragma once

#include <GA/GA_Iterator.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVDB.h>
#include <LOP/LOP_Node.h>
#include <LOP/LOP_ZibraVDBCompressionMarker.h>
#include <OP/OP_Node.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_StringHolder.h>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace Zibra::Utils::USD
{
    void SearchUpstreamForSOPNode(OP_Node* markerNode, fpreal t, std::function<void(SOP_Node*, fpreal)> extractVDBCallback);
    ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker* findMarkerNodeByName(const std::string& nodeName);

    void TraverseSOPNodes(OP_Node* lop_node, fpreal t, std::function<void(SOP_Node*, fpreal)> extractVDBCallback);
    void SearchUpstreamForSOPNodesRecursive(OP_Node* node, fpreal t, std::set<OP_Node*>& visitedNodes, std::function<void(SOP_Node*, fpreal)> extractVDBCallback);
} // namespace Zibra::Utils::USD