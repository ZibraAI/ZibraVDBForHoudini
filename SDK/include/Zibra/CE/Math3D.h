#pragma once

#include <cstdint>
#include <climits>
#include <cmath>

namespace Zibra::Math3D
{
    struct uint3
    {
        uint32_t x;
        uint32_t y;
        uint32_t z;
    };

    struct float4
    {
        float x;
        float y;
        float z;
        float w;
    };

    struct Transform
    {
        float matrix[16];
    };
    static constexpr Transform Ident = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    struct AABB
    {
        int32_t minX = INT_MAX;
        int32_t minY = INT_MAX;
        int32_t minZ = INT_MAX;
        int32_t maxX = INT_MIN;
        int32_t maxY = INT_MIN;
        int32_t maxZ = INT_MIN;
    };

    inline bool IsEmpty(const AABB& aabb) noexcept
    {
        return aabb.minX >= aabb.maxX || aabb.minY >= aabb.maxY || aabb.minZ >= aabb.maxZ;
    }

    /**
     * a AND b
     * @return AABB Intersection
     */
    inline AABB operator&(const AABB& a, const AABB& b) noexcept
    {
        AABB result{};
        result.minX = std::max(a.minX, b.minX);
        result.minY = std::max(a.minY, b.minY);
        result.minZ = std::max(a.minZ, b.minZ);
        result.maxX = std::min(a.maxX, b.maxX);
        result.maxY = std::min(a.maxY, b.maxY);
        result.maxZ = std::min(a.maxZ, b.maxZ);
        return result;
    }

    /**
     * a OR b
     * @return AABB Union
     */
    inline AABB operator|(const AABB& a, const AABB& b) noexcept
    {
        AABB result{};
        result.minX = std::min(a.minX, b.minX);
        result.minY = std::min(a.minY, b.minY);
        result.minZ = std::min(a.minZ, b.minZ);
        result.maxX = std::max(a.maxX, b.maxX);
        result.maxY = std::max(a.maxY, b.maxY);
        result.maxZ = std::max(a.maxZ, b.maxZ);
        return result;
    }

    inline bool operator>(const AABB& a, const AABB& b) noexcept
    {
        const int32_t volA = (a.maxX - a.minX) * (a.maxY - a.minY) * (a.maxZ - a.minZ);
        const int32_t volB = (b.maxX - b.minX) * (b.maxY - b.minY) * (b.maxZ - b.minZ);
        return volA > volB;
    }

    inline bool operator<(const AABB& a, const AABB& b) noexcept
    {
        const int32_t volA = (a.maxX - a.minX) * (a.maxY - a.minY) * (a.maxZ - a.minZ);
        const int32_t volB = (b.maxX - b.minX) * (b.maxY - b.minY) * (b.maxZ - b.minZ);
        return volA < volB;
    }

    inline  bool operator==(const AABB& a, const AABB& b) noexcept
    {
        const bool lowerBound = a.minX == b.minX && a.minY == b.minY && a.minZ == b.minZ;
        const bool upperBound = a.maxX == b.maxX && a.maxY == b.maxY && a.maxZ == b.maxZ;
        return lowerBound && upperBound;
    }

    inline bool operator!=(const AABB& a, const AABB& b) noexcept
    {
        return !(a == b);
    }
} // namespace Zibra::Math3D
