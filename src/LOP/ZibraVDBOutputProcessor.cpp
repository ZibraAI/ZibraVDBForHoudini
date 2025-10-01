#include "PrecompiledHeader.h"

#include "ZibraVDBOutputProcessor.h"

#include "SOP/SOP_ZibraVDBUSDExport.h"

namespace Zibra::ZibraVDBOutputProcessor
{
    using namespace std::literals;

    ZibraVDBOutputProcessor::ZibraVDBOutputProcessor() = default;

    ZibraVDBOutputProcessor::~ZibraVDBOutputProcessor() = default;

    bool ZibraVDBOutputProcessor::CheckLibrary(UT_String& error)
    {
        std::cout << "[ZibraVDB] CheckLibrary: ENTER" << std::endl;
        
        if (!LibraryUtils::IsPlatformSupported())
        {
            error = "ZibraVDB Output Processor Error: Platform not supported. Required: Windows/Linux/macOS. Falling back to uncompressed "
                    "VDB files.";
            std::cout << "[ZibraVDB] CheckLibrary: ERROR - " << error.toStdString() << std::endl;
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }

        LibraryUtils::LoadSDKLibrary();
        if (!LibraryUtils::IsSDKLibraryLoaded())
        {
            error = "ZibraVDB Output Processor Error: Failed to load ZibraVDB SDK library. Please check installation.";
            std::cout << "[ZibraVDB] CheckLibrary: ERROR - " << error.toStdString() << std::endl;
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }

        std::cout << "[ZibraVDB] CheckLibrary: EXIT - SUCCESS" << std::endl;
        return true;
    }

    bool ZibraVDBOutputProcessor::CheckLicense(UT_String& error)
    {
        std::cout << "[ZibraVDB] CheckLicense: ENTER" << std::endl;
        
        if (!LibraryUtils::IsSDKLibraryLoaded())
        {
            error = "ZibraVDB Output Processor Error: Failed to load ZibraVDB SDK library. Please check installation.";
            std::cout << "[ZibraVDB] CheckLicense: ERROR - " << error.toStdString() << std::endl;
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }
        if (!LicenseManager::GetInstance().CheckLicense(LicenseManager::Product::Compression))
        {
            error = "ZibraVDB Output Processor Error: No valid license found for compression. Please check your license.";
            std::cout << "[ZibraVDB] CheckLicense: ERROR - " << error.toStdString() << std::endl;
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }

        std::cout << "[ZibraVDB] CheckLicense: EXIT - SUCCESS" << std::endl;
        return true;
    }

    UT_StringHolder ZibraVDBOutputProcessor::displayName() const
    {
        return {"ZibraVDBCompressor"};
    }

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor()
    {
        return std::make_shared<ZibraVDBOutputProcessor>();
    }

    const PI_EditScriptedParms* ZibraVDBOutputProcessor::parameters() const
    {
        return nullptr;
    }

    void ZibraVDBOutputProcessor::beginSave(OP_Node* config_node, const UT_Options& config_overrides, OP_Node* lop_node, fpreal t,
                                            const UT_Options& stage_variables
#if UT_VERSION_INT >= 0x15000000 // Houdini 21.0+
                                            ,
                                            UT_String& error
#endif
    )
    {
        std::cout << "[ZibraVDB] beginSave: ENTER - t=" << t << std::endl;
        m_CompressionEntries.clear();
        m_ConfigNode = config_node;
        std::cout << "[ZibraVDB] beginSave: EXIT" << std::endl;
    }

