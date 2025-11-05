#pragma once

#include <HUSD/HUSD_OutputProcessor.h>
#include <UT/UT_Version.h>

#include "ROP/CompressorManager/CompressorManager.h"
#include "SOP/SOP_ZibraVDBUSDExport.h"

namespace Zibra::ZibraVDBOutputProcessor
{
    constexpr const char* OUTPUT_PROCESSOR_INNER_NAME = "ZibraVDBCompressionProcessor";
    constexpr const char* OUTPUT_PROCESSOR_UI_NAME = "ZibraVDB Processor";
    const std::string ERROR_PREFIX = "ZibraVDB Output Processor Error: ";

    class ZibraVDBOutputProcessor final : public HUSD_OutputProcessor
    {
    private:
        struct CompressionEntry
        {
            std::unique_ptr<CE::Compression::CompressorManager> compressorManager;
            std::string referencingLayerPath;
            std::filesystem::path compressedFilePath;
            std::set<int> requestedFrames;
            float quality;
        };

    public:
        void beginSave(OP_Node* configNode, const UT_Options& configOverrides, OP_Node* lopNode, fpreal t, const UT_Options& stageVariables
#if UT_VERSION_INT >= 0x15000000 // Houdini 21.0+
                       ,
                       UT_String& error
#endif
                       ) final;

        bool processSavePath(const UT_StringRef& assetPath, const UT_StringRef& referencingLayerPath, bool assetIsLayer, UT_String& newPath,
                             UT_String& error) final;

        bool processReferencePath(const UT_StringRef& assetPath, const UT_StringRef& referencingLayerPath, bool assetIsLayer,
                                  UT_String& newPath, UT_String& error) final;

        bool processLayer(const UT_StringRef& identifier, UT_String& error) final;

        UT_StringHolder displayName() const final;
        const PI_EditScriptedParms* parameters() const final;

    private:
        static void RecookNodeAndCompressVDBGrids(SOP_Node* sopNode, fpreal t, CE::Compression::CompressorManager* compressorManager);
        static void CompressGrids(std::vector<openvdb::GridBase::ConstPtr>& grids, const std::vector<std::string>& gridNames,
                                  CE::Compression::CompressorManager* compressorManager, const GU_Detail* gdp);

        static bool CheckLibrary(UT_String& error);
        static bool CheckLicense(UT_String& error);

    private:
        std::map<ZibraVDBUSDExport::SOP_ZibraVDBUSDExport*, CompressionEntry> m_CompressionEntries;
    };

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor();

} // namespace Zibra::ZibraVDBOutputProcessor