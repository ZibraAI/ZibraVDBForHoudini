#pragma once

#include <istream>
#include <ostream>
#include <string>

#include "Stream.h"

namespace Zibra
{
    class STDIStreamWrapper : public IStream
    {
    public:
        STDIStreamWrapper(std::istream& stream)
            : m_IStream(stream)
        {
        }

        void read(Span<char> output) noexcept final
        {
            m_IStream.read(output.data(), output.size());
        }

        [[nodiscard]] bool fail() const noexcept final
        {
            return m_IStream.fail();
        }

        [[nodiscard]] bool good() const noexcept final
        {
            return m_IStream.good();
        }

        [[nodiscard]] bool bad() const noexcept final
        {
            return m_IStream.bad();
        }

        [[nodiscard]] bool eof() const noexcept final
        {
            return m_IStream.eof();
        }

        IStream& seekg(size_t pos) noexcept final
        {
            m_IStream.seekg(pos);
            return *this;
        }

        [[nodiscard]] size_t tellg() noexcept final
        {
            return m_IStream.tellg();
        }

        [[nodiscard]] size_t gcount() noexcept final
        {
            return m_IStream.gcount();
        }

    private:
        std::istream& m_IStream;
    };

    class MemoryIStream : public IStream
    {
    public:
        explicit MemoryIStream(const void* memory, size_t size) noexcept
            : m_Mem(static_cast<const char*>(memory))
            , m_MemSize(size)
            , m_GPos(0)
            , m_GCount(0)
        {
        }

    public:
        void read(Span<char> output) noexcept override
        {
            m_GCount = std::min(output.size(), m_MemSize - m_GPos);
            memcpy(output.data(), &m_Mem[m_GPos], m_GCount);
            m_Fail = (m_GPos + output.size()) > m_MemSize;
            m_GPos += m_GCount;
        }
        [[nodiscard]] bool fail() const noexcept override
        {
            return m_Bad || m_Fail;
        }
        [[nodiscard]] bool good() const noexcept override
        {
            return !eof() && !m_Bad && !m_Fail;
        }
        [[nodiscard]] bool bad() const noexcept override
        {
            return m_Bad;
        }
        [[nodiscard]] bool eof() const noexcept override
        {
            return m_GPos >= m_MemSize;
        }
        IStream& seekg(size_t pos) noexcept override
        {
            m_Fail = pos > m_MemSize;
            m_GPos = std::min(pos, m_MemSize);
            return *this;
        }
        [[nodiscard]] size_t tellg() noexcept override
        {
            return m_GPos;
        }
        [[nodiscard]] size_t gcount() noexcept override
        {
            return m_GCount;
        }

    protected:
        const char* m_Mem;
        size_t m_MemSize;
        size_t m_GPos;
        size_t m_GCount;
        bool m_Bad = false;
        bool m_Fail = false;
    };

    class STDOStreamWrapper : public OStream
    {
    public:
        STDOStreamWrapper(std::ostream& os)
            : m_OStream(os)
        {
        }

        void write(Span<const char> input) noexcept final
        {
            m_OStream.write(input.begin(), input.size());
        }

        [[nodiscard]] bool fail() const noexcept final
        {
            return m_OStream.fail();
        }

        [[nodiscard]] size_t tellp() noexcept final
        {
            return m_OStream.tellp();
        }

        OStream& seekp(size_t pos) noexcept final
        {
            m_OStream.seekp(pos);
            return *this;
        }

    private:
        std::ostream& m_OStream;
    };
} // namespace Zibra