    bool ZibraVDBOutputProcessor::processSavePath(const UT_StringRef& asset_path, const UT_StringRef& referencing_layer_path,
                                                  bool asset_is_layer, UT_String& newpath, UT_String& error)
    {
        std::cout << "[ZibraVDB] processSavePath: ENTER - asset_path='" << asset_path.toStdString() << "', layer='" << referencing_layer_path.toStdString() << "', is_layer=" << asset_is_layer << std::endl;
        
        std::string pathStr = asset_path.toStdString();

        if (asset_is_layer)
        {
            std::cout << "[ZibraVDB] processSavePath: EXIT - asset is layer, returning false" << std::endl;
            return false;
        }
        std::unordered_map<std::string, std::string> parsedURI;
        if (!Helpers::ParseZibraVDBURI(pathStr, parsedURI))
        {
            std::cout << "[ZibraVDB] processSavePath: EXIT - failed to parse URI, returning false" << std::endl;
            return false;
        }
        
        std::cout << "[ZibraVDB] processSavePath: Parsed URI - path='" << parsedURI["path"] << "', name='" << parsedURI["name"] << "'" << std::endl;

        UT_String errorString;
        if (!CheckLibrary(errorString))
        {
            error = errorString;
            std::cout << "[ZibraVDB] processSavePath: ERROR - CheckLibrary failed: " << error.toStdString() << std::endl;
            return false;
        }

        if (!CheckLicense(errorString))
        {
            error = errorString;
            std::cout << "[ZibraVDB] processSavePath: ERROR - CheckLicense failed: " << error.toStdString() << std::endl;
            return false;
        }

        auto nodeIt = parsedURI.find("node");
        if (nodeIt == parsedURI.end() || nodeIt->second.empty())
        {
            error = nodeIt == parsedURI.end() ? "ZibraVDB Output Processor Error: Missing required node parameter in .zibravdb path"
                                              : "ZibraVDB Output Processor Error: Empty node parameter in .zibravdb path";
            std::cout << "[ZibraVDB] processSavePath: ERROR - " << error.toStdString() << std::endl;
            return false;
        }

        auto frameIt = parsedURI.find("frame");
        if (frameIt == parsedURI.end() || frameIt->second.empty())
        {
            error = frameIt == parsedURI.end() ? "ZibraVDB Output Processor Error: Missing required frame parameter in .zibravdb path"
                                               : "ZibraVDB Output Processor Error: Empty frame parameter in .zibravdb path";
            std::cout << "[ZibraVDB] processSavePath: ERROR - " << error.toStdString() << std::endl;
            return false;
        }
        
        std::cout << "[ZibraVDB] processSavePath: Found node='" << nodeIt->second << "', frame='" << frameIt->second << "'" << std::endl;

        std::string decoded_node_name = nodeIt->second;
        std::string frame_str = frameIt->second;
        std::string file_path = parsedURI["path"] + "/" + parsedURI["name"];
        
        std::cout << "[ZibraVDB] processSavePath: Looking for node '" << decoded_node_name << "'" << std::endl;

        OP_Node* op_node = OPgetDirector()->findNode(decoded_node_name.c_str());
        if (!op_node)
        {
            error = "ZibraVDB Output Processor Error: Could not find SOP node at path: " + decoded_node_name;
            std::cout << "[ZibraVDB] processSavePath: ERROR - " << error.toStdString() << std::endl;
            return false;
        }
        
        std::cout << "[ZibraVDB] processSavePath: Found node, attempting cast to ZibraVDB USD Export" << std::endl;

        auto* sop_node = dynamic_cast<Zibra::ZibraVDBUSDExport::SOP_ZibraVDBUSDExport*>(op_node);
        if (!sop_node)
        {
            error = "ZibraVDB Output Processor Error: Node is not a ZibraVDB USD Export node: " + decoded_node_name;
            std::cout << "[ZibraVDB] processSavePath: ERROR - " << error.toStdString() << std::endl;
            return false;
        }
        
        std::cout << "[ZibraVDB] processSavePath: Successfully cast to ZibraVDB USD Export node" << std::endl;

        int frame_index = std::stoi(frame_str);
        float quality = sop_node->GetCompressionQuality();

        std::string dir = parsedURI["path"];
        std::filesystem::path namePath(parsedURI["name"]);
        std::string name = namePath.stem().string();
        std::string output_file_str = "zibravdb://" + dir + "/" + name + "." + std::to_string(frame_index) + ".vdb";

        auto it = m_CompressionEntries.find(sop_node);
        if (it == m_CompressionEntries.end())
        {
            CompressionEntry entry{};
            entry.referencingLayerPath = referencing_layer_path.toStdString();
            entry.outputFile = file_path;
            entry.quality = quality;
            entry.compressorManager = new CE::Compression::CompressorManager();

            CE::Compression::FrameMappingDecs frameMappingDesc{};
            frameMappingDesc.sequenceStartIndex = frame_index;
            frameMappingDesc.sequenceIndexIncrement = 1;
            std::vector<std::pair<UT_String, float>> perChannelSettings;
            if (sop_node->UsePerChannelCompressionSettings())
            {
                perChannelSettings = sop_node->GetPerChannelCompressionSettings();
            }

            std::filesystem::create_directories(std::filesystem::path(file_path).parent_path());
            auto status = entry.compressorManager->Initialize(frameMappingDesc, quality, perChannelSettings);
            assert(status == CE::ZCE_SUCCESS);

            status = entry.compressorManager->StartSequence(UT_String(file_path));
            if (status != CE::ZCE_SUCCESS)
            {
                error = "ZibraVDB Output Processor Error: Failed to start compression sequence, status: " + std::to_string(status);
                return false;
            }
            entry.frameOutputs.emplace_back(frame_index, output_file_str);
            m_CompressionEntries[sop_node] = std::move(entry);
        }
        else
        {
            it->second.frameOutputs.emplace_back(frame_index, output_file_str);
        }

        // Store the zibravdb path for processReferencePath lookup, but return invalid path to prevent original VDB saving
        // This makes Linux behavior consistent with Windows where original VDBs are not saved
        newpath = "."; // Return illegal path "." to prevent saving original VDB
        
        std::cout << "[ZibraVDB] processSavePath: EXIT - SUCCESS, newpath='" << newpath.toStdString() << "'" << std::endl;
        return true;
    }

