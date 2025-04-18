#include "TrialManager.h"

#include "LicenseManager.h"
#include "bridge/LibraryUtils.h"
#include "ui/MessageBox.h"

namespace Zibra
{
    using clock_t = std::chrono::system_clock;
    using time_point_t = std::chrono::time_point<clock_t>;

    static int g_CachedRemaining = -1;
    static time_point_t g_LastCheckTime;
    static bool g_TrialCompressionPermitted = false;

    bool TrialManager::RequestTrialCompression()
    {
        assert(LicenseManager::GetInstance().GetLicenseStatus(LicenseManager::Product::Compression) != LicenseManager::Status::OK);

        if (g_TrialCompressionPermitted)
        {
            return true;
        }

        Zibra::LibraryUtils::LoadLibrary();
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            return false;
        }

        int result = CE::Trial::CAPI::RequestTrialCompression();

        g_CachedRemaining = result;
        g_LastCheckTime = clock_t::now();
        g_TrialCompressionPermitted = result >= 0;

        return g_TrialCompressionPermitted;
    }

    void TrialManager::CheckoutTrialCompression()
    {
        g_TrialCompressionPermitted = false;
    }

    int TrialManager::GetRemainingTrialCompressions()
    {
        assert(LicenseManager::GetInstance().GetLicenseStatus(LicenseManager::Product::Compression) != LicenseManager::Status::OK);

        Zibra::LibraryUtils::LoadLibrary();
        if (!Zibra::LibraryUtils::IsLibraryLoaded())
        {
            return -1;
        }

        time_point_t now = clock_t::now();
        if (g_CachedRemaining != -1 && now - g_LastCheckTime < std::chrono::minutes(5))
        {
            return g_CachedRemaining;
        }

        g_CachedRemaining = CE::Trial::CAPI::TrialCompressionsRemaining();
        g_LastCheckTime = now;

        return g_CachedRemaining;
    }

} // namespace Zibra
