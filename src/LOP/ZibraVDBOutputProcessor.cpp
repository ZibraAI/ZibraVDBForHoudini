#include "PrecompiledHeader.h"

#include "ZibraVDBOutputProcessor.h"

#include <GA/GA_Iterator.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVDB.h>
#include <LOP/LOP_Node.h>
#include <OP/OP_Context.h>
#include <OP/OP_Director.h>
#include <OP/OP_Node.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_ErrorManager.h>
#include <UT/UT_FileUtil.h>
#include <UT/UT_StringHolder.h>
#include <UT/UT_WorkBuffer.h>
#include <Zibra/CE/Addons/OpenVDBFrameLoader.h>
#include <Zibra/CE/Compression.h>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <sstream>

#include "SOP/SOP_ZibraVDBUSDExport.h"
#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
#include "utils/GAAttributesDump.h"

namespace Zibra::ZibraVDBOutputProcessor
{
    using namespace std::literals;

    bool ZibraVDBOutputProcessor::CompressionSequenceEntryKey::operator<(const CompressionSequenceEntryKey& other) const
    {
        if (sopNode != other.sopNode) return sopNode < other.sopNode;
        if (referencingLayerPath != other.referencingLayerPath) return referencingLayerPath < other.referencingLayerPath;
        if (outputFile != other.outputFile) return outputFile < other.outputFile;
        if (quality != other.quality) return quality < other.quality;
        return compressorManager < other.compressorManager;
    }

    ZibraVDBOutputProcessor::ZibraVDBOutputProcessor()
    {
    }

    ZibraVDBOutputProcessor::~ZibraVDBOutputProcessor()
    {
    }

    bool ZibraVDBOutputProcessor::CheckLibAndLicense(UT_String& error)
    {
        if (!LibraryUtils::IsPlatformSupported())
        {
            error = "ZibraVDB Output Processor Error: Platform not supported. Required: Windows/Linux/macOS. Falling back to uncompressed VDB files.";
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }

        LibraryUtils::LoadZibSDKLibrary();
        if (!LibraryUtils::IsZibSDKLoaded())
        {
            error = "ZibraVDB Output Processor Error: Failed to load ZibraVDB SDK library. Please check installation. Falling back to uncompressed VDB files.";
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }
        
        if (!LicenseManager::GetInstance().CheckLicense(LicenseManager::Product::Compression))
        {
            error = "ZibraVDB Output Processor Error: No valid license found for compression. Please check your license. Falling back to uncompressed VDB files.";
            UTaddError(error.buffer(), UT_ERROR_MESSAGE, error.buffer());
            return false;
        }

        return true;
    }

    std::string ZibraVDBOutputProcessor::ConvertToUncompressedPath(const std::string& zibravdbPath)
    {
        // This is used as fallback when no sdk lib present or license failed
        // path/name.zibravdb?node=nodename&frame=X&quality=Y to path/name.frame.vdb
        size_t query_pos = zibravdbPath.find('?');
        if (query_pos == std::string::npos) return "";

        std::string file_path = zibravdbPath.substr(0, query_pos);
        std::string query_string = zibravdbPath.substr(query_pos + 1);

        // Extract frame parameter
        std::string frame_str;
        std::regex param_regex(R"(([^&=]+)=([^&=]+))");
        std::sregex_iterator iter(query_string.begin(), query_string.end(), param_regex);
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
            std::string key = (*iter)[1];
            std::string value = (*iter)[2];
            if (key == "frame") {
                frame_str = value;
                break;
            }
        }

        if (frame_str.empty()) return "";

        std::filesystem::path zibravdb_path(file_path);
        std::string dir = zibravdb_path.parent_path().string();
        std::string stem = zibravdb_path.stem().string();
        
