#include "PrecompiledHeader.h"

#include "LibraryDownloadManager.h"

#include "bridge/CompressionEngine.h"
#include "ui/MessageBox.h"

namespace Zibra::UI
{
    void LibraryDownloadManager::DownloadLibrary()
    {
        if (CompressionEngine::IsLibraryLoaded())
        {
            MessageBox::Show(MessageBox::Type::OK, "Library is already downloaded.");
            return;
        }

        MessageBox::Show(MessageBox::Type::YesNo,
                                            "By downloading ZibraVDB library you agree to ZibraVDB for Houdini Terms of Service - "
                                            "https://effects.zibra.ai/vdb-terms-of-services-trial. Do you wish to proceed?",
                                            &MessageBoxCallback);
    }

    void LibraryDownloadManager::MessageBoxCallback(MessageBox::Result result)
    {
        if (result == MessageBox::Result::No)
        {
            return;
        }
        CompressionEngine::DownloadLibrary();
        if (!CompressionEngine::IsLibraryLoaded())
        {
            MessageBox::Show(MessageBox::Type::OK, ZVDB_ERR_MSG_FAILED_TO_DOWNLOAD_LIBRARY);
            return;
        }

        if (!CompressionEngine::IsLicenseValid(CompressionEngine::ZCE_Product::Compression))
        {
            MessageBox::Show(MessageBox::Type::OK, ZVDB_MSG_LIB_DOWNLOADED_SUCCESSFULLY_WITH_NO_LICENSE);
            return;
        }
        MessageBox::Show(MessageBox::Type::OK, ZVDB_MSG_LIB_DOWNLOADED_SUCCESSFULLY_WITH_LICENSE);
    }
} // namespace Zibra::UI
