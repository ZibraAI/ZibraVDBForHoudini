#include "PrecompiledHeader.h"

#include "MathHelpers.h"

namespace Zibra::OpenVDBSupport::MathHelpers
{

    bool IsNearlyEqual(float a, float b) noexcept
    {
        return std::abs(a - b) < std::numeric_limits<float>::epsilon();
    }

    bool IsNearlyEqual(double a, double b) noexcept
    {
        return std::abs(a - b) < std::numeric_limits<double>::epsilon();
    }

    bool IsNearlyInteger(float value) noexcept
    {
        float dummy;
        float frac = std::modf(value, &dummy);
        return IsNearlyEqual(frac, 0.0f);
    }

    bool IsNearlyInteger(double value) noexcept
    {
        double dummy;
        double frac = std::modf(value, &dummy);
        return IsNearlyEqual(frac, 0.0);
    }

    float RoundIfNearlyZero(float value) noexcept
    {
        if (value < std::numeric_limits<float>::epsilon())
            return 0.0f;
        return value;
    }

    int FloorWithEpsilon(float value) noexcept
    {
        return static_cast<int>(std::floorf(value + std::numeric_limits<float>::epsilon()));
    }

    int CeilWithEpsilon(float value) noexcept
    {
        return static_cast<int>(std::ceilf(value - std::numeric_limits<float>::epsilon()));
    }

    int ModBlockSize(int value) noexcept
    {
        return value & (Zibra::CE::SPARSE_BLOCK_SIZE - 1);
    }

    int FloorToBlockSize(int value) noexcept
    {
        return value - ModBlockSize(value);
    }

    int CeilToBlockSize(int value) noexcept
    {
        return FloorToBlockSize(value + Zibra::CE::SPARSE_BLOCK_SIZE - 1);
    }

    int ComputeChannelCountFromMask(uint8 mask) noexcept
    {
        int count = 0;
        while (mask)
        {
            count += mask & 1;
            mask >>= 1;
        }
        return count;
    }

    float Lerp(float a, float b, float t) noexcept
    {
        return a + t * (b - a);
    }

    double Lerp(double a, double b, double t) noexcept
    {
        return a + t * (b - a);
    }

    uint32_t CountBits(uint32_t x) noexcept
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

} // namespace Zibra::OpenVDBSupport::MathHelpers