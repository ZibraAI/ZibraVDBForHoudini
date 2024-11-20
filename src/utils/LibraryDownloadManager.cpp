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
            MessageBox::Show(MessageBox::Type::OK, "Failed to download ZibraVDB library.");
            return;
        }

        if (!CompressionEngine::IsLicenseValid(CompressionEngine::ZCE_Product::Compression))
        {
            MessageBox::Show(MessageBox::Type::OK, "Library downloaded successfully, but no valid license found. Visit "
                                                   "'https://effects.zibra.ai/zibravdbhoudini', set up your license and restart Houdini.");
            return;
        }
        MessageBox::Show(MessageBox::Type::OK, "Library downloaded successfully.");
    }
} // namespace Zibra::UI
