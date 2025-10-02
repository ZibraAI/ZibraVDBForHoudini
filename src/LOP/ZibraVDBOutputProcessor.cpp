#include "PrecompiledHeader.h"

#include "ZibraVDBOutputProcessor.h"

#include "SOP/SOP_ZibraVDBUSDExport.h"

namespace Zibra::ZibraVDBOutputProcessor
{
    using namespace std::literals;

    bool ZibraVDBOutputProcessor::CheckLibrary(UT_String& error)
    {
        if constexpr (!LibraryUtils::IsPlatformSupported())
        {
            error = "ZibraVDB Output Processor Error: Platform not supported. Required: Windows/Linux/macOS. Falling back to uncompressed "
                    "VDB files.";
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }

        LibraryUtils::LoadSDKLibrary();
        if (!LibraryUtils::IsSDKLibraryLoaded())
        {
            error = "ZibraVDB Output Processor Error: Failed to load ZibraVDB SDK library. Please check installation.";
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }

        return true;
    }

    bool ZibraVDBOutputProcessor::CheckLicense(UT_String& error)
    {
        if (!LibraryUtils::IsSDKLibraryLoaded())
        {
            error = "ZibraVDB Output Processor Error: Failed to load ZibraVDB SDK library. Please check installation.";
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }
        if (!LicenseManager::GetInstance().CheckLicense(LicenseManager::Product::Compression))
        {
            error = "ZibraVDB Output Processor Error: No valid license found for compression. Please check your license.";
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }

        return true;
    }

    UT_StringHolder ZibraVDBOutputProcessor::displayName() const
    {
        return {OUTPUT_PROCESSOR_NAME};
    }

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor()
    {
        return std::make_shared<ZibraVDBOutputProcessor>();
    }

    const PI_EditScriptedParms* ZibraVDBOutputProcessor::parameters() const
    {
        return nullptr;
    }

    void ZibraVDBOutputProcessor::beginSave(OP_Node* configNode, const UT_Options& configOverrides, OP_Node* lopNode, fpreal t,
                                            const UT_Options& stageVariables
#if UT_VERSION_INT >= 0x15000000 // Houdini 21.0+
                                            ,
                                            UT_String& error
#endif
    )
    {
        m_CompressionEntries.clear();
    }

    bool ZibraVDBOutputProcessor::processSavePath(const UT_StringRef& assetPath, const UT_StringRef& referencingLayerPath,
                                                  bool assetIsLayer, UT_String& newPath, UT_String& error)
    {
        std::string pathStr = assetPath.toStdString();

        if (assetIsLayer)
        {
            return false;
        }
        std::unordered_map<std::string, std::string> parsedURI;
        if (!Helpers::ParseZibraVDBURI(pathStr, parsedURI))
        {
            return false;
        }

        if (UT_String errorString; !CheckLibrary(errorString) || !CheckLicense(errorString))
        {
            error = errorString;
            return false;
        }

        auto nodeIt = parsedURI.find("node");
        if (nodeIt == parsedURI.end() || nodeIt->second.empty())
        {
            error = nodeIt == parsedURI.end() ? "ZibraVDB Output Processor Error: Missing required node parameter in .zibravdb path"
                                              : "ZibraVDB Output Processor Error: Empty node parameter in .zibravdb path";
            return false;
        }

        auto frameIt = parsedURI.find("frame");
        if (frameIt == parsedURI.end() || frameIt->second.empty())
        {
            error = frameIt == parsedURI.end() ? "ZibraVDB Output Processor Error: Missing required frame parameter in .zibravdb path"
                                               : "ZibraVDB Output Processor Error: Empty frame parameter in .zibravdb path";
            return false;
        }

        std::string decodedNodeName = nodeIt->second;
        std::string frameStr = frameIt->second;
        std::string filePath = parsedURI["path"] + "/" + parsedURI["name"];

        OP_Node* opNode = OPgetDirector()->findNode(decodedNodeName.c_str());
        if (!opNode)
        {
            error = "ZibraVDB Output Processor Error: Could not find SOP node at path: " + decodedNodeName;
            return false;
        }

        auto* sopNode = dynamic_cast<Zibra::ZibraVDBUSDExport::SOP_ZibraVDBUSDExport*>(opNode);
        if (!sopNode)
        {
            error = "ZibraVDB Output Processor Error: Node is not a ZibraVDB USD Export node: " + decodedNodeName;
            return false;
        }

        int frameIndex;
        try
        {
            frameIndex = std::stoi(frameStr);
        }
        catch (const std::exception& e)
        {
            error = "Cant parse frame index from string " + frameStr + ". " + e.what();
            return false;
        }

        float quality = sopNode->GetCompressionQuality();

        std::string dir = parsedURI["path"];
        std::filesystem::path namePath(parsedURI["name"]);
        std::string name = namePath.stem().string();
        std::string outputFileStr = "zibravdb://" + dir + "/" + name + "." + std::to_string(frameIndex) + ".vdb";

        if (auto it = m_CompressionEntries.find(sopNode); it == m_CompressionEntries.end())
        {
            CompressionEntry entry{};
            entry.referencingLayerPath = referencingLayerPath.toStdString();
            entry.outputFile = filePath;
            entry.quality = quality;
            entry.compressorManager = std::make_unique<CE::Compression::CompressorManager>();

            CE::Compression::FrameMappingDecs frameMappingDesc{};
            frameMappingDesc.sequenceStartIndex = frameIndex;
            frameMappingDesc.sequenceIndexIncrement = 1;
            std::vector<std::pair<UT_String, float>> perChannelSettings;
            if (sopNode->UsePerChannelCompressionSettings())
            {
                perChannelSettings = sopNode->GetPerChannelCompressionSettings();
            }

            std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
            auto status = entry.compressorManager->Initialize(frameMappingDesc, quality, perChannelSettings);
            assert(status == CE::ZCE_SUCCESS);

            status = entry.compressorManager->StartSequence(UT_String(filePath));
            if (status != CE::ZCE_SUCCESS)
            {
                error = "ZibraVDB Output Processor Error: Failed to start compression sequence, status: " + std::to_string(status);
                return false;
            }
            entry.frameOutputs.emplace_back(frameIndex, outputFileStr);
            m_CompressionEntries[sopNode] = std::move(entry);
        }
        else
        {
            it->second.frameOutputs.emplace_back(frameIndex, outputFileStr);
        }

        // Store the zibravdb path for processReferencePath lookup, but return invalid path to prevent original VDB saving
        // This makes Linux behavior consistent with Windows where original VDBs are not saved
        newPath = "."; // Return illegal path "." to prevent saving original VDB

        return true;
    }

