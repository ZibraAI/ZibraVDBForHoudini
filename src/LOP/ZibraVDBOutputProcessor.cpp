#include "PrecompiledHeader.h"
#include "ZibraVDBOutputProcessor.h"

#include "bridge/LibraryUtils.h"
#include "licensing/LicenseManager.h"
#include <UT/UT_StringHolder.h>
#include <UT/UT_FileUtil.h>
#include <OP/OP_Node.h>
#include <Zibra/CE/Addons/OpenVDBFrameLoader.h>
#include <Zibra/CE/Compression.h>
#include <openvdb/openvdb.h>
#include <openvdb/io/File.h>

#include <filesystem>
#include <regex>

namespace Zibra::ZibraVDBOutputProcessor
{
    using namespace std::literals;

    ZibraVDBOutputProcessor::ZibraVDBOutputProcessor()
        : m_Parameters(nullptr)
    {
        LibraryUtils::LoadLibrary();
        
        // Create parameter interface
        m_Parameters = new PI_EditScriptedParms();
        // Add parameters as needed
    }

    ZibraVDBOutputProcessor::~ZibraVDBOutputProcessor()
    {
        delete m_Parameters;
        
        if (LibraryUtils::IsLibraryLoaded())
        {
            m_CompressorManager.Release();
        }
    }

    UT_StringHolder ZibraVDBOutputProcessor::displayName() const
    {
        return UT_StringHolder(OUTPUT_PROCESSOR_NAME);
    }

    const PI_EditScriptedParms *ZibraVDBOutputProcessor::parameters() const
    {
        return m_Parameters;
    }

    void ZibraVDBOutputProcessor::beginSave(OP_Node *config_node,
                                           const UT_Options &config_overrides,
                                           OP_Node *lop_node,
                                           fpreal t,
                                           const UT_Options &stage_variables)
    {
        // Extract output directory from stage variables if available
        m_CurrentOutputDir.clear();
        
        // Clear compression cache for new save operation
        m_CompressionCache.clear();
    }

    bool ZibraVDBOutputProcessor::processReferencePath(const UT_StringRef &asset_path,
                                                      const UT_StringRef &referencing_layer_path,
                                                      bool asset_is_layer,
                                                      UT_String &newpath,
                                                      UT_String &error)
    {
        if (!LibraryUtils::IsLibraryLoaded())
        {
            return false;
        }

        // Only process VDB files
        if (!shouldProcessPath(asset_path))
        {
            return false;
        }

        // Get output directory
        std::string outputDir = getOutputDirectory(referencing_layer_path);
        if (outputDir.empty())
        {
            error = "Could not determine output directory";
            return false;
        }

        // Compress VDB file and get zibravdb:// URI
        return compressVDBFile(asset_path.toStdString(), outputDir, newpath, error);
    }

    bool ZibraVDBOutputProcessor::shouldProcessPath(const UT_StringRef &asset_path) const
    {
        std::string pathStr = asset_path.toStdString();
        
        // Skip if already a zibravdb:// URI
        if (pathStr.find("zibravdb://") == 0)
        {
            return false;
        }
        
        // Only process .vdb files
        return pathStr.find(".vdb") != std::string::npos;
    }

    std::string ZibraVDBOutputProcessor::getOutputDirectory(const UT_StringRef &referencing_layer_path) const
    {
        if (!m_CurrentOutputDir.empty())
        {
            return m_CurrentOutputDir;
        }
        
        std::string layerPath = referencing_layer_path.toStdString();
        if (layerPath.empty())
        {
            return "";
        }
        
        std::filesystem::path path(layerPath);
        return path.parent_path().string();
    }

