#pragma once

namespace Zibra::CE::Licensing
{
    enum class ProductType
    {
        Compression,
        Count
    };

    void CheckoutLicenseWithKey(const char* licenseKey) noexcept;
    void CheckoutLicenseOffline(const char* license, int licenseSize) noexcept;
    bool IsLicenseValidated(ProductType product) noexcept;
    const char* GetHardwareID() noexcept;
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
    macro(ZCE_LICENSING_FUNCS_EXPORT_FNPFX(IsLicenseValidated));     \
    macro(ZCE_LICENSING_FUNCS_EXPORT_FNPFX(GetHardwareID));          \
    macro(ZCE_LICENSING_FUNCS_EXPORT_FNPFX(ReleaseLicense))

#define ZCE_FNPFX(name) ZCE_LICENSING_FUNCS_EXPORT_FNPFX(name)

typedef void (*ZCE_PFN(ZCE_FNPFX(CheckoutLicenseWithKey)))(const char* licenseKey);
typedef void (*ZCE_PFN(ZCE_FNPFX(CheckoutLicenseOffline)))(const char* license, int licenseSize);
typedef bool (*ZCE_PFN(ZCE_FNPFX(IsLicenseValidated)))(ZCE_NS::ProductType product);
typedef const char* (*ZCE_PFN(ZCE_FNPFX(GetHardwareID)))();
typedef void (*ZCE_PFN(ZCE_FNPFX(ReleaseLicense)))();

#ifndef ZCE_NO_STATIC_API_DECL
ZCE_API_IMPORT void ZCE_FNPFX(CheckoutLicenseWithKey)(const char* licenseKey) noexcept;
ZCE_API_IMPORT void ZCE_FNPFX(CheckoutLicenseOffline)(const char* license, int licenseSize) noexcept;
ZCE_API_IMPORT bool ZCE_FNPFX(IsLicenseValidated)(ZCE_NS::ProductType product) noexcept;
ZCE_API_IMPORT const char* ZCE_FNPFX(GetHardwareID)() noexcept;
ZCE_API_IMPORT void ZCE_FNPFX(ReleaseLicense)() noexcept;
#else
#define ZCE_DECLARE_API_EXTERN_FUNCS(name) extern ZCE_PFN(name) name;
ZCE_LICENSING_FUNCS_API_APPLY(ZCE_DECLARE_API_EXTERN_FUNCS);
#undef ZCE_DECLARE_API_EXTERN_FUNCS
#endif

namespace ZCE_NS::CAPI
{
    inline void CheckoutLicenseWithKey(const char* licenseKey) noexcept
    {
        ZCE_FNPFX(CheckoutLicenseWithKey)(licenseKey);
    }
    inline void CheckoutLicenseOffline(const char* license, int licenseSize) noexcept
    {
        ZCE_FNPFX(CheckoutLicenseOffline)(license, licenseSize);
    }
    inline bool IsLicenseValidated(ProductType product) noexcept
    {
        return ZCE_FNPFX(IsLicenseValidated)(product);
    }
    inline const char* GetHardwareID() noexcept
    {
        return ZCE_FNPFX(GetHardwareID)();
    }
    inline void ReleaseLicense() noexcept
    {
        ZCE_FNPFX(ReleaseLicense)();
    }
} // namespace ZCE_NS::CAPI

#undef ZCE_FNPFX
#pragma endregion Funcs

#define ZCE_LICENSING_API_APPLY(macro) ZCE_LICENSING_FUNCS_API_APPLY(macro)

#endif //ZCE_NO_CAPI_IMPL

#undef ZCE_NS
#undef ZCE_API_IMPORT

#pragma endregion CAPI
