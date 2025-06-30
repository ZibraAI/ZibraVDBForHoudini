#include "PrecompiledHeader.h"
#include "LOP_ZibraVDBCompressionMarker.h"
#include <HUSD/HUSD_DataHandle.h>
#include <HUSD/XUSD_Data.h>
#include <UT/UT_StringHolder.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/references.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/layer.h>
#include <HUSD/XUSD_Utils.h>
#include <HUSD/HUSD_ConfigureLayer.h>
#include <HUSD/HUSD_FindPrims.h>
#include <LOP/LOP_PRMShared.h>
#include <iostream>
#include <regex>
#include <filesystem>
#include <algorithm>
#include <cctype>

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

    OP_ERROR LOP_ZibraVDBCompressionMarker::cookMyLop(OP_Context &context)
    {
        if (cookModifyInput(context) >= UT_ERROR_FATAL)
            return error();

        // Get the current time for other parameter evaluation
        fpreal t = context.getTime();
        
        // Build the save path using parameters
        std::string outputDir = getOutputDirectory(t);
        std::string outputFilename = getOutputFilename(t);
        
        // Get marker node name in lowercase for filename
        std::string markerNodeName = getName().toStdString();
        std::transform(markerNodeName.begin(), markerNodeName.end(), markerNodeName.begin(), ::tolower);
        
        std::string upstreamNodeName = "unknown";
        if (nInputs() > 0 && getInput(0))
        {
            upstreamNodeName = getInput(0)->getName().toStdString();
            std::transform(upstreamNodeName.begin(), upstreamNodeName.end(), upstreamNodeName.begin(), ::tolower);
        }

        std::string layerName = markerNodeName + "_" + upstreamNodeName;

        // Replace $OS token with the generated layer name if present
        if (outputFilename.find("$OS") != std::string::npos)
        {
            std::regex osToken("\\$OS");
            outputFilename = std::regex_replace(outputFilename, osToken, layerName);
        }
        else if (outputFilename == "ZibraVDBCompressor")
        {
            // If using default, replace with our generated name
            outputFilename = layerName;
        }
        
        // Ensure the filename has .usda extension
        if (outputFilename.find(".usda") == std::string::npos && outputFilename.find(".usd") == std::string::npos)
        {
            outputFilename += ".usda";
        }
        
        std::string layerPath = outputDir + "/" + outputFilename;

        // Create the output directory
        //std::filesystem::create_directories(outputDir);

        // Use editableDataHandle to get write access to our data handle
        HUSD_AutoWriteLock writelock(editableDataHandle());
        HUSD_AutoLayerLock layerlock(writelock);

        // Configure the layer for saving to the specified path
        HUSD_ConfigureLayer configure_layer(writelock);
        configure_layer.setSavePath(layerPath.c_str(), false); // false = don't flatten
        
        std::cout << "[ZibraVDB] Configured layer save to: " << layerPath << std::endl;

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