#pragma once

#include <HUSD/HUSD_OutputProcessor.h>
#include <PI/PI_EditScriptedParms.h>
#include <UT/UT_Version.h>

namespace Zibra::ZibraVDBOutputProcessor
{
    constexpr const char* OUTPUT_PROCESSOR_INNER_NAME = "zibravdboutputprocessor";
    constexpr const char* OUTPUT_PROCESSOR_UI_NAME = "ZibraVDB Output Processor";

    constexpr const char* PARM_MAKE_PATHS_RELATIVE = "zibravdb_relativepaths";
    constexpr const char* PARM_COPY_ASSETS = "zibravdb_copyassets";
    constexpr const char* PARM_COPY_SUBDIRECTORY = "zibravdb_outputsubdir";

    class ZibraVDBOutputProcessor final : public HUSD_OutputProcessor
    {
    public:
        void beginSave(OP_Node* configNode, const UT_Options& configOverrides, OP_Node* lopNode, fpreal t
#if UT_VERSION_INT >= 0x14050000 // Houdini 20.5+
                       ,
                       const UT_Options& stageVariables
#endif
#if UT_VERSION_INT >= 0x15000000 // Houdini 21.0+
                       ,
                       UT_String& error
#endif
                       ) final;

        bool processSavePath(const UT_StringRef& assetPath, const UT_StringRef& referencingLayerPath, bool assetIsLayer,
                             UT_String& newPath, UT_String& error) final;

        bool processReferencePath(const UT_StringRef& assetPath, const UT_StringRef& referencingLayerPath, bool assetIsLayer,
                                  UT_String& newPath, UT_String& error) final;

        UT_StringHolder displayName() const final;
        const PI_EditScriptedParms* parameters() const final;

    private:

        static bool CopyAssetFile(const std::string& sourcePath, const std::string& targetDir, std::string& newPath,
                                  UT_String& error);

        bool m_MakePathsRelative = false;
        bool m_CopyAssets = false;
        std::string m_CopySubdirectory;
    };

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor();

} // namespace Zibra::ZibraVDBOutputProcessor
