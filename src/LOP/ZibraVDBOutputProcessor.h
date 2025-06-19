#pragma once

#include <HUSD/HUSD_OutputProcessor.h>
#include <PI/PI_EditScriptedParms.h>
#include <UT/UT_String.h>
#include <UT/UT_Options.h>
#include <string>
#include <map>

#include "Globals.h"
#include "ROP/CompressorManager/CompressorManager.h"

namespace Zibra::ZibraVDBOutputProcessor
{
    using namespace CE::Compression;

    constexpr const char* OUTPUT_PROCESSOR_NAME = "ZibraVDB Compressor";

    class ZibraVDBOutputProcessor : public HUSD_OutputProcessor
    {
    public:
        ZibraVDBOutputProcessor();
        ~ZibraVDBOutputProcessor() override;

        // HUSD_OutputProcessor interface
        void beginSave(OP_Node *config_node,
                      const UT_Options &config_overrides,
                      OP_Node *lop_node,
                      fpreal t,
                      const UT_Options &stage_variables) override;

        bool processReferencePath(const UT_StringRef &asset_path,
                                 const UT_StringRef &referencing_layer_path,
                                 bool asset_is_layer,
                                 UT_String &newpath,
                                 UT_String &error) override;

        UT_StringHolder displayName() const override;
        const PI_EditScriptedParms *parameters() const override;

    private:
        struct VDBCompressionInfo
        {
            std::string originalPath;
            std::string compressedPath;
            std::vector<int> frameIndices;
        };

        // Check if path is a VDB file that should be compressed
        bool shouldProcessPath(const UT_StringRef &asset_path) const;

        // Compress VDB file and return zibravdb:// URI
        bool compressVDBFile(const std::string& vdbPath,
                           const std::string& outputDir,
                           UT_String& zibraVDBPath,
                           UT_String& error);

        // Get output directory from referencing layer path
        std::string getOutputDirectory(const UT_StringRef &referencing_layer_path) const;

    private:
        Zibra::CE::Compression::CompressorManager m_CompressorManager;
        std::map<std::string, VDBCompressionInfo> m_CompressionCache;
        std::string m_CurrentOutputDir;
        PI_EditScriptedParms *m_Parameters;
    };

    // Factory function for registration
    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor();

} // namespace Zibra::ZibraVDBOutputProcessor