    bool ZibraVDBOutputProcessor::processReferencePath(const UT_StringRef& asset_path, const UT_StringRef& referencing_layer_path,
                                                       bool asset_is_layer, UT_String& newpath, UT_String& error)
    {
        std::cout << "[ZibraVDB] processReferencePath: ENTER - asset_path='" << asset_path.toStdString() << "', layer='" << referencing_layer_path.toStdString() << "'" << std::endl;
        
        if (asset_is_layer)
        {
            std::cout << "[ZibraVDB] processReferencePath: EXIT - asset is layer, returning false" << std::endl;
            return false;
        }

        std::string pathStr = asset_path.toStdString();
        double t;
        std::string extractedPath;

        if (!Helpers::ParseRelSOPNodeParams(pathStr, t, extractedPath))
        {
            std::cout << "[ZibraVDB] processReferencePath: EXIT - failed to parse SOP node params, returning false" << std::endl;
            return false;
        }
        
        std::cout << "[ZibraVDB] processReferencePath: Parsed - t=" << t << ", extractedPath='" << extractedPath << "'" << std::endl;

        std::string layerPath = referencing_layer_path.toStdString();

        // We search for the compression entry which we added in processSavePath
        auto it = std::find_if(m_CompressionEntries.begin(), m_CompressionEntries.end(), [&layerPath, &extractedPath](const auto& entry) {
            bool layerMatches = entry.second.referencingLayerPath == layerPath;
            std::string sopNodePath = entry.first->getFullPath().c_str();

            bool pathMatches = extractedPath.empty() || sopNodePath.find(extractedPath) == 0;
            return layerMatches && pathMatches;
        });

        if (it == m_CompressionEntries.end())
        {
            return false;
        }

        auto& [sopNode, entry] = *it;
        int compressionFrameIndex = OPgetDirector()->getChannelManager()->getFrame(t);
        std::string outputRef = entry.outputFile + "?frame=" + std::to_string(compressionFrameIndex);
        newpath = UT_String(outputRef);

        // If we want to bake not from beginning, we want to cook our SOP for every preceding frame
        int frameIndex = entry.frameOutputs[0].first;
        if (compressionFrameIndex == frameIndex)
        {
            if (compressionFrameIndex > 0)
            {
                for (int i = 0; i < compressionFrameIndex; ++i)
                {
                    fpreal tmpTime = OPgetDirector()->getChannelManager()->getTime(i);
                    ExtractVDBFromSOP(sopNode, tmpTime, entry.compressorManager, false);
                }
            }
        }
        ExtractVDBFromSOP(sopNode, t, entry.compressorManager);
        
        std::cout << "[ZibraVDB] processReferencePath: EXIT - SUCCESS, newpath='" << newpath.toStdString() << "'" << std::endl;
        return true;
    }

    bool ZibraVDBOutputProcessor::processLayer(const UT_StringRef& identifier, UT_String& error)
    {
        for (const auto& [sopNode, entry] : m_CompressionEntries)
        {
            if (entry.compressorManager)
            {
                std::string warning;
                auto status = entry.compressorManager->FinishSequence(warning);
                if (status != CE::ZCE_SUCCESS)
                {
                    error = "Failed to finish compression sequence for in-memory VDBs. Status: " + std::to_string(status);
                    return false;
                }
                entry.compressorManager->Release();
                delete entry.compressorManager;
            }
        }
        m_CompressionEntries.clear();

        return true;
    }

