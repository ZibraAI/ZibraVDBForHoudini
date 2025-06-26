#include "PrecompiledHeader.h"
#include "ZibraVDBOutputProcessor.h"

#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
#include <UT/UT_StringHolder.h>
#include <UT/UT_FileUtil.h>
#include <OP/OP_Node.h>
#include <SOP/SOP_Node.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVDB.h>
#include <GA/GA_Iterator.h>
#include <LOP/LOP_Node.h>
#include <Zibra/CE/Addons/OpenVDBFrameLoader.h>
#include <Zibra/CE/Compression.h>
#include <openvdb/openvdb.h>
#include <openvdb/io/File.h>
#include <iostream>
#include <thread>
#include <chrono>

#include <filesystem>
#include <regex>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Zibra::ZibraVDBOutputProcessor
{
    using namespace std::literals;
    
    /*
     * ZibraVDB Output Processor - Dual Scenario Compression
     * 
     * SCENARIO 1: In-Memory VDB Compression (Immediate)
     * - Detects VDB grids from solvers/SOP Create nodes in memory
     * - Compresses immediately when found during USD export
     * - Called from: extractVDBFromSOP() -> compressVDBGridsFromMemory()
     * 
     * SCENARIO 2: Existing VDB File Compression (Deferred) 
     * - Detects imported VDB files referenced in USD stage
     * - Adds to deferred compression list during processReferencePath()
     * - Compresses all collected files at end via processDeferredCompressions()
     */

    ZibraVDBOutputProcessor::ZibraVDBOutputProcessor()
        : m_Parameters(nullptr)
    {
        LibraryUtils::LoadLibrary();

        //TODO find the better place we can initialize the compression engine
        CE::Compression::FrameMappingDecs frameMappingDesc{};
        frameMappingDesc.sequenceStartIndex = 0;
        frameMappingDesc.sequenceIndexIncrement = 1;
        float defaultQuality = 0.6f;
        std::vector<std::pair<UT_String, float>> perChannelSettings;
        auto status = m_CompressorManager.Initialize(frameMappingDesc, defaultQuality, perChannelSettings);
        assert(status == CE::ZCE_SUCCESS);

        m_Parameters = new PI_EditScriptedParms();
    }

    ZibraVDBOutputProcessor::~ZibraVDBOutputProcessor()
    {
        delete m_Parameters;
        if (LibraryUtils::IsLibraryLoaded())
        {
            m_CompressorManager.Release();
        }
    }

    UT_StringHolder ZibraVDBOutputProcessor::displayName() const
    {
        return {"ZibraVDBCompressor"};
    }

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor()
    {
        return HUSD_OutputProcessorPtr(new ZibraVDBOutputProcessor());
    }

    const PI_EditScriptedParms *ZibraVDBOutputProcessor::parameters() const
    {
        return m_Parameters;
    }

    void ZibraVDBOutputProcessor::beginSave(OP_Node *config_node,
                                           const UT_Options &config_overrides,
                                           OP_Node *lop_node,
                                           fpreal t,
                                           const UT_Options &stage_variables)
    {
        m_CurrentOutputDir.clear();
        m_DeferredCompressionPaths.clear();
        
//        if (lop_node)
//        {
//            traverseSOPCreateNodes(lop_node, t);
//        }
//        else
//        {
//            if (config_node)
//            {
//                findLOPNetworksFromConfig(config_node, t);
//            }
//            else
//            {
//                //TODO add error to node output
//                assert(false && "Both lop_node and config_node are null. Cannot proceed with traversal.");
//            }
//        }

        std::vector<OP_Node*> markerNodes;
        detectCompressionMarkerNodes(lop_node, config_node, markerNodes);
        for (OP_Node* markerNode : markerNodes)
        {
            searchUpstreamForSOPCreate(markerNode, t);
        }
    }

    bool ZibraVDBOutputProcessor::processSavePath(const UT_StringRef &asset_path,
                                                 const UT_StringRef &referencing_layer_path,
                                                 bool asset_is_layer,
                                                 UT_String &newpath,
                                                 UT_String &error)
    {
        // TODO double-check if we need to redirect VDBs based on user settings
        static bool shouldRedirectVDBs = false;
        if (!shouldRedirectVDBs)
        {
            std::cout << "[ZibraVDB] Skipping VDB redirection as per user settings" << std::endl;
            return false;
        }

        std::string pathStr = asset_path.toStdString();
        if (!asset_is_layer && pathStr.find(".vdb") != std::string::npos)
        {
            std::string outputDir = getOutputDirectory(referencing_layer_path);
            if (outputDir.empty())
            {
                error = "Could not determine output directory";
                return false;
            }

            std::filesystem::path originalPath(pathStr);
            std::filesystem::path outputPath(outputDir);
            std::filesystem::path vdbDir = outputPath / "vdb_files";
            std::string redirectedPath = (vdbDir / originalPath.filename()).string();

            std::filesystem::create_directories(vdbDir);
            newpath = redirectedPath;

            return true;
        }

        return false;
    }

    bool ZibraVDBOutputProcessor::processReferencePath(const UT_StringRef &asset_path,
                                                      const UT_StringRef &referencing_layer_path,
                                                      bool asset_is_layer,
                                                      UT_String &newpath,
                                                      UT_String &error)
    {
        std::string pathStr = asset_path.toStdString();
        if (siutableForDeferredCompression(asset_path))
        {
            std::string outputDir = getOutputDirectory(referencing_layer_path);
            if (outputDir.empty())
            {
                assert(false && "Could not determine output directory");
                error = "Could not determine output directory";
                return false;
            }

            std::filesystem::path outputPath(outputDir);
            std::filesystem::path vdbDir = outputPath / "compressed";
            std::filesystem::path originalPath(pathStr);
            std::string compressedName = originalPath.stem().string() + "_compressed.zibravdb";
            std::string compressedPath = (vdbDir / compressedName).string();

            DeferredCompressionEntry entry;
            entry.vdbPath = pathStr;
            entry.outputDir = outputDir;
            entry.compressedPath = compressedPath;
            m_DeferredCompressionPaths.push_back(entry);

            std::filesystem::create_directories(vdbDir);
        }
        return false;
    }

    bool ZibraVDBOutputProcessor::processLayer(const UT_StringRef& identifier, UT_String& error)
    {
        processDeferredCompressions();
        return true;
    }

    bool ZibraVDBOutputProcessor::siutableForDeferredCompression(const UT_StringRef &asset_path)
    {
        std::string pathStr = asset_path.toStdString();
        bool isVDB = pathStr.find(".vdb") != std::string::npos;
        if (isVDB)
        {
            return std::filesystem::exists(pathStr);
        }
        return false;
    }

    std::string ZibraVDBOutputProcessor::getOutputDirectory(const UT_StringRef &referencing_layer_path) const
    {
        if (!m_CurrentOutputDir.empty())
        {
            return m_CurrentOutputDir;
        }
        
        std::string layerPath = referencing_layer_path.toStdString();
        if (layerPath.empty())
        {
            return "";
        }
        
        std::filesystem::path path(layerPath);
        return path.parent_path().string();
    }

    void ZibraVDBOutputProcessor::processDeferredCompressions()
    {
        if (m_DeferredCompressionPaths.empty())
        {
            return;
        }

        if (!LibraryUtils::IsLibraryLoaded())
        {
            assert(false && "Library not loaded for deferred compression");
            return;
        }

        auto first_entry = m_DeferredCompressionPaths.at(0);
        std::filesystem::path compressedDir = std::filesystem::path(first_entry.outputDir) / "compressed";
        std::filesystem::create_directories(compressedDir);
        auto status = m_CompressorManager.StartSequence(UT_String(first_entry.compressedPath));

        if (status != CE::ZCE_SUCCESS)
        {
            assert(false && "Failed to start compression sequence for deferred VDBs. Status: " + std::to_string(static_cast<int>(status)));
            return;
        }

        for (const auto& entry : m_DeferredCompressionPaths)
        {
            openvdb::io::File vdbFile(entry.vdbPath);
            vdbFile.open();

            std::vector<openvdb::GridBase::ConstPtr> volumes;
            std::vector<std::string> channelNames;
            std::vector<const char*> channelCStrings;

            for (auto nameIter = vdbFile.beginName(); nameIter != vdbFile.endName(); ++nameIter)
            {
                std::string gridName = nameIter.gridName();
                openvdb::GridBase::ConstPtr grid = vdbFile.readGrid(gridName);
                if (grid)
                {
                    volumes.push_back(grid);
                    channelNames.push_back(gridName);
                    channelCStrings.push_back(channelNames.back().c_str());
                }
                else
                {
                    assert(false && "Failed to read grid from VDB file: "/gridName);
                }
            }
            vdbFile.close();

            if (volumes.empty())
            {
                continue;
            }

            CE::Addons::OpenVDBUtils::FrameLoader frameLoader{volumes.data(), volumes.size()};
            CE::Addons::OpenVDBUtils::EncodingMetadata encodingMetadata{};
            auto* frame = frameLoader.LoadFrame(&encodingMetadata);
            if (!frame)
            {
                assert(false && "Failed to load frame from VDB file: " + entry.vdbPath);
                continue;
            }

            CE::Compression::CompressFrameDesc compressFrameDesc{};
            compressFrameDesc.channelsCount = channelNames.size();
            compressFrameDesc.channels = channelCStrings.data();
            compressFrameDesc.frame = frame;

            CE::Compression::FrameManager* frameManager = nullptr;
            status = m_CompressorManager.CompressFrame(compressFrameDesc, &frameManager);
            if (status == CE::ZCE_SUCCESS && frameManager)
            {
                status = frameManager->Finish();
            }
            else
            {
                assert(false && "CompressFrame failed or frameManager is null for VDB: status "/+ std::to_string(static_cast<int>(status)));
            }

            frameLoader.ReleaseFrame(frame);
        }

        std::string warning;
        status = m_CompressorManager.FinishSequence(warning);
        assert(status == CE::ZCE_SUCCESS && "FinishSequence failed for deferred VDBs. Status: " + std::to_string(static_cast<int>(status)));
    }

    void ZibraVDBOutputProcessor::detectCompressionMarkerNodes(OP_Node* lop_node, OP_Node* config_node, std::vector<OP_Node*>& markerNodes)
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

    /*void ZibraVDBOutputProcessor::processMarkerNode(OP_Node* markerNode, fpreal t)
    {
        if (!markerNode)
            return;

        std::cout << "[ZibraVDB] ========== Processing Marker Node: " << markerNode->getName().toStdString() << " ==========" << std::endl;

        // Get the SOP path parameter from the marker node
        UT_String sopPath;
        if (markerNode->hasParm("soppath"))
        {
            markerNode->evalString(sopPath, "soppath", 0, t);
            std::cout << "[ZibraVDB] Marker points to SOP: " << sopPath.toStdString() << std::endl;

            if (sopPath.length() > 0)
            {
                // Find the SOP node and compress VDBs from memory (Scenario 1)
                OP_Node* referencedNode = markerNode->findNode(sopPath);
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
                        OP_Node* displayNode = sopNetwork->getDisplayNodePtr();
                        if (displayNode && displayNode->getOpTypeID() == SOP_OPTYPE_ID)
                        {
                            sopNode = static_cast<SOP_Node*>(displayNode);
                        }
                    }

                    if (sopNode)
                    {
                        std::cout << "[ZibraVDB] Found SOP node, extracting VDB grids from memory..." << std::endl;
                        extractVDBFromSOP(sopNode, t);
                    }
                    else
                    {
                        std::cout << "[ZibraVDB] Could not find valid SOP node at path: " << sopPath.toStdString() << std::endl;
                    }
                }
                else
                {
                    std::cout << "[ZibraVDB] Could not find node at path: " << sopPath.toStdString() << std::endl;
                }
            }
        }
        else
        {
            std::cout << "[ZibraVDB] Marker node has no soppath parameter - checking for existing VDB files" << std::endl;
            // This marker might be for existing VDB files (Scenario 2)
            // The processReferencePath method will handle these when files are referenced
        }
    }*/

    void ZibraVDBOutputProcessor::searchUpstreamForSOPCreate(OP_Node* markerNode, fpreal t)
    {
        if (!markerNode)
            return;
            
        std::cout << "[ZibraVDB] ========== Searching upstream from marker: " << markerNode->getName().toStdString() << " ==========" << std::endl;
        
        std::set<OP_Node*> visitedNodes;
        searchUpstreamForSOPCreateRecursive(markerNode, t, visitedNodes);
    }

    void ZibraVDBOutputProcessor::searchUpstreamForSOPCreateRecursive(OP_Node* node, fpreal t, std::set<OP_Node*>& visitedNodes)
    {
        if (!node || visitedNodes.find(node) != visitedNodes.end())
            return;
            
        visitedNodes.insert(node);
        
        // Check if this node is a SOP Create node
        if (node->getOperator()->getName().equal("sopcreate"))
        {
            std::cout << "[ZibraVDB] Found SOP Create node upstream: " << node->getName().toStdString() << std::endl;
            traverseSOPCreateNodes(node, t);
            return; // Found what we're looking for, no need to go further upstream
        }
        
        // Search through all inputs of this node
        for (int i = 0; i < node->nInputs(); ++i)
        {
            OP_Node* input = node->getInput(i);
            if (input)
            {
                searchUpstreamForSOPCreateRecursive(input, t, visitedNodes);
            }
        }
    }

    void ZibraVDBOutputProcessor::searchForMarkerNodesRecursive(OP_Node* node, std::vector<OP_Node*>& markerNodes, std::set<OP_Node*>& visitedNodes)
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

    void ZibraVDBOutputProcessor::compressVDBGridsFromMemory(std::vector<openvdb::GridBase::ConstPtr>& grids,
                                                            std::vector<std::string>& gridNames,
                                                            fpreal t)
    {
        std::cout << "[ZibraVDB] ========== SCENARIO 1: Compressing VDB Grids from Memory ==========" << std::endl;
        std::cout << "[ZibraVDB] IMMEDIATE COMPRESSION: " << grids.size() << " VDB grids from solvers/SOP Create nodes" << std::endl;
        
        if (grids.empty())
        {
            std::cout << "[ZibraVDB] No grids to compress" << std::endl;
            return;
        }
        
        try
        {
            // Determine output path - for now use a default location
            // In a multi-frame scenario, this would include frame numbers
            std::string outputDir = "C:/src/usd/ZibraAI/output"; // TODO: Get from actual export path
            std::string compressedDir = outputDir + "/compressed";
            std::filesystem::create_directories(compressedDir);
            
            std::string compressedPath = compressedDir + "/gpu_explosion_frame_0.zibravdb";
            std::cout << "[ZibraVDB] Output file: " << compressedPath << std::endl;
            
            // Start compression sequence
            auto status = m_CompressorManager.StartSequence(UT_String(compressedPath));
            if (status != CE::ZCE_SUCCESS)
            {
                std::cout << "[ZibraVDB] ERROR: Failed to start sequence, status: " << static_cast<int>(status) << std::endl;
                return;
            }
            
            std::cout << "[ZibraVDB] Compression sequence started" << std::endl;
            
            // Prepare channel names as C strings
            std::vector<const char*> channelCStrings;
            for (const auto& name : gridNames)
            {
                channelCStrings.push_back(name.c_str());
                std::cout << "[ZibraVDB] Channel: " << name << std::endl;
            }
            
            // Create frame loader with the in-memory grids
            CE::Addons::OpenVDBUtils::FrameLoader frameLoader{grids.data(), grids.size()};
            CE::Addons::OpenVDBUtils::EncodingMetadata encodingMetadata{};
            
            // Load frame from memory
            auto frame = frameLoader.LoadFrame(&encodingMetadata);
            if (!frame)
            {
                std::cout << "[ZibraVDB] ERROR: Failed to load frame from memory grids" << std::endl;
                return;
            }
            
            std::cout << "[ZibraVDB] Frame loaded from memory grids successfully" << std::endl;
            
            // Set up compression descriptor
            CE::Compression::CompressFrameDesc compressFrameDesc{};
            compressFrameDesc.channelsCount = gridNames.size();
            compressFrameDesc.channels = channelCStrings.data();
            compressFrameDesc.frame = frame;
            
            std::cout << "[ZibraVDB] Compressing frame with " << compressFrameDesc.channelsCount << " channels..." << std::endl;

            // Compress frame
            CE::Compression::FrameManager* frameManager = nullptr;
            status = m_CompressorManager.CompressFrame(compressFrameDesc, &frameManager);
            std::cout << "[ZibraVDB] CompressFrame status: " << static_cast<int>(status) << std::endl;
            
            if (status == CE::ZCE_SUCCESS && frameManager)
            {
                std::cout << "[ZibraVDB] Finishing frame compression..." << std::endl;
                status = frameManager->Finish();
                std::cout << "[ZibraVDB] FrameManager Finish status: " << static_cast<int>(status) << std::endl;
            }
            else
            {
                std::cout << "[ZibraVDB] ERROR: CompressFrame failed or frameManager is null" << std::endl;
            }

            // Release frame
            frameLoader.ReleaseFrame(frame);

            if (status == CE::ZCE_SUCCESS)
            {
                std::cout << "[ZibraVDB] Frame compression successful" << std::endl;
            }
            else
            {
                std::cout << "[ZibraVDB] Frame compression failed with status: " << static_cast<int>(status) << std::endl;
            }
            
            // Finish sequence
            std::string warning;
            status = m_CompressorManager.FinishSequence(warning);
            std::cout << "[ZibraVDB] FinishSequence status: " << static_cast<int>(status) << std::endl;
            
            if (status == CE::ZCE_SUCCESS)
            {
                std::cout << "[ZibraVDB] ========== SUCCESS: VDB compression completed ==========" << std::endl;
                std::cout << "[ZibraVDB] Compressed file: " << compressedPath << std::endl;
            }
            else
            {
                std::cout << "[ZibraVDB] ERROR: FinishSequence failed with status: " << static_cast<int>(status) << std::endl;
                if (!warning.empty()) {
                    std::cout << "[ZibraVDB] Warning: " << warning << std::endl;
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "[ZibraVDB] ERROR: Exception during compression: " << e.what() << std::endl;
        }
    }

    void ZibraVDBOutputProcessor::traverseSOPCreateNodes(OP_Node* lop_node, fpreal t)
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
                        extractVDBFromSOP(sopNode, t);
                        return;
                    }
                }
                else
                {
                    assert(false && "Referenced SOP node not found");
                    //TODO add error to node output
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
                            extractVDBFromSOP(static_cast<SOP_Node*>(inputNode), t);
                            return;
                        }
                        else if (inputNode->isNetwork())
                        {
                            OP_Network* inputNetwork = static_cast<OP_Network*>(inputNode);
                            OP_Node* displayNode = inputNetwork->getDisplayNodePtr();
                            if (displayNode && displayNode->getOpTypeID() == SOP_OPTYPE_ID)
                            {
                                extractVDBFromSOP(static_cast<SOP_Node*>(displayNode), t);
                                return;
                            }
                            OP_Node* renderNode = inputNetwork->getRenderNodePtr();
                            if (renderNode && renderNode->getOpTypeID() == SOP_OPTYPE_ID)
                            {
                                extractVDBFromSOP(static_cast<SOP_Node*>(renderNode), t);
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
                                        extractVDBFromSOP(sopChild, t);
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
                                    extractVDBFromSOP(static_cast<SOP_Node*>(displayNode), t);
                                    return;
                                }
                            }
                        }
                    }
                }

                assert(false && "Could not find VDB data in SOP Create node");
                //TODO add error to node output
            }
        }

        if (!lop_node->getOperator()->getName().equal("sopcreate") && lop_node->isNetwork())
        {
            auto* network = static_cast<OP_Network*>(lop_node);
            for (int i = 0; i < network->getNchildren(); ++i)
            {
                OP_Node* child = network->getChild(i);
                traverseSOPCreateNodes(child, t);
            }
        }
    }

    void ZibraVDBOutputProcessor::extractVDBFromSOP(SOP_Node* sopNode, fpreal t)
    {
        if (!sopNode)
            return;
        OP_Context context(t);
        const GU_Detail* gdp = sopNode->getCookedGeo(context);

        if (!gdp)
        {
            assert(false && "SOP node returned null geometry");
            //TODO add error to node output
            return;
        }

        std::vector<openvdb::GridBase::ConstPtr> grids;
        std::vector<std::string> gridNames;

        for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
        {
            const GA_Primitive* prim = gdp->getPrimitive(*it);
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                const auto* vdbPrim = static_cast<const GEO_PrimVDB*>(prim);
                const char* gridName = vdbPrim->getGridName();

                openvdb::GridBase::ConstPtr grid = vdbPrim->getConstGridPtr();
                if (grid)
                {
                    grids.push_back(grid);
                    gridNames.push_back(gridName);
                }
            }
        }

        if (!grids.empty())
        {
            compressVDBGridsFromMemory(grids, gridNames, t);
        }
        else
        {
            assert(false && "No VDB grids found in SOP node");
            //TODO add error to node output
        }
    }

