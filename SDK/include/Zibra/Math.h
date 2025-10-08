#pragma once

#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace Zibra::Math
{
    template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    constexpr auto IntDivideCeil(T intValue, T factor) noexcept
    {
        return (intValue + factor - 1) / factor;
    }

    template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    constexpr auto CeilToPowerOfTwo(T intValue, T powerOfTwo) noexcept
    {
        assert(powerOfTwo != 0 && (powerOfTwo & (powerOfTwo - 1)) == 0);
        return (intValue + (powerOfTwo - 1)) & (~(powerOfTwo - 1));
    }

    template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    constexpr auto FloorToPowerOfTwo(T intValue, T powerOfTwo) noexcept
    {
        assert(powerOfTwo != 0 && (powerOfTwo & (powerOfTwo - 1)) == 0);
        return intValue & (~(powerOfTwo - 1));
    }

    template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    constexpr auto CeilToMultipleOf(T intValue, T multiple) noexcept
    {
        return ((intValue + multiple - 1) / multiple) * multiple;
    }

    template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    constexpr inline T Lerp(T a, T b, T t) noexcept
    {
        return a + t * (b - a);
    }

    template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    constexpr inline bool IsNearlyEqual(T a, T b) noexcept
    {
        return std::abs(a - b) < std::numeric_limits<T>::epsilon();
    }

    template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    constexpr inline bool IsNearlyInteger(T value) noexcept
    {
        T dummy = 0.0f;
        T frac = std::modf(value, &dummy);
        return IsNearlyEqual(frac, T(0.0));
    }

    template <class T, std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    constexpr inline float RoundIfNearlyZero(T value) noexcept
    {
        if (value < std::numeric_limits<T>::epsilon())
            return T(0.0);
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
        constexpr Vector2()
            : x(0)
            , y(0)
        {
        }
        constexpr explicit Vector2(T val)
            : x(val)
            , y(val)
        {
        }
        constexpr Vector2(T _x, T _y)
            : x(_x)
            , y(_y)
        {
        }
        constexpr explicit Vector2(const std::array<T, 2>& arr)
            : x(arr[0])
            , y(arr[1])
        {
        }
        constexpr Vector2 operator-() const
        {
            return {-x, -y};
        }
        constexpr Vector2 operator-(const Vector2& v) const
        {
            return {x - v.x, y - v.y};
        }
        constexpr Vector2 operator+() const
        {
            return *this;
        }
        constexpr Vector2 operator+(const Vector2& v) const
        {
            return {x + v.x, y + v.y};
        }
        constexpr Vector2 operator*(T scalar) const
        {
            return {x * scalar, y * scalar};
        }
        constexpr Vector2 operator*(const Vector2& v) const
        {
            return {x * v.x, y * v.y};
        }
        constexpr Vector2 operator/(T scalar) const
        {
            assert(scalar != 0);
            return {x / scalar, y / scalar};
        }
        constexpr Vector2 operator/(const Vector2& v) const
        {
            assert(v.x != 0 && v.y != 0);
            return {x / v.x, y / v.y};
        }
        constexpr bool operator==(const Vector2& v) const
        {
            return x == v.x && y == v.y;
        }
        constexpr bool operator!=(const Vector2& v) const
        {
            return !(*this == v);
        }
        constexpr bool IsNearlyEqualTo(const Vector2& v) noexcept
        {
            return IsNearlyEqual(x, v.x) && IsNearlyEqual(y, v.y);
        }
        constexpr explicit operator std::array<T, 2>() const
        {
            return {x, y};
        }
    };

    template <typename T>
    struct Vector3
    {
        T x, y, z;
        constexpr Vector3()
            : x(0)
            , y(0)
            , z(0)
        {
        }
        constexpr explicit Vector3(T val)
            : x(val)
            , y(val)
            , z(val)
        {
        }
        constexpr Vector3(T _x, T _y, T _z)
            : x(_x)
            , y(_y)
            , z(_z)
        {
        }
        constexpr explicit Vector3(const std::array<T, 3>& arr)
            : x(arr[0])
            , y(arr[1])
            , z(arr[2])
        {
        }
        constexpr Vector3 operator+() const
        {
            return *this;
        }
        constexpr Vector3 operator+(const Vector3& v) const
        {
            return {x + v.x, y + v.y, z + v.z};
        }
        constexpr Vector3 operator-() const
        {
            return {-x, -y, -z};
        }
        constexpr Vector3 operator-(const Vector3& v) const
        {
            return {x - v.x, y - v.y, z - v.z};
        }
        constexpr Vector3 operator*(T scalar) const
        {
            return {x * scalar, y * scalar, z * scalar};
        }
        constexpr Vector3 operator*(const Vector3& v) const
        {
            return {x * v.x, y * v.y, z * v.z};
        }
        constexpr Vector3 operator/(T scalar) const
        {
            assert(scalar != 0);
            return {x / scalar, y / scalar, z / scalar};
        }
        constexpr Vector3 operator/(const Vector3& v) const
        {
            assert(v.x != 0 && v.y != 0 && v.z != 0);
            return {x / v.x, y / v.y, z / v.z};
        }
        constexpr bool operator==(const Vector3& v) const
        {
            return x == v.x && y == v.y && z == v.z;
        }
        constexpr bool operator!=(const Vector3& v) const
        {
            return !(*this == v);
        }
        constexpr bool IsNearlyEqualTo(const Vector3& v) noexcept
        {
            return IsNearlyEqual(x, v.x) && IsNearlyEqual(y, v.y) && IsNearlyEqual(z, v.z);
        }
        constexpr explicit operator std::array<T, 3>() const
        {
            return {x, y, z};
        }
    };

    template <typename T>
    struct Vector4
    {
        T x, y, z, w;
        constexpr Vector4()
            : x(0)
            , y(0)
            , z(0)
            , w(0)
        {
        }
        constexpr explicit Vector4(T val)
            : x(val)
            , y(val)
            , z(val)
            , w(val)
        {
        }
        constexpr Vector4(T _x, T _y, T _z, T _w)
            : x(_x)
            , y(_y)
            , z(_z)
            , w(_w)
        {
        }
        constexpr explicit Vector4(const std::array<T, 4>& arr)
            : x(arr[0])
            , y(arr[1])
            , z(arr[2])
            , w(arr[3])
        {
        }
        constexpr Vector4 operator+() const
        {
            return *this;
        }
        constexpr Vector4 operator+(const Vector4& v) const
        {
            return {x + v.x, y + v.y, z + v.z, w + v.w};
        }
        constexpr Vector4 operator-() const
        {
            return {-x, -y, -z, -w};
        }
        constexpr Vector4 operator-(const Vector4& v) const
        {
            return {x - v.x, y - v.y, z - v.z, w - v.w};
        }
        constexpr Vector4 operator*(T scalar) const
        {
            return {x * scalar, y * scalar, z * scalar, w * scalar};
        }
        constexpr Vector4 operator*(const Vector4& v) const
        {
            return {x * v.x, y * v.y, z * v.z, w * v.w};
        }
        constexpr Vector4 operator/(T scalar) const
        {
            assert(scalar != 0);
            return {x / scalar, y / scalar, z / scalar, w / scalar};
        }
        constexpr Vector4 operator/(const Vector4& v) const
        {
            assert(v.x != 0 && v.y != 0 && v.z != 0 && v.w != 0);
            return {x / v.x, y / v.y, z / v.z, w / v.w};
        }
        constexpr bool operator==(const Vector4& v) const
        {
            return x == v.x && y == v.y && z == v.z && w == v.w;
        }
        constexpr bool operator!=(const Vector4& v) const
        {
            return !(*this == v);
        }
        constexpr bool IsNearlyEqualTo(const Vector4& v) noexcept
        {
            return IsNearlyEqual(x, v.x) && IsNearlyEqual(y, v.y) && IsNearlyEqual(z, v.z) && IsNearlyEqual(w, v.w);
        }
        constexpr explicit operator std::array<T, 4>() const
        {
            return {x, y, z, w};
        }
    };

    using float2 = Vector2<float>;
    using double2 = Vector2<double>;
    using int2 = Vector2<int32_t>;
    using uint2 = Vector2<uint32_t>;
    using float3 = Vector3<float>;
    using double3 = Vector3<double>;
    using int3 = Vector3<int32_t>;
    using uint3 = Vector3<uint32_t>;
    using float4 = Vector4<float>;
    using double4 = Vector4<double>;
    using int4 = Vector4<int32_t>;
    using uint4 = Vector4<uint32_t>;

    template <typename T>
    struct Matrix2x2
    {
        Vector2<T> x;
        Vector2<T> y;
        constexpr Matrix2x2() = default;
        constexpr Matrix2x2(Vector2<T> _x, Vector2<T> _y)
            : x(_x)
            , y(_y) {};
    };
    template <typename T>
    struct Matrix3x3
    {
        Vector3<T> x;
        Vector3<T> y;
        Vector3<T> z;
        constexpr Matrix3x3() = default;
        constexpr Matrix3x3(Vector3<T> _x, Vector3<T> _y, Vector3<T> _z)
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
        constexpr Matrix4x4() = default;
        constexpr Matrix4x4(Vector4<T> _x, Vector4<T> _y, Vector4<T> _z, Vector4<T> _w)
            : x(_x)
            , y(_y)
            , z(_z)
            , w(_w) {};
    };
    using float2x2 = Matrix2x2<float>;
    using double2x2 = Matrix2x2<double>;
    using int2x2 = Matrix2x2<int32_t>;
    using uint2x2 = Matrix2x2<uint32_t>;
    using float3x3 = Matrix3x3<float>;
    using double3x3 = Matrix3x3<double>;
    using int3x3 = Matrix3x3<int32_t>;
    using uint3x3 = Matrix3x3<uint32_t>;
    using float4x4 = Matrix4x4<float>;
    using double4x4 = Matrix4x4<double>;
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

        constexpr Transform(float4 m0, float4 m1, float4 m2, float4 m3)
            : raw{m0.x, m0.y, m0.z, m0.w, m1.x, m1.y, m1.z, m1.w, m2.x, m2.y, m2.z, m2.w, m3.x, m3.y, m3.z, m3.w}
        {
        }

        constexpr Transform operator*(const Transform& other) const noexcept
        {
            Transform result{};
            for (int i = 0; i < 4; ++i)
            {
                for (int j = 0; j < 4; ++j)
                {
                    result.raw[i * 4 + j] = raw[i * 4 + 0] * other.raw[0 * 4 + j] + raw[i * 4 + 1] * other.raw[1 * 4 + j] +
                                            raw[i * 4 + 2] * other.raw[2 * 4 + j] + raw[i * 4 + 3] * other.raw[3 * 4 + j];
                }
            }
            return result;
        }

        static Transform Translation(const float3& translation) noexcept
        {
            return Transform{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, translation.x, translation.y, translation.z, 1};
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

        uint3 GetSize() const noexcept
        {
            return {static_cast<uint32_t>(std::abs(maxX - minX)), static_cast<uint32_t>(std::abs(maxY - minY)),
                    static_cast<uint32_t>(std::abs(maxZ - minZ))};
        }

        bool IsEmpty() const noexcept
        {
            return minX >= maxX || minY >= maxY || minZ >= maxZ;
        }

        AABB operator&(const AABB& other)
        {
            AABB result{};
            result.minX = std::max(minX, other.minX);
            result.minY = std::max(minY, other.minY);
            result.minZ = std::max(minZ, other.minZ);
            result.maxX = std::min(maxX, other.maxX);
            result.maxY = std::min(maxY, other.maxY);
            result.maxZ = std::min(maxZ, other.maxZ);
            return result;
        }

        AABB operator|(const AABB& other)
        {
            AABB result{};
            result.minX = std::min(minX, other.minX);
            result.minY = std::min(minY, other.minY);
            result.minZ = std::min(minZ, other.minZ);
            result.maxX = std::max(maxX, other.maxX);
            result.maxY = std::max(maxY, other.maxY);
            result.maxZ = std::max(maxZ, other.maxZ);
            return result;
        }

        AABB& operator&=(const AABB& other)
        {
            minX = std::max(minX, other.minX);
            minY = std::max(minY, other.minY);
            minZ = std::max(minZ, other.minZ);
            maxX = std::min(maxX, other.maxX);
            maxY = std::min(maxY, other.maxY);
            maxZ = std::min(maxZ, other.maxZ);
            return *this;
        }

        AABB& operator|=(const AABB& other)
        {
            minX = std::min(minX, other.minX);
            minY = std::min(minY, other.minY);
            minZ = std::min(minZ, other.minZ);
            maxX = std::max(maxX, other.maxX);
            maxY = std::max(maxY, other.maxY);
            maxZ = std::max(maxZ, other.maxZ);
            return *this;
        }

        bool operator==(const AABB& other) const noexcept
        {
            return minX == other.minX && minY == other.minY && minZ == other.minZ && maxX == other.maxX && maxY == other.maxY &&
                   maxZ == other.maxZ;
        }

        bool operator!=(const AABB& other) const noexcept
        {
            return !(*this == other);
        }
    };

} // namespace Zibra::Math
