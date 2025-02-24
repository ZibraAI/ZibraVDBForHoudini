#include "PrecompiledHeader.h"

#include "LibraryDownloadManager.h"

#include "Zibra/CE/Licensing.h"
#include "bridge/LibraryUtils.h"
#include "bridge/Licensing/Licensing.h"
#include "ui/MessageBox.h"

namespace Zibra::UI
{
    void LibraryDownloadManager::DownloadLibrary()
    {
        if (LibraryUtils::IsLibraryLoaded())
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

        LibraryUtils::DownloadLibrary();

        if (!LibraryUtils::IsLibraryLoaded())
        {
            MessageBox::Show(MessageBox::Type::OK, ZVDB_MSG_FAILED_TO_DOWNLOAD_LIBRARY);
            return;
        }

        if (!CE::Licensing::CAPI::CheckoutLicenseWithKey(LicenseManager::GetKey().c_str()) &&
            !CE::Licensing::CAPI::CheckoutLicenseOffline(LicenseManager::GetOfflineLicense().c_str()))
        {
            MessageBox::Show(MessageBox::Type::OK, ZVDB_MSG_LIB_DOWNLOADED_SUCCESSFULLY_WITH_NO_LICENSE);
            return;
        }

        MessageBox::Show(MessageBox::Type::OK, ZVDB_MSG_LIB_DOWNLOADED_SUCCESSFULLY_WITH_LICENSE);
    }
} // namespace Zibra::UI
