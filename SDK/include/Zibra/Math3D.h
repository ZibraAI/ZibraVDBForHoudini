#pragma once

#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <array>

namespace Zibra::Math3D
{
    using float32_t = float;
    using float64_t = double;

    using uint = uint32_t;

    inline float Lerp(float a, float b, float t) noexcept
    {
        return a + t * (b - a);
    }
    inline double Lerp(double a, double b, double t) noexcept
    {
        return a + t * (b - a);
    }

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
// Workaround for GCC bug
#if defined(__GNUC__) && (__GNUC__ < 14)
        return static_cast<int>(::floorf(value + std::numeric_limits<float>::epsilon()));
#else
        return static_cast<int>(std::floorf(value + std::numeric_limits<float>::epsilon()));
#endif
    }

    inline int CeilWithEpsilon(float value) noexcept
    {
// Workaround for GCC bug
#if defined(__GNUC__) && (__GNUC__ < 14)
        return static_cast<int>(::ceilf(value - std::numeric_limits<float>::epsilon()));
#else
        return static_cast<int>(std::ceilf(value - std::numeric_limits<float>::epsilon()));
#endif
    }

    template <typename T>
    struct Vector2
    {
        T x, y;
        Vector2()
            : x(0)
            , y(0)
        {
        }
        explicit Vector2(T val)
            : x(val)
            , y(val)
        {
        }
        Vector2(T _x, T _y)
            : x(_x)
            , y(_y)
        {
        }
        explicit Vector2(const std::array<T, 2>& arr)
            : x(arr[0])
            , y(arr[1])
        {
        }
        Vector2 operator-() const
        {
            return {-x, -y};
        }
        Vector2 operator-(const Vector2& v) const
        {
            return {x - v.x, y - v.y};
        }
        Vector2 operator+() const
        {
            return *this;
        }
        Vector2 operator+(const Vector2& v) const
        {
            return {x + v.x, y + v.y};
        }
        Vector2 operator*(T scalar) const
        {
            return {x * scalar, y * scalar};
        }
        Vector2 operator*(const Vector2& v) const
        {
            return {x * v.x, y * v.y};
        }
        Vector2 operator/(T scalar) const
        {
            assert(scalar != 0);
            return {x / scalar, y / scalar};
        }
        Vector2 operator/(const Vector2& v) const
        {
            assert(v.x != 0 && v.y != 0);
            return {x / v.x, y / v.y};
        }
        bool operator==(const Vector2& v) const
        {
            return x == v.x && y == v.y;
        }
        bool operator!=(const Vector2& v) const
        {
            return !(*this == v);
        }
        bool IsNearlyEqualTo(const Vector2& v) noexcept
        {
            return IsNearlyEqual(x, v.x) && IsNearlyEqual(y, v.y);
        }
        explicit operator std::array<T, 2>() const
        {
            return {x, y};
        }
    };

    template <typename T>
    struct Vector3
    {
        T x, y, z;
        Vector3()
            : x(0)
            , y(0)
            , z(0)
        {
        }
        explicit Vector3(T val)
            : x(val)
            , y(val)
            , z(val)
        {
        }
        Vector3(T _x, T _y, T _z)
            : x(_x)
            , y(_y)
            , z(_z)
        {
        }
        explicit Vector3(const std::array<T, 3>& arr)
            : x(arr[0])
            , y(arr[1])
            , z(arr[2])
        {
        }
        Vector3 operator+() const
        {
            return *this;
        }
        Vector3 operator+(const Vector3& v) const
        {
            return {x + v.x, y + v.y, z + v.z};
        }
        Vector3 operator-() const
        {
            return {-x, -y, -z};
        }
        Vector3 operator-(const Vector3& v) const
        {
            return {x - v.x, y - v.y, z - v.z};
        }
        Vector3 operator*(T scalar) const
        {
            return {x * scalar, y * scalar, z * scalar};
        }
        Vector3 operator*(const Vector3& v) const
        {
            return {x * v.x, y * v.y, z * v.z};
        }
        Vector3 operator/(T scalar) const
        {
            assert(scalar != 0);
            return {x / scalar, y / scalar, z / scalar};
        }
        Vector3 operator/(const Vector3& v) const
        {
            assert(v.x != 0 && v.y != 0 && v.z != 0);
            return {x / v.x, y / v.y, z / v.z};
        }
        bool operator==(const Vector3& v) const
        {
            return x == v.x && y == v.y && z == v.z;
        }
        bool operator!=(const Vector3& v) const
        {
            return !(*this == v);
        }
        bool IsNearlyEqualTo(const Vector3& v) noexcept
        {
            return IsNearlyEqual(x, v.x) && IsNearlyEqual(y, v.y) && IsNearlyEqual(z, v.z);
        }
        explicit operator std::array<T, 3>() const
        {
            return {x, y, z};
        }
    };

    template <typename T>
    struct Vector4
    {
        T x, y, z, w;
        Vector4()
            : x(0)
            , y(0)
            , z(0)
            , w(0)
        {
        }
        explicit Vector4(T val)
            : x(val)
            , y(val)
            , z(val)
            , w(val)
        {
        }
        Vector4(T _x, T _y, T _z, T _w)
            : x(_x)
            , y(_y)
            , z(_z)
            , w(_w)
        {
        }
        explicit Vector4(const std::array<T, 4>& arr)
            : x(arr[0])
            , y(arr[1])
            , z(arr[2])
            , w(arr[3])
        {
        }
        Vector4 operator+() const
        {
            return *this;
        }
        Vector4 operator+(const Vector4& v) const
        {
            return {x + v.x, y + v.y, z + v.z, w + v.w};
        }
        Vector4 operator-() const
        {
            return {-x, -y, -z, -w};
        }
        Vector4 operator-(const Vector4& v) const
        {
            return {x - v.x, y - v.y, z - v.z, w - v.w};
        }
        Vector4 operator*(T scalar) const
        {
            return {x * scalar, y * scalar, z * scalar, w * scalar};
        }
        Vector4 operator*(const Vector4& v) const
        {
            return {x * v.x, y * v.y, z * v.z, w * v.w};
        }
        Vector4 operator/(T scalar) const
        {
            assert(scalar != 0);
            return {x / scalar, y / scalar, z / scalar, w / scalar};
        }
        Vector4 operator/(const Vector4& v) const
        {
            assert(v.x != 0 && v.y != 0 && v.z != 0 && v.w != 0);
            return {x / v.x, y / v.y, z / v.z, w / v.w};
        }
        bool operator==(const Vector4& v) const
        {
            return x == v.x && y == v.y && z == v.z && w == v.w;
        }
        bool operator!=(const Vector4& v) const
        {
            return !(*this == v);
        }
        bool IsNearlyEqualTo(const Vector4& v) noexcept
        {
            return IsNearlyEqual(x, v.x) && IsNearlyEqual(y, v.y) && IsNearlyEqual(z, v.z) && IsNearlyEqual(w, v.w);
        }
        explicit operator std::array<T, 4>() const
        {
            return {x, y, z, w};
        }
    };

    using float2 = Vector2<float32_t>;
    using double2 = Vector2<float64_t>;
    using int2 = Vector2<int32_t>;
    using uint2 = Vector2<uint32_t>;
    using float3 = Vector3<float32_t>;
    using double3 = Vector3<float64_t>;
    using int3 = Vector3<int32_t>;
    using uint3 = Vector3<uint32_t>;
    using float4 = Vector4<float32_t>;
    using double4 = Vector4<float64_t>;
    using int4 = Vector4<int32_t>;
    using uint4 = Vector4<uint32_t>;

    template <typename T>
    struct Matrix2x2
    {
        Vector2<T> x;
        Vector2<T> y;
        Matrix2x2() = default;
        Matrix2x2(Vector2<T> _x, Vector2<T> _y)
            : x(_x)
            , y(_y) {};
    };
    template <typename T>
    struct Matrix3x3
    {
        Vector3<T> x;
        Vector3<T> y;
        Vector3<T> z;
        Matrix3x3() = default;
        Matrix3x3(Vector3<T> _x, Vector3<T> _y, Vector3<T> _z)
            : x(_x)
            , y(_y)
            , z(_z) {};
    };
    template <typename T>
    struct Matrix4x4
    {
        Vector4<T> x;
        Vector4<T> y;
        Vector4<T> z;
        Vector4<T> w;
        Matrix4x4() = default;
        Matrix4x4(Vector4<T> _x, Vector4<T> _y, Vector4<T> _z, Vector4<T> _w)
            : x(_x)
            , y(_y)
            , z(_z)
            , w(_w) {};
    };
    using float2x2 = Matrix2x2<float32_t>;
    using double2x2 = Matrix2x2<float64_t>;
    using int2x2 = Matrix2x2<int32_t>;
    using uint2x2 = Matrix2x2<uint32_t>;
    using float3x3 = Matrix3x3<float32_t>;
    using double3x3 = Matrix3x3<float64_t>;
    using int3x3 = Matrix3x3<int32_t>;
    using uint3x3 = Matrix3x3<uint32_t>;
    using float4x4 = Matrix4x4<float32_t>;
    using double4x4 = Matrix4x4<float64_t>;
    using int4x4 = Matrix4x4<int32_t>;
    using uint4x4 = Matrix4x4<uint32_t>;

    struct Transform
    {
        float raw[16]{};

        constexpr Transform()
            : raw{}
        {
        }
        
        constexpr Transform(float m00, float m01, float m02, float m03, float m10, float m11, float m12, float m13, float m20, float m21,
                            float m22, float m23, float m30, float m31, float m32, float m33)
            : raw{m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33}
        {
        }
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

    inline bool operator==(const AABB& a, const AABB& b) noexcept
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
