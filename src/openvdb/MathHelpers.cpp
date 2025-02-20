#include "PrecompiledHeader.h"

#include "MathHelpers.h"

#include "bridge/CompressionEngine.h"

namespace Zibra::OpenVDBSupport::MathHelpers
{

    float FloorFloat(float value) noexcept
    {
// Workaround for GCC bug
#if defined(__GNUC__) && (__GNUC__ < 14)
        return ::floorf(value);
#else
        return std::floorf(value);
#endif
    }

    float CeilFloat(float value) noexcept
    {
// Workaround for GCC bug
#if defined(__GNUC__) && (__GNUC__ < 14)
        return ::ceilf(value);
#else
        return std::ceilf(value);
#endif
    }

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
        return static_cast<int>(FloorFloat(value + std::numeric_limits<float>::epsilon()));
    }

    int CeilWithEpsilon(float value) noexcept
    {
        return static_cast<int>(CeilFloat(value - std::numeric_limits<float>::epsilon()));
    }

    int ModBlockSize(int value) noexcept
    {
        return value & (ZIB_BLOCK_SIZE - 1);
    }

    int FloorToBlockSize(int value) noexcept
    {
        return value - ModBlockSize(value);
    }

    int CeilToBlockSize(int value) noexcept
    {
        return FloorToBlockSize(value + ZIB_BLOCK_SIZE - 1);
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

    Zibra::CompressionEngine::ZCE_AABB operator|(const CompressionEngine::ZCE_AABB& a, const CompressionEngine::ZCE_AABB& b) noexcept
    {
        CompressionEngine::ZCE_AABB result = {};
        result.minX = std::min(a.minX, b.minX);
        result.minY = std::min(a.minY, b.minY);
        result.minZ = std::min(a.minZ, b.minZ);
        result.maxX = std::max(a.maxX, b.maxX);
        result.maxY = std::max(a.maxY, b.maxY);
        result.maxZ = std::max(a.maxZ, b.maxZ);
        return result;
    }

    bool IsEmpty(const CompressionEngine::ZCE_AABB& a) noexcept
    {
        return a.minX >= a.maxX || a.minY >= a.maxY || a.minZ >= a.maxZ;
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