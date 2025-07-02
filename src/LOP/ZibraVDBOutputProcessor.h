#pragma once

#include <HUSD/HUSD_OutputProcessor.h>
#include <PI/PI_EditScriptedParms.h>
#include <UT/UT_Options.h>
#include <UT/UT_String.h>
#include <map>
#include <openvdb/openvdb.h>
#include <set>
#include <string>
#include <vector>

#include "Globals.h"
#include "LOP_ZibraVDBCompressionMarker.h"
#include "ROP/CompressorManager/CompressorManager.h"
#include "utils/ZibraUSDUtils.h"

class SOP_Node;

namespace Zibra::ZibraVDBOutputProcessor
{
    using namespace CE::Compression;

    constexpr const char* OUTPUT_PROCESSOR_NAME = "ZibraVDBCompressionProcessor";

    class ZibraVDBOutputProcessor : public HUSD_OutputProcessor
    {
    private:
        struct CompressionSequenceEntry
        {
            Zibra::ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker* markerNode;
            Zibra::CE::Compression::CompressorManager* compressorManager;
            std::vector<std::string> vdbFiles;
            std::string outputFile;
        };

    public:
        ZibraVDBOutputProcessor();
        ~ZibraVDBOutputProcessor() override;

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

        bool processLayer(const UT_StringRef &identifier, UT_String &error) override;

        UT_StringHolder displayName() const override;
        const PI_EditScriptedParms *parameters() const override;

    private:
        void compressInMemoryGrids(ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker* markerNode);

        void processDeferredSequence(const CompressionSequenceEntry& vdbFiles);

        void extractVDBFromSOP(SOP_Node* sopNode, fpreal t, CompressorManager* compressorManager);
        
    private:
        std::vector<CompressionSequenceEntry> m_InMemoryCompressionEntries;
        std::vector<CompressionSequenceEntry> m_DeferredCompressions;

        PI_EditScriptedParms *m_Parameters;
    };

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor();

} // namespace Zibra::ZibraVDBOutputProcessor