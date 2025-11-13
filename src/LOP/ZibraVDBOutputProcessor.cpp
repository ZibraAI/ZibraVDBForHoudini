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
            error = ERROR_PREFIX + ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED;
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }

        LibraryUtils::LoadSDKLibrary();
        if (!LibraryUtils::IsSDKLibraryLoaded())
        {
            error = ERROR_PREFIX + ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING;
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }

        return true;
    }

    bool ZibraVDBOutputProcessor::CheckLicense(UT_String& error)
    {
        if (!LibraryUtils::IsSDKLibraryLoaded())
        {
            error = ERROR_PREFIX + ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING;
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }
        if (!LicenseManager::GetInstance().CheckLicense(LicenseManager::Product::Compression))
        {
            if (LicenseManager::GetInstance().IsAnyLicenseValid())
            {
                error = ERROR_PREFIX + ZIBRAVDB_ERROR_MESSAGE_LICENSE_NO_COMPRESSION;
            }
            else
            {
                error = ERROR_PREFIX + ZIBRAVDB_ERROR_MESSAGE_LICENSE_ERROR;
            }
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }

        return true;
    }

    UT_StringHolder ZibraVDBOutputProcessor::displayName() const
    {
        return {OUTPUT_PROCESSOR_UI_NAME};
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
        const URI assetURI(assetPath.toStdString());
        if (assetIsLayer || Helpers::GetExtension(assetURI) != ZIB_ZIBRAVDB_EXT)
        {
            return false;
        }

        // Store the zibravdb path for processReferencePath lookup, but return invalid path to prevent original VDB saving
        // This makes Linux behavior consistent with Windows where original VDBs are not saved
        newPath = "."; // Return illegal path "." to prevent saving original VDB

        UT_String errorString;
        if (!CheckLibrary(errorString) || !CheckLicense(errorString))
        {
            error = errorString;
            return false;
        }

        auto nodeIt = assetURI.queryParams.find("node");
        if (nodeIt == assetURI.queryParams.end() || nodeIt->second.empty())
        {
            error = ERROR_PREFIX + ZIBRAVDB_ERROR_EMPTY_NODE_PARAM;
            return false;
        }
        
        auto frameIt = assetURI.queryParams.find("frame");
        if (frameIt == assetURI.queryParams.end())
        {
            error = ERROR_PREFIX + ZIBRAVDB_ERROR_EMPTY_FRAME_PARAM;
            return false;
        }
        
        int frameIndex;
        if (!Helpers::TryParseInt(frameIt->second, frameIndex) || frameIndex <= 0)
        {
            error = ERROR_PREFIX + ZIBRAVDB_ERROR_EMPTY_FRAME_PARAM;
            return false;
        }

        std::string decodedNodeName = nodeIt->second;
        std::string frameStr = frameIt->second;
        std::string filePath = assetURI.path;

        OP_Node* opNode = OPgetDirector()->findNode(decodedNodeName.c_str());
        if (!opNode)
        {
            error = ERROR_PREFIX + ZIBRAVDB_ERROR_NODE_NOT_FOUND_TEMPLATE + decodedNodeName;
            return false;
        }

        auto* sopNode = dynamic_cast<ZibraVDBUSDExport::SOP_ZibraVDBUSDExport*>(opNode);
        if (!sopNode)
        {
            error = ERROR_PREFIX + ZIBRAVDB_ERROR_WRONG_NODE_TYPE_TEMPLATE + decodedNodeName;
            return false;
        }

        float quality = sopNode->GetCompressionQuality();

        auto it = m_CompressionEntries.find(sopNode);
        if (it == m_CompressionEntries.end())
        {
            CompressionEntry entry{};
            entry.referencingLayerPath = referencingLayerPath.toStdString();
            entry.compressedFilePath = filePath;
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

            std::filesystem::path outputDir = std::filesystem::path(filePath).parent_path();
            if (!std::filesystem::exists(outputDir))
            {
                try
                {
                    std::filesystem::create_directories(outputDir);
                }
                catch (const std::exception& e)
                {
                    error = ERROR_PREFIX + ZIBRAVDB_ERROR_CREATE_OUTPUT_DIRECTORY_TEMPLATE + std::string(e.what());
                    return false;
                }
            }

            auto status = entry.compressorManager->Initialize(frameMappingDesc, quality, perChannelSettings);
            assert(status == CE::ZCE_SUCCESS);

            status = entry.compressorManager->StartSequence(UT_String(filePath));
            if (status != CE::ZCE_SUCCESS)
            {
                error = ERROR_PREFIX + ZIBRAVDB_ERROR_START_COMPRESSION_TEMPLATE + LibraryUtils::ErrorCodeToString(status);
                return false;
            }
            entry.requestedFrames.insert(frameIndex);
            m_CompressionEntries[sopNode] = std::move(entry);
        }
        else
        {
            it->second.requestedFrames.insert(frameIndex);
        }

        return true;
    }

    // Parses a SOP node reference path and extracts node path and time information
    // Format: op:/path/to/node.sop.volumes:FORMAT_ARGS:param1=value1&t=0.123
    // Returns true if parsing was successful
    bool ParseSOPNodeReferencePath(const std::string& pathStr, std::string& nodePath, double& time)
    {
        if (pathStr.compare(0, 3, "op:") != 0)
        {
            return false;
        }

        // Find the first colon after "op:" to get the query string part
        const size_t colonAfterPrefix = pathStr.find(':', 3);
        if (colonAfterPrefix == std::string::npos || colonAfterPrefix + 1 >= pathStr.length())
        {
            return false;
        }

        const std::string queryString = pathStr.substr(colonAfterPrefix + 1);
        auto queryParams = Helpers::ParseQueryParamsString(queryString);
        std::string fullPath = pathStr.substr(3, colonAfterPrefix - 3);
        
        // Remove the last component to get the parent path (node path)
        const size_t lastSlash = fullPath.find_last_of('/');
        nodePath = lastSlash != std::string::npos ? fullPath.substr(0, lastSlash) : fullPath;

        // Extract frame parameter (t parameter)
        auto frameIt = queryParams.find("t");
        if (frameIt != queryParams.end())
        {
            char* endPtr;
            time = std::strtod(frameIt->second.c_str(), &endPtr);
            if (endPtr == frameIt->second.c_str() || *endPtr != '\0')
            {
                // Invalid frame format
                return false;
            }
        }

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
        
        std::string configSopNodePath;
        double time = 0.0;
        if (!ParseSOPNodeReferencePath(pathStr, configSopNodePath, time))
        {
            return false;
        }

        std::string layerPath = referencingLayerPath.toStdString();

        // We search for the compression entry which we added in processSavePath
        auto it = std::find_if(m_CompressionEntries.begin(), m_CompressionEntries.end(), [&layerPath, &configSopNodePath](const auto& entry) {
            bool layerMatches = entry.second.referencingLayerPath == layerPath;
            std::string sopNodePath = entry.first->getFullPath().c_str();

            bool pathMatches = configSopNodePath.empty() || sopNodePath.find(configSopNodePath) == 0;
            return layerMatches && pathMatches;
        });

        if (it == m_CompressionEntries.end())
        {
            return false;
        }

        auto& [sopNode, entry] = *it;
        int compressionFrameIndex = OPgetDirector()->getChannelManager()->getFrame(time);
        std::string outputRef = entry.compressedFilePath.string() + "?frame=" + std::to_string(compressionFrameIndex);
        newPath = UT_String(outputRef);

        // When starting compression from a frame other than 0, cook all preceding frames first
        // Without this, all compressed frames would contain identical data from the last frame
        const int firstRequestedFrameIndex = *entry.requestedFrames.begin();
        if (compressionFrameIndex == firstRequestedFrameIndex)
        {
            if (compressionFrameIndex > 0)
            {
                for (int i = 0; i < compressionFrameIndex; ++i)
                {
                    fpreal tmpTime = OPgetDirector()->getChannelManager()->getTime(i);
                    OP_Context context(tmpTime);
                    sopNode->cook(context);
                }
            }
        }
        CompressVDBGrids(sopNode, time, entry.compressorManager.get());

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
                    error = ERROR_PREFIX + ZIBRAVDB_ERROR_FINISH_COMPRESSION_TEMPLATE + LibraryUtils::ErrorCodeToString(status);
                    return false;
                }
                entry.compressorManager->Release();
            }
        }
        m_CompressionEntries.clear();

        return true;
    }

    void ZibraVDBOutputProcessor::CompressVDBGrids(SOP_Node* sopNode, fpreal t, CE::Compression::CompressorManager* compressorManager)
    {
        assert(sopNode && "SOP node must not be null in CompressVDBGrids");

        OP_Context context(t);
        const GU_DetailHandle gdh = sopNode->getCookedGeoHandle(context);
        const GU_DetailHandleAutoReadLock gdl(gdh);
        const GU_Detail* gdp = gdl.getGdp();

        if (!gdp)
        {
            UT_String error(ERROR_PREFIX + ZIBRAVDB_ERROR_NULL_GEOMETRY);
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
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

        if (grids.empty())
        {
            UT_String error(ERROR_PREFIX + ZIBRAVDB_ERROR_NO_VDB_GRIDS);
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
            return;
        }

        CompressGrids(grids, gridNames, compressorManager, gdp);
    }

    void ZibraVDBOutputProcessor::CompressGrids(std::vector<openvdb::GridBase::ConstPtr>& grids, const std::vector<std::string>& gridNames,
                                                CE::Compression::CompressorManager* compressorManager, const GU_Detail* gdp)
    {
        if (grids.empty())
        {
            UT_String error(ERROR_PREFIX + ZIBRAVDB_ERROR_NO_GRIDS_TO_COMPRESS);
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
            UT_String error(ERROR_PREFIX + ZIBRAVDB_ERROR_LOAD_FRAME_FAILED);
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
            return;
        }
        CE::Compression::CompressFrameDesc compressFrameDesc{};
        compressFrameDesc.channelsCount = gridNames.size();
        compressFrameDesc.channels = channelCStrings.data();
        compressFrameDesc.frame = frame;

        CE::Compression::FrameManager* frameManager = nullptr;
        auto status = compressorManager->CompressFrame(compressFrameDesc, &frameManager);
        if (status != CE::ZCE_SUCCESS)
        {
            UT_String error(ERROR_PREFIX + ZIBRAVDB_ERROR_COMPRESS_FRAME_FAILED_TEMPLATE + LibraryUtils::ErrorCodeToString(status));
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
            frameLoader.ReleaseFrame(frame);
            return;
        }
        if (!frameManager)
        {
            UT_String error(ERROR_PREFIX + ZIBRAVDB_ERROR_FRAME_MANAGER_NULL);
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
            UT_String error(ERROR_PREFIX + ZIBRAVDB_ERROR_FINISH_FRAME_MANAGER_TEMPLATE + LibraryUtils::ErrorCodeToString(status));
            UTaddError(error.c_str(), UT_ERROR_ABORT, error.c_str());
        }
        frameLoader.ReleaseFrame(frame);
    }

} // namespace Zibra::ZibraVDBOutputProcessor