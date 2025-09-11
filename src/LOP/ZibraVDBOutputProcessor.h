#pragma once

#include <HUSD/HUSD_OutputProcessor.h>
#include <UT/UT_Version.h>
#include <openvdb/openvdb.h>
#include <string>
#include <vector>

#include "ROP/CompressorManager/CompressorManager.h"
#include "SOP/SOP_ZibraVDBUSDExport.h"

namespace Zibra::ZibraVDBOutputProcessor
{
    constexpr const char* OUTPUT_PROCESSOR_NAME = "ZibraVDBCompressionProcessor";

    class ZibraVDBOutputProcessor : public HUSD_OutputProcessor
    {
    private:
        struct CompressionSequenceEntryKey
        {
            ZibraVDBUSDExport::SOP_ZibraVDBUSDExport* sopNode;
            CE::Compression::CompressorManager* compressorManager;
            std::string referencingLayerPath;
            std::string outputFile;
            float quality;
            
            bool operator<(const CompressionSequenceEntryKey& other) const;
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
        std::string ConvertToUncompressedPath(const std::string& zibravdbPath);
        void ExtractVDBFromSOP(SOP_Node* sopNode, fpreal t, CE::Compression::CompressorManager* compressorManager, bool compress = true);
        static void CompressGrids(std::vector<openvdb::GridBase::ConstPtr>& grids, std::vector<std::string>& gridNames,
                                  CE::Compression::CompressorManager* compressorManager, const GU_Detail* gdp);
        static std::vector<std::pair<std::string, std::string>> DumpAttributes(const GU_Detail* gdp, const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata) noexcept;
        static void DumpVisualisationAttributes(std::vector<std::pair<std::string, std::string>>& attributes, const GEO_PrimVDB* vdbPrim) noexcept;
        static void DumpOpenVDBGridMetadata(std::vector<std::pair<std::string, std::string>>& attributes, const GEO_PrimVDB* vdbPrim) noexcept;
        static void DumpVDBFileMetadata(std::vector<std::pair<std::string, std::string>>& attributes, const GU_Detail* gdp) noexcept;
        static nlohmann::json DumpGridsShuffleInfo(const std::vector<CE::Addons::OpenVDBUtils::VDBGridDesc> gridDescs) noexcept;
        static void DumpDecodeMetadata(std::vector<std::pair<std::string, std::string>>& result, const CE::Addons::OpenVDBUtils::EncodingMetadata& encodingMetadata);

    private:
        std::vector<std::pair<CompressionSequenceEntryKey, std::vector<std::pair<int, std::string>>>> m_CompressionEntries;
    };

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor();

} // namespace Zibra::ZibraVDBOutputProcessor