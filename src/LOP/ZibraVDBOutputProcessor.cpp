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

    ZibraVDBOutputProcessor::ZibraVDBOutputProcessor()
        : m_Parameters(nullptr)
    {
        LibraryUtils::LoadLibrary();
        m_Parameters = new PI_EditScriptedParms();
    }

    ZibraVDBOutputProcessor::~ZibraVDBOutputProcessor()
    {
        delete m_Parameters;
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
        m_DeferredCompressions.clear();
        m_InMemoryCompressionEntries.clear();
        m_curTime = t;
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
        std::cout << "[ZibraVDB] Processing save path: " << asset_path.toStdString() << std::endl;
        if (!asset_is_layer && pathStr.find(".vdb") != std::string::npos)
        {
            std::filesystem::path originalPath(pathStr);
            std::filesystem::path outputPath = originalPath.parent_path();
            std::filesystem::path vdbDir = outputPath / "vdb_files";
            std::string redirectedPath = (vdbDir / (originalPath.stem().string() + ".zibravdb")).string();

            std::filesystem::create_directories(vdbDir);
            newpath = redirectedPath;

            std::cout << "[ZibraVDB] Redirected to new path: " << redirectedPath << std::endl;

            return true;
        }

        std::cout << "[ZibraVDB] Couldn't process save path: " << asset_path.toStdString() << std::endl;
        return false;
    }

    void ZibraVDBOutputProcessor::compressInMemoryGrids(ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker* markerNode)
    {
        auto entry = std::find_if(m_InMemoryCompressionEntries.begin(), m_InMemoryCompressionEntries.end(),
                                  [markerNode](const CompressionSequenceEntry& entry) { return markerNode == entry.markerNode; });

        if (entry == m_InMemoryCompressionEntries.end())
        {
            assert(false && "Marker node not found in in-memory compression entries. This should not happen.");
            return;
        }

        if (!entry->compressorManager)
        {
            entry->compressorManager = new Zibra::CE::Compression::CompressorManager();

            CE::Compression::FrameMappingDecs frameMappingDesc{};
            frameMappingDesc.sequenceStartIndex = 0;
            frameMappingDesc.sequenceIndexIncrement = 1;
            float defaultQuality = entry->markerNode->getCompressionQuality(m_curTime);
            std::vector<std::pair<UT_String, float>> perChannelSettings;

            auto status = entry->compressorManager->Initialize(frameMappingDesc, defaultQuality, perChannelSettings);
            assert(status == CE::ZCE_SUCCESS);

            status = entry->compressorManager->StartSequence(UT_String(entry->outputFile));
            if (status != CE::ZCE_SUCCESS)
            {
                assert(false && "Failed to start sequence for compression, status: " + std::to_string(static_cast<int>(status)));
                return;
            }
        }

        Zibra::Utils::ZibraUSDUtils::SearchUpstreamForSOPNode(
            markerNode, m_curTime,
            [this, cm = entry->compressorManager](SOP_Node* sopNode, fpreal time) { this->extractVDBFromSOP(sopNode, time, cm); });
    }

    void ZibraVDBOutputProcessor::processDeferredSequence(const CompressionSequenceEntry& deferredEntry)
    {
        CE::Compression::FrameMappingDecs frameMappingDesc{};
        frameMappingDesc.sequenceStartIndex = 0;
        frameMappingDesc.sequenceIndexIncrement = 1;
        float quality = deferredEntry.markerNode->getCompressionQuality(m_curTime);

        std::vector<std::pair<UT_String, float>> perChannelSettings;
        Zibra::CE::Compression::CompressorManager compressorManager;
        auto status = compressorManager.Initialize(frameMappingDesc, quality, perChannelSettings);
        assert(status == CE::ZCE_SUCCESS);

        std::filesystem::create_directories(std::filesystem::path(deferredEntry.outputFile).parent_path());
        status = compressorManager.StartSequence(UT_String(deferredEntry.outputFile));
        if (status != CE::ZCE_SUCCESS)
        {
            assert(false && "Failed to start compression sequence for deferred VDBs. Status: " + std::to_string(static_cast<int>(status)));
            return;
        }

        for (const auto& entry : deferredEntry.vdbFiles)
        {
            openvdb::io::File vdbFile(entry);
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

    bool ZibraVDBOutputProcessor::processReferencePath(const UT_StringRef &asset_path,
                                                      const UT_StringRef &referencing_layer_path,
                                                      bool asset_is_layer,
                                                      UT_String &newpath,
                                                      UT_String &error)
    {
        std::string pathStr = asset_path.toStdString();
        std::cout << "[ZibraVDB] Processing reference path: " << asset_path.toStdString() << " for layer" << referencing_layer_path.toStdString() << std::endl;
        if (!asset_is_layer && pathStr.find(".vdb") == pathStr.length() - 4)
        {
            std::string outLayer = referencing_layer_path.toStdString();
            std::filesystem::path outLayerPath(outLayer);
            std::string outLayerName = outLayerPath.stem().string();

            auto* curMarkerNode = findMarkerNodeByName(outLayerName);
            if (!curMarkerNode)
            {
                std::cout << "[ZibraVDB] Could not find marker node: " << outLayerName << std::endl;
                return false;
            }

            std::string newFileName = (outLayerPath.parent_path()/outLayerName).string() + ".zibravdb";
            std::vector<CompressionSequenceEntry>& entries =
                std::filesystem::exists(pathStr) ? m_DeferredCompressions : m_InMemoryCompressionEntries;
            auto sequence =
                std::find_if(entries.begin(), entries.end(),
                             [curMarkerNode](const CompressionSequenceEntry& entry) { return entry.markerNode == curMarkerNode; });
            if (sequence == entries.end())
            {
                entries.push_back({curMarkerNode, nullptr, {pathStr}, newFileName});
            }
            else
            {
                sequence->vdbFiles.push_back(pathStr);
            }

            int frameIndex = parseFrameIndexFromVDBPath(pathStr);
            newpath = "zibravdb://" + newFileName + "?frame=" + std::to_string(frameIndex);

            if (!std::filesystem::exists(pathStr))
            {
                compressInMemoryGrids(curMarkerNode);
            }

            return true;
        }
        std::cout << "[ZibraVDB] Reference path didnt processed: " << asset_path.toStdString() << std::endl;
        return false;
    }

    bool ZibraVDBOutputProcessor::processLayer(const UT_StringRef& identifier, UT_String& error)
    {
        for (const auto& deferredEntry : m_DeferredCompressions)
        {
            processDeferredSequence(deferredEntry);
        }

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


    ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker* ZibraVDBOutputProcessor::findMarkerNodeByName(const std::string& nodeName) const
    {
        OP_Node* rootNode = OPgetDirector()->findNode("/stage");
        if (!rootNode || !rootNode->isNetwork())
        {
            return nullptr;
        }
        
        OP_Network* stageNetwork = static_cast<OP_Network*>(rootNode);
        OP_Node* node = stageNetwork->findNode(nodeName.c_str());
        if (node && dynamic_cast<ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker*>(node))
        {
            return static_cast<ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker*>(node);
        }
        
        return nullptr;
    }

    int ZibraVDBOutputProcessor::parseFrameIndexFromVDBPath(const std::string& vdbPath) const
    {
        try 
        {
            std::filesystem::path path(vdbPath);
            std::string filename = path.stem().string(); // Remove .vdb extension
            
            // Pattern 1: path/name.FRAME_INDEX.vdb (e.g., "volume.1234.vdb")
            // Pattern 2: path/FRAME_INDEX.vdb (e.g., "1234.vdb") 
            // Pattern 3: path/name.vdb (no frame, default to 0)
            
            // Find the last dot in the filename
            size_t lastDot = filename.find_last_of('.');
            if (lastDot != std::string::npos)
            {
                // Extract potential frame number after the last dot
                std::string frameStr = filename.substr(lastDot + 1);
                
                // Check if it's all digits
                if (!frameStr.empty() && std::all_of(frameStr.begin(), frameStr.end(), ::isdigit))
                {
                    return std::stoi(frameStr);
                }
            }
            
            // If no dot found or not a valid frame number, check if entire filename is a number
            if (std::all_of(filename.begin(), filename.end(), ::isdigit) && !filename.empty())
            {
                return std::stoi(filename);
            }
            
            // Default to frame 0 if no frame number found
            std::cout << "[ZibraVDB] No frame number found in: " << vdbPath << ", defaulting to frame 0" << std::endl;
            return 0;
        }
        catch (const std::exception& e)
        {
            std::cout << "[ZibraVDB] Error parsing frame from: " << vdbPath << ", defaulting to frame 0. Error: " << e.what() << std::endl;
            return 0;
        }
    }

//    std::string ZibraVDBOutputProcessor::generateOutputFilePath(std::string sequenceID)
//    {
//        // TODO: Get from actual export path from config node
//        std::string outputDir = "C:/src/usd/ZibraAI/output/compressed";
//        std::filesystem::create_directories(outputDir);
//        // TODO: Get from actual export path from marker node
//        return outputDir + "/" + sequenceID + ".zibravdb";
//    }
//
//    std::string ZibraVDBOutputProcessor::generateOutputFilePathFromMarker(ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker* markerNode, const std::string& sequenceID, fpreal t)
//    {
//        if (!markerNode)
//        {
//            return generateOutputFilePath(sequenceID);  // Fallback to default
//        }
//
//        std::string outputDir = markerNode->getOutputDirectory(t);
//        std::string outputFilename = markerNode->getOutputFilename(t);
//
//        // Replace $OS with the sequence ID if present in the filename
//        std::regex osToken("\\$OS");
//        outputFilename = std::regex_replace(outputFilename, osToken, sequenceID);
//
//        // If no extension specified, add .zibravdb
//        if (outputFilename.find(".zibravdb") == std::string::npos)
//        {
//            outputFilename += ".zibravdb";
//        }
//
//        std::filesystem::create_directories(outputDir);
//        std::filesystem::path fullPath = std::filesystem::path(outputDir) / outputFilename;
//
//        return fullPath.string();
//    }
} // namespace Zibra::ZibraVDBOutputProcessor