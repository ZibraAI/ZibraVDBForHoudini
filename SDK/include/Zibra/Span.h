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

    template <typename T>
    class BasicSpan
    {
    public:
        BasicSpan()
            : m_Data(nullptr)
            , m_Size(0)
        {
        }

        BasicSpan(const T* data, size_t size)
            : m_Data(data)
            , m_Size(size)
        {
        }

        // Automatic conversion from containers
        template <size_t N>
        BasicSpan(const T (&data)[N])
            : m_Data(data)
            , m_Size(N)
        {
        }

        BasicSpan(const std::vector<T>& vec)
            : m_Data(vec.data())
            , m_Size(vec.size())
        {
        }

        template <std::size_t N>
        BasicSpan(const std::array<T, N>& arr)
            : m_Data(arr.data())
            , m_Size(N)
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

        const T* data() const
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
        const T* m_Data;
        size_t m_Size;
    };

    template <typename T>
    class ReinterpretSpan : public BasicSpan<T>
    {
    public:
        using BasicSpan<T>::BasicSpan;

        // Specialization for char/unsigned char/std::byte to allow automatic reinterpreting
        template <typename T2, size_t N>
        ReinterpretSpan(const T2 (&data)[N])
            : BasicSpan<T>(reinterpret_cast<const T*>(data), sizeof(data) / sizeof(T))
        {
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        template <typename T2>
        ReinterpretSpan(const std::vector<T2>& vec)
            : BasicSpan<T>(reinterpret_cast<const T*>(vec.data()), vec.size() * sizeof(T2) / sizeof(T))
        {
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        template <typename T2, size_t N>
        ReinterpretSpan(const std::array<T2, N>& arr)
            : BasicSpan<T>(reinterpret_cast<const T*>(arr.data()), N * sizeof(T2) / sizeof(T))
        {
            static_assert(sizeof(T2) % sizeof(T) == 0);
        }

        ReinterpretSpan(const std::string& str)
            : BasicSpan<T>(reinterpret_cast<const T*>(str.data()), str.size())
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

    template <>
    class Span<char> : public ReinterpretSpan<char>
    {
    public:
        using ReinterpretSpan<char>::ReinterpretSpan;
    };

    template <>
    class Span<unsigned char> : public ReinterpretSpan<unsigned char>
    {
    public:
        using ReinterpretSpan<unsigned char>::ReinterpretSpan;
    };

    template <>
    class Span<std::byte> : public ReinterpretSpan<std::byte>
    {
    public:
        using ReinterpretSpan<std::byte>::ReinterpretSpan;
    };

} // namespace Zibra
