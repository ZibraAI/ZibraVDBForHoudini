#include "PrecompiledHeader.h"

#include "ZibraVDBOutputProcessor.h"

#include <GA/GA_Iterator.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVDB.h>
#include <LOP/LOP_Node.h>
#include <OP/OP_Director.h>
#include <OP/OP_Node.h>
#include <OP/OP_Context.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_FileUtil.h>
#include <UT/UT_StringHolder.h>
#include <Zibra/CE/Addons/OpenVDBFrameLoader.h>
#include <Zibra/CE/Compression.h>
#include <algorithm>
#include <filesystem>
#include <openvdb/io/File.h>
#include <regex>

#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"

namespace Zibra::ZibraVDBOutputProcessor
{
    using namespace std::literals;
    using namespace Zibra::Utils::USD;

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
            float defaultQuality = entry->markerNode->getCompressionQuality();
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

        // TODO get correct time
        int currentFrame = entry->vdbFiles.size() - 1;
        fpreal frameRate = 24.0;
        fpreal frameTime = static_cast<fpreal>(currentFrame) / frameRate;
        
        SearchUpstreamForSOPNode(
            markerNode, frameTime,
            [this, cm = entry->compressorManager, frameTime, currentFrame](SOP_Node* sopNode, fpreal time) {
                this->extractVDBFromSOP(sopNode, frameTime, cm);
            }
        );
    }

    void ZibraVDBOutputProcessor::processDeferredSequence(const CompressionSequenceEntry& deferredEntry)
    {
        CE::Compression::FrameMappingDecs frameMappingDesc{};
        frameMappingDesc.sequenceStartIndex = 0;
        frameMappingDesc.sequenceIndexIncrement = 1;
        float quality = deferredEntry.markerNode->getCompressionQuality();

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
        if (!asset_is_layer && pathStr.find(".vdb") == pathStr.length() - 4)
        {
            std::string outLayer = referencing_layer_path.toStdString();
            std::filesystem::path outLayerPath(outLayer);
            std::string outLayerName = outLayerPath.stem().string();

            auto* curMarkerNode = findMarkerNodeByName(outLayerName);
            if (!curMarkerNode)
            {
                assert(false && "Could not find marker node for VDB compression: " + outLayerName);
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
                sequence = std::prev(entries.end());
            }
            else
            {
                sequence->vdbFiles.push_back(pathStr);
            }

            int frameIndex = sequence->vdbFiles.size() - 1;
            newpath = "zibravdb://" + newFileName + "?frame=" + std::to_string(frameIndex);

            if (!std::filesystem::exists(pathStr))
            {
                compressInMemoryGrids(curMarkerNode);
            }

            return true;
        }

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
        //const GU_Detail* gdp = sopNode->getCookedGeo(context);

        sopNode->flags().setTimeDep(true);
        sopNode->forceRecook();

        GU_DetailHandle gdh = sopNode->getCookedGeoHandle(context);
        GU_DetailHandleAutoReadLock gdl(gdh);
        const GU_Detail* gdp = gdl.getGdp();

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
} // namespace Zibra::ZibraVDBOutputProcessor