    bool ZibraVDBOutputProcessor::compressVDBFile(const std::string& vdbPath,
                                                 const std::string& outputDir,
                                                 UT_String& zibraVDBPath,
                                                 UT_String& error)
    {
        try
        {
            // Check if VDB file exists
            if (!std::filesystem::exists(vdbPath))
            {
                error = "VDB file not found: " + vdbPath;
                return false;
            }

            // Create compressed directory
            std::filesystem::path compressedDir = std::filesystem::path(outputDir) / "compressed";
            std::filesystem::create_directories(compressedDir);

            // Generate compressed file name
            std::filesystem::path vdbFilePath(vdbPath);
            std::string compressedName = vdbFilePath.stem().string() + "_compressed.zibravdb";
            std::string compressedPath = (compressedDir / compressedName).string();

            // Check cache first
            if (m_CompressionCache.find(vdbPath) != m_CompressionCache.end())
            {
                const auto& info = m_CompressionCache[vdbPath];
                zibraVDBPath = "zibravdb://./compressed/" + std::filesystem::path(info.compressedPath).filename().string() + "?frame=0";
                return true;
            }

            // Set up frame mapping and compression settings
            CE::Compression::FrameMappingDecs frameMappingDesc{};
            frameMappingDesc.sequenceStartIndex = 0;
            frameMappingDesc.sequenceIndexIncrement = 1;
            
            float defaultQuality = 0.75f; // Medium quality
            std::vector<std::pair<UT_String, float>> perChannelSettings; // Empty for default

            // Initialize compressor
            auto status = m_CompressorManager.Initialize(frameMappingDesc, defaultQuality, perChannelSettings);
            if (status != CE::ZCE_SUCCESS)
            {
                error = "Failed to initialize compressor";
                return false;
            }

            // Start compression sequence
            status = m_CompressorManager.StartSequence(UT_String(compressedPath));
            if (status != CE::ZCE_SUCCESS)
            {
                error = "Failed to start compression sequence";
                return false;
            }

            // Load and compress the VDB file
            std::vector<openvdb::GridBase::ConstPtr> volumes;
            std::vector<std::string> channelNames;
            std::vector<const char*> channelCStrings;
            CE::Compression::CompressFrameDesc compressFrameDesc{};
            CE::Addons::OpenVDBUtils::FrameLoader* vdbFrameLoader = nullptr;
            
            try
            {
                // Read VDB file and extract grids
                openvdb::io::File vdbFile(vdbPath);
                vdbFile.open();
                
                // Read all grids from the VDB file
                for (auto nameIter = vdbFile.beginName(); nameIter != vdbFile.endName(); ++nameIter)
                {
                    openvdb::GridBase::ConstPtr grid = vdbFile.readGrid(nameIter.gridName());
                    if (grid)
                    {
                        volumes.push_back(grid);
                        channelNames.push_back(nameIter.gridName());
                        channelCStrings.push_back(channelNames.back().c_str());
                    }
                }
                vdbFile.close();
                
                if (volumes.empty())
                {
                    error = "No grids found in VDB file";
                    return false;
                }
                
                // Create frame loader with OpenVDB grids
                vdbFrameLoader = new CE::Addons::OpenVDBUtils::FrameLoader{volumes.data(), volumes.size()};
                CE::Addons::OpenVDBUtils::EncodingMetadata encodingMetadata{};
                
                compressFrameDesc.channelsCount = channelNames.size();
                compressFrameDesc.channels = channelCStrings.data();
                compressFrameDesc.frame = vdbFrameLoader->LoadFrame(&encodingMetadata);

                // Compress the frame
                CE::Compression::FrameManager* frameManager = nullptr;
                status = m_CompressorManager.CompressFrame(compressFrameDesc, &frameManager);
                if (status != CE::ZCE_SUCCESS)
                {
                    error = "Failed to compress VDB frame";
                    return false;
                }

                // Finish the frame
                if (frameManager)
                {
                    status = frameManager->Finish();
                    if (status != CE::ZCE_SUCCESS)
                    {
                        error = "Failed to finish frame compression";
                        return false;
                    }
                }

                // Finish the sequence
                std::string warning;
                status = m_CompressorManager.FinishSequence(warning);
                if (status != CE::ZCE_SUCCESS)
                {
                    error = "Failed to finish compression sequence";
                    if (vdbFrameLoader && compressFrameDesc.frame) {
                        vdbFrameLoader->ReleaseFrame(compressFrameDesc.frame);
                    }
                    delete vdbFrameLoader;
                    return false;
                }
                
                // Release the frame and cleanup
                if (vdbFrameLoader && compressFrameDesc.frame) {
                    vdbFrameLoader->ReleaseFrame(compressFrameDesc.frame);
                }
                delete vdbFrameLoader;
                vdbFrameLoader = nullptr;
            }
            catch (const std::exception& e)
            {
                error = "Exception during VDB loading: " + std::string(e.what());
                if (vdbFrameLoader && compressFrameDesc.frame) {
                    vdbFrameLoader->ReleaseFrame(compressFrameDesc.frame);
                }
                delete vdbFrameLoader;
                m_CompressorManager.Release();
                return false;
            }

            // Release compressor resources
            m_CompressorManager.Release();

            // Cache the result
            VDBCompressionInfo info;
            info.originalPath = vdbPath;
            info.compressedPath = compressedPath;
            info.frameIndices.push_back(0);
            m_CompressionCache[vdbPath] = info;

            // Return zibravdb:// URI
            zibraVDBPath = "zibravdb://./compressed/" + compressedName + "?frame=0";
            return true;
        }
        catch (const std::exception& e)
        {
            error = "Exception during compression: " + std::string(e.what());
            m_CompressorManager.Release();
            return false;
        }
    }


    // Factory function for registration
    HUSD_OutputProcessorPtr createZibraVDBOutputProcessor()
    {
        return HUSD_OutputProcessorPtr(new ZibraVDBOutputProcessor());
    }

} // namespace Zibra::ZibraVDBOutputProcessor