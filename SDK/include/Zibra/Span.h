#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <vector>

namespace Zibra
{
    // std::span is not in C++17
    template <typename T>
    class Span
    {
    public:
        Span()
            : m_Data(nullptr)
            , m_Size(0)
        {
        }

        Span(const T* data, size_t size)
            : m_Data(data)
            , m_Size(size)
        {
        }

        // Automatic conversion from containers
        template <size_t N>
        Span(const T (&data)[N])
            : m_Data(data)
            , m_Size(N)
        {
        }

        Span(const std::vector<T>& vec)
            : m_Data(vec.data())
            , m_Size(vec.size())
        {
        }

        template <std::size_t N>
        Span(const std::array<T, N>& arr)
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
        bool ContentEquals(const Span<T>& other) const
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
} // namespace Zibra
