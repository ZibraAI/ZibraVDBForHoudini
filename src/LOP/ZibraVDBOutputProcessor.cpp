#include "PrecompiledHeader.h"

#include "ZibraVDBOutputProcessor.h"

#include <PRM/PRM_Conditional.h>
#include <PRM/PRM_Include.h>
#include <UT/UT_DirUtil.h>
#include <UT/UT_FileUtil.h>
#include <pxr/base/tf/fileUtils.h>
#include <pxr/base/tf/pathUtils.h>
#include <pxr/base/tf/stringUtils.h>

#include "Globals.h"
#include "Types.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::ZibraVDBOutputProcessor
{
    static PI_EditScriptedParms* CreateParameters()
    {
        static PRM_Name s_CopyAssetsName(PARM_COPY_ASSETS, "Copy ZibraVDB Assets to Layer Subdirectory");
        static PRM_Name s_CopySubdirName(PARM_COPY_SUBDIRECTORY, "Output Subdirectory");
        static PRM_Name s_RelativePathsName(PARM_MAKE_PATHS_RELATIVE, "Make ZibraVDB Paths Relative");

        static PRM_Default s_CopyAssetsDefault(0);
        static PRM_Default s_CopySubdirDefault(0, "");
        static PRM_Default s_RelativePathsDefault(1);

        static PRM_Template s_ParameterTemplates[] = {
            PRM_Template(PRM_TOGGLE, 1, &s_CopyAssetsName, &s_CopyAssetsDefault),
            PRM_Template(PRM_STRING, 1, &s_CopySubdirName, &s_CopySubdirDefault),
            PRM_Template(PRM_TOGGLE, 1, &s_RelativePathsName, &s_RelativePathsDefault),
            PRM_Template()
        };

        return new PI_EditScriptedParms(nullptr, s_ParameterTemplates, false, true, false);
    }

    UT_StringHolder ZibraVDBOutputProcessor::displayName() const
    {
        return {OUTPUT_PROCESSOR_UI_NAME};
    }

    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor()
    {
        return std::make_shared<ZibraVDBOutputProcessor>();
    }

    const PI_EditScriptedParms* ZibraVDBOutputProcessor::parameters() const
    {
        static PI_EditScriptedParms* s_Parameters = CreateParameters();
        return s_Parameters;
    }

    void ZibraVDBOutputProcessor::beginSave(OP_Node* configNode, const UT_Options& configOverrides, OP_Node* lopNode, fpreal t
#if UT_VERSION_INT >= 0x14050000 // Houdini 20.5+ added stageVariables parameter
                                            ,
                                            const UT_Options& stageVariables
#endif
#if UT_VERSION_INT >= 0x15000000 // Houdini 21.0+ added error parameter
                                            ,
                                            UT_String& error
#endif
    )
    {
        m_MakePathsRelative = false;
        m_CopyAssets = false;
        m_CopySubdirectory.clear();

        if (configNode)
        {
            if (configNode->hasParm(PARM_MAKE_PATHS_RELATIVE))
            {
                m_MakePathsRelative = configNode->evalInt(PARM_MAKE_PATHS_RELATIVE, 0, t) != 0;
            }
            if (configNode->hasParm(PARM_COPY_ASSETS))
            {
                m_CopyAssets = configNode->evalInt(PARM_COPY_ASSETS, 0, t) != 0;
            }
            if (configNode->hasParm(PARM_COPY_SUBDIRECTORY))
            {
                UT_String str;
                configNode->evalString(str, PARM_COPY_SUBDIRECTORY, 0, t);
                m_CopySubdirectory = str.toStdString();
            }
        }
    }

    bool ZibraVDBOutputProcessor::CopyAssetFile(const std::string& sourcePath, const std::string& targetDir, std::string& newPath,
                                                UT_String& error)
    {
        if (!TfPathExists(sourcePath))
        {
            error = ("ZibraVDB Output Processor: Source file does not exist: " + sourcePath).c_str();
            return false;
        }

        if (!TfPathExists(targetDir))
        {
            if (!TfMakeDirs(targetDir, -1, true))
            {
                error = ("ZibraVDB Output Processor: Failed to create directory: " + targetDir).c_str();
                return false;
            }
        }

        std::string fileName = TfGetBaseName(sourcePath);
        std::string targetPath = TfStringCatPaths(targetDir, fileName);

        std::ostringstream errorStream;
        int result = UT_FileUtil::copyFile(sourcePath.c_str(), targetPath.c_str(), &errorStream);
        if (result != 0)
        {
            std::string errorMsg = errorStream.str();
            error = ("ZibraVDB Output Processor: Failed to copy file: " + errorMsg).c_str();
            return false;
        }

        newPath = TfNormPath(targetPath);
        return true;
    }

    bool ZibraVDBOutputProcessor::processSavePath(const UT_StringRef& assetPath, const UT_StringRef& referencingLayerPath,
                                                  bool assetIsLayer, UT_String& newPath, UT_String& error)
    {
        // This processor doesn't modify asset save destinations
        return false;
    }

    bool ZibraVDBOutputProcessor::processReferencePath(const UT_StringRef& assetPath, const UT_StringRef& referencingLayerPath,
                                                       bool assetIsLayer, UT_String& newPath, UT_String& error)
    {
        if (assetIsLayer)
        {
            return false;
        }

        URI uri(assetPath.toStdString());
        if (!uri.isValid || uri.scheme != ZIB_ZIBRAVDB_SCHEME)
        {
            return false;
        }

        std::string referencingLayerStr = referencingLayerPath.toStdString();

        if (m_CopyAssets)
        {
            std::string targetDir = TfGetPathName(referencingLayerStr);

            if (!m_CopySubdirectory.empty())
            {
                targetDir = TfStringCatPaths(targetDir, m_CopySubdirectory);
            }

            std::string copiedPath;
            if (!CopyAssetFile(uri.path, targetDir, copiedPath, error))
            {
                return false;
            }
            uri.path = copiedPath;
        }

        if (m_MakePathsRelative)
        {
            UT_String utPath(uri.path);
            UTmakeRelativeFilePath(utPath, TfGetPathName(referencingLayerStr).c_str());
            uri.path = utPath.toStdString();
        }

        newPath = uri.ToString();
        return true;
    }
} // namespace Zibra::ZibraVDBOutputProcessor
