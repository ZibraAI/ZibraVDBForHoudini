#pragma once

namespace Zibra::CE::Licensing
{
    enum class LicenseStatus
    {
        NotInitialized,
        OK,
        InvalidKeyFormat,
        NetworkError,
        ValidationError,
        NoKey
    };

    enum class ProductType
    {
        Compression,
        Count
    };

    bool CheckoutLicenseWithKey(const char* licenseKey) noexcept;
    bool CheckoutLicenseOffline(const char* license) noexcept;
    LicenseStatus GetLicenseStatus(ProductType product) noexcept;
    void ReleaseLicense() noexcept;
} // namespace Zibra::CE::Licensing

#pragma region CAPI

#define ZCE_API_IMPORT extern "C" __declspec(dllimport)
#define ZCE_NS Zibra::CE::Licensing

#ifndef ZCE_NO_CAPI_IMPL

#pragma region Funcs
#define ZCE_LICENSING_FUNCS_EXPORT_FNPFX(name) Zibra_CE_Licensing_##name

#define ZCE_LICENSING_FUNCS_API_APPLY(macro)                         \
    macro(ZCE_LICENSING_FUNCS_EXPORT_FNPFX(CheckoutLicenseWithKey)); \
    macro(ZCE_LICENSING_FUNCS_EXPORT_FNPFX(CheckoutLicenseOffline)); \
    macro(ZCE_LICENSING_FUNCS_EXPORT_FNPFX(GetLicenseStatus));       \
    macro(ZCE_LICENSING_FUNCS_EXPORT_FNPFX(ReleaseLicense))

#define ZCE_FNPFX(name) ZCE_LICENSING_FUNCS_EXPORT_FNPFX(name)

typedef bool (*ZCE_PFN(ZCE_FNPFX(CheckoutLicenseWithKey)))(const char* licenseKey);
typedef bool (*ZCE_PFN(ZCE_FNPFX(CheckoutLicenseOffline)))(const char* licenseKey);
typedef ZCE_NS::LicenseStatus(*ZCE_PFN(ZCE_FNPFX(GetLicenseStatus)))(ZCE_NS::ProductType product);
typedef void (*ZCE_PFN(ZCE_FNPFX(ReleaseLicense)))();

#ifndef ZCE_NO_STATIC_API_DECL
ZCE_API_IMPORT bool ZCE_FNPFX(CheckoutLicenseWithKey)(const char* licenseKey) noexcept;
ZCE_API_IMPORT bool ZCE_FNPFX(CheckoutLicenseOffline)(const char* licenseKey) noexcept;
ZCE_API_IMPORT ZCE_NS::LicenseStatus ZCE_FNPFX(GetLicenseStatus)(ZCE_NS::ProductType product) noexcept;
ZCE_API_IMPORT void ZCE_FNPFX(ReleaseLicense)() noexcept;
#else
#define ZCE_DECLARE_API_EXTERN_FUNCS(name) extern ZRHI_PFN(name) name;
ZCE_LICENSING_FUNCS_API_APPLY(ZCE_DECLARE_API_EXTERN_FUNCS);
#undef ZCE_DECLARE_API_EXTERN_FUNCS
#endif

namespace ZCE_NS::CAPI
{
    inline bool CheckoutLicenseWithKey(const char* licenseKey) noexcept
    {
        return ZCE_FNPFX(CheckoutLicenseWithKey)(licenseKey);
    }
    inline bool CheckoutLicenseOffline(const char* license) noexcept
    {
        return ZCE_FNPFX(CheckoutLicenseOffline)(license);
    }
    inline LicenseStatus GetLicenseStatus(ProductType product) noexcept
    {
        return ZCE_FNPFX(GetLicenseStatus)(product);
    }
    inline void ReleaseLicense() noexcept
    {
        ZCE_FNPFX(ReleaseLicense);
    }
} // namespace ZCE_NS::CAPI

#undef ZCE_FNPFX
#pragma endregion Funcs

#define ZRHI_LICENSING_API_APPLY(macro) ZCE_LICENSING_FUNCS_API_APPLY(macro)

#endif //ZCE_NO_CAPI_IMPL

#undef ZCE_NS
#undef ZCE_API_IMPORT

#pragma endregion CAPI
