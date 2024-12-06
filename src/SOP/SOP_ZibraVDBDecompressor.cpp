#include "PrecompiledHeader.h"

#include "SOP_ZibraVDBDecompressor.h"

#include "bridge/CompressionEngine.h"
#include "openvdb/OpenVDBEncoder.h"
#include "ui/MessageBox.h"
#include "utils/GAAttributesDump.h"

#ifdef _DEBUG
#define DBG_NAME(expression) expression
#else
#define DBG_NAME(expression) ""
#endif

namespace Zibra::ZibraVDBDecompressor
{
    using namespace std::literals;

    class StreamAutorelease
    {
    public:
        explicit StreamAutorelease(std::ifstream& stream) noexcept
            : m_ManagedStream(stream)
        {
        }
        ~StreamAutorelease() noexcept
        {
            m_ManagedStream.close();
        }

    private:
        std::ifstream& m_ManagedStream;
    };

    OP_Node* SOP_ZibraVDBDecompressor::Constructor(OP_Network* net, const char* name, OP_Operator* op) noexcept
    {
        return new SOP_ZibraVDBDecompressor{net, name, op};
    }

    PRM_Template* SOP_ZibraVDBDecompressor::GetTemplateList() noexcept
    {
        static PRM_Name theFileName(FILENAME_PARAM_NAME, "Input File");
        static PRM_Default theFileDefault(0, "$HIP/vol/$HIPNAME.$OS.zibravdb");

        static PRM_Name theFrameName(FRAME_PARAM_NAME, "Sequence Frame");
        static PRM_Default theFrameDefault(0, "$F");

        static PRM_Name theReloadCacheName(REFRESH_CALLBACK_PARAM_NAME, "Reload Cache");
        static PRM_Callback theReloadCallback{[](void* node, int index, fpreal64 time, const PRM_Template* tplate) -> int {
            auto self = static_cast<SOP_ZibraVDBDecompressor*>(node);
            self->deleteCookedData();
            self->refreshGdp();
            return 1;
        }};

        static PRM_Name theCoreLibPathName(CORE_LIB_PATH_FIELD_NAME, "Core Lib");
        static PRM_Default theCoreLibPathDefault(0, CompressionEngine::g_LibraryPath);

        static PRM_Name theDownloadLibraryButtonName(DOWNLOAD_LIBRARY_BUTTON_NAME, "Download Library");

        static PRM_Template templateList[] = {
            PRM_Template(PRM_FILE, 1, &theFileName, &theFileDefault),
            PRM_Template(PRM_INT, 1, &theFrameName, &theFrameDefault),
            PRM_Template(PRM_CALLBACK, 1, &theReloadCacheName, nullptr, nullptr, nullptr, theReloadCallback),
            PRM_Template(PRM_CALLBACK, 1, &theDownloadLibraryButtonName, nullptr, nullptr, nullptr,
                         &SOP_ZibraVDBDecompressor::DownloadLibrary),
            PRM_Template(PRM_STRING_E, 1, &theCoreLibPathName, &theCoreLibPathDefault, nullptr, nullptr, 0, &PRM_SpareData::saveValueNever),
            PRM_Template()};
        return templateList;
    }

    SOP_ZibraVDBDecompressor::SOP_ZibraVDBDecompressor(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : SOP_Node(net, name, entry)
    {
        CompressionEngine::LoadLibrary();
        if (!CompressionEngine::IsLibraryLoaded())
        {
            return;
        }

        if (!CompressionEngine::IsLicenseValid(CompressionEngine::ZCE_Product::Render))
        {
            return;
        }
        m_DecompressorInstanceID = CompressionEngine::CreateDecompressorInstance();
    }

    SOP_ZibraVDBDecompressor::~SOP_ZibraVDBDecompressor() noexcept
    {
        if (m_DecompressorInstanceID == uint32_t(-1))
        {
            return;
        }
        if (!CompressionEngine::IsLibraryLoaded())
        {
            return;
        }
        if (!CompressionEngine::IsLicenseValid(CompressionEngine::ZCE_Product::Render))
        {
            return;
        }
        CompressionEngine::ReleaseDecompressorInstance(m_DecompressorInstanceID);
    }

    OP_ERROR SOP_ZibraVDBDecompressor::cookMySop(OP_Context& context)
    {
        gdp->clearAndDestroy();

        if (!CompressionEngine::IsPlatformSupported())
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED);
            return error(context);
        }

