#include "PrecompiledHeader.h"
#include "ZibraVDBOutputProcessor.h"

#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
#include <UT/UT_StringHolder.h>
#include <UT/UT_FileUtil.h>
#include <OP/OP_Node.h>
#include <OP/OP_Director.h>
#include <OP/OP_Network.h>
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

    void compressGrids(std::vector<openvdb::GridBase::ConstPtr>& grids, std::vector<std::string>& gridNames,
                       CE::Compression::CompressorManager* compressorManager)
    {
        if (grids.empty())
        {
            assert(false && "No grids to compress");
            return;
        }

        std::vector<const char*> channelCStrings;
        for (const auto& name : gridNames)
        {
            channelCStrings.push_back(name.c_str());
        }
        CE::Addons::OpenVDBUtils::FrameLoader frameLoader{grids.data(), grids.size()};
        CE::Addons::OpenVDBUtils::EncodingMetadata encodingMetadata{};
        auto frame = frameLoader.LoadFrame(&encodingMetadata);
        if (!frame)
        {
            assert(false && "Failed to load frame from memory grids");
            return;
        }
        CE::Compression::CompressFrameDesc compressFrameDesc{};
        compressFrameDesc.channelsCount = gridNames.size();
        compressFrameDesc.channels = channelCStrings.data();
        compressFrameDesc.frame = frame;

        CE::Compression::FrameManager* frameManager = nullptr;
        auto status = compressorManager->CompressFrame(compressFrameDesc, &frameManager);
        if (status == CE::ZCE_SUCCESS && frameManager)
        {
            status = frameManager->Finish();
        }
        else
        {
            assert(false && "CompressFrame failed or frameManager is null for in-memory grids: status " + std::to_string(static_cast<int>(status)));
        }
        frameLoader.ReleaseFrame(frame);
    }

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

