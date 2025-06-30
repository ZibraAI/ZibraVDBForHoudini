#include "PrecompiledHeader.h"
#include "LOP_ZibraVDBCompressionMarker.h"
#include <HUSD/HUSD_DataHandle.h>
#include <HUSD/XUSD_Data.h>
#include <HUSD/HUSD_Utils.h>
#include <HUSD/HUSD_Constants.h>
#include <LOP/LOP_Error.h>
#include <UT/UT_StringHolder.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/references.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <iostream>

PXR_NAMESPACE_USING_DIRECTIVE

namespace Zibra::ZibraVDBCompressionMarker
{
    // Parameter names and defaults
    static PRM_Name PRMoutputDirName("output_dir", "Output Directory");
    static PRM_Default PRMoutputDirDefault(0.0f, "./compressed");
    
    static PRM_Name PRMoutputFilenameName("output_filename", "Output Filename");
    static PRM_Default PRMoutputFilenameDefault(0.0f, "$OS");
    
    static PRM_Name PRMremoveOriginalName("remove_original", "Remove Original Files");
    static PRM_Default PRMremoveOriginalDefault(0);  // 0 = unchecked by default
    
    static PRM_Name PRMqualityName("quality", "Compression Quality");
    static PRM_Default PRMqualityDefault(0.6f);  // Default quality of 0.6
    static PRM_Range PRMqualityRange(PRM_RANGE_RESTRICTED, 0.0f, PRM_RANGE_RESTRICTED, 1.0f);
    OP_Node* LOP_ZibraVDBCompressionMarker::Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new LOP_ZibraVDBCompressionMarker{net, name, op};
    }

    PRM_Template* LOP_ZibraVDBCompressionMarker::GetTemplateList() noexcept
    {
        static PRM_Template templateList[] = {
            PRM_Template(PRM_STRING, 1, &PRMoutputDirName, &PRMoutputDirDefault),
            PRM_Template(PRM_STRING, 1, &PRMoutputFilenameName, &PRMoutputFilenameDefault),
            PRM_Template(PRM_FLT_J, 1, &PRMqualityName, &PRMqualityDefault, 0, &PRMqualityRange),
            PRM_Template(PRM_TOGGLE, 1, &PRMremoveOriginalName, &PRMremoveOriginalDefault),
            PRM_Template()  // Terminator
        };
        return templateList;
    }

    LOP_ZibraVDBCompressionMarker::LOP_ZibraVDBCompressionMarker(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : LOP_Node(net, name, entry)
    {
    }

    OP_ERROR LOP_ZibraVDBCompressionMarker::cookMyLop(OP_Context& context)
    {
        // Make this node time-dependent so it cooks every frame
        flags().setTimeDep(true);
        
        HUSD_DataHandle &dataHandle = editableDataHandle();
        if (nInputs() > 0)
        {
            HUSD_DataHandle inputHandle = lockedInputData(context, 0);
            if (inputHandle.hasData())
            {
                dataHandle = inputHandle;
            }
            else
            {
                addError(LOP_MESSAGE, "Input data handle is empty. Cannot mark for compression.");
                return error();
            }
        }
        else
        {
            addError(LOP_MESSAGE, "ZibraVDBCompressionMarker node requires at least one input");
            return error();
        }

        // Now modify the stage with write lock
        HUSD_AutoWriteLock lock(dataHandle);
        if (!lock.isStageValid())
        {
            addError(LOP_MESSAGE, "Failed to acquire write lock for adding compression metadata.");
            return error();
        }

        UsdStageRefPtr stage = lock.data()->stage();
        if (!stage)
            return error();

        UsdPrimRange primRange = stage->Traverse();
        for (UsdPrim prim : primRange)
        {
            if (!prim.IsValid() || !prim.IsActive())
                continue;

            // Example: Match USD Volume prims
            if (prim.GetTypeName() == TfToken("Volume"))
            {
                std::cout << "Found volume: " << prim.GetPath() << std::endl;

                // Print reference paths, if present
                if (prim.HasAuthoredReferences())
                {
                    const UsdReferences &refs = prim.GetReferences();
                    std::cout << "  Has references." << std::endl;
                    // (you may need to inspect the reference list here)
                }

                // Also look for OpenVDB asset children
                for (const UsdPrim &child : prim.GetChildren())
                {
                    if (child.GetTypeName() == TfToken("OpenVDBAsset"))
                    {
                        std::cout << "  OpenVDBAsset child: " << child.GetPath() << std::endl;

                        UsdAttribute fileAttr = child.GetAttribute(TfToken("filePath"));
                        if (fileAttr.HasAuthoredValue())
                        {
                            SdfAssetPath asset;
                            fileAttr.Get(&asset);
                            std::cout << "    File: " << asset.GetResolvedPath() << std::endl;
                        }
                    }
                }

                // Add ZibraVDB compression marker metadata
                std::string markerIdentifier = getName().toStdString() + "_" + std::to_string(getUniqueId());
                prim.SetCustomDataByKey(TfToken("zibravdb:compression_marker"), VtValue(markerIdentifier));
                
                std::cout << "  Tagged with ZibraVDB marker: " << markerIdentifier << std::endl;
            }
        }

        return error();
    }

    bool LOP_ZibraVDBCompressionMarker::updateParmsFlags()
    {
        bool changed = LOP_Node::updateParmsFlags();
        flags().setTimeDep(true);
        return changed;
    }
    
    std::string LOP_ZibraVDBCompressionMarker::getOutputDirectory(fpreal t) const
    {
        UT_String outputDir;
        evalString(outputDir, "output_dir", 0, t);
        return outputDir.toStdString();
    }
    
    std::string LOP_ZibraVDBCompressionMarker::getOutputFilename(fpreal t) const
    {
        UT_String outputFilename;
        evalString(outputFilename, "output_filename", 0, t);
        return outputFilename.toStdString();
    }
    
    float LOP_ZibraVDBCompressionMarker::getCompressionQuality(fpreal t) const
    {
        return static_cast<float>(evalFloat("quality", 0, t));
    }
    
    bool LOP_ZibraVDBCompressionMarker::getRemoveOriginalFiles(fpreal t) const
    {
        return evalInt("remove_original", 0, t) != 0;
    }

} // namespace Zibra::ZibraVDBCompressionMarker