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
        Decompression,
        Count
    };

    bool CheckoutLicenseWithKey(const char* licenseKey) noexcept;
    bool CheckoutLicenseOffline(const char* license) noexcept;
    LicenseStatus GetLicenseStatus(ProductType product) noexcept;
    void ReleaseLicense() noexcept;
}

#pragma region CAPI

#define ZCE_API_IMPORT __declspec(dllimport)
#define ZCE_CONCAT_HELPER(A, B) A##B
#define ZCE_PFN(name) ZCE_CONCAT_HELPER(PFN_, name)
#define ZCE_NS Zibra::CE::Licensing

#if !defined(ZCE_NO_CAPI_IMPL) && !defined(ZCE_NO_STATIC_API_DECL) && !defined(ZCE_API_CALL)
#define ZCE_API_CALL(func, ...) func(__VA_ARGS__)
#elif !defined(ZCE_NO_CAPI_IMPL) && defined(ZCE_NO_STATIC_API_DECL) && !defined(ZCE_API_CALL)
#error "You must define ZCE_API_CALL(func, ...) macro before using CAPI without static function declaration."
#endif

#pragma region Funcs
#define ZCE_FNPFX(name) Zibra_CE_Licensing_##name

typedef bool (*ZCE_PFN(ZCE_FNPFX(CheckoutLicenseWithKey)))(const char* licenseKey);
typedef bool (*ZCE_PFN(ZCE_FNPFX(CheckoutLicenseOffline)))(const char* licenseKey);
typedef ZCE_NS::LicenseStatus (*ZCE_PFN(ZCE_FNPFX(GetLicenseStatus)))(ZCE_NS::ProductType product);
typedef void (*ZCE_PFN(ZCE_FNPFX(ReleaseLicense)))();

#ifdef ZRHI_STATIC_API_DECL
ZCE_API_IMPORT bool ZCE_FNPFX(CheckoutLicenseWithKey)(const char* licenseKey) noexcept;
ZCE_API_IMPORT bool ZCE_FNPFX(CheckoutLicenseOffline)(const char* licenseKey) noexcept;
ZCE_API_IMPORT ZCE_NS::LicenseStatus ZCE_FNPFX(GetLicenseStatus)(ZCE_NS::ProductType product) noexcept;
ZCE_API_IMPORT void ZCE_FNPFX(ReleaseLicense)() noexcept;
#else
extern ZCE_PFN(ZCE_FNPFX(CheckoutLicenseWithKey)) ZCE_FNPFX(CheckoutLicenseWithKey);
extern ZCE_PFN(ZCE_FNPFX(CheckoutLicenseOffline)) ZCE_FNPFX(CheckoutLicenseOffline);
extern ZCE_PFN(ZCE_FNPFX(GetLicenseStatus)) ZCE_FNPFX(GetLicenseStatus);
extern ZCE_PFN(ZCE_FNPFX(ReleaseLicense)) ZCE_FNPFX(ReleaseLicense);
#endif

#ifndef ZCE_NO_CAPI_IMPL
namespace ZCE_NS::CAPI
{
    inline bool CheckoutLicenseWithKey(const char* licenseKey) noexcept
    {
        return ZCE_API_CALL(ZCE_FNPFX(CheckoutLicenseWithKey), licenseKey);
    }
    inline bool CheckoutLicenseOffline(const char* license) noexcept
    {
        return ZCE_API_CALL(ZCE_FNPFX(CheckoutLicenseOffline), license);
    }
    inline LicenseStatus GetLicenseStatus(ProductType product) noexcept
    {
        return ZCE_API_CALL(ZCE_FNPFX(GetLicenseStatus), product);
    }
    inline void ReleaseLicense() noexcept
    {
        ZCE_API_CALL(ZCE_FNPFX(ReleaseLicense));
    }
}
#endif

#undef ZCE_FNPFX
#pragma endregion Funcs

#undef ZCE_NS
#undef ZCE_PFN
#undef ZCE_CONCAT_HELPER
#undef ZCE_API_IMPORT
#pragma endregion CAPI