    void ZibraVDBOutputProcessor::ExtractVDBFromSOP(SOP_Node* sopNode, fpreal t, CE::Compression::CompressorManager* compressorManager,
                                                    bool compress)
    {
        std::cout << "[ZibraVDB] ExtractVDBFromSOP: ENTER - t=" << t << ", compress=" << compress << std::endl;
        
        if (!sopNode)
        {
            std::cout << "[ZibraVDB] ExtractVDBFromSOP: ERROR - sopNode is null" << std::endl;
            return;
        }
        
        std::cout << "[ZibraVDB] ExtractVDBFromSOP: sopNode path='" << sopNode->getFullPath().c_str() << "'" << std::endl;

        OP_Context context(t);
        sopNode->flags().setTimeDep(true);
        sopNode->forceRecook();
        sopNode->cook(context);

        if (!compress)
        {
            // If we are not compressing, we just need to ensure the SOP node is cooked
            std::cout << "[ZibraVDB] ExtractVDBFromSOP: EXIT - not compressing, just cooked node" << std::endl;
            return;
        }
        
        std::cout << "[ZibraVDB] ExtractVDBFromSOP: Getting cooked geometry" << std::endl;

        GU_DetailHandle gdh = sopNode->getCookedGeoHandle(context);
        GU_DetailHandleAutoReadLock gdl(gdh);
        const GU_Detail* gdp = gdl.getGdp();

        if (!gdp)
        {
            UT_String error = "SOP node returned null geometry";
            std::cout << "[ZibraVDB] ExtractVDBFromSOP: ERROR - " << error.toStdString() << std::endl;
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
            return;
        }
        
        std::cout << "[ZibraVDB] ExtractVDBFromSOP: Found " << gdp->getNumPrimitives() << " primitives" << std::endl;

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
        std::cout << "[ZibraVDB] ExtractVDBFromSOP: Found " << grids.size() << " VDB grids" << std::endl;
        
        if (!grids.empty())
        {
            CompressGrids(grids, gridNames, compressorManager, gdp);
        }
        else
        {
            UT_String error = "No VDB grids found in SOP node";
            std::cout << "[ZibraVDB] ExtractVDBFromSOP: ERROR - " << error.toStdString() << std::endl;
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
        }
        
        std::cout << "[ZibraVDB] ExtractVDBFromSOP: EXIT" << std::endl;
    }

    void ZibraVDBOutputProcessor::CompressGrids(std::vector<openvdb::GridBase::ConstPtr>& grids, const std::vector<std::string>& gridNames,
                                                CE::Compression::CompressorManager* compressorManager, const GU_Detail* gdp)
    {
        std::cout << "[ZibraVDB] CompressGrids: ENTER - " << grids.size() << " grids" << std::endl;
        
        if (grids.empty())
        {
            UT_String error = "No grids to compress";
            std::cout << "[ZibraVDB] CompressGrids: ERROR - " << error.toStdString() << std::endl;
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
            return;
        }

        std::vector<const char*> channelCStrings;
        for (const auto& name : gridNames)
        {
            channelCStrings.push_back(name.c_str());
        }
        CE::Addons::OpenVDBUtils::FrameLoader frameLoader{grids.data(), grids.size()};
        CE::Addons::OpenVDBUtils::EncodingMetadata encodingMetadata{};
        std::cout << "[ZibraVDB] CompressGrids: Loading frame" << std::endl;
        auto frame = frameLoader.LoadFrame(&encodingMetadata);
        if (!frame)
        {
            UT_String error = "Failed to load frame from memory grids";
            std::cout << "[ZibraVDB] CompressGrids: ERROR - " << error.toStdString() << std::endl;
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
            return;
        }
        
        std::cout << "[ZibraVDB] CompressGrids: Frame loaded successfully" << std::endl;
        CE::Compression::CompressFrameDesc compressFrameDesc{};
        compressFrameDesc.channelsCount = gridNames.size();
        compressFrameDesc.channels = channelCStrings.data();
        compressFrameDesc.frame = frame;

        CE::Compression::FrameManager* frameManager = nullptr;
        std::cout << "[ZibraVDB] CompressGrids: Compressing frame with " << gridNames.size() << " channels" << std::endl;
        auto status = compressorManager->CompressFrame(compressFrameDesc, &frameManager);
        if (status != CE::ZCE_SUCCESS || !frameManager)
        {
            UT_String error = ("CompressFrame failed or frameManager is null for in-memory grids: status " + std::to_string(status)).c_str();
            std::cout << "[ZibraVDB] CompressGrids: ERROR - " << error.toStdString() << std::endl;
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
            frameLoader.ReleaseFrame(frame);
            return;
        }
        
        std::cout << "[ZibraVDB] CompressGrids: Frame compressed successfully" << std::endl;

        const auto& gridsShuffleInfo = frameLoader.GetGridsShuffleInfo();

        auto frameMetadata = Utils::MetadataHelper::DumpAttributes(gdp, encodingMetadata);
        frameMetadata.emplace_back("chShuffle", Utils::MetadataHelper::DumpGridsShuffleInfo(gridsShuffleInfo).dump());
        for (const auto& [key, val] : frameMetadata)
        {
            frameManager->AddMetadata(key.c_str(), val.c_str());
        }

        std::cout << "[ZibraVDB] CompressGrids: Finishing frame manager" << std::endl;
        status = frameManager->Finish();
        frameLoader.ReleaseFrame(frame);
        
        std::cout << "[ZibraVDB] CompressGrids: EXIT - status=" << status << std::endl;
    }
} // namespace Zibra::ZibraVDBOutputProcessor