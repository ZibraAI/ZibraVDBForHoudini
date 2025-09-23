#pragma once

#include <HUSD/HUSD_OutputProcessor.h>
#include <UT/UT_Version.h>
#include <unordered_map>

#include "ROP/CompressorManager/CompressorManager.h"
#include "SOP/SOP_ZibraVDBUSDExport.h"

namespace Zibra::ZibraVDBOutputProcessor
{
    constexpr const char* OUTPUT_PROCESSOR_NAME = "ZibraVDBCompressionProcessor";

    class ZibraVDBOutputProcessor final : public HUSD_OutputProcessor
    {
    private:
        struct CompressionEntry
        {
            CE::Compression::CompressorManager* compressorManager;
            std::string referencingLayerPath;
            std::string outputFile;
            float quality;
            std::vector<std::pair<int, std::string>> frameOutputs;
        };

    public:
        ZibraVDBOutputProcessor();
        ~ZibraVDBOutputProcessor() override;

        void beginSave(OP_Node* config_node, const UT_Options& config_overrides, OP_Node* lop_node, fpreal t,
                       const UT_Options& stage_variables
#if UT_VERSION_INT >= 0x15000000 // Houdini 21.0+
                       ,
                       UT_String& error
#endif
                       ) final;

        bool processSavePath(const UT_StringRef& asset_path, const UT_StringRef& referencing_layer_path, bool asset_is_layer,
                             UT_String& newpath, UT_String& error) final;

        bool processReferencePath(const UT_StringRef& asset_path, const UT_StringRef& referencing_layer_path, bool asset_is_layer,
                                  UT_String& newpath, UT_String& error) final;

        bool processLayer(const UT_StringRef& identifier, UT_String& error) final;

        UT_StringHolder displayName() const final;
        const PI_EditScriptedParms* parameters() const final;

    private:
        bool CheckLibrary(UT_String& error);
        bool CheckLicense(UT_String& error);
        void ExtractVDBFromSOP(SOP_Node* sopNode, fpreal t, CE::Compression::CompressorManager* compressorManager, bool compress = true);
        static void CompressGrids(std::vector<openvdb::GridBase::ConstPtr>& grids, const std::vector<std::string>& gridNames,
                                  CE::Compression::CompressorManager* compressorManager, const GU_Detail* gdp);

    private:
        std::unordered_map<ZibraVDBUSDExport::SOP_ZibraVDBUSDExport*, CompressionEntry> m_CompressionEntries;
        OP_Node* m_ConfigNode = nullptr;
    };

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor();

} // namespace Zibra::ZibraVDBOutputProcessor