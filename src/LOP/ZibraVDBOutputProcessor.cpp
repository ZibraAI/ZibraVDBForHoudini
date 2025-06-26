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

    ZibraVDBOutputProcessor::ZibraVDBOutputProcessor()
        : m_Parameters(nullptr)
    {
        LibraryUtils::LoadLibrary();

        //TODO this may be later
        CE::Compression::FrameMappingDecs frameMappingDesc{};
        frameMappingDesc.sequenceStartIndex = 0;
        frameMappingDesc.sequenceIndexIncrement = 1;

        float defaultQuality = 0.6f;
        std::vector<std::pair<UT_String, float>> perChannelSettings;

        auto status = m_CompressorManager.Initialize(frameMappingDesc, defaultQuality, perChannelSettings);
        assert(status == CE::ZCE_SUCCESS);

        // Create parameter interface
        m_Parameters = new PI_EditScriptedParms();
        // Add parameters as needed
    }

    ZibraVDBOutputProcessor::~ZibraVDBOutputProcessor()
    {
        std::cout << "[ZibraVDB] Destructor called - checking for deferred compressions" << std::endl;
        
        // If we have deferred compressions, try to process them now
        if (!m_DeferredCompressionPaths.empty())
        {
            std::cout << "[ZibraVDB] Found " << m_DeferredCompressionPaths.size() << " deferred tasks in destructor" << std::endl;
            
            // Add a small delay to ensure files are written
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            processDeferredCompressions();
        }
        
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
        std::cout << "[ZibraVDBOutputProcessor] Finished scanning VDBs" << std::endl;

        std::cout << "[ZibraVDB] BeginSave: Starting USD export processing" << std::endl;
        
        // Extract output directory from stage variables if available
        m_CurrentOutputDir.clear();
        
        // Clear deferred compression list
        m_DeferredCompressionPaths.clear();
        
        // Traverse SOP Create nodes to find VDB data
        // Use config_node when lop_node is not available
        OP_Node* nodeToTraverse = lop_node ? lop_node : config_node;
        if (nodeToTraverse)
        {
            if (!lop_node && config_node)
            {
                std::cout << "[ZibraVDB] lop_node is null, using config_node for traversal" << std::endl;
                // Start from config_node and search for LOP networks
                findLOPNetworksFromConfig(config_node, t);
            }
            else
            {
                std::cout << "[ZibraVDB] Using lop_node for traversal" << std::endl;
                traverseSOPCreateNodes(lop_node, t);
            }
        }
    }

    bool ZibraVDBOutputProcessor::processSavePath(const UT_StringRef &asset_path,
                                                 const UT_StringRef &referencing_layer_path,
                                                 bool asset_is_layer,
                                                 UT_String &newpath,
                                                 UT_String &error)
    {
        std::cout << "[ZibraVDB] ========== processSavePath ==========" << std::endl;
        std::cout << "[ZibraVDB] Asset path: " << asset_path.toStdString() << std::endl;
        std::cout << "[ZibraVDB] Layer path: " << referencing_layer_path.toStdString() << std::endl;
        std::cout << "[ZibraVDB] Is layer: " << (asset_is_layer ? "true" : "false") << std::endl;

        // Check if this is a VDB file being saved
        std::string pathStr = asset_path.toStdString();
        if (!asset_is_layer && pathStr.find(".vdb") != std::string::npos)
        {
            std::cout << "[ZibraVDB] Found VDB save operation, redirecting..." << std::endl;

            // Get output directory
            std::string outputDir = getOutputDirectory(referencing_layer_path);
            if (outputDir.empty())
            {
                error = "Could not determine output directory";
                std::cout << "[ZibraVDB] ERROR: Could not determine output directory" << std::endl;
                return false;
            }

            // Extract filename and create custom path
            std::filesystem::path originalPath(pathStr);
            std::filesystem::path outputPath(outputDir);
            std::filesystem::path vdbDir = outputPath / "vdb_files";

            // Use original filename but in our custom directory
            //std::string redirectedPath = (vdbDir / originalPath.filename()).string();
            std::string redirectedPath = ("zibravdb://" / vdbDir / "compressed.zibravdb").string();

            // Create the directory
            std::filesystem::create_directories(vdbDir);

            newpath = redirectedPath;
            std::cout << "[ZibraVDB] Redirecting VDB save from: " << pathStr << std::endl;
            std::cout << "[ZibraVDB] To: " << newpath.toStdString() << std::endl;

            return true;
        }

        return false;
    }

