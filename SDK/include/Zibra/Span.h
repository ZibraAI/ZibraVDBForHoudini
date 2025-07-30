#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace Zibra
{
    // std::span is not in C++17

    class SpanFlag
    {
    };

    template <typename T>
    class BasicSpan : public SpanFlag
    {
    public:
        using ElementType = std::remove_const_t<T>;
        using VectorType = std::conditional_t<std::is_const_v<T>, const std::vector<ElementType>, std::vector<ElementType>>;
        template <std::size_t N>
        using ArrayType = std::conditional_t<std::is_const_v<T>, const std::array<ElementType, N>, std::array<ElementType, N>>;

        BasicSpan()
            : m_Data(nullptr)
            , m_Size(0)
        {
        }

        BasicSpan(T& element)
            : m_Data(&element)
            , m_Size(1)
        {
            static_assert(std::is_trivially_destructible_v<T>);
            static_assert(std::is_trivially_copyable_v<T>);
        }

        BasicSpan(T* data, size_t size)
            : m_Data(data)
            , m_Size(size)
        {
        }

        // Automatic conversion from containers
        template <size_t N>
        BasicSpan(T (&data)[N])
            : m_Data(data)
            , m_Size(N)
        {
        }

        BasicSpan(VectorType& vec)
            : m_Data(vec.data())
            , m_Size(vec.size())
        {
        }

        template <std::size_t N>
        BasicSpan(ArrayType<N>& arr)
            : m_Data(arr.data())
            , m_Size(N)
        {
        }

        template <typename U = T>
        BasicSpan(const BasicSpan<ElementType>& nonConstSpan, std::enable_if_t<std::is_const_v<U>, int> = 0)
            : m_Data(nonConstSpan.m_Data)
            , m_Size(nonConstSpan.m_Size)
        {
        }

        // STL container compatibility
        const T* begin() const
        {
            return m_Data;
        }

        const T* end() const
        {
            return m_Data + m_Size;
        }

        T* data()
        {
            return m_Data;
        }

        size_t size() const
        {
            return m_Size;
        }

        const T& operator[](size_t index) const
        {
            assert(index < m_Size);
            return m_Data[index];
        }

        // Helper methods
        bool ContentEquals(const BasicSpan<T>& other) const
        {
            if (m_Size != other.m_Size)
            {
                return false;
            }
            return memcmp(m_Data, other.m_Data, m_Size * sizeof(T)) == 0;
        }

    private:
        T* m_Data;
        size_t m_Size;
    };

    template <typename T>
    class ReinterpretReadSpan : public BasicSpan<T>
    {
    private:
        struct DummyStruct
        {
        };
    public:
        using BasicSpan<T>::BasicSpan;

        template <typename T2>
        ReinterpretReadSpan(const T2& element, std::enable_if_t<!std::is_base_of_v<SpanFlag, T2>, DummyStruct> = {})
            : BasicSpan<T>(reinterpret_cast<const T*>(&element), sizeof(T2) / sizeof(T))
        {
            static_assert(std::is_trivially_destructible_v<T2>);
            static_assert(std::is_trivially_copyable_v<T2>);
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        template <typename T2>
        ReinterpretReadSpan(const T2* elements, size_t elementCount,
                            std::enable_if_t < !std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>>, DummyStruct> = {})
            : BasicSpan<T>(reinterpret_cast<const T*>(elements), sizeof(T2) * elementCount / sizeof(T))
        {
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        template <typename T2, size_t N>
        ReinterpretReadSpan(const T2 (&data)[N])
            : BasicSpan<T>(reinterpret_cast<const T*>(data), sizeof(data) / sizeof(T))
        {
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        template <typename T2>
        ReinterpretReadSpan(const std::vector<T2>& vec)
            : BasicSpan<T>(reinterpret_cast<const T*>(vec.data()), vec.size() * sizeof(T2) / sizeof(T))
        {
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        template <typename T2, size_t N>
        ReinterpretReadSpan(const std::array<T2, N>& arr)
            : BasicSpan<T>(reinterpret_cast<const T*>(arr.data()), N * sizeof(T2) / sizeof(T))
        {
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        ReinterpretReadSpan(const std::string& str)
            : BasicSpan<T>(reinterpret_cast<const T*>(str.data()), str.size())
        {
            static_assert(sizeof(T) == 1);
        }
    };

    template <typename T>
    class ReinterpretWriteSpan : public BasicSpan<T>
    {
    private:
        struct DummyStruct
        {
        };
    public:
        using BasicSpan<T>::BasicSpan;

        template <typename T2>
        ReinterpretWriteSpan(T2& element, std::enable_if_t<!std::is_base_of_v<SpanFlag, T2>, DummyStruct> = {})
            : BasicSpan<T>(reinterpret_cast<T*>(&element), sizeof(T2) / sizeof(T))
        {
            static_assert(std::is_trivially_destructible_v<T2>);
            static_assert(std::is_trivially_copyable_v<T2>);
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        template <typename T2>
        ReinterpretWriteSpan(T2* elements, size_t elementCount,
                             std::enable_if_t<!std::is_same_v<std::remove_const_t<T>, std::remove_const_t<T2>>, DummyStruct> = {})
            : BasicSpan<T>(reinterpret_cast<T*>(elements), sizeof(T2) * elementCount / sizeof(T))
        {
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        template <typename T2, size_t N>
        ReinterpretWriteSpan(T2 (&data)[N])
            : BasicSpan<T>(reinterpret_cast<T*>(data), sizeof(data) / sizeof(T))
        {
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        template <typename T2>
        ReinterpretWriteSpan(std::vector<T2>& vec)
            : BasicSpan<T>(reinterpret_cast<T*>(vec.data()), vec.size() * sizeof(T2) / sizeof(T))
        {
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        template <typename T2, size_t N>
        ReinterpretWriteSpan(std::array<T2, N>& arr)
            : BasicSpan<T>(reinterpret_cast<T*>(arr.data()), N * sizeof(T2) / sizeof(T))
        {
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        ReinterpretWriteSpan(std::string& str)
            : BasicSpan<T>(reinterpret_cast<T*>(str.data()), str.size())
        {
            static_assert(sizeof(T) == 1);
        }
    };

    template <typename T>
    class Span : public BasicSpan<T>
    {
    public:
        using BasicSpan<T>::BasicSpan;
    };

    // Specializations for char/unsigned char/std::byte to allow automatic reinterpreting
    template <>
    class Span<char> : public ReinterpretWriteSpan<char>
    {
    public:
        using ReinterpretWriteSpan<char>::ReinterpretWriteSpan;
    };

    template <>
    class Span<const char> : public ReinterpretReadSpan<const char>
    {
    public:
        using ReinterpretReadSpan<const char>::ReinterpretReadSpan;
    };

    template <>
    class Span<unsigned char> : public ReinterpretWriteSpan<unsigned char>
    {
    public:
        using ReinterpretWriteSpan<unsigned char>::ReinterpretWriteSpan;
    };

    template <>
    class Span<const unsigned char> : public ReinterpretReadSpan<const unsigned char>
    {
    public:
        using ReinterpretReadSpan<const unsigned char>::ReinterpretReadSpan;
    };

    template <>
    class Span<std::byte> : public ReinterpretWriteSpan<std::byte>
    {
    public:
        using ReinterpretWriteSpan<std::byte>::ReinterpretWriteSpan;
    };

    template <>
    class Span<const std::byte> : public ReinterpretReadSpan<const std::byte>
    {
    public:
        using ReinterpretReadSpan<const std::byte>::ReinterpretReadSpan;
    };

} // namespace Zibra
