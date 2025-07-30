#pragma once

#include "Span.h"

namespace Zibra
{
    class IStream
    {
    public:
        virtual ~IStream() noexcept = default;

    public:
        virtual void read(Span<char> output) noexcept = 0;
        [[nodiscard]] virtual bool fail() const noexcept = 0;
        [[nodiscard]] virtual bool good() const noexcept = 0;
        [[nodiscard]] virtual bool bad() const noexcept = 0;
        [[nodiscard]] virtual bool eof() const noexcept = 0;
        virtual IStream& seekg(size_t pos) noexcept = 0;
        [[nodiscard]] virtual size_t tellg() noexcept = 0;
        [[nodiscard]] virtual size_t gcount() noexcept = 0;
    };

    class OStream
    {
    public:
        virtual ~OStream() noexcept = default;

    public:
        virtual void write(Span<const char> input) noexcept = 0;
        [[nodiscard]] virtual bool fail() const noexcept = 0;
        [[nodiscard]] virtual size_t tellp() noexcept = 0;
        virtual OStream& seekp(size_t pos) noexcept = 0;
    };
} // namespace Zibra
