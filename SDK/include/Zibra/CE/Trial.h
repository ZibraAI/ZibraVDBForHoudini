#pragma once

namespace Zibra::CE::Trial
{
    int RequestTrialCompression() noexcept;
    int TrialCompressionsRemaining() noexcept;
} // namespace Zibra::CE::Trial

#pragma region CAPI

#define ZCE_API_IMPORT extern "C" __declspec(dllimport)
#define ZCE_NS Zibra::CE::Trial

#ifndef ZCE_NO_CAPI_IMPL

#pragma region Funcs
#define ZCE_TRIAL_FUNCS_EXPORT_FNPFX(name) Zibra_CE_Trial_##name

#define ZCE_TRIAL_FUNCS_API_APPLY(macro)                         \
    macro(ZCE_TRIAL_FUNCS_EXPORT_FNPFX(RequestTrialCompression)); \
    macro(ZCE_TRIAL_FUNCS_EXPORT_FNPFX(TrialCompressionsRemaining));

#define ZCE_FNPFX(name) ZCE_TRIAL_FUNCS_EXPORT_FNPFX(name)

typedef int (*ZCE_PFN(ZCE_FNPFX(RequestTrialCompression)))();
typedef int (*ZCE_PFN(ZCE_FNPFX(TrialCompressionsRemaining)))();

#ifndef ZCE_NO_STATIC_API_DECL
ZCE_API_IMPORT int ZCE_FNPFX(RequestTrialCompression)() noexcept;
ZCE_API_IMPORT int ZCE_FNPFX(TrialCompressionsRemaining)() noexcept;
#else
#define ZCE_DECLARE_API_EXTERN_FUNCS(name) extern ZCE_PFN(name) name;
ZCE_TRIAL_FUNCS_API_APPLY(ZCE_DECLARE_API_EXTERN_FUNCS);
#undef ZCE_DECLARE_API_EXTERN_FUNCS
#endif

namespace ZCE_NS::CAPI
{
    inline int RequestTrialCompression() noexcept
    {
        return ZCE_FNPFX(RequestTrialCompression)();
    }
    inline int TrialCompressionsRemaining() noexcept
    {
        return ZCE_FNPFX(TrialCompressionsRemaining)();
    }
} // namespace ZCE_NS::CAPI

#undef ZCE_FNPFX
#pragma endregion Funcs

#define ZCE_TRIAL_API_APPLY(macro) ZCE_TRIAL_FUNCS_API_APPLY(macro)

#endif //ZCE_NO_CAPI_IMPL

#undef ZCE_NS
#undef ZCE_API_IMPORT

#pragma endregion CAPI
