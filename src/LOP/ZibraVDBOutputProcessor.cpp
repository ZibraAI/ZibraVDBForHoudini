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
#include <sstream>

#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
#include "SOP/SOP_ZibraVDBUSDExport.h"
#include "utils/GAAttributesDump.h"

namespace Zibra::ZibraVDBOutputProcessor
{
    using namespace std::literals;

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
        m_InMemoryCompressionEntries.clear();
    }

    bool ZibraVDBOutputProcessor::processSavePath(const UT_StringRef& asset_path, const UT_StringRef& referencing_layer_path,
                                                  bool asset_is_layer, UT_String& newpath, UT_String& error)
    {
        std::string pathStr = asset_path.toStdString();
        if (!asset_is_layer && pathStr.find(".zibravdb?") != std::string::npos)
        {
            // Parse the .zibravdb path: path/name.zibravdb?node=nodename&frame=X&quality=Y
            size_t query_pos = pathStr.find('?');
            if (query_pos == std::string::npos) return false;

            std::string file_path = pathStr.substr(0, query_pos);
            std::string query_string = pathStr.substr(query_pos + 1);

            // Parse query parameters
            std::string node_name, frame_str, quality_str;
            std::regex param_regex(R"(([^&=]+)=([^&=]+))");
            std::sregex_iterator iter(query_string.begin(), query_string.end(), param_regex);
            std::sregex_iterator end;


            for (; iter != end; ++iter) {
                std::string key = (*iter)[1];
                std::string value = (*iter)[2];
                if (key == "node") node_name = value;
                else if (key == "frame") frame_str = value;
                else if (key == "quality") quality_str = value;
            }

            if (node_name.empty() || frame_str.empty()) return false;

            std::string decoded_node_name = node_name;
            size_t pos = 0;
            while ((pos = decoded_node_name.find("%2F", pos)) != std::string::npos) {
                decoded_node_name.replace(pos, 3, "/");
                pos += 1;
            }

            OP_Node* op_node = OPgetDirector()->findNode(decoded_node_name.c_str());
            if (!op_node) {
                std::cout << "Could not find node with path: " << decoded_node_name << std::endl;
                return false;
            }

            auto* sop_node = dynamic_cast<Zibra::ZibraVDBUSDExport::SOP_ZibraVDBUSDExport*>(op_node);
            if (!sop_node) return false;

            int frame_index = std::stoi(frame_str);
            float quality = sop_node->getCompressionQuality();

            std::filesystem::path zibravdb_path(file_path);
            std::string dir = zibravdb_path.parent_path().string();
            std::string name = zibravdb_path.stem().string();
            std::string output_file_str = dir + "/" + name + "." + std::to_string(frame_index) + ".vdb";

            auto& entries = m_InMemoryCompressionEntries;
            auto it =
                std::find_if(entries.begin(), entries.end(), [sop_node](const auto& entry) { return entry.first.sopNode == sop_node; });
            if (it == entries.end()) {
                CompressionSequenceEntryKey entryKey{};
                entryKey.sopNode = sop_node;
                entryKey.outputFile = file_path;
                entryKey.quality = quality;
                entryKey.compressorManager = new Zibra::CE::Compression::CompressorManager();

                CE::Compression::FrameMappingDecs frameMappingDesc{};
                frameMappingDesc.sequenceStartIndex = frame_index;
                frameMappingDesc.sequenceIndexIncrement = 1;
                std::vector<std::pair<UT_String, float>> perChannelSettings;
                if (sop_node->usePerChannelCompressionSettings())
                {
                    perChannelSettings = sop_node->getPerChannelCompressionSettings();
                }

                std::filesystem::create_directories(std::filesystem::path(file_path).parent_path());
                auto status = entryKey.compressorManager->Initialize(frameMappingDesc, quality, perChannelSettings);
                assert(status == CE::ZCE_SUCCESS);

                status = entryKey.compressorManager->StartSequence(UT_String(file_path));
                if (status != CE::ZCE_SUCCESS)
                {
                    assert(false && "Failed to start sequence for compression, status: " + std::to_string(static_cast<int>(status)));
                    return false;
                }
                entries[entryKey] = {{frame_index, output_file_str}};
                it = std::prev(entries.end());
            }
            else {
                it->second.push_back({frame_index, output_file_str});
            }

            newpath = output_file_str;
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
        if (!asset_is_layer && pathStr.find(".vdb") != std::string::npos)
        {
            auto it = std::find_if(m_InMemoryCompressionEntries.begin(), m_InMemoryCompressionEntries.end(), [&pathStr](const auto& entry) {
                return std::find_if(entry.second.begin(), entry.second.end(),
                                    [&pathStr](const auto& file) { return file.second == pathStr; }) != entry.second.end();
            });
            if (it != m_InMemoryCompressionEntries.end()) {
                auto framesIt = std::find_if(it->second.begin(), it->second.end(), [&pathStr](const auto& file) { return file.second == pathStr; });
                auto compressionFrameIndex = framesIt->first;
                std::string outputRef = it->first.outputFile + "?frame=" + std::to_string(compressionFrameIndex);
                newpath = UT_String(outputRef);

                auto& entry = *it;

                if (compressionFrameIndex == entry.second[0].first)
                {
                    if (compressionFrameIndex > 0)
                    {
                        for (int i = 0; i < compressionFrameIndex; ++i)
                        {
                            fpreal tmpTime = OPgetDirector()->getChannelManager()->getTime(i);
                            std::cout << "Simulating frame " << i << " with time " << tmpTime << std::endl;
                            extractVDBFromSOP(entry.first.sopNode, tmpTime, entry.first.compressorManager, false);
                        }
                    }
                }

                fpreal sceneFrameTime = OPgetDirector()->getChannelManager()->getTime(compressionFrameIndex);
                std::cout << "Compressing frame " << compressionFrameIndex << " with time " << sceneFrameTime << std::endl;
                extractVDBFromSOP(entry.first.sopNode, sceneFrameTime, entry.first.compressorManager);

                return true;
            }
        }
        return false;
    }

    bool ZibraVDBOutputProcessor::processLayer(const UT_StringRef& identifier, UT_String& error)
    {
        for (const auto& sequence : m_InMemoryCompressionEntries)
        {
            if (sequence.first.compressorManager)
            {
                std::string warning;
                auto status = sequence.first.compressorManager->FinishSequence(warning);
                if (status != CE::ZCE_SUCCESS)
                {
                    assert(false && "Failed to finish compression sequence for in-memory VDBs. Status: " + std::to_string(static_cast<int>(status)));
                }
                sequence.first.compressorManager->Release();
            }
        }
        m_InMemoryCompressionEntries.clear();

        return true;
    }

    void ZibraVDBOutputProcessor::extractVDBFromSOP(SOP_Node* sopNode, fpreal t, CompressorManager* compressorManager, bool compress)
    {
        if (!sopNode)
            return;

        OP_Context context(t);
        sopNode->flags().setTimeDep(true);
        sopNode->forceRecook();
        sopNode->cook(context);

        if (!compress)
        {
            // If we are not compressing, we just need to ensure the SOP node is cooked
            return;
        }

        GU_DetailHandle gdh = sopNode->getCookedGeoHandle(context);
        GU_DetailHandleAutoReadLock gdl(gdh);
        const GU_Detail* gdp = gdl.getGdp();

        if (!gdp)
        {
            assert(false && "SOP node returned null geometry");
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
            compressGrids(grids, gridNames, compressorManager, gdp);
        }
        else
        {
            assert(false && "No VDB grids found in SOP node");
        }
    }

    void ZibraVDBOutputProcessor::compressGrids(std::vector<openvdb::GridBase::ConstPtr>& grids, std::vector<std::string>& gridNames,
                                                CE::Compression::CompressorManager* compressorManager, const GU_Detail* gdp)
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
            const auto& gridsShuffleInfo = frameLoader.GetGridsShuffleInfo();
            
            auto frameMetadata = DumpAttributes(gdp, encodingMetadata);
            frameMetadata.push_back({"chShuffle", DumpGridsShuffleInfo(gridsShuffleInfo).dump()});
            for (const auto& [key, val] : frameMetadata)
            {
                frameManager->AddMetadata(key.c_str(), val.c_str());
            }
            
            status = frameManager->Finish();
        }
        else
        {
            assert(false && "CompressFrame failed or frameManager is null for in-memory grids: status " + std::to_string(static_cast<int>(status)));
        }
        frameLoader.ReleaseFrame(frame);
    }

    std::vector<std::pair<std::string, std::string>> ZibraVDBOutputProcessor::DumpAttributes(const GU_Detail* gdp, const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata) noexcept
    {
        std::vector<std::pair<std::string, std::string>> result{};

        const GEO_Primitive* prim;
        GA_FOR_ALL_PRIMITIVES(gdp, prim)
        {
            if (prim->getTypeId() == GEO_PRIMVDB)
            {
                auto vdbPrim = dynamic_cast<const GEO_PrimVDB*>(prim);

                nlohmann::json primAttrDump = Utils::DumpAttributesForSingleEntity(gdp, GA_ATTRIB_PRIMITIVE, prim->getMapOffset());
                std::string primKeyName = "houdiniPrimitiveAttributes_"s + vdbPrim->getGridName();
                result.emplace_back(primKeyName, primAttrDump.dump());

                DumpVisualisationAttributes(result, vdbPrim);
            }
        }

        nlohmann::json detailAttrDump = Utils::DumpAttributesForSingleEntity(gdp, GA_ATTRIB_DETAIL, 0);
        result.emplace_back("houdiniDetailAttributes", detailAttrDump.dump());

        DumpDecodeMetadata(result, encodingMetadata);
        return result;
    }

    void ZibraVDBOutputProcessor::DumpVisualisationAttributes(std::vector<std::pair<std::string, std::string>>& attributes,
                                                             const GEO_PrimVDB* vdbPrim) noexcept
    {
        const std::string keyPrefix = "houdiniVisualizationAttributes_"s + vdbPrim->getGridName();

        std::string keyVisMode = keyPrefix + "_mode";
        std::string valueVisMode = std::to_string(static_cast<int>(vdbPrim->getVisualization()));
        attributes.emplace_back(std::move(keyVisMode), std::move(valueVisMode));

        std::string keyVisIso = keyPrefix + "_iso";
        std::string valueVisIso = std::to_string(vdbPrim->getVisIso());
        attributes.emplace_back(std::move(keyVisIso), std::move(valueVisIso));

        std::string keyVisDensity = keyPrefix + "_density";
        std::string valueVisDensity = std::to_string(vdbPrim->getVisDensity());
        attributes.emplace_back(std::move(keyVisDensity), std::move(valueVisDensity));

        std::string keyVisLod = keyPrefix + "_lod";
        std::string valueVisLod = std::to_string(static_cast<int>(vdbPrim->getVisLod()));
        attributes.emplace_back(std::move(keyVisLod), std::move(valueVisLod));
    }

    nlohmann::json ZibraVDBOutputProcessor::DumpGridsShuffleInfo(const std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> gridDescs) noexcept
    {
        static std::map<CE::Addons::OpenVDBUtils::GridVoxelType, std::string> voxelTypeToString = {
            {CE::Addons::OpenVDBUtils::GridVoxelType::Float1, "Float1"},
            {CE::Addons::OpenVDBUtils::GridVoxelType::Float3, "Float3"}
        };

        nlohmann::json result = nlohmann::json::array();
        for (const CE::Addons::OpenVDBUtils::VDBGridDesc& gridDesc : gridDescs)
        {
            nlohmann::json serializedDesc = nlohmann::json{
                {"gridName", gridDesc.gridName},
                {"voxelType", voxelTypeToString.at(gridDesc.voxelType)},
            };
            for (size_t i = 0; i < std::size(gridDesc.chSource); ++i)
            {
                std::string name{"chSource"};
                if (gridDesc.chSource[i])
                {
                    serializedDesc[name + std::to_string(i)] = gridDesc.chSource[i];
                }
                else
                {
                    serializedDesc[name + std::to_string(i)] = nullptr;
                }
            }
            result.emplace_back(serializedDesc);
        }
        return result;
    }

    void ZibraVDBOutputProcessor::DumpDecodeMetadata(std::vector<std::pair<std::string, std::string>>& result,
                                                    const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata)
    {
        std::ostringstream oss;
        oss << encodingMetadata.offsetX << " " << encodingMetadata.offsetY << " " << encodingMetadata.offsetZ;
        result.emplace_back("houdiniDecodeMetadata", oss.str());
    }

} // namespace Zibra::ZibraVDBOutputProcessor