        CompressionEngine::LoadLibrary();
        if (!CompressionEngine::IsLibraryLoaded())
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING);
            return error(context);
        }

        if (!CompressionEngine::IsLicenseValid(CompressionEngine::ZCE_Product::Render))
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_LICENSE_ERROR);
            return error(context);
        }

        if (m_DecompressorInstanceID == uint32_t(-1))
        {
            m_DecompressorInstanceID = CompressionEngine::CreateDecompressorInstance();
        }

        UT_String filename = "";
        evalString(filename, "filename", nullptr, 0, context.getTime());

        if (filename == "")
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_NO_FILE_SELECTED);
            return error(context);
        }

        bool isFileOpened = CompressionEngine::SetInputFile(m_DecompressorInstanceID, filename.c_str());
        if (!isFileOpened)
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_COULDNT_OPEN_FILE);
            return error(context);
        }

        CompressionEngine::ZCE_SequenceInfo sequenceInfo{};
        CompressionEngine::GetSequenceInfo(m_DecompressorInstanceID, &sequenceInfo);

        const exint frameIndex = evalInt(FRAME_PARAM_NAME, 0, context.getTime());

        if (frameIndex < sequenceInfo.frameRangeBegin || frameIndex > sequenceInfo.frameRangeEnd)
        {
            addWarning(SOP_MESSAGE, "Frame index is out of range.");
            return error(context);
        }

        CompressionEngine::ZCE_DecompressedFrameContainer* frameContainer = nullptr;
        CompressionEngine::DecompressFrame(m_DecompressorInstanceID, frameIndex, &frameContainer);

        if (frameContainer == nullptr)
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_FRAME_INDEX_OUT_OF_RANGE);
            return error(context);
        }

        auto vdbGrids = OpenVDBSupport::OpenVDBEncoder::EncodeFrame(frameContainer->frameInfo, frameContainer->frameData);

        auto metadataBegin = frameContainer->metadata;
        auto metadataEnd = frameContainer->metadata + frameContainer->metadataCount;

        gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "name", 1);
        GA_RWHandleS nameAttr{gdp->findPrimitiveAttribute("name")};
        for (size_t i = 0; i < vdbGrids.size(); ++i)
        {
            const openvdb::GridBase::Ptr grid = vdbGrids[i];
            if (!grid)
            {
                addError(SOP_MESSAGE, ("Failed to decompress channel: "s + frameContainer->frameInfo.channelNames[i]).c_str());
                continue;
            }

            GU_PrimVDB* vdbPrim = GU_PrimVDB::build(gdp);
            nameAttr.set(vdbPrim->getMapOffset(), grid->getName());
            vdbPrim->setGrid(*grid);

            const std::string metadataName = "houdiniPrimitiveAttributes_"s + grid->getName();
            auto primMetaIt = std::find_if(metadataBegin, metadataEnd, [&metadataName](const CompressionEngine::ZCE_MetadataEntry& entry) {
                return entry.key == metadataName;
            });
            if (primMetaIt != metadataEnd)
            {
                auto primAttribMeta = nlohmann::json::parse(primMetaIt->value);
                switch (Utils::LoadEntityAttributesFromMeta(gdp, GA_ATTRIB_PRIMITIVE, vdbPrim->getMapOffset(), primAttribMeta))
                {
                case Utils::MetaAttributesLoadStatus::SUCCESS:
                    break;
                case Utils::MetaAttributesLoadStatus::FATAL_ERROR_INVALID_METADATA:
                    addWarning(SOP_MESSAGE, "Corrupted metadata for channel. Canceling attributes transfer.");
                    break;
                case Utils::MetaAttributesLoadStatus::ERROR_PARTIALLY_INVALID_METADATA:
                    addWarning(SOP_MESSAGE, "Partially corrupted metadata for channel. Skipping invalid attributes.");
                    break;
                }
            }
        }

        auto detailMetaIt = std::find_if(metadataBegin, metadataEnd, [](const CompressionEngine::ZCE_MetadataEntry& entry) {
            return entry.key == "houdiniDetailAttributes"s;
        });

        if (detailMetaIt != metadataEnd)
        {
            auto detailAttribMeta = nlohmann::json::parse(detailMetaIt->value);
            switch (Utils::LoadEntityAttributesFromMeta(gdp, GA_ATTRIB_DETAIL, 0, detailAttribMeta))
            {
            case Utils::MetaAttributesLoadStatus::SUCCESS:
                break;
            case Utils::MetaAttributesLoadStatus::FATAL_ERROR_INVALID_METADATA:
                addWarning(SOP_MESSAGE, "Corrupted metadata for channel. Canceling attributes transfer.");
                break;
            case Utils::MetaAttributesLoadStatus::ERROR_PARTIALLY_INVALID_METADATA:
                addWarning(SOP_MESSAGE, "Partially corrupted metadata for channel. Skipping invalid attributes.");
                break;
            }
        }

        CompressionEngine::FreeFrameData(frameContainer);

        return error(context);
    }

    int SOP_ZibraVDBDecompressor::DownloadLibrary(void* data, int index, fpreal32 time, const PRM_Template* tplate)
    {
        using namespace Zibra::UI;

        auto node = static_cast<SOP_ZibraVDBDecompressor*>(data);

        if (CompressionEngine::IsLibraryLoaded())
        {
            MessageBox::Result result = MessageBox::Show(MessageBox::Type::OK, "Library is already downloaded.", "ZibraVDB");
            return 0;
        }
        MessageBox::Result result = MessageBox::Show(MessageBox::Type::YesNo,
                                                     "By downloading ZibraVDB library you agree to ZibraVDB for Houdini Terms of Service - "
                                                     "https://effects.zibra.ai/vdb-terms-of-services-trial. Do you wish to proceed?",
                                                     "ZibraVDB");
        if (result == MessageBox::Result::No)
        {
            return 0;
        }
        CompressionEngine::DownloadLibrary();
        if (!CompressionEngine::IsLibraryLoaded())
        {
            node->addError(SOP_MESSAGE, "Failed to download ZibraVDB library.");
            return 0;
        }
        if (!CompressionEngine::IsLicenseValid(CompressionEngine::ZCE_Product::Render))
        {
            node->addWarning(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_NO_LICENSE_AFTER_DOWNLOAD);
            MessageBox::Show(MessageBox::Type::OK, ZIBRAVDB_ERROR_MESSAGE_NO_LICENSE_AFTER_DOWNLOAD, "ZibraVDB");
            return 0;
        }
        MessageBox::Show(MessageBox::Type::OK, "Library downloaded successfully.", "ZibraVDB");
        return 0;
    }

} // namespace Zibra::ZibraVDBDecompressor
