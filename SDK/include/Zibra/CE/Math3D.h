#pragma once

#include <cstdint>
#include <climits>
#include <cmath>

namespace Zibra::Math3D
{
    using float32_t = float;
    using float64_t = double;

    using uint = uint32_t;

    struct float2
    {
        float32_t x;
        float32_t y;

        float2();
        float2(float _x);
        float2(float _x, float _y);
        float2(const std::array<float, 2>& v);
        float2 operator-();
        float2 operator-(const float2& v);
        float2 operator*(float scalar);
        float2 operator*(const float2& v);
        float2 operator/(float scalar);
        float2 operator/(const float2& v);
        float2 operator+();
        float2 operator+(const float2& v);
        operator std::array<float, 2>() const;
    };

    struct int2
    {
        int32_t x;
        int32_t y;

        int2();
        int2(int _x);
        int2(int _x, int _y);
    };

    struct float3
    {
        float32_t x;
        float32_t y;
        float32_t z;

        float3();
        float3(float _x);
        float3(float _x, float _y, float _z);
        float3(const std::array<float, 3>& v);
        float3 operator-() const;
        float3 operator-(const float3& v) const;
        float3 operator*(float scalar) const;
        float3 operator*(const float3& v) const;
        float3 operator/(float scalar) const;
        float3 operator/(const float3& v) const;
        float3 operator+() const;
        float3 operator+(const float3& v) const;
        operator std::array<float, 3>() const;
    };

    struct int3
    {
        int32_t x;
        int32_t y;
        int32_t z;

        int3();
        int3(int _x);
        int3(int _x, int _y, int _z);
    };

    struct float4
    {
        float32_t x;
        float32_t y;
        float32_t z;
        float32_t w;

        float4();
        float4(float _x);
        float4(float _x, float _y, float _z, float _w);
        float4(const std::array<float, 4>& v);
        float4 operator-() const;
        float4 operator-(const float4& v) const;
        float4 operator*(float scalar) const;
        float4 operator*(const float4& v) const;
        float4 operator/(float scalar) const;
        float4 operator/(const float4& v) const;
        float4 operator+() const;
        float4 operator+(const float4& v) const;
        operator std::array<float, 4>() const;
    };

    struct int4
    {
        int32_t x;
        int32_t y;
        int32_t z;
        int32_t w;

        int4();
        int4(int _x);
        int4(int _x, int _y, int _z, int _w);
    };

    struct uint2
    {
        uint32_t x;
        uint32_t y;

        uint2();
        uint2(uint32_t _x);
        uint2(uint32_t _x, uint32_t _y);
    };

    struct uint3
    {
        uint32_t x;
        uint32_t y;
        uint32_t z;

        uint3();
        uint3(uint32_t _x);
        uint3(uint32_t _x, uint32_t _y, uint32_t _z);
    };

    struct uint4
    {
        uint32_t x;
        uint32_t y;
        uint32_t z;
        uint32_t w;

        uint4();
        uint4(uint32_t _x);
        uint4(uint32_t _x, uint32_t _y, uint32_t _z, uint32_t _w);
    };

    struct float4x4
    {
        float4 x;
        float4 y;
        float4 z;
        float4 w;

        float4x4();
        float4x4(float4 _x, float4 _y, float4 _z, float4 _w);
    };

    union Transform
    {
        float raw[16];
        float4x4 matrix;
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
