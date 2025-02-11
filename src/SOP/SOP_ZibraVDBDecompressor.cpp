#include "PrecompiledHeader.h"

#include "SOP_ZibraVDBDecompressor.h"

#include "bridge/LibraryUtils.h"
#include "bridge/Licensing/Licensing.h"
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
    using namespace CE::Decompression;

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

        static PRM_Name theDownloadLibraryButtonName(DOWNLOAD_LIBRARY_BUTTON_NAME, "Download Library");

        static PRM_Template templateList[] = {
            PRM_Template(PRM_FILE, 1, &theFileName, &theFileDefault), PRM_Template(PRM_INT, 1, &theFrameName, &theFrameDefault),
            PRM_Template(PRM_CALLBACK, 1, &theReloadCacheName, nullptr, nullptr, nullptr, theReloadCallback),
            PRM_Template(PRM_CALLBACK, 1, &theDownloadLibraryButtonName, nullptr, nullptr, nullptr,
                         &SOP_ZibraVDBDecompressor::DownloadLibrary),
            PRM_Template()};
        return templateList;
    }

    SOP_ZibraVDBDecompressor::SOP_ZibraVDBDecompressor(OP_Network* net, const char* name, OP_Operator* entry) noexcept
        : SOP_Node(net, name, entry)
    {
        Zibra::LibraryUtils::LoadLibrary();
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            return;
        }

        if (!CE::Licensing::CAPI::CheckoutLicenseWithKey(LicenseManager::GetKey().c_str()) &&
            !CE::Licensing::CAPI::CheckoutLicenseOffline(LicenseManager::GetOfflineLicense().c_str()))
        {
            addError(ROP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_LICENSE_ERROR);
            return;
        }

        m_RHIWrapper = new RHIWrapper();
        m_RHIWrapper->Initialize();

        CAPI::CreateDecompressorFactory(&m_Factory);
        m_Factory->UseRHI(m_RHIWrapper->GetRHIRuntime());
    }

    SOP_ZibraVDBDecompressor::~SOP_ZibraVDBDecompressor() noexcept
    {
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            return;
        }
        if (CE::Licensing::CAPI::GetLicenseStatus(CE::Licensing::ProductType::Decompression) != CE::Licensing::LicenseStatus::OK)
        {
            return;
        }
        if (m_Decompressor)
        {
            m_Decompressor->Release();
            m_Decompressor = nullptr;
        }
        if (m_Factory)
        {
            m_Factory->Release();
            m_Factory = nullptr;
        }
        if (m_RHIWrapper)
        {
            m_RHIWrapper->Release();
            delete m_RHIWrapper;
            m_RHIWrapper = nullptr;
        }
    }

    OP_ERROR SOP_ZibraVDBDecompressor::cookMySop(OP_Context& context)
    {
        gdp->clearAndDestroy();

        if (!Zibra::LibraryUtils::IsPlatformSupported())
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_PLATFORM_NOT_SUPPORTED);
            return error(context);
        }

        Zibra::LibraryUtils::LoadLibrary();
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_COMPRESSION_ENGINE_MISSING);
            return error(context);
        }
        if (CE::Licensing::CAPI::GetLicenseStatus(CE::Licensing::ProductType::Decompression) != CE::Licensing::LicenseStatus::OK)
        {
            addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_LICENSE_ERROR);
            return error(context);
        }

        const bool filenameIsDirty = isParmDirty("filename", context.getTime());

        // getCookedData() used as a hack for Reload Cache detection
        if (filenameIsDirty || getCookedData(context) == nullptr)
        {
            if (m_Decoder)
            {
                CE::Decompression::CAPI::ReleaseDecoder(m_Decoder);
                m_Decoder = nullptr;
            }

            UT_String filename = "";
            evalString(filename, "filename", nullptr, 0, context.getTime());
            if (filename == "")
            {
                addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_NO_FILE_SELECTED);
                return error(context);
            }

            auto status = CE::Decompression::CAPI::CreateDecoder(filename.c_str(), &m_Decoder);
            if (status != CE::ZCE_SUCCESS)
            {
                addError(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_COULDNT_OPEN_FILE);
                return error(context);
            }

            status = m_Factory->UseDecoder(m_Decoder);
            if (status != CE::ZCE_SUCCESS)
            {
                addError(SOP_MESSAGE, "Failed to assign decoder to compressor factory.");
                return error(context);
            }

            if (m_Decompressor)
            {
                m_RHIWrapper->FreeExternalBuffers();
                m_Decompressor->Release();
                m_Decompressor = nullptr;
            }

            status = m_Factory->Create(&m_Decompressor);
            if (status != CE::ZCE_SUCCESS)
            {
                addError(SOP_MESSAGE, "Failed to create decompressor instance.");
                return error(context);
            }

            status = m_Decompressor->Initialize();
            if (status != CE::ZCE_SUCCESS)
            {
                addError(SOP_MESSAGE, "Failed to initialize decompressor instance.");
                return error(context);
            }

            m_RHIWrapper->AllocateExternalBuffers(m_Decompressor->GetResourcesRequirements());

            status = m_Decompressor->RegisterResources(m_RHIWrapper->GetDecompressorResources());
            if (status != CE::ZCE_SUCCESS)
            {
                addError(SOP_MESSAGE, "Failed to register external decompressor resources.");
                return error(context);
            }
        }

        const exint frameIndex = evalInt(FRAME_PARAM_NAME, 0, context.getTime());

        CE::Decompression::CompressedFrameContainer* frameContainer = nullptr;
        CE::Decompression::FormatMapper* formatMapper = m_Decompressor->GetFormatMapper();
        FrameRange frameRange = formatMapper->GetFrameRange();

        if (frameIndex < frameRange.start || frameIndex > frameRange.end)
        {
            addWarning(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_FRAME_INDEX_OUT_OF_RANGE);
            return error(context);
        }

        formatMapper->FetchFrame(frameIndex, &frameContainer);

        if (frameContainer == nullptr)
        {
            addError(SOP_MESSAGE, "Error when trying to fetch frame");
            return error(context);
        }

        m_RHIWrapper->StartRecording();
        m_Decompressor->DecompressFrame(frameContainer);
        m_RHIWrapper->StopRecording();
        m_RHIWrapper->GarbageCollect();

        FrameInfo frameInfo = frameContainer->GetInfo();
        OpenVDBSupport::EncodeMetadata encodeMetadata = ReadEncodeMetadata(frameContainer);
        OpenVDBSupport::DecompressedFrameData decompressedFrameData = m_RHIWrapper->GetDecompressedFrameData(frameInfo);
        auto vdbGrids = OpenVDBSupport::OpenVDBEncoder::EncodeFrame(frameInfo, decompressedFrameData, encodeMetadata);

        gdp->addStringTuple(GA_ATTRIB_PRIMITIVE, "name", 1);
        GA_RWHandleS nameAttr{gdp->findPrimitiveAttribute("name")};
        for (size_t i = 0; i < vdbGrids.size(); ++i)
        {
            const openvdb::GridBase::Ptr grid = vdbGrids[i];

            if (!grid)
            {
                addError(SOP_MESSAGE, ("Failed to decompress channel: "s + frameInfo.channels[i].name).c_str());
                continue;
            }

            GU_PrimVDB* vdbPrim = GU_PrimVDB::build(gdp);
            nameAttr.set(vdbPrim->getMapOffset(), grid->getName());
            vdbPrim->setGrid(*grid);

            ApplyGridMetadata(vdbPrim, frameContainer);
        }

        ApplyDetailMetadata(gdp, frameContainer);

        frameContainer->Release();

        return error(context);
    }

    int SOP_ZibraVDBDecompressor::DownloadLibrary(void* data, int index, fpreal32 time, const PRM_Template* tplate)
    {
        return 1;

        // using namespace Zibra::UI;

        // auto node = static_cast<SOP_ZibraVDBDecompressor*>(data);

        // if (CompressionEngine::IsLibraryLoaded())
        //{
        //     MessageBox::Result result = MessageBox::Show(MessageBox::Type::OK, "Library is already downloaded.", "ZibraVDB");
        //     return 0;
        // }
        // MessageBox::Result result = MessageBox::Show(MessageBox::Type::YesNo,
        //                                              "By downloading ZibraVDB library you agree to ZibraVDB for Houdini Terms of Service
        //                                              - " "https://effects.zibra.ai/vdb-terms-of-services-trial. Do you wish to proceed?",
        //                                              "ZibraVDB");
        // if (result == MessageBox::Result::No)
        //{
        //     return 0;
        // }
        // CompressionEngine::DownloadLibrary();
        // if (!CompressionEngine::IsLibraryLoaded())
        //{
        //     node->addError(SOP_MESSAGE, ZVDB_ERR_MSG_FAILED_TO_DOWNLOAD_LIBRARY);
        //     MessageBox::Show(MessageBox::Type::OK, ZVDB_ERR_MSG_FAILED_TO_DOWNLOAD_LIBRARY, "ZibraVDB");
        //     return 0;
        // }
        // if (CE::Licensing::GetLicenseStatus(CE::Licensing::ProductType::Decompression) != CE::Licensing::LicenseStatus::OK)
        //{
        //     node->addWarning(SOP_MESSAGE, ZIBRAVDB_ERROR_MESSAGE_NO_LICENSE_AFTER_DOWNLOAD);
        //     MessageBox::Show(MessageBox::Type::OK, ZVDB_MSG_LIB_DOWNLOADED_SUCCESSFULLY_WITH_NO_LICENSE, "ZibraVDB");
        //     return 0;
        // }
        // MessageBox::Show(MessageBox::Type::OK, ZVDB_MSG_LIB_DOWNLOADED_SUCCESSFULLY_WITH_LICENSE, "ZibraVDB");
        // return 0;
    }

    void SOP_ZibraVDBDecompressor::ApplyGridMetadata(GU_PrimVDB* vdbPrim, CE::Decompression::CompressedFrameContainer* const frameContainer)
    {
        ApplyGridAttributeMetadata(vdbPrim, frameContainer);
        ApplyGridVisualizationMetadata(vdbPrim, frameContainer);
    }

    void SOP_ZibraVDBDecompressor::ApplyGridAttributeMetadata(GU_PrimVDB* vdbPrim,
                                                              CE::Decompression::CompressedFrameContainer* const frameContainer)
    {
        const std::string attributeMetadataName = "houdiniPrimitiveAttributes_"s + vdbPrim->getGridName();

        const char* metadataEntry = frameContainer->GetMetadataByKey(attributeMetadataName.c_str());

        if (metadataEntry)
        {
            auto primAttribMeta = nlohmann::json::parse(metadataEntry);
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

    void SOP_ZibraVDBDecompressor::ApplyGridVisualizationMetadata(GU_PrimVDB* vdbPrim,
                                                                  CE::Decompression::CompressedFrameContainer* const frameContainer)
    {
        const std::string keyPrefix = "houdiniVisualizationAttributes_"s + vdbPrim->getGridName();

        const std::string keyVisMode = keyPrefix + "_mode";
        const char* visModeMetadata = frameContainer->GetMetadataByKey(keyVisMode.c_str());

        const std::string keyVisIso = keyPrefix + "_iso";
        const char* visIsoMetadata = frameContainer->GetMetadataByKey(keyVisIso.c_str());

        const std::string keyVisDensity = keyPrefix + "_density";
        const char* visDensityMetadata = frameContainer->GetMetadataByKey(keyVisDensity.c_str());

        const std::string keyVisLod = keyPrefix + "_lod";
        const char* visLodMetadata = frameContainer->GetMetadataByKey(keyVisLod.c_str());

        if (visModeMetadata && visIsoMetadata && visDensityMetadata && visLodMetadata)
        {
            GEO_VolumeOptions visOptions{};
            visOptions.myMode = static_cast<GEO_VolumeVis>(std::stoi(visModeMetadata));
            visOptions.myIso = std::stof(visIsoMetadata);
            visOptions.myDensity = std::stof(visDensityMetadata);
            visOptions.myLod = static_cast<GEO_VolumeVisLod>(std::stoi(visLodMetadata));
            vdbPrim->setVisOptions(visOptions);
        }
    }

    void SOP_ZibraVDBDecompressor::ApplyDetailMetadata(GU_Detail* gdp, CE::Decompression::CompressedFrameContainer* const frameContainer)
    {
        const char* detailMetadata = frameContainer->GetMetadataByKey("houdiniDetailAttributes");

        if (!detailMetadata)
        {
            return;
        }

        auto detailAttribMeta = nlohmann::json::parse(detailMetadata);
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

    OpenVDBSupport::EncodeMetadata SOP_ZibraVDBDecompressor::ReadEncodeMetadata(
        CE::Decompression::CompressedFrameContainer* const frameContainer)
    {
        OpenVDBSupport::EncodeMetadata encodeMetadata{};
        const char* metadataKey = "houdiniDecodeMetadata";
        const char* metadataValue = frameContainer->GetMetadataByKey(metadataKey);
        if (metadataValue)
        {
            std::istringstream metadataStream(metadataValue);
            metadataStream >> encodeMetadata.offsetX >> encodeMetadata.offsetY >> encodeMetadata.offsetZ;
        }

        return encodeMetadata;
    }

} // namespace Zibra::ZibraVDBDecompressor