//    void ZibraVDBOutputProcessor::findLOPNetworksFromConfig(OP_Node* config_node, fpreal t)
//    {
//        if (!config_node)
//            return;
//
//        // Strategy 1: Check if config_node itself is a LOP node
//        if (config_node->getOpTypeID() == LOP_OPTYPE_ID)
//        {
//            traverseSOPCreateNodes(config_node, t);
//            return;
//        }
//
//        // Strategy 2: Check config_node's inputs for LOP nodes
//        for (int i = 0; i < config_node->nInputs(); ++i)
//        {
//            OP_Node* input = config_node->getInput(i);
//            if (input && input->getOpTypeID() == LOP_OPTYPE_ID)
//            {
//                traverseSOPCreateNodes(input, t);
//            }
//        }
//
//        // Strategy 3: Look for LOP context in parent network
//        OP_Network* parent = config_node->getParent();
//        if (parent)
//        {
//            OP_Node* stageContext = parent->findNode("/stage");
//            if (stageContext && stageContext->isNetwork())
//            {
//                traverseLOPNetwork(static_cast<OP_Network*>(stageContext), t);
//            }
//            traverseLOPNetwork(parent, t);
//        }
//
//        // Strategy 4: Search for SOP networks with VDB data directly
//        searchForSOPNetworks(config_node, t);
//    }