//    bool ZibraVDBOutputProcessor::processReferencePath(const UT_StringRef &asset_path,
//                                                      const UT_StringRef &referencing_layer_path,
//                                                      bool asset_is_layer,
//                                                      UT_String &newpath,
//                                                      UT_String &error)
//    {
//        std::cout << "[ZibraVDB] ProcessReferencePath: " << asset_path.toStdString() << std::endl;
//
//        std::string pathStr = asset_path.toStdString();
//
//        // Check if this is a SOP volumes reference that we want to convert to VDB file
////        if (pathStr.find("op:/") == 0 && pathStr.find("volumes") != std::string::npos)
////        {
////            std::cout << "[ZibraVDB] Found SOP volumes reference - converting to VDB file" << std::endl;
////
////            // Get output directory
////            std::string outputDir = getOutputDirectory(referencing_layer_path);
////            if (outputDir.empty())
////            {
////                error = "Could not determine output directory";
////                std::cout << "[ZibraVDB] ERROR: Could not determine output directory" << std::endl;
////                return false;
////            }
////
////            std::cout << "[ZibraVDB] Output directory: " << outputDir << std::endl;
////
////            // Extract time parameter for frame numbering
////            std::string frameInfo = "0001";
////            std::regex timeRegex(R"(t=([0-9.-]+))");
////            std::smatch match;
////            if (std::regex_search(pathStr, match, timeRegex))
////            {
////                float timeValue = std::stof(match[1].str());
////                int frameNumber = static_cast<int>(std::round(timeValue * 24.0f)) + 1;
////                frameInfo = std::to_string(frameNumber);
////                frameInfo = std::string(4 - std::min(4, (int)frameInfo.length()), '0') + frameInfo;
////            }
////
////            // Create custom VDB file path
////            std::filesystem::path outputPath(outputDir);
////            std::filesystem::path vdbDir = outputPath / "vdb_files";
////            std::string vdbFileName = "volume_frame_" + frameInfo + ".vdb";
////            std::string fullVdbPath = (vdbDir / vdbFileName).string();
////
////            // Create the directory
////            std::filesystem::create_directories(vdbDir);
////
////            newpath = fullVdbPath;
////            std::cout << "[ZibraVDB] Converting SOP volumes to VDB file: " << newpath.toStdString() << std::endl;
////            std::cout << "[ZibraVDB] Frame info extracted: " << frameInfo << std::endl;
////
////            return true;
////        }
//
//        // Handle regular VDB files
//        if (shouldProcessPath(asset_path))
//        {
//            std::cout << "[ZibraVDB] Found VDB file, attempting redirection..." << std::endl;
//
//            // Get output directory
//            std::string outputDir = getOutputDirectory(referencing_layer_path);
//            if (outputDir.empty())
//            {
//                error = "Could not determine output directory";
//                std::cout << "[ZibraVDB] ERROR: Could not determine output directory" << std::endl;
//                return false;
//            }
//
//            // Create custom VDB file path
//            std::filesystem::path outputPath(outputDir);
//            std::filesystem::path vdbDir = outputPath / "vdb_files";
//            std::filesystem::path originalPath(pathStr);
//            //std::string fullVdbPath = (vdbDir / originalPath.filename()).string();
//            std::string fullVdbPath = ("zibravdb://" / vdbDir / "compressed.zibravdb").string();
//
//            // Create the directory
//            std::filesystem::create_directories(vdbDir);
//
//            newpath = fullVdbPath;
//            std::cout << "[ZibraVDB] Redirected VDB file to: " << newpath.toStdString() << std::endl;
//
//            return true;
//        }
//
//        return false;
//    }

