#pragma once

#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "Stream.h"

#ifndef ZIB_FOUNDATION_MEMORY_CACHE_STREAM_PAGE_SIZE_POWER_OF_TWO
// 4MiB by default
#define ZIB_FOUNDATION_MEMORY_CACHE_STREAM_PAGE_SIZE_POWER_OF_TWO 22
#endif

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

    class MemoryCacheStream final : public OStream, public IStream
    {
    public:
        MemoryCacheStream() noexcept = default;
        ~MemoryCacheStream() noexcept = default;
        MemoryCacheStream(MemoryCacheStream&&) noexcept = default;

        MemoryCacheStream(const MemoryCacheStream&) = delete;
        MemoryCacheStream& operator=(const MemoryCacheStream&) = delete;
        MemoryCacheStream& operator=(MemoryCacheStream&&) = delete;

        // Used in both interfaces
        [[nodiscard]] bool fail() const noexcept final
        {
            return m_Fail;
        }

        //~ Begin Zibra::OStream Interface.
        void write(Zibra::Span<const char> input) noexcept final
        {
            if (input.size() == 0)
            {
                return;
            }

            size_t requiredSize = m_CurrentWritePos + input.size();
            if (requiredSize > m_Size)
            {
                size_t newPageCount = ((requiredSize + MemoryPage::PageSize - 1) >> MemoryPage::PageSizePowerOfTwo);
                m_Storage.resize(newPageCount);
                m_Size = requiredSize;
            }

            const char* inputData = input.data();
            size_t remainingSize = input.size();
            // Potentially partial page write
            {
                size_t currentPage = (m_CurrentWritePos >> MemoryPage::PageSizePowerOfTwo);
                size_t currentPageWrittenBytes = (m_CurrentWritePos & (MemoryPage::PageSize - 1));
                size_t remainingSizeInCurrentPage = MemoryPage::PageSize - currentPageWrittenBytes;
                if (remainingSizeInCurrentPage >= remainingSize)
                {
                    memcpy(m_Storage[currentPage].Data + currentPageWrittenBytes, inputData, remainingSize);
                    m_CurrentWritePos += remainingSize;
                    return;
                }
                else
                {
                    memcpy(m_Storage[currentPage].Data + currentPageWrittenBytes, inputData, remainingSizeInCurrentPage);
                    m_CurrentWritePos += remainingSizeInCurrentPage;
                    inputData += remainingSizeInCurrentPage;
                    remainingSize -= remainingSizeInCurrentPage;
                }
            }
            while (remainingSize >= MemoryPage::PageSize)
            {
                assert((m_CurrentWritePos & (MemoryPage::PageSize - 1)) == 0);
                size_t currentPage = (m_CurrentWritePos >> MemoryPage::PageSizePowerOfTwo);
                memcpy(m_Storage[currentPage].Data, inputData, MemoryPage::PageSize);
                m_CurrentWritePos += MemoryPage::PageSize;
                inputData += MemoryPage::PageSize;
                remainingSize -= MemoryPage::PageSize;
            }
            // Potentially partial page write
            if (remainingSize > 0)
            {
                assert((m_CurrentWritePos & (MemoryPage::PageSize - 1)) == 0);
                size_t currentPage = (m_CurrentWritePos >> MemoryPage::PageSizePowerOfTwo);
                memcpy(m_Storage[currentPage].Data, inputData, remainingSize);
                m_CurrentWritePos += remainingSize;
            }
        }

        [[nodiscard]] size_t tellp() noexcept final
        {
            return m_CurrentWritePos;
        }

        Zibra::OStream& seekp(size_t pos) noexcept final
        {
            if (pos > m_Size)
            {
                size_t newPageCount = ((pos + MemoryPage::PageSize - 1) >> MemoryPage::PageSizePowerOfTwo);
                m_Storage.resize(newPageCount);
                m_Size = pos;
            }

            m_CurrentWritePos = pos;

            return *this;
        }
        //~ End Zibra::OStream Interface.

        //~ Begin Zibra::IStream Interface.
        void read(Zibra::Span<char> output) noexcept final
        {
            if (output.size() == 0)
            {
                return;
            }

            m_GCount = std::min(output.size(), m_Size - m_CurrentReadPos);
            m_Fail = (m_GCount != output.size());

            char* outputData = output.data();
            size_t remainingSize = m_GCount;
            // Potentially partial page read
            {
                size_t currentPage = (m_CurrentReadPos >> MemoryPage::PageSizePowerOfTwo);
                size_t currentPageReadBytes = (m_CurrentReadPos & (MemoryPage::PageSize - 1));
                size_t remainingSizeInCurrentPage = MemoryPage::PageSize - currentPageReadBytes;
                if (remainingSizeInCurrentPage > remainingSize)
                {
                    memcpy(outputData, m_Storage[currentPage].Data + currentPageReadBytes, remainingSize);
                    m_CurrentReadPos += remainingSize;
                    return;
                }
                else
                {
                    memcpy(outputData, m_Storage[currentPage].Data + currentPageReadBytes, remainingSizeInCurrentPage);
                    m_CurrentReadPos += remainingSizeInCurrentPage;
                    outputData += remainingSizeInCurrentPage;
                    remainingSize -= remainingSizeInCurrentPage;
                }
            }
            while (remainingSize >= MemoryPage::PageSize)
            {
                assert((m_CurrentReadPos & (MemoryPage::PageSize - 1)) == 0);
                size_t currentPage = (m_CurrentReadPos >> MemoryPage::PageSizePowerOfTwo);
                memcpy(outputData, m_Storage[currentPage].Data, MemoryPage::PageSize);
                m_CurrentReadPos += MemoryPage::PageSize;
                outputData += MemoryPage::PageSize;
                remainingSize -= MemoryPage::PageSize;
            }
            // Potentially partial page write
            if (remainingSize > 0)
            {
                assert((m_CurrentReadPos & (MemoryPage::PageSize - 1)) == 0);
                size_t currentPage = (m_CurrentReadPos >> MemoryPage::PageSizePowerOfTwo);
                memcpy(outputData, m_Storage[currentPage].Data, remainingSize);
                m_CurrentReadPos += remainingSize;
            }
        }

        [[nodiscard]] bool good() const noexcept final
        {
            return !eof() && !m_Fail;
        }

        [[nodiscard]] bool bad() const noexcept final
        {
            return false;
        }

        [[nodiscard]] bool eof() const noexcept final
        {
            return m_CurrentReadPos == m_Size;
        }

        IStream& seekg(size_t pos) noexcept final
        {
            m_Fail = (pos > m_Size);
            m_CurrentReadPos = std::min(pos, m_Size);

            return *this;
        }

        [[nodiscard]] size_t tellg() noexcept final
        {
            return m_CurrentReadPos;
        }

        [[nodiscard]] size_t gcount() noexcept final
        {
            return m_GCount;
        }
        //~ End Zibra::IStream Interface.

    private:
        struct MemoryPage
        {
            static constexpr size_t PageSizePowerOfTwo = ZIB_FOUNDATION_MEMORY_CACHE_STREAM_PAGE_SIZE_POWER_OF_TWO;
            static constexpr size_t PageSize = (1 << PageSizePowerOfTwo);

            // PageSize must be power of 2
            static_assert((PageSize & (PageSize - 1)) == 0);

            MemoryPage() noexcept
                : Data(new char[PageSize])
            {
            }

            MemoryPage(MemoryPage&& other) noexcept
                : Data(other.Data)
            {
                other.Data = nullptr;
            }

            ~MemoryPage() noexcept
            {
                delete[] Data;
            }

            MemoryPage(const MemoryPage&) = delete;
            MemoryPage& operator=(const MemoryPage&) = delete;
            MemoryPage& operator=(MemoryPage&& other) = delete;

            char* Data = nullptr;
        };

        std::vector<MemoryPage> m_Storage;
        size_t m_Size = 0;
        size_t m_CurrentWritePos = 0;
        size_t m_CurrentReadPos = 0;
        size_t m_GCount = 0;
        bool m_Fail = false;
    };
} // namespace Zibra
