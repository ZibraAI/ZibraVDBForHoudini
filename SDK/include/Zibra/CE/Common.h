#pragma once

#include <Zibra/Foundation.h>

#define ZCE_CONCAT_HELPER(A, B) A##B
#define ZCE_PFN(name) ZCE_CONCAT_HELPER(PFN_, name)

namespace Zibra::CE
{
    static constexpr int SPARSE_BLOCK_SIZE = 8;
    static constexpr int SPARSE_BLOCK_VOXEL_COUNT = SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE;
    static constexpr size_t ZCE_MAX_CHANNEL_COUNT = 8;

    enum ReturnCode
    {
        // Successfully finished operation
        ZCE_SUCCESS = 0,
        // Unexpected error
        ZCE_ERROR = 100,
        // Fatal error
        ZCE_FATAL_ERROR = 110,

        ZCE_ERROR_NOT_INITIALIZED = 200,
        ZCE_ERROR_ALREADY_INITIALIZED = 201,

        ZCE_ERROR_INVALID_USAGE = 300,
        ZCE_ERROR_INVALID_ARGUMENTS = 301,
        ZCE_ERROR_NOT_IMPLEMENTED = 310,
        ZCE_ERROR_NOT_SUPPORTED = 311,

        ZCE_ERROR_NOT_FOUND = 400,
        // Out of CPU memory
        ZCE_ERROR_OUT_OF_CPU_MEMORY = 410,
        // Out of GPU memory
        ZCE_ERROR_OUT_OF_GPU_MEMORY = 411,
        // Time out
        ZCE_ERROR_TIME_OUT = 430,
    };

    struct MetadataEntry
    {
        const char* key = nullptr;
        const char* value = nullptr;
    };


    inline bool IsNearlyEqual(float a, float b) noexcept
    {
        return std::abs(a - b) < std::numeric_limits<float>::epsilon();
    }
    inline bool IsNearlyEqual(double a, double b) noexcept
    {
        return std::abs(a - b) < std::numeric_limits<double>::epsilon();
    }

    inline bool IsNearlyInteger(float value) noexcept
    {
        float dummy;
        float frac = std::modf(value, &dummy);
        return IsNearlyEqual(frac, 0.0f);
    }

    inline bool IsNearlyInteger(double value) noexcept
    {
        double dummy;
        double frac = std::modf(value, &dummy);
        return IsNearlyEqual(frac, 0.0);
    }

    inline float RoundIfNearlyZero(float value) noexcept
    {
        if (value < std::numeric_limits<float>::epsilon())
            return 0.0f;
        return value;
    }

    inline int FloorWithEpsilon(float value) noexcept
    {
        return static_cast<int>(std::floorf(value + std::numeric_limits<float>::epsilon()));
    }

    inline int CeilWithEpsilon(float value) noexcept
    {
        return static_cast<int>(std::ceilf(value - std::numeric_limits<float>::epsilon()));
    }

    inline int ModBlockSize(int value) noexcept
    {
        return value & (SPARSE_BLOCK_SIZE - 1);
    }

    inline int FloorToBlockSize(int value) noexcept
    {
        return value - ModBlockSize(value);
    }

    inline int CeilToBlockSize(int value) noexcept
    {
        return FloorToBlockSize(value + SPARSE_BLOCK_SIZE - 1);
    }

    inline float Lerp(float a, float b, float t) noexcept
    {
        return a + t * (b - a);
    }
    inline double Lerp(double a, double b, double t) noexcept
    {
        return a + t * (b - a);
    }

    template <class T>
    inline constexpr void NormalizeRange(T* data, const size_t size, const T minValue, const T maxValue, const T newMinValue,
                                         const T newMaxValue) noexcept
    {
        for (size_t i = 0; i < size; i++)
        {
            data[i] = (data[i] - minValue) / (maxValue - minValue) * (newMaxValue - newMinValue) + newMinValue;
        }
    };

    inline uint32_t CountBits(uint32_t x) noexcept
    {
#if __cplusplus >= 202002L
        return std::popcount(x);
#else
        uint32_t count = 0;
        while (x)
        {
            x &= (x - 1); // Clears the lowest set bit
            count++;
        }
        return count;
#endif
    }

    inline float Float16ToFloat32(uint16_t float16Value) noexcept
    {
        uint32_t t1, t2, t3;

        t1 = float16Value & 0x7fffu; // Non-sign bits
        t2 = float16Value & 0x8000u; // Sign bit
        t3 = float16Value & 0x7c00u; // Exponent

        t1 <<= 13u; // Align mantissa on MSB
        t2 <<= 16u; // Shift sign bit into position

        t1 += 0x38000000; // Adjust bias

        t1 = (t3 == 0 ? 0 : t1); // Denormals-as-zero

        t1 |= t2; // Re-insert sign bit

        uint32_t float32_value = t1;

        return *(reinterpret_cast<float*>(&float32_value));
    }
}
