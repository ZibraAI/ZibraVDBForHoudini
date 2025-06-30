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

    constexpr const char* OUTPUT_PROCESSOR_NAME = "ZibraVDBCompressor";

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

//        struct DeferredCompressionEntry
//        {
//            std::vector<std::string> vdbFiles;
//            std::string outputFile;
//            ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker* markerNode = nullptr;
//        };

    public:
        ZibraVDBOutputProcessor();
        ~ZibraVDBOutputProcessor() override;

        void beginSave(OP_Node *config_node,
                      const UT_Options &config_overrides,
                      OP_Node *lop_node,
                      fpreal t,
                      const UT_Options &stage_variables) override;

        bool processSavePath(const UT_StringRef &asset_path,
                            const UT_StringRef &referencing_layer_path,
                            bool asset_is_layer,
                            UT_String &newpath,
                            UT_String &error) override;

        bool processReferencePath(const UT_StringRef &asset_path,
                                 const UT_StringRef &referencing_layer_path,
                                 bool asset_is_layer,
                                 UT_String &newpath,
                                 UT_String &error) override;

        bool processLayer(const UT_StringRef &identifier, UT_String &error) override;

        UT_StringHolder displayName() const override;
        const PI_EditScriptedParms *parameters() const override;

    private:
        std::string processSOPCreateNode(const std::string& layerPath);

        void processDeferredSequence(const CompressionSequenceEntry& vdbFiles);

        void extractVDBFromSOP(SOP_Node* sopNode, fpreal t, CompressorManager* compressorManager);
        
        ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker* findMarkerNodeByName(const std::string& nodeName) const;
        
        int parseFrameIndexFromVDBPath(const std::string& vdbPath) const;

    private:
        std::vector<CompressionSequenceEntry> m_InMemoryCompressionEntries;
        std::vector<CompressionSequenceEntry> m_DeferredCompressions;

        PI_EditScriptedParms *m_Parameters;
        fpreal m_curTime;

//        std::string generateOutputFilePath(std::string sequenceID);
//        std::string generateOutputFilePathFromMarker(ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker* markerNode, const std::string& sequenceID, fpreal t);
    };

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor();

} // namespace Zibra::ZibraVDBOutputProcessor