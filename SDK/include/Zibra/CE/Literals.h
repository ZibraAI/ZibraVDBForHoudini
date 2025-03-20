#pragma once

namespace Zibra::CE::Literals::Memory
{
    // User-defined literals for memory size conversion to bytes.

    /// Literal for converting bytes to bytes (trivial case).
    constexpr unsigned long long operator""_B(const unsigned long long b)
    {
        return b * 1;
    }

    // ---- IEC (Binary) Prefixes ----

    /// Literal for converting kibibytes (KiB) to bytes (1 KiB = 1024 B).
    constexpr unsigned long long operator""_KiB(const unsigned long long kib)
    {
        return kib * 1024_B;
    }

    /// Literal for converting mebibytes (MiB) to bytes (1 MiB = 1024 KiB).
    constexpr unsigned long long operator""_MiB(const unsigned long long mib)
    {
        return mib * 1024_KiB;
    }

    /// Literal for converting gibibytes (GiB) to bytes (1 GiB = 1024 MiB).
    constexpr unsigned long long operator""_GiB(const unsigned long long gib)
    {
        return gib * 1024_MiB;
    }

    /// Literal for converting tebibytes (TiB) to bytes (1 TiB = 1024 GiB).
    constexpr unsigned long long operator""_TiB(const unsigned long long tib)
    {
        return tib * 1024_GiB;
    }
} // namespace Zibra::CE::Literals::Memory