//    void ZibraVDBOutputProcessor::traverseLOPNetwork(OP_Network* network, fpreal t)
//    {
//        if (!network)
//            return;
//
//        for (int i = 0; i < network->getNchildren(); ++i)
//        {
//            OP_Node* child = network->getChild(i);
//            if (child)
//            {
//                if (child->getOpTypeID() == LOP_OPTYPE_ID &&
//                    child->getOperator()->getName().equal("sopcreate"))
//                {
//                    traverseSOPCreateNodes(child, t);
//                }
//                else if (child->isNetwork())
//                {
//                    traverseLOPNetwork(static_cast<OP_Network*>(child), t);
//                }
//            }
//        }
//    }

//    void ZibraVDBOutputProcessor::searchForSOPNetworks(OP_Node* startNode, fpreal t)
//    {
//        if (!startNode)
//            return;
//
//        OP_Network* parent = startNode->getParent();
//        if (parent)
//        {
//            OP_Node* objContext = parent->findNode("/obj");
//            if (objContext && objContext->isNetwork())
//            {
//                auto* objNetwork = static_cast<OP_Network*>(objContext);
//                for (int i = 0; i < objNetwork->getNchildren(); i++)
//                {
//                    OP_Node* child = objNetwork->getChild(i);
//                    if (child && child->isNetwork())
//                    {
//                        auto* childNetwork = static_cast<OP_Network*>(child);
//                        OP_Node* displayNode = childNetwork->getDisplayNodePtr();
//                        if (displayNode && displayNode->getOpTypeID() == SOP_OPTYPE_ID)
//                        {
//                            extractVDBFromSOP(static_cast<SOP_Node*>(displayNode), t);
//                        }
//                    }
//                }
//            }
//        }
//    }
} // namespace Zibra::ZibraVDBOutputProcessor