//    bool ZibraVDBOutputProcessor::processLayer(const UT_StringRef &identifier,
//                                              UT_String &error)
//    {
//        std::cout << "[ZibraVDB] ========== ProcessLayer ==========" << std::endl;
//        std::cout << "[ZibraVDB] Layer identifier: '" << identifier.toStdString() << "'" << std::endl;
//
//        // Compression disabled for now - just logging layer processing
//        std::string identifierStr = identifier.toStdString();
//        if (identifierStr.find(".usda") != std::string::npos ||
//            identifierStr.find(".usd") != std::string::npos ||
//            identifierStr.empty())
//        {
//            std::cout << "[ZibraVDB] Main layer processed (compression disabled)" << std::endl;
//        }
//        else
//        {
//            std::cout << "[ZibraVDB] Sublayer processed" << std::endl;
//        }
//
//        return false; // Return false to indicate we didn't modify the layer
//    }

    bool ZibraVDBOutputProcessor::shouldProcessPath(const UT_StringRef &asset_path) const
    {
        std::cout << "[ZibraVDB] We check if the asset path need to be processed "<< std::endl;
        std::string pathStr = asset_path.toStdString();
        std::cout << "[ZibraVDB] FILEPATH: " << pathStr << std::endl;
        
        // Skip if already a zibravdb:// URI
        if (pathStr.find("zibravdb://") == 0)
        {
            std::cout << "[ZibraVDB] Skipping zibravdb:// URI: " << pathStr << std::endl;
            return false;
        }
        
        // Only process .vdb files
        bool isVDB = pathStr.find(".vdb") != std::string::npos;
        bool exists = std::filesystem::exists(pathStr);

        if (isVDB)
        {
            if (exists)
            {
                std::cout << "[ZibraVDB] FILE ALLREADY EXISTS" << std::endl;
                return false;
            }
            else
            {
                std::cout << "[ZibraVDB] FILE DOESNT EXISTS: " << std::endl;
                return true;
            }
        }
        std::cout << "KAPLUN [ZibraVDB] IS NOT VDB" << std::endl;
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

    bool ZibraVDBOutputProcessor::compressVDBFile(const std::string& vdbPath,
                                                 const std::string& outputDir,
                                                 UT_String& zibraVDBPath,
                                                 UT_String& error)
    {
        std::cout << "[ZibraVDB] ========== CompressVDBFile ==========" << std::endl;
        std::cout << "[ZibraVDB] vdbPath: '" << vdbPath << "'" << std::endl;
        std::cout << "[ZibraVDB] outputDir: '" << outputDir << "'" << std::endl;
        
        // Generate zibravdb:// URI immediately based on the VDB filename
        std::filesystem::path vdbFilePath(vdbPath);
        std::string compressedName = vdbFilePath.stem().string() + "_compressed.zibravdb";
        zibraVDBPath = "zibravdb://./compressed/" + compressedName + "?frame=0";
        
        std::cout << "[ZibraVDB] Generated URI: " << zibraVDBPath.toStdString() << std::endl;
        
        try
        {
            // Check if VDB file exists
            if (!std::filesystem::exists(vdbPath))
            {
                // VDB file doesn't exist yet - defer compression but return URI now
                std::cout << "[ZibraVDB] VDB file will be created during export, scheduling for later compression..." << std::endl;
                
                // Store the path for later processing with the generated compressed filename
                std::string compressedPath = (std::filesystem::path(outputDir) / "compressed" / compressedName).string();
                m_DeferredCompressionPaths.push_back({vdbPath, outputDir, compressedPath});
                
                std::cout << "[ZibraVDB] Scheduled for compression: " << vdbPath << " -> " << compressedPath << std::endl;
                std::cout << "[ZibraVDB] Total deferred paths: " << m_DeferredCompressionPaths.size() << std::endl;
                return true; // Return true with the URI
            }
            
            std::cout << "[ZibraVDB] VDB file exists, starting compression..." << std::endl;

            // Create compressed directory
            std::filesystem::path compressedDir = std::filesystem::path(outputDir) / "compressed";
            std::filesystem::create_directories(compressedDir);

            // Generate compressed file name (use the same logic as the URI generation above)
            std::string compressedName = vdbFilePath.stem().string() + "_compressed.zibravdb";
            std::string compressedPath = (compressedDir / compressedName).string();

            // Start compression sequence
            auto status = m_CompressorManager.StartSequence(UT_String(compressedPath));
            if (status != CE::ZCE_SUCCESS)
            {
                error = "Failed to start compression sequence";
                return false;
            }

            // Load and compress the VDB file
            std::vector<openvdb::GridBase::ConstPtr> volumes;
            std::vector<std::string> channelNames;
            std::vector<const char*> channelCStrings;
            CE::Compression::CompressFrameDesc compressFrameDesc{};
            CE::Addons::OpenVDBUtils::FrameLoader* vdbFrameLoader = nullptr;
            
            try
            {
                // Read VDB file and extract grids
                openvdb::io::File vdbFile(vdbPath);
                vdbFile.open();
                
                // Read all grids from the VDB file
                for (auto nameIter = vdbFile.beginName(); nameIter != vdbFile.endName(); ++nameIter)
                {
                    std::cout << "[ZibraVDB] Found grid: " << nameIter.gridName() << std::endl;
                    
                    openvdb::GridBase::ConstPtr grid = vdbFile.readGrid(nameIter.gridName());
                    if (grid)
                    {
                        volumes.push_back(grid);
                        channelNames.push_back(nameIter.gridName());
                        channelCStrings.push_back(channelNames.back().c_str());
                    }
                }
                vdbFile.close();
                
                if (volumes.empty())
                {
                    error = "No grids found in VDB file";
                    std::cout << "[ZibraVDB] ERROR: No grids found" << std::endl;
                    return false;
                }
                
                std::cout << "[ZibraVDB] Loaded " << volumes.size() << " grids" << std::endl;
                
                // Create frame loader with OpenVDB grids
                vdbFrameLoader = new CE::Addons::OpenVDBUtils::FrameLoader{volumes.data(), volumes.size()};
                CE::Addons::OpenVDBUtils::EncodingMetadata encodingMetadata{};
                
                compressFrameDesc.channelsCount = channelNames.size();
                compressFrameDesc.channels = channelCStrings.data();
                compressFrameDesc.frame = vdbFrameLoader->LoadFrame(&encodingMetadata);

                // Compress the frame
                CE::Compression::FrameManager* frameManager = nullptr;
                status = m_CompressorManager.CompressFrame(compressFrameDesc, &frameManager);
                if (status != CE::ZCE_SUCCESS)
                {
                    error = "Failed to compress VDB frame";
                    return false;
                }

                // Finish the frame
                if (frameManager)
                {
                    status = frameManager->Finish();
                    if (status != CE::ZCE_SUCCESS)
                    {
                        error = "Failed to finish frame compression";
                        return false;
                    }
                }

                // Finish the sequence
                std::string warning;
                status = m_CompressorManager.FinishSequence(warning);
                if (status != CE::ZCE_SUCCESS)
                {
                    error = "Failed to finish compression sequence";
                    if (vdbFrameLoader && compressFrameDesc.frame) {
                        vdbFrameLoader->ReleaseFrame(compressFrameDesc.frame);
                    }
                    delete vdbFrameLoader;
                    return false;
                }
                
                // Release the frame and cleanup
                if (vdbFrameLoader && compressFrameDesc.frame) {
                    vdbFrameLoader->ReleaseFrame(compressFrameDesc.frame);
                }
                delete vdbFrameLoader;
                vdbFrameLoader = nullptr;
            }
            catch (const std::exception& e)
            {
                error = "Exception during VDB loading: " + std::string(e.what());
                if (vdbFrameLoader && compressFrameDesc.frame) {
                    vdbFrameLoader->ReleaseFrame(compressFrameDesc.frame);
                }
                delete vdbFrameLoader;
                return false;
            }

            std::cout << "[ZibraVDB] Immediate compression completed successfully" << std::endl;
            return true;
        }
        catch (const std::exception& e)
        {
            error = "Exception during compression: " + std::string(e.what());
            return false;
        }
    }


    void ZibraVDBOutputProcessor::processDeferredCompressions()
    {
        std::cout << "[ZibraVDB] ========== Processing Deferred Compressions ==========" << std::endl;
        std::cout << "[ZibraVDB] Found " << m_DeferredCompressionPaths.size() << " deferred compression tasks" << std::endl;
        
        // Make sure library is still loaded
        if (!LibraryUtils::IsLibraryLoaded())
        {
            std::cout << "[ZibraVDB] ERROR: Library not loaded for deferred compression" << std::endl;
            return;
        }

        if (m_DeferredCompressionPaths.empty())
        {
            std::cout << "[ZibraVDB] No deferred compressions to process" << std::endl;
            return;
        }

        auto first_entry = m_DeferredCompressionPaths.at(0);
        // Create compressed directory
        std::filesystem::path compressedDir = std::filesystem::path(first_entry.outputDir) / "compressed";
        std::filesystem::create_directories(compressedDir);

        std::cout << "[ZibraVDB] Compressing to: " << first_entry.compressedPath << std::endl;

        // Start compression sequence using predetermined path
        auto status = m_CompressorManager.StartSequence(UT_String(first_entry.compressedPath));
        std::cout << "[ZibraVDB] StartSequence status: " << static_cast<int>(status) << std::endl;

        if (status != CE::ZCE_SUCCESS)
        {
            std::cout << "[ZibraVDB] Failed to start compression sequence" << std::endl;
            //                        localCompressor.Release();
            return;
        }

        for (const auto& entry : m_DeferredCompressionPaths)
        {
            std::cout << "[ZibraVDB] Processing deferred: " << entry.vdbPath << std::endl;
            
            // Check if the VDB file now exists
            if (std::filesystem::exists(entry.vdbPath))
            {
                // Check VDB file size to ensure it's fully written
                auto fileSize = std::filesystem::file_size(entry.vdbPath);
                std::cout << "[ZibraVDB] VDB file exists, size: " << fileSize << " bytes" << std::endl;
                
                if (fileSize < 1024) // If less than 1KB, might not be fully written
                {
                    std::cout << "[ZibraVDB] VDB file too small, might not be fully written yet" << std::endl;
                    continue;
                }
                
                std::cout << "[ZibraVDB] VDB file ready for compression..." << std::endl;
                
                try
                {
                    // Load VDB file and validate content
                    std::cout << "[ZibraVDB] Loading VDB file: " << entry.vdbPath << std::endl;
                    
                    openvdb::io::File vdbFile(entry.vdbPath);
                    vdbFile.open();
                    
                    std::vector<openvdb::GridBase::ConstPtr> volumes;
                    std::vector<std::string> channelNames;
                    std::vector<const char*> channelCStrings;
                    
                    // Read all grids (same as ROP implementation)
                    for (auto nameIter = vdbFile.beginName(); nameIter != vdbFile.endName(); ++nameIter)
                    {
                        std::string gridName = nameIter.gridName();
                        std::cout << "[ZibraVDB] Found grid: " << gridName << std::endl;
                        
                        openvdb::GridBase::ConstPtr grid = vdbFile.readGrid(gridName);
                        if (grid)
                        {
                            volumes.push_back(grid);
                            channelNames.push_back(gridName);
                            channelCStrings.push_back(channelNames.back().c_str());
                            
                            // Print grid details for debugging
                            auto bbox = grid->evalActiveVoxelBoundingBox();
                            std::cout << "[ZibraVDB] Grid '" << gridName << "' loaded, active voxels: " 
                                      << grid->activeVoxelCount() << ", bbox: " 
                                      << bbox.min().asPointer()[0] << "," << bbox.min().asPointer()[1] << "," << bbox.min().asPointer()[2]
                                      << " to " 
                                      << bbox.max().asPointer()[0] << "," << bbox.max().asPointer()[1] << "," << bbox.max().asPointer()[2] << std::endl;
                        }
                        else
                        {
                            std::cout << "[ZibraVDB] Failed to load grid: " << gridName << std::endl;
                        }
                    }
                    vdbFile.close();
                    
                    if (volumes.empty())
                    {
                        std::cout << "[ZibraVDB] ERROR: No grids found in VDB file" << std::endl;
                        continue;
                    }
                    
                    std::cout << "[ZibraVDB] Successfully loaded " << volumes.size() << " grids" << std::endl;

                    // Create frame loader (exactly like ROP)
                    CE::Addons::OpenVDBUtils::FrameLoader frameLoader{volumes.data(), volumes.size()};
                    CE::Addons::OpenVDBUtils::EncodingMetadata encodingMetadata{};
                    
                    std::cout << "[ZibraVDB] Loading frame with " << volumes.size() << " volumes..." << std::endl;
                    
                    // Load frame
                    auto* frame = frameLoader.LoadFrame(&encodingMetadata);
                    if (!frame)
                    {
                        std::cout << "[ZibraVDB] ERROR: Failed to load frame" << std::endl;
                        continue;
                    }
                    
                    std::cout << "[ZibraVDB] Frame loaded successfully" << std::endl;
                    
                    // Set up compression descriptor (exactly like ROP)
                    CE::Compression::CompressFrameDesc compressFrameDesc{};
                    compressFrameDesc.channelsCount = channelNames.size();
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

                    // Release frame (important!)
                    frameLoader.ReleaseFrame(frame);

                    if (status == CE::ZCE_SUCCESS)
                    {
                        std::cout << "[ZibraVDB] Successfully compressed: " << entry.vdbPath << " -> " << entry.compressedPath << std::endl;
                    }
                    else
                    {
                        std::cout << "[ZibraVDB] Compression failed with status: " << static_cast<int>(status) << std::endl;
                    }
                }
                catch (const std::exception& e)
                {
                    std::cout << "[ZibraVDB] Exception during deferred compression: " << e.what() << std::endl;
                }
            }
            else
            {
                std::cout << "[ZibraVDB] VDB file still doesn't exist: " << entry.vdbPath << std::endl;
            }
        }

        std::string warning;
        status = m_CompressorManager.FinishSequence(warning);
        std::cout << "[ZibraVDB] FinishSequence status: " << static_cast<int>(status) << std::endl;
        
        if (status != CE::ZCE_SUCCESS)
        {
            std::cout << "[ZibraVDB] ERROR: FinishSequence failed with status: " << static_cast<int>(status) << std::endl;
            if (!warning.empty()) {
                std::cout << "[ZibraVDB] Warning: " << warning << std::endl;
            }
        }
        else
        {
            std::cout << "[ZibraVDB] Compression sequence finished successfully" << std::endl;
        }
        
        std::cout << "[ZibraVDB] Deferred compression processing complete" << std::endl;
    }

    void ZibraVDBOutputProcessor::traverseSOPCreateNodes(OP_Node* lop_node, fpreal t)
    {
        if (!lop_node)
            return;
            
        std::cout << "[ZibraVDB] Traversing SOP Create nodes: " << lop_node->getName() << std::endl;
        
        // Check if this is a SOP Create LOP node
        if (lop_node->getOperator()->getName().equal("sopcreate"))
        {
            std::cout << "[ZibraVDB] Found SOP Create node: " << lop_node->getName() << std::endl;
            
            // Debug: List all parameters on this node with their values
            std::cout << "[ZibraVDB] Examining parameters on node: " << lop_node->getName() << std::endl;
            for (int i = 0; i < lop_node->getParmList()->getEntries(); ++i)
            {
                PRM_Parm* parm = lop_node->getParmList()->getParmPtr(i);
                if (parm)
                {
                    UT_String paramValue;
                    // Try to get the parameter value
                    try 
                    {
                        lop_node->evalString(paramValue, parm->getToken(), 0, t);
                        std::cout << "[ZibraVDB] Parameter " << i << ": '" << parm->getToken() 
                                  << "' = '" << paramValue.toStdString() << "'" << std::endl;
                    }
                    catch (...)
                    {
                        std::cout << "[ZibraVDB] Parameter " << i << ": '" << parm->getToken() 
                                  << "' (value not accessible as string)" << std::endl;
                    }
                }
            }
            
            // Try common SOP Create LOP parameter names to find the SOP path
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
                    std::cout << "[ZibraVDB] Parameter '" << paramName << "': '" << sopPath.toStdString() << "'" << std::endl;
                    if (sopPath.length() > 0) break;
                }
            }
            
            if (sopPath.length() > 0)
            {
                OP_Node* referencedNode = lop_node->findNode(sopPath);
                if (referencedNode)
                {
                    std::cout << "[ZibraVDB] Referenced node found: " << referencedNode->getName() << std::endl;
                    SOP_Node* sopNode = nullptr;
                    
                    if (referencedNode->getOpTypeID() == SOP_OPTYPE_ID)
                    {
                        std::cout << "[ZibraVDB] Direct SOP node reference" << std::endl;
                        sopNode = static_cast<SOP_Node*>(referencedNode);
                    }
                    else if (referencedNode->isNetwork())
                    {
                        std::cout << "[ZibraVDB] SOP network reference, finding display node" << std::endl;
                        OP_Network* sopNetwork = static_cast<OP_Network*>(referencedNode);
                        OP_Node* outputNode = sopNetwork->getDisplayNodePtr();
                        if (outputNode && outputNode->getOpTypeID() == SOP_OPTYPE_ID)
                        {
                            std::cout << "[ZibraVDB] Found display SOP node: " << outputNode->getName() << std::endl;
                            sopNode = static_cast<SOP_Node*>(outputNode);
                        }
                    }
                    
                    if (sopNode)
                    {
                        extractVDBFromSOP(sopNode, t);
                        return; // Exit after processing the SOP node - don't continue recursion
                    }
                }
                else
                {
                    std::cout << "[ZibraVDB] Referenced node not found: " << sopPath.toStdString() << std::endl;
                }
            }
            else
            {
                std::cout << "[ZibraVDB] No SOP path found in parameters, trying alternative methods..." << std::endl;
                
                // Alternative 1: Check if there's a direct SOP input connection
                std::cout << "[ZibraVDB] Checking " << lop_node->nInputs() << " input connections..." << std::endl;
                for (int input_idx = 0; input_idx < lop_node->nInputs(); ++input_idx)
                {
                    OP_Node* inputNode = lop_node->getInput(input_idx);
                    if (inputNode)
                    {
                        std::cout << "[ZibraVDB] Input " << input_idx << ": " << inputNode->getName() 
                                  << " (type: " << inputNode->getOpTypeID() << ")" << std::endl;
                        
                        if (inputNode->getOpTypeID() == SOP_OPTYPE_ID)
                        {
                            std::cout << "[ZibraVDB] Found direct SOP input connection: " << inputNode->getName() << std::endl;
                            extractVDBFromSOP(static_cast<SOP_Node*>(inputNode), t);
                            return;
                        }
                        // Check if input is a SOP network
                        else if (inputNode->isNetwork())
                        {
                            std::cout << "[ZibraVDB] Input is a network, checking for SOP content..." << std::endl;
                            OP_Network* inputNetwork = static_cast<OP_Network*>(inputNode);
                            
                            // Try display node
                            OP_Node* displayNode = inputNetwork->getDisplayNodePtr();
                            if (displayNode && displayNode->getOpTypeID() == SOP_OPTYPE_ID)
                            {
                                std::cout << "[ZibraVDB] Found SOP via input network display node: " << displayNode->getName() << std::endl;
                                extractVDBFromSOP(static_cast<SOP_Node*>(displayNode), t);
                                return;
                            }
                            
                            // Try render node
                            OP_Node* renderNode = inputNetwork->getRenderNodePtr();
                            if (renderNode && renderNode->getOpTypeID() == SOP_OPTYPE_ID)
                            {
                                std::cout << "[ZibraVDB] Found SOP via input network render node: " << renderNode->getName() << std::endl;
                                extractVDBFromSOP(static_cast<SOP_Node*>(renderNode), t);
                                return;
                            }
                        }
                    }
                }
                
                // Alternative 2: Try to access the SOP Create node's internal SOP network
                std::cout << "[ZibraVDB] Attempting to access internal SOP network of SOP Create node..." << std::endl;
                
                if (lop_node->isNetwork())
                {
                    OP_Network* sopCreateNetwork = static_cast<OP_Network*>(lop_node);
                    std::cout << "[ZibraVDB] SOP Create node has " << sopCreateNetwork->getNchildren() << " child nodes" << std::endl;
                    
                    // Look for internal SOP nodes
                    for (int i = 0; i < sopCreateNetwork->getNchildren(); ++i)
                    {
                        OP_Node* child = sopCreateNetwork->getChild(i);
                        if (child)
                        {
                            std::cout << "[ZibraVDB] Child " << i << ": " << child->getName() 
                                      << " (type: " << child->getOpTypeID() << ")" << std::endl;
                            
                            if (child->getOpTypeID() == SOP_OPTYPE_ID)
                            {
                                std::cout << "[ZibraVDB] Found internal SOP node: " << child->getName() << std::endl;
                                
                                // Check if this SOP has VDB data
                                SOP_Node* sopChild = static_cast<SOP_Node*>(child);
                                OP_Context context(t);
                                const GU_Detail* testGdp = sopChild->getCookedGeo(context);
                                if (testGdp)
                                {
                                    // Check for VDB primitives
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
                                        std::cout << "[ZibraVDB] Found VDB data in internal SOP: " << child->getName() << std::endl;
                                        extractVDBFromSOP(sopChild, t);
                                        return;
                                    }
                                }
                            }
                            else if (child->isNetwork())
                            {
                                // Recursively check child networks for SOPs
                                OP_Network* childNetwork = static_cast<OP_Network*>(child);
                                OP_Node* displayNode = childNetwork->getDisplayNodePtr();
                                if (displayNode && displayNode->getOpTypeID() == SOP_OPTYPE_ID)
                                {
                                    std::cout << "[ZibraVDB] Found SOP in child network: " << displayNode->getName() << std::endl;
                                    extractVDBFromSOP(static_cast<SOP_Node*>(displayNode), t);
                                    return;
                                }
                            }
                        }
                    }
                }
                
                std::cout << "[ZibraVDB] Could not find VDB data in SOP Create node through any method" << std::endl;
            }
        }
        
        // Only traverse child nodes if this is NOT a SOP Create node (to avoid infinite loops)
        if (!lop_node->getOperator()->getName().equal("sopcreate") && lop_node->isNetwork())
        {
            OP_Network* network = static_cast<OP_Network*>(lop_node);
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
            
        std::cout << "[ZibraVDB] Extracting VDB data from SOP node: " << sopNode->getName() << std::endl;
        
        // Create context for cooking
        OP_Context context(t);
        
        // Get geometry from SOP node
        const GU_Detail* gdp = sopNode->getCookedGeo(context);
        
        if (!gdp)
        {
            std::cout << "[ZibraVDB] Could not get geometry from SOP node" << std::endl;
            return;
        }
        
        std::cout << "[ZibraVDB] Got geometry, checking for VDB primitives..." << std::endl;
        
        // Extract VDB primitives and cache them
        std::vector<openvdb::GridBase::ConstPtr> grids;
        std::vector<std::string> gridNames;
        
        for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
        {
            const GA_Primitive* prim = gdp->getPrimitive(*it);
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                const GEO_PrimVDB* vdbPrim = static_cast<const GEO_PrimVDB*>(prim);
                const char* gridName = vdbPrim->getGridName();
                
                openvdb::GridBase::ConstPtr grid = vdbPrim->getConstGridPtr();
                if (grid)
                {
                    grids.push_back(grid);
                    gridNames.push_back(gridName);
                    
                    std::cout << "[ZibraVDB] Found VDB grid: " << gridName 
                              << ", active voxels: " << grid->activeVoxelCount() << std::endl;
                }
            }
        }
        
        if (!grids.empty())
        {
            std::cout << "[ZibraVDB] Found " << grids.size() << " VDB grids in SOP node" << std::endl;
            // Store grids for later use in compression
            m_CachedVDBGrids.insert(m_CachedVDBGrids.end(), grids.begin(), grids.end());
            m_CachedGridNames.insert(m_CachedGridNames.end(), gridNames.begin(), gridNames.end());
        }
        else
        {
            std::cout << "[ZibraVDB] No VDB grids found in SOP node" << std::endl;
        }
    }
    
    void ZibraVDBOutputProcessor::findLOPNetworksFromConfig(OP_Node* config_node, fpreal t)
    {
        if (!config_node)
            return;
            
        std::cout << "[ZibraVDB] Searching for LOP networks from config_node: " << config_node->getName() << std::endl;
        
        // Strategy 1: Check if config_node itself is a LOP node
        if (config_node->getOpTypeID() == LOP_OPTYPE_ID)
        {
            std::cout << "[ZibraVDB] config_node is a LOP node, traversing it" << std::endl;
            traverseSOPCreateNodes(config_node, t);
            return;
        }
        
        // Strategy 2: Check config_node's inputs for LOP nodes
        for (int i = 0; i < config_node->nInputs(); ++i)
        {
            OP_Node* input = config_node->getInput(i);
            if (input && input->getOpTypeID() == LOP_OPTYPE_ID)
            {
                std::cout << "[ZibraVDB] Found LOP input node: " << input->getName() << std::endl;
                traverseSOPCreateNodes(input, t);
            }
        }
        
        // Strategy 3: Look for LOP context in parent network
        OP_Network* parent = config_node->getParent();
        if (parent)
        {
            std::cout << "[ZibraVDB] Searching parent network for LOP context" << std::endl;
            
            // Look for /stage context (common LOP network location)
            OP_Node* stageContext = parent->findNode("/stage");
            if (stageContext && stageContext->isNetwork())
            {
                std::cout << "[ZibraVDB] Found /stage context, searching for SOP Create nodes" << std::endl;
                traverseLOPNetwork(static_cast<OP_Network*>(stageContext), t);
            }
            
            // Also search current parent network for LOP nodes
            traverseLOPNetwork(parent, t);
        }
        
        // Strategy 4: Search for SOP networks with VDB data directly
        searchForSOPNetworks(config_node, t);
    }
    
    void ZibraVDBOutputProcessor::traverseLOPNetwork(OP_Network* network, fpreal t)
    {
        if (!network)
            return;
            
        std::cout << "[ZibraVDB] Traversing LOP network: " << network->getName() << std::endl;
        
        for (int i = 0; i < network->getNchildren(); ++i)
        {
            OP_Node* child = network->getChild(i);
            if (child)
            {
                // Check if this is a SOP Create LOP node
                if (child->getOpTypeID() == LOP_OPTYPE_ID && 
                    child->getOperator()->getName().equal("sopcreate"))
                {
                    std::cout << "[ZibraVDB] Found SOP Create LOP node: " << child->getName() << std::endl;
                    traverseSOPCreateNodes(child, t);
                }
                // Recursively traverse child networks
                else if (child->isNetwork())
                {
                    traverseLOPNetwork(static_cast<OP_Network*>(child), t);
                }
            }
        }
    }
    
    void ZibraVDBOutputProcessor::searchForSOPNetworks(OP_Node* startNode, fpreal t)
    {
        if (!startNode)
            return;
            
        std::cout << "[ZibraVDB] Searching for SOP networks with VDB data" << std::endl;
        
        OP_Network* parent = startNode->getParent();
        if (parent)
        {
            // Look for /obj context (common SOP network location)
            OP_Node* objContext = parent->findNode("/obj");
            if (objContext && objContext->isNetwork())
            {
                std::cout << "[ZibraVDB] Found /obj context, searching for SOP networks" << std::endl;
                OP_Network* objNetwork = static_cast<OP_Network*>(objContext);
                
                for (int i = 0; i < objNetwork->getNchildren(); i++)
                {
                    OP_Node* child = objNetwork->getChild(i);
                    if (child && child->isNetwork())
                    {
                        // Look for SOP networks with VDB content
                        OP_Network* childNetwork = static_cast<OP_Network*>(child);
                        OP_Node* displayNode = childNetwork->getDisplayNodePtr();
                        if (displayNode && displayNode->getOpTypeID() == SOP_OPTYPE_ID)
                        {
                            std::cout << "[ZibraVDB] Found SOP network: " << child->getName() << std::endl;
                            // Check if this SOP has VDB data
                            extractVDBFromSOP(static_cast<SOP_Node*>(displayNode), t);
                        }
                    }
                }
            }
        }
    }

    // Factory function for registration
    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor()
    {
        return HUSD_OutputProcessorPtr(new ZibraVDBOutputProcessor());
    }

} // namespace Zibra::ZibraVDBOutputProcessor