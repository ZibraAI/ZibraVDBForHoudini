#pragma once

#if defined(_MSC_VER)
#define ZCE_API_IMPORT extern "C" __declspec(dllimport)
#define ZCE_CALL_CONV __cdecl
#elif defined(__GNUC__)
#define ZCE_API_IMPORT extern "C"
#define ZCE_CALL_CONV
#else
#error "Unsupported compiler"
#endif

namespace Zibra::CE::Licensing
{
    enum class ProductType
    {
        Compression,
        Decompression,
        Count
    };

    class LicenseManager
    {
    public:
        virtual void CheckoutLicenseWithKey(const char* licenseKey) noexcept = 0;
        virtual void CheckoutLicenseOffline(const char* license, int licenseSize) noexcept = 0;
        virtual void CheckoutLicenseLicenseServer(const char* licenseServerAddress) noexcept = 0;
        virtual bool IsLicenseValidated(ProductType product) noexcept = 0;
        virtual int GetProductLicenseTier(ProductType product) noexcept = 0;
        virtual const char* GetProductLicenseType(ProductType product) noexcept = 0;
        virtual const char* GetLicenseError() noexcept = 0;
        virtual void ReleaseLicense() noexcept = 0;
        virtual const char* GetHardwareID() noexcept = 0;
    };

    typedef LicenseManager*(ZCE_CALL_CONV* PFN_GetLicenseManager)();
#ifdef ZCE_STATIC_LINKING
    LicenseManager* GetLicenseManager() noexcept;
#elif ZCE_DYNAMIC_IMPLICIT_LINKING
    ZCE_API_IMPORT LicenseManager* ZCE_CALL_CONV Zibra_CE_Licensing_GetLicenseManager() noexcept;
#else
    constexpr const char* GetLicenseManagerExportName = "Zibra_CE_Licensing_GetLicenseManager";
#endif
} // namespace Zibra::CE::Licensing

#undef ZCE_API_IMPORT
#undef ZCE_CALL_CONV