//        //TODO find the better place we can initialize the compression engine
//        CE::Compression::FrameMappingDecs frameMappingDesc{};
//        frameMappingDesc.sequenceStartIndex = 0;
//        frameMappingDesc.sequenceIndexIncrement = 1;
//        float defaultQuality = 0.6f;
//        std::vector<std::pair<UT_String, float>> perChannelSettings;
//        auto status = m_CompressorManager.Initialize(frameMappingDesc, defaultQuality, perChannelSettings);
//        assert(status == CE::ZCE_SUCCESS);

        m_Parameters = new PI_EditScriptedParms();
    }

    ZibraVDBOutputProcessor::~ZibraVDBOutputProcessor()
    {
        delete m_Parameters;
//        if (LibraryUtils::IsLibraryLoaded())
//        {
//            m_CompressorManager.Release();
//        }
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
        m_InMemoryCompressionEntries.clear();

        m_curTime = t;

//        std::vector<OP_Node*> markerNodes;
//        Zibra::Utils::ZibraUSDUtils::DetectCompressionMarkerNodes(lop_node, config_node, markerNodes);
//        for (OP_Node* markerNode : markerNodes)
//        {
//            Zibra::Utils::ZibraUSDUtils::SearchUpstreamForSOPCreate(markerNode, t,
//                [this](SOP_Node* sopNode, fpreal time) { this->extractVDBFromSOP(sopNode, time); });
//        }
    }

    bool ZibraVDBOutputProcessor::processSavePath(const UT_StringRef &asset_path,
                                                 const UT_StringRef &referencing_layer_path,
                                                 bool asset_is_layer,
                                                 UT_String &newpath,
                                                 UT_String &error)
    {
        std::string layerPath = referencing_layer_path.toStdString();
        std::cout << "[ZibraVDB] Processing save path: " << asset_path.toStdString()
                  << " for layer: " << layerPath << std::endl;
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

    void ZibraVDBOutputProcessor::processSOPCreateNode(std::string sopReference)
    {
        //TODO re-implement the extractNodeNameFromSopReference
        std::string nodeName = extractNodeNameFromSopReference(sopReference);
        if (!nodeName.empty())
        {
            //TODO re-implement the findMarkerNodeByNodePath
            OP_Node* associatedMarker = findMarkerNodeByNodePath(nodeName);
            if (associatedMarker)
            {
                auto entry = std::find_if(m_InMemoryCompressionEntries.begin(), m_InMemoryCompressionEntries.end(), [associatedMarker](const InMemoryCompressionEntry& entry) {
                    return associatedMarker == entry.markerNode;
                });

                if (entry == m_InMemoryCompressionEntries.end())
                {
                    InMemoryCompressionEntry entr = {static_cast<ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker*>(associatedMarker), nullptr};
                    m_InMemoryCompressionEntries.emplace_back(entr);
                    entry = std::prev(m_InMemoryCompressionEntries.end());
                }

                if (!entry->compressorManager)
                {
                    entry->compressorManager = new Zibra::CE::Compression::CompressorManager();

                    //TODO place these parameters into the marker node
                    CE::Compression::FrameMappingDecs frameMappingDesc{};
                    frameMappingDesc.sequenceStartIndex = 0;
                    frameMappingDesc.sequenceIndexIncrement = 1;
                    float defaultQuality = 0.6f;
                    std::vector<std::pair<UT_String, float>> perChannelSettings;

                    auto status = entry->compressorManager->Initialize(frameMappingDesc, defaultQuality, perChannelSettings);
                    assert(status == CE::ZCE_SUCCESS);

                    // TODO: Get from actual export path from config node
                    std::string outputDir = "C:/src/usd/ZibraAI/output/compressed";
                    std::filesystem::create_directories(outputDir);
                    // TODO: Get from actual export path from marker node
                    std::string compressedPath = outputDir + "/" + nodeName + ".zibravdb";

                    status = entry->compressorManager->StartSequence(UT_String(compressedPath));
                    if (status != CE::ZCE_SUCCESS)
                    {
                        assert(false && "Failed to start sequence for compression, status: " + std::to_string(static_cast<int>(status)));
                        return;
                    }
                }
                //TODO cache the SOP node
                Zibra::Utils::ZibraUSDUtils::SearchUpstreamForSOPCreate(associatedMarker, m_curTime, [this, cm = entry->compressorManager](SOP_Node* sopNode, fpreal time) { this->extractVDBFromSOP(sopNode, time, cm); });
            }
        }
    }

    bool ZibraVDBOutputProcessor::processReferencePath(const UT_StringRef &asset_path,
                                                      const UT_StringRef &referencing_layer_path,
                                                      bool asset_is_layer,
                                                      UT_String &newpath,
                                                      UT_String &error)
    {
        std::cout << "[ZibraVDB] Processing reference path: " << asset_path.toStdString()
                  << " for layer: " << referencing_layer_path.toStdString() << std::endl;

        std::string pathStr = asset_path.toStdString();

        if (pathStr.find(".vdb") == pathStr.length() - 4)
        {

        }
        else if (pathStr.find("op:/stage/") == 0 && pathStr.find("sopcreate") == std::string::npos)
        {
            processSOPCreateNode(pathStr);
        }
        return false;

//        return false;
//
//        // Check if this is a SOP reference that contains node path information
//        if (pathStr.find("op:/stage/") != std::string::npos)
//        {
//            // Extract the node name from the SOP reference path
//            // Format: op:/stage/BBB/sopnet/OUT.sop.volumes:...
//            std::string nodeName = extractNodeNameFromSopReference(pathStr);
//            if (!nodeName.empty())
//            {
//                std::cout << "[ZibraVDB] Found SOP reference with node: " << nodeName << std::endl;
//
//                // Try to find a marker node that controls this node path
//                OP_Node* associatedMarker = findMarkerNodeByNodePath(nodeName);
//                if (associatedMarker)
//                {
//                    std::cout << "[ZibraVDB] Associated SOP reference with marker: "
//                             << associatedMarker->getName().toStdString() << std::endl;
//                }
//            }
//        }
//
//        if (siutableForDeferredCompression(asset_path))
//        {
//            std::string outputDir = getOutputDirectory(referencing_layer_path);
//            if (outputDir.empty())
//            {
//                assert(false && "Could not determine output directory");
//                error = "Could not determine output directory";
//                return false;
//            }
//
//            std::filesystem::path outputPath(outputDir);
//            std::filesystem::path vdbDir = outputPath / "compressed";
//            std::filesystem::path originalPath(pathStr);
//            std::string compressedName = originalPath.stem().string() + "_compressed.zibravdb";
//            std::string compressedPath = (vdbDir / compressedName).string();
//
//            DeferredCompressionEntry entry;
//            entry.vdbPath = pathStr;
//            entry.outputDir = outputDir;
//            entry.compressedPath = compressedPath;
//            m_DeferredCompressionPaths.push_back(entry);
//
//            std::filesystem::create_directories(vdbDir);
//        }
//        return false;
    }

    bool ZibraVDBOutputProcessor::processLayer(const UT_StringRef& identifier, UT_String& error)
    {
        std::cout << "[ZibraVDB] Processing layer: " << identifier.toStdString() << std::endl;
        processDeferredCompressions();

        for (const auto& entry : m_InMemoryCompressionEntries)
        {
            if (entry.compressorManager)
            {
                std::string warning;
                auto status = entry.compressorManager->FinishSequence(warning);
                if (status != CE::ZCE_SUCCESS)
                {
                    assert(false && "Failed to finish compression sequence for in-memory VDBs. Status: " + std::to_string(static_cast<int>(status)));
                }
                entry.compressorManager->Release();
            }
        }

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

        auto compressorManager = createCompressorManager();

        auto first_entry = m_DeferredCompressionPaths.at(0);
        std::filesystem::path compressedDir = std::filesystem::path(first_entry.outputDir) / "compressed";
        std::filesystem::create_directories(compressedDir);
        auto status = compressorManager.StartSequence(UT_String(first_entry.compressedPath));

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
            status = compressorManager.CompressFrame(compressFrameDesc, &frameManager);
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
        status = compressorManager.FinishSequence(warning);
        assert(status == CE::ZCE_SUCCESS && "FinishSequence failed for deferred VDBs. Status: " + std::to_string(static_cast<int>(status)));
        compressorManager.Release();
    }



//    void ZibraVDBOutputProcessor::compressVDBGridsFromMemory(std::vector<openvdb::GridBase::ConstPtr>& grids,
//                                                            std::vector<std::string>& gridNames,
//                                                            fpreal t)
//    {
//        std::cout << "[ZibraVDB] ========== SCENARIO 1: Compressing VDB Grids from Memory ==========" << std::endl;
//        std::cout << "[ZibraVDB] IMMEDIATE COMPRESSION: " << grids.size() << " VDB grids from solvers/SOP Create nodes" << std::endl;
//
//        if (grids.empty())
//        {
//            std::cout << "[ZibraVDB] No grids to compress" << std::endl;
//            return;
//        }
//
//        try
//        {
//            auto compressorManager = createCompressorManager();
//            // Determine output path - for now use a default location
//            // In a multi-frame scenario, this would include frame numbers
//            std::string outputDir = "C:/src/usd/ZibraAI/output"; // TODO: Get from actual export path
//            std::string compressedDir = outputDir + "/compressed";
//            std::filesystem::create_directories(compressedDir);
//
//            std::string compressedPath = compressedDir + "/gpu_explosion_frame_0.zibravdb";
//            std::cout << "[ZibraVDB] Output file: " << compressedPath << std::endl;
//
//            // Start compression sequence
//            auto status = compressorManager.StartSequence(UT_String(compressedPath));
//            if (status != CE::ZCE_SUCCESS)
//            {
//                std::cout << "[ZibraVDB] ERROR: Failed to start sequence, status: " << static_cast<int>(status) << std::endl;
//                return;
//            }
//
//            std::cout << "[ZibraVDB] Compression sequence started" << std::endl;
//
//            // Prepare channel names as C strings
//            std::vector<const char*> channelCStrings;
//            for (const auto& name : gridNames)
//            {
//                channelCStrings.push_back(name.c_str());
//                std::cout << "[ZibraVDB] Channel: " << name << std::endl;
//            }
//
//            // Create frame loader with the in-memory grids
//            CE::Addons::OpenVDBUtils::FrameLoader frameLoader{grids.data(), grids.size()};
//            CE::Addons::OpenVDBUtils::EncodingMetadata encodingMetadata{};
//
//            // Load frame from memory
//            auto frame = frameLoader.LoadFrame(&encodingMetadata);
//            if (!frame)
//            {
//                std::cout << "[ZibraVDB] ERROR: Failed to load frame from memory grids" << std::endl;
//                return;
//            }
//
//            std::cout << "[ZibraVDB] Frame loaded from memory grids successfully" << std::endl;
//
//            // Set up compression descriptor
//            CE::Compression::CompressFrameDesc compressFrameDesc{};
//            compressFrameDesc.channelsCount = gridNames.size();
//            compressFrameDesc.channels = channelCStrings.data();
//            compressFrameDesc.frame = frame;
//
//            std::cout << "[ZibraVDB] Compressing frame with " << compressFrameDesc.channelsCount << " channels..." << std::endl;
//
//            // Compress frame
//            CE::Compression::FrameManager* frameManager = nullptr;
//            status = compressorManager.CompressFrame(compressFrameDesc, &frameManager);
//            std::cout << "[ZibraVDB] CompressFrame status: " << static_cast<int>(status) << std::endl;
//
//            if (status == CE::ZCE_SUCCESS && frameManager)
//            {
//                std::cout << "[ZibraVDB] Finishing frame compression..." << std::endl;
//                status = frameManager->Finish();
//                std::cout << "[ZibraVDB] FrameManager Finish status: " << static_cast<int>(status) << std::endl;
//            }
//            else
//            {
//                std::cout << "[ZibraVDB] ERROR: CompressFrame failed or frameManager is null" << std::endl;
//            }
//
//            // Release frame
//            frameLoader.ReleaseFrame(frame);
//
//            if (status == CE::ZCE_SUCCESS)
//            {
//                std::cout << "[ZibraVDB] Frame compression successful" << std::endl;
//            }
//            else
//            {
//                std::cout << "[ZibraVDB] Frame compression failed with status: " << static_cast<int>(status) << std::endl;
//            }
//
//            // Finish sequence
//            std::string warning;
//            status = compressorManager.FinishSequence(warning);
//            std::cout << "[ZibraVDB] FinishSequence status: " << static_cast<int>(status) << std::endl;
//
//            if (status == CE::ZCE_SUCCESS)
//            {
//                std::cout << "[ZibraVDB] ========== SUCCESS: VDB compression completed ==========" << std::endl;
//                std::cout << "[ZibraVDB] Compressed file: " << compressedPath << std::endl;
//            }
//            else
//            {
//                std::cout << "[ZibraVDB] ERROR: FinishSequence failed with status: " << static_cast<int>(status) << std::endl;
//                if (!warning.empty()) {
//                    std::cout << "[ZibraVDB] Warning: " << warning << std::endl;
//                }
//            }
//            compressorManager.Release();
//        }
//        catch (const std::exception& e)
//        {
//            std::cout << "[ZibraVDB] ERROR: Exception during compression: " << e.what() << std::endl;
//        }
//    }

    void ZibraVDBOutputProcessor::extractVDBFromSOP(SOP_Node* sopNode, fpreal t, CompressorManager* compressorManager)
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
            compressGrids(grids, gridNames, compressorManager);
        }
        else
        {
            assert(false && "No VDB grids found in SOP node");
            //TODO add error to node output
        }
    }

    Zibra::CE::Compression::CompressorManager ZibraVDBOutputProcessor::createCompressorManager()
    {
        //TODO find the better place we can initialize the compression engine
        CE::Compression::FrameMappingDecs frameMappingDesc{};
        frameMappingDesc.sequenceStartIndex = 0;
        frameMappingDesc.sequenceIndexIncrement = 1;
        float defaultQuality = 0.6f;
        std::vector<std::pair<UT_String, float>> perChannelSettings;
        Zibra::CE::Compression::CompressorManager result;
        auto status = result.Initialize(frameMappingDesc, defaultQuality, perChannelSettings);
        assert(status == CE::ZCE_SUCCESS);

        return result;
    }

    std::string ZibraVDBOutputProcessor::extractNodeNameFromSopReference(const std::string& sopReference) const
    {
        // Parse SOP reference like: "op:/stage/BBB/sopnet/OUT.sop.volumes:..."
        // We want to extract "BBB"
        
        std::cout << "[ZibraVDB] Parsing SOP reference: " << sopReference << std::endl;
        
        std::string stagePrefix = "op:/stage/";
        size_t stagePos = sopReference.find(stagePrefix);
        if (stagePos == std::string::npos)
        {
            std::cout << "[ZibraVDB] No 'op:/stage/' prefix found" << std::endl;
            return "";
        }
        
        // Find the node name after "/stage/"
        size_t nodeStart = stagePos + stagePrefix.length();
        size_t nodeEnd = sopReference.find('/', nodeStart);
        
        if (nodeEnd == std::string::npos)
        {
            std::cout << "[ZibraVDB] No '/' found after node name" << std::endl;
            return "";
        }
        
        std::string nodeName = sopReference.substr(nodeStart, nodeEnd - nodeStart);
        std::cout << "[ZibraVDB] Extracted node name: " << nodeName << std::endl;
        
        return nodeName;
    }

    OP_Node* ZibraVDBOutputProcessor::findMarkerNodeByNodePath(const std::string& nodeName) const
    {
        std::cout << "[ZibraVDB] Looking for marker node controlling: " << nodeName << std::endl;
        
        // We need to find the LOP node named "BBB" and then look for compression markers
        // connected to it (either upstream or downstream)
        
        // Start from the root node and search for the named node
        OP_Node* rootNode = OPgetDirector()->findNode("/stage");
        if (!rootNode || !rootNode->isNetwork())
        {
            std::cout << "[ZibraVDB] Cannot find /stage network" << std::endl;
            return nullptr;
        }
        
        OP_Network* stageNetwork = static_cast<OP_Network*>(rootNode);
        OP_Node* targetNode = stageNetwork->findNode(nodeName.c_str());
        
        if (!targetNode)
        {
            std::cout << "[ZibraVDB] Cannot find node: " << nodeName << std::endl;
            return nullptr;
        }
        
        std::cout << "[ZibraVDB] Found target node: " << targetNode->getName().toStdString() << std::endl;
        
        // Search the network for compression markers and check if they're connected to our target
        for (int i = 0; i < stageNetwork->getNchildren(); ++i)
        {
            OP_Node* child = stageNetwork->getChild(i);
            if (child && child->getOperator()->getName().contains("zibravdb_mark_for_compression"))
            {
                std::cout << "[ZibraVDB] Found compression marker in network: " << child->getName().toStdString() << std::endl;
                
                // Check if this marker has the target node in its input chain
                if (isNodeInInputChain(child, targetNode))
                {
                    std::cout << "[ZibraVDB] Confirmed marker controls target node" << std::endl;
                    return child;
                }
                
                // Also check if the target node has this marker in its input chain (reverse direction)
                if (isNodeInInputChain(targetNode, child))
                {
                    std::cout << "[ZibraVDB] Found marker upstream of target node" << std::endl;
                    return child;
                }
            }
        }
        
        std::cout << "[ZibraVDB] No compression marker found for node: " << nodeName << std::endl;
        return nullptr;
    }

    bool ZibraVDBOutputProcessor::isNodeInInputChain(OP_Node* sourceNode, OP_Node* targetNode) const
    {
        if (!sourceNode || !targetNode)
            return false;
        
        // Simple recursive check to see if targetNode is upstream of sourceNode
        std::set<OP_Node*> visitedNodes;
        return isNodeInInputChainRecursive(sourceNode, targetNode, visitedNodes);
    }

    bool ZibraVDBOutputProcessor::isNodeInInputChainRecursive(OP_Node* currentNode, OP_Node* targetNode, std::set<OP_Node*>& visitedNodes) const
    {
        if (!currentNode || visitedNodes.find(currentNode) != visitedNodes.end())
            return false;
        
        visitedNodes.insert(currentNode);
        
        // Check all inputs of current node
        for (int i = 0; i < currentNode->nInputs(); ++i)
        {
            OP_Node* input = currentNode->getInput(i);
            if (input == targetNode)
            {
                return true; // Found target node in input chain
            }
            
            // Recursively check input's input chain
            if (isNodeInInputChainRecursive(input, targetNode, visitedNodes))
            {
                return true;
            }
        }
        
        return false;
    }

} // namespace Zibra::ZibraVDBOutputProcessor