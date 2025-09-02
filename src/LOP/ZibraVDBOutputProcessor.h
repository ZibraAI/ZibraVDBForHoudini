#pragma once

#include <HUSD/HUSD_OutputProcessor.h>
#include <UT/UT_Version.h>

#include <map>
#include <openvdb/openvdb.h>
#include <set>
#include <string>
#include <vector>

#include "ROP/CompressorManager/CompressorManager.h"
#include "SOP/SOP_ZibraVDBUSDExport.h"

namespace Zibra::ZibraVDBOutputProcessor
{
    using namespace CE::Compression;

    constexpr const char* OUTPUT_PROCESSOR_NAME = "ZibraVDBCompressionProcessor";

    class ZibraVDBOutputProcessor : public HUSD_OutputProcessor
    {
    private:
        struct CompressionSequenceEntryKey
        {
            Zibra::ZibraVDBUSDExport::SOP_ZibraVDBUSDExport* sopNode;
            Zibra::CE::Compression::CompressorManager* compressorManager;
            std::string referencingLayerPath;
            std::string outputFile;
            float quality;
            
            bool operator<(const CompressionSequenceEntryKey& other) const
            {
                if (sopNode != other.sopNode) return sopNode < other.sopNode;
                if (referencingLayerPath != other.referencingLayerPath) return referencingLayerPath < other.referencingLayerPath;
                if (outputFile != other.outputFile) return outputFile < other.outputFile;
                if (quality != other.quality) return quality < other.quality;
                return compressorManager < other.compressorManager;
            }
        };

    public:
        ZibraVDBOutputProcessor();
        ~ZibraVDBOutputProcessor() override;

        void beginSave(OP_Node *config_node,
                      const UT_Options &config_overrides,
                      OP_Node *lop_node,
                      fpreal t,
                      const UT_Options &stage_variables
#if UT_VERSION_INT >= 0x15000000  // Houdini 21.0+
                      ,
                      UT_String &error
#endif
                      ) override;

        bool processSavePath(const UT_StringRef& asset_path, const UT_StringRef& referencing_layer_path, bool asset_is_layer,
                             UT_String& newpath, UT_String& error) override;

        bool processReferencePath(const UT_StringRef &asset_path,
                                 const UT_StringRef &referencing_layer_path,
                                 bool asset_is_layer,
                                 UT_String &newpath,
                                 UT_String &error) override;

        bool processLayer(const UT_StringRef &identifier, UT_String &error) override;

        UT_StringHolder displayName() const override;
        const PI_EditScriptedParms *parameters() const override;

    private:
        bool CheckLibAndLicense(UT_String& error);
        std::string convertToUncompressedPath(const std::string& zibravdbPath);
        void extractVDBFromSOP(SOP_Node* sopNode, fpreal t, CompressorManager* compressorManager, bool compress = true);
        static void compressGrids(std::vector<openvdb::GridBase::ConstPtr>& grids, std::vector<std::string>& gridNames,
                                  CE::Compression::CompressorManager* compressorManager, const GU_Detail* gdp);
        static std::vector<std::pair<std::string, std::string>> DumpAttributes(const GU_Detail* gdp, const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata) noexcept;
        static void DumpVisualisationAttributes(std::vector<std::pair<std::string, std::string>>& attributes, const GEO_PrimVDB* vdbPrim) noexcept;
        static void DumpOpenVDBGridMetadata(std::vector<std::pair<std::string, std::string>>& attributes, const GEO_PrimVDB* vdbPrim) noexcept;
        static void DumpVDBFileMetadata(std::vector<std::pair<std::string, std::string>>& attributes, const GU_Detail* gdp) noexcept;
        static nlohmann::json DumpGridsShuffleInfo(const std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> gridDescs) noexcept;
        static void DumpDecodeMetadata(std::vector<std::pair<std::string, std::string>>& result, const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata);

    private:
        std::map<CompressionSequenceEntryKey, std::vector<std::pair<int, std::string>>> m_InMemoryCompressionEntries;
    };

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor();

} // namespace Zibra::ZibraVDBOutputProcessor