    bool ZibraVDBOutputProcessor::processReferencePath(const UT_StringRef& assetPath, const UT_StringRef& referencingLayerPath,
                                                       bool assetIsLayer, UT_String& newPath, UT_String& error)
    {
        if (assetIsLayer)
        {
            return false;
        }

        const std::string pathStr = assetPath.toStdString();
        double t;
        std::string extractedPath;

        if (!Helpers::ParseRelSOPNodeParams(pathStr, t, extractedPath))
        {
            return false;
        }

        std::string layerPath = referencingLayerPath.toStdString();

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
        newPath = UT_String(outputRef);

        // If we want to bake not from beginning, we want to cook our SOP for every preceding frame
        int frameIndex = entry.frameOutputs[0].first;
        if (compressionFrameIndex == frameIndex)
        {
            if (compressionFrameIndex > 0)
            {
                for (int i = 0; i < compressionFrameIndex; ++i)
                {
                    fpreal tmpTime = OPgetDirector()->getChannelManager()->getTime(i);
                    ExtractVDBFromSOP(sopNode, tmpTime, entry.compressorManager.get(), false);
                }
            }
        }
        ExtractVDBFromSOP(sopNode, t, entry.compressorManager.get());

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
            }
        }
        m_CompressionEntries.clear();

        return true;
    }

    void ZibraVDBOutputProcessor::ExtractVDBFromSOP(SOP_Node* sopNode, fpreal t, CE::Compression::CompressorManager* compressorManager,
                                                    bool compress)
    {
        if (!sopNode)
        {
            return;
        }

        OP_Context context(t);
        sopNode->flags().setTimeDep(true);
        sopNode->forceRecook();
        sopNode->cook(context);

        if (!compress)
        {
            // If we are not compressing, we just need to ensure the SOP node is cooked
            return;
        }

        const GU_DetailHandle gdh = sopNode->getCookedGeoHandle(context);
        const GU_DetailHandleAutoReadLock gdl(gdh);
        const GU_Detail* gdp = gdl.getGdp();

        if (!gdp)
        {
            UT_String error = "SOP node returned null geometry";
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
            return;
        }

        std::vector<openvdb::GridBase::ConstPtr> grids;
        std::vector<std::string> gridNames;

        for (GA_Iterator it(gdp->getPrimitiveRange()); !it.atEnd(); ++it)
        {
            if (const GA_Primitive* prim = gdp->getPrimitive(*it); prim->getTypeId() == GEO_PRIMVDB)
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
            CompressGrids(grids, gridNames, compressorManager, gdp);
        }
        else
        {
            UT_String error = "No VDB grids found in SOP node";
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
        }
    }

    void ZibraVDBOutputProcessor::CompressGrids(std::vector<openvdb::GridBase::ConstPtr>& grids, const std::vector<std::string>& gridNames,
                                                CE::Compression::CompressorManager* compressorManager, const GU_Detail* gdp)
    {
        if (grids.empty())
        {
            UT_String error = "No grids to compress";
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
        auto frame = frameLoader.LoadFrame(&encodingMetadata);
        if (!frame)
        {
            UT_String error = "Failed to load frame from memory grids";
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
            return;
        }
        CE::Compression::CompressFrameDesc compressFrameDesc{};
        compressFrameDesc.channelsCount = gridNames.size();
        compressFrameDesc.channels = channelCStrings.data();
        compressFrameDesc.frame = frame;

        CE::Compression::FrameManager* frameManager = nullptr;
        auto status = compressorManager->CompressFrame(compressFrameDesc, &frameManager);
        if (status != CE::ZCE_SUCCESS || !frameManager)
        {
            UT_String error =
                ("CompressFrame failed or frameManager is null for in-memory grids: status " + std::to_string(status)).c_str();
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
            frameLoader.ReleaseFrame(frame);
            return;
        }

        const auto& gridsShuffleInfo = frameLoader.GetGridsShuffleInfo();

        auto frameMetadata = Utils::MetadataHelper::DumpAttributes(gdp, encodingMetadata);
        frameMetadata.emplace_back("chShuffle", Utils::MetadataHelper::DumpGridsShuffleInfo(gridsShuffleInfo).dump());
        for (const auto& [key, val] : frameMetadata)
        {
            frameManager->AddMetadata(key.c_str(), val.c_str());
        }

        status = frameManager->Finish();
        if (status != CE::ZCE_SUCCESS)
        {
            UT_String error = ("Failed to finish frame manager: status " + std::to_string(status)).c_str();
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
        }
        frameLoader.ReleaseFrame(frame);
    }

} // namespace Zibra::ZibraVDBOutputProcessor