        if (dir.empty()) {
            return stem + "." + frame_str + ".vdb";
        } else {
            return dir + "/" + stem + "." + frame_str + ".vdb";
        }
    }

    UT_StringHolder ZibraVDBOutputProcessor::displayName() const
    {
        return {"ZibraVDBCompressor"};
    }

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor()
    {
        return static_cast<HUSD_OutputProcessorPtr>(new ZibraVDBOutputProcessor());
    }

    const PI_EditScriptedParms *ZibraVDBOutputProcessor::parameters() const
    {
        return nullptr;
    }

    void ZibraVDBOutputProcessor::beginSave(OP_Node *config_node,
                                           const UT_Options &config_overrides,
                                           OP_Node *lop_node,
                                           fpreal t,
                                           const UT_Options &stage_variables
#if UT_VERSION_INT >= 0x15000000  // Houdini 21.0+
                                           ,
                                           UT_String &error
#endif
                                           )
    {
        m_CompressionEntries.clear();
    }

    bool ZibraVDBOutputProcessor::processSavePath(const UT_StringRef& asset_path, const UT_StringRef& referencing_layer_path,
                                                  bool asset_is_layer, UT_String& newpath, UT_String& error)
    {
        std::string pathStr = asset_path.toStdString();
        if (!asset_is_layer && pathStr.find(".zibravdb?") != std::string::npos)
        {
            UT_String libError;
            if (!CheckLibAndLicense(libError))
            {
                std::string uncompressedPath = ConvertToUncompressedPath(pathStr);
                newpath = UT_String(uncompressedPath);
                error = libError;
                return true;
            }
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
                return false;
            }

            auto* sop_node = dynamic_cast<Zibra::ZibraVDBUSDExport::SOP_ZibraVDBUSDExport*>(op_node);
            if (!sop_node) return false;

            int frame_index = std::stoi(frame_str);
            float quality = sop_node->getCompressionQuality();

            std::filesystem::path zibravdb_path(file_path);
            std::string dir = zibravdb_path.parent_path().string();
            std::string name = zibravdb_path.stem().string();
            std::string output_file_str = "zibravdb://" + dir + "/" + name + "." + std::to_string(frame_index) + ".vdb";

            auto& entries = m_CompressionEntries;
            auto it =
                std::find_if(entries.begin(), entries.end(), [sop_node](const auto& entry) { return entry.first.sopNode == sop_node; });
            if (it == entries.end()) {
                CompressionSequenceEntryKey entryKey{};
                entryKey.sopNode = sop_node;
                entryKey.referencingLayerPath = referencing_layer_path.toStdString();
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
                    assert(false && ("Failed to start sequence for compression, status: " + std::to_string(static_cast<int>(status))).c_str());
                    return false;
                }
                entries.emplace_back(entryKey, std::vector<std::pair<int, std::string>>{{frame_index, output_file_str}});
            }
            else {
                it->second.push_back({frame_index, output_file_str});
            }

            // Store the zibravdb path for processReferencePath lookup, but return invalid path to prevent original VDB saving
            // This makes Linux behavior consistent with Windows where original VDBs are not saved
            newpath = "."; // Return illegal path "." to prevent saving original VDB
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
        if (!asset_is_layer && pathStr.find("op:/") != std::string::npos)
        {
            std::regex t_regex(R"(&t=([0-9.]+))");
            std::smatch match;
            if (!std::regex_search(pathStr, match, t_regex)) {
                return false;
            }
            
            // Extract the SOP path from op:/ URL (e.g., "op:/stage/cloud/sopnet/OUT.sop.volumes" -> "/stage/cloud/sopnet")
            std::regex op_path_regex(R"(op:(/[^:]+))");
            std::smatch op_match;
            std::string extractedPath;
            if (std::regex_search(pathStr, op_match, op_path_regex)) {
                std::string fullPath = op_match[1].str();
                // Remove the last component (e.g., "/OUT.sop.volumes") to get the parent path
                size_t lastSlash = fullPath.find_last_of('/');
                if (lastSlash != std::string::npos) {
                    extractedPath = fullPath.substr(0, lastSlash);
                } else {
                    extractedPath = fullPath;
                }
            }

            fpreal t = std::stod(match[1].str());
            std::string layerPath = referencing_layer_path.toStdString();
            
            // Find matching entry by comparing SOP node paths
            auto it = std::find_if(m_CompressionEntries.begin(), m_CompressionEntries.end(), 
                [&layerPath, &extractedPath](const auto& entry) {
                    bool layerMatches = entry.first.referencingLayerPath == layerPath;
                    std::string sopNodePath = entry.first.sopNode->getFullPath().c_str();

                    bool pathMatches = extractedPath.empty() || sopNodePath.find(extractedPath) == 0;
                    return layerMatches && pathMatches;
                });
                
            if (it != m_CompressionEntries.end()) {
                auto& entry = *it;
                int compressionFrameIndex = OPgetDirector()->getChannelManager()->getFrame(t);
                std::string outputRef = entry.first.outputFile + "?frame=" + std::to_string(compressionFrameIndex);
                newpath = UT_String(outputRef);
                if (compressionFrameIndex == entry.second[0].first)
                {
                    if (compressionFrameIndex > 0)
                    {
                        for (int i = 0; i < compressionFrameIndex; ++i)
                        {
                            fpreal tmpTime = OPgetDirector()->getChannelManager()->getTime(i);
                            ExtractVDBFromSOP(entry.first.sopNode, tmpTime, entry.first.compressorManager, false);
                        }
                    }
                }
                ExtractVDBFromSOP(entry.first.sopNode, t, entry.first.compressorManager);
                return true;
            }
        }
        return false;
    }

    bool ZibraVDBOutputProcessor::processLayer(const UT_StringRef& identifier, UT_String& error)
    {
        for (const auto& sequence : m_CompressionEntries)
        {
            if (sequence.first.compressorManager)
            {
                std::string warning;
                auto status = sequence.first.compressorManager->FinishSequence(warning);
                if (status != CE::ZCE_SUCCESS)
                {
                    assert(false && ("Failed to finish compression sequence for in-memory VDBs. Status: " + std::to_string(static_cast<int>(status))).c_str());
                }
                sequence.first.compressorManager->Release();
                delete sequence.first.compressorManager;
            }
        }
        m_CompressionEntries.clear();

        return true;
    }

    void ZibraVDBOutputProcessor::ExtractVDBFromSOP(SOP_Node* sopNode, fpreal t, CE::Compression::CompressorManager* compressorManager, bool compress)
    {
        if (!sopNode)
            return;

        int frameIndex = OPgetDirector()->getChannelManager()->getFrame(t);

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
            CompressGrids(grids, gridNames, compressorManager, gdp);
        }
        else
        {
            assert(false && "No VDB grids found in SOP node");
        }
    }

    void ZibraVDBOutputProcessor::CompressGrids(std::vector<openvdb::GridBase::ConstPtr>& grids, std::vector<std::string>& gridNames,
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
            assert(false && ("CompressFrame failed or frameManager is null for in-memory grids: status " + std::to_string(static_cast<int>(status))).c_str());
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
                DumpOpenVDBGridMetadata(result, vdbPrim);
            }
        }

        nlohmann::json detailAttrDump = Utils::DumpAttributesForSingleEntity(gdp, GA_ATTRIB_DETAIL, 0);
        result.emplace_back("houdiniDetailAttributes", detailAttrDump.dump());

        DumpVDBFileMetadata(result, gdp);
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

    void ZibraVDBOutputProcessor::DumpOpenVDBGridMetadata(std::vector<std::pair<std::string, std::string>>& attributes,
                                                          const GEO_PrimVDB* vdbPrim) noexcept
    {
        const std::string keyPrefix = "houdiniOpenVDBGridMetadata_"s + vdbPrim->getGridName();
        
        auto gridPtr = vdbPrim->getGridPtr();
        if (!gridPtr) return;

        nlohmann::json metadataJson = nlohmann::json::object();
        
        for (auto metaIter = gridPtr->beginMeta(); metaIter != gridPtr->endMeta(); ++metaIter) {
            const std::string& metaName = metaIter->first;
            const openvdb::Metadata& metadata = *metaIter->second;
            
            // Convert metadata to string representation
            std::string metaValue = metadata.str();
            
            // Remove quotes if it's a string value  
            if (metaValue.size() >= 2 && metaValue.front() == '"' && metaValue.back() == '"') {
                metaValue = metaValue.substr(1, metaValue.size() - 2);
            }
            
            metadataJson[metaName] = metaValue;
        }
        
        if (!metadataJson.empty()) {
            attributes.emplace_back(keyPrefix, metadataJson.dump());
        }
    }

    void ZibraVDBOutputProcessor::DumpVDBFileMetadata(std::vector<std::pair<std::string, std::string>>& attributes,
                                                      const GU_Detail* gdp) noexcept
    {
        nlohmann::json fileMetadata = nlohmann::json::object();
        
        // Try to capture file metadata from detail attributes first
        for (auto it = gdp->getAttributeDict(GA_ATTRIB_DETAIL).obegin(GA_SCOPE_PUBLIC); !it.atEnd(); it.advance())
        {
            GA_Attribute* attr = *it;
            const char* attrName = attr->getName().c_str();
            
            // Check if this looks like VDB file metadata
            if (strstr(attrName, "volvis_") || 
                strcmp(attrName, "creator") == 0 ||
                strcmp(attrName, "doppath") == 0 ||
                strstr(attrName, "_ratio") ||
                strcmp(attrName, "timescale") == 0)
            {
                switch (attr->getStorageClass())
                {
                case GA_STORECLASS_STRING: {
                    auto strTuple = attr->getAIFStringTuple();
                    const char* attrString = (strTuple && attr->getTupleSize()) ? strTuple->getString(attr, 0, 0) : "";
                    if (attrString && strlen(attrString) > 0) {
                        fileMetadata[attrName] = attrString;
                    }
                    break;
                }
                case GA_STORECLASS_INT:
                case GA_STORECLASS_FLOAT: {
                    auto tuple = attr->getAIFTuple();
                    if (tuple) {
                        const GA_Storage storage = tuple->getStorage(attr);
                        int tupleSize = attr->getTupleSize();
                        
                        if (storage == GA_STORE_REAL32 || storage == GA_STORE_REAL64) {
                            if (tupleSize == 1) {
                                fpreal64 data;
                                tuple->get(attr, 0, data, 0);
                                fileMetadata[attrName] = data;
                            } else {
                                std::vector<fpreal64> data(tupleSize);
                                tuple->get(attr, 0, data.data(), tupleSize);
                                fileMetadata[attrName] = data;
                            }
                        } else if (storage == GA_STORE_INT32 || storage == GA_STORE_INT64) {
                            if (tupleSize == 1) {
                                int64 data;
                                tuple->get(attr, 0, data, 0);
                                fileMetadata[attrName] = static_cast<int>(data);
                            } else {
                                std::vector<int64> data(tupleSize);
                                tuple->get(attr, 0, data.data(), tupleSize);
                                std::vector<int> intData;
                                for (auto d : data) intData.push_back(static_cast<int>(d));
                                fileMetadata[attrName] = intData;
                            }
                        }
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }
        
        if (!fileMetadata.empty()) {
            attributes.emplace_back("houdiniFileMetadata", fileMetadata.dump());
        }
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