#pragma once

#include <HUSD/HUSD_OutputProcessor.h>
#include <PI/PI_EditScriptedParms.h>
#include <UT/UT_Options.h>
#include <UT/UT_String.h>
#include <map>
#include <openvdb/openvdb.h>
#include <set>
#include <string>
#include <vector>

#include "Globals.h"
#include "LOP_ZibraVDBCompressionMarker.h"
#include "ROP/CompressorManager/CompressorManager.h"
#include "utils/ZibraUSDUtils.h"

// Forward declarations
class SOP_Node;

namespace Zibra::ZibraVDBOutputProcessor
{
    using namespace CE::Compression;

    constexpr const char* OUTPUT_PROCESSOR_NAME = "ZibraVDBCompressor";

    class ZibraVDBOutputProcessor : public HUSD_OutputProcessor
    {
    public:
        ZibraVDBOutputProcessor();
        ~ZibraVDBOutputProcessor() override;

        void beginSave(OP_Node *config_node,
                      const UT_Options &config_overrides,
                      OP_Node *lop_node,
                      fpreal t,
                      const UT_Options &stage_variables) override;

        bool processSavePath(const UT_StringRef &asset_path,
                            const UT_StringRef &referencing_layer_path,
                            bool asset_is_layer,
                            UT_String &newpath,
                            UT_String &error) override;

        bool processReferencePath(const UT_StringRef &asset_path,
                                 const UT_StringRef &referencing_layer_path,
                                 bool asset_is_layer,
                                 UT_String &newpath,
                                 UT_String &error) override;

        bool processLayer(const UT_StringRef &identifier, UT_String &error) override;

        UT_StringHolder displayName() const override;
        const PI_EditScriptedParms *parameters() const override;

    private:
        void processSOPCreateNode(std::string sopReference);

        // Check if path is a VDB file that should be compressed
        static bool siutableForDeferredCompression(const UT_StringRef &asset_path) ;

        // Get output directory from referencing layer path
        std::string getOutputDirectory(const UT_StringRef &referencing_layer_path) const;
        
        // Public method to process deferred compressions after export completes
        void processDeferredCompressions();
        
        
        // Extract VDB data from SOP node
        void extractVDBFromSOP(SOP_Node* sopNode, fpreal t, CompressorManager* compressorManager);
        
        // Extract node name from SOP reference path (e.g., "BBB" from "op:/stage/BBB/sopnet/...")
        std::string extractNodeNameFromSopReference(const std::string& sopReference) const;
        
        // Find marker node that controls a specific node path
        OP_Node* findMarkerNodeByNodePath(const std::string& nodeName) const;
        
        // Check if a target node is in the input chain of a source node
        bool isNodeInInputChain(OP_Node* sourceNode, OP_Node* targetNode) const;
        
        // Recursive helper for input chain checking
        bool isNodeInInputChainRecursive(OP_Node* currentNode, OP_Node* targetNode, std::set<OP_Node*>& visitedNodes) const;
        
        // Find LOP networks starting from config_node
//        void findLOPNetworksFromConfig(OP_Node* config_node, fpreal t);
        
        // Traverse a LOP network looking for SOP Create nodes
//        void traverseLOPNetwork(OP_Network* network, fpreal t);
        
        // Search for SOP networks with VDB data
//        void searchForSOPNetworks(OP_Node* startNode, fpreal t);
        
        // Compress VDB grids from memory immediately
//        void compressVDBGridsFromMemory(std::vector<openvdb::GridBase::ConstPtr>& grids,
//                                       std::vector<std::string>& gridNames,
//                                       fpreal t);
        Zibra::CE::Compression::CompressorManager createCompressorManager();
    private:
        struct InMemoryCompressionEntry
        {
            Zibra::ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker* markerNode;
            Zibra::CE::Compression::CompressorManager* compressorManager;
//            std::string outputFilePath;
        };
        std::vector<InMemoryCompressionEntry> m_InMemoryCompressionEntries;

        fpreal m_curTime;

        struct DeferredCompressionEntry
        {
            std::string vdbPath;
            std::string outputDir;
            std::string compressedPath;
        };

        //Zibra::CE::Compression::CompressorManager m_CompressorManager;
        std::string m_CurrentOutputDir;
        PI_EditScriptedParms *m_Parameters;
        std::vector<DeferredCompressionEntry> m_DeferredCompressionPaths;
        
        // Cached VDB grids found during traversal
        std::vector<openvdb::GridBase::ConstPtr> m_CachedVDBGrids;
        std::vector<std::string> m_CachedGridNames;

    };

    // Factory function for registration
    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor();

} // namespace Zibra::ZibraVDBOutputProcessor