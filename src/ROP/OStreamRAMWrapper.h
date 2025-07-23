#pragma once

namespace Zibra
{
    class OStreamRAMWrapper : public OStream
    {
    public:
        void write(const void* s, size_t size) noexcept final
        {
            assert(!m_Fail);
            EnsureDataFits(size);
            memcpy(m_Data.data() + m_Pos, s, size);
            m_Pos += size;
        }
        [[nodiscard]] bool fail() const noexcept final
        {
            return m_Fail;
        }
        [[nodiscard]] size_t tellp() noexcept final
        {
            return m_Pos;
        }
        OStream& seekp(size_t pos) noexcept final
        {
            m_Pos = pos;
            if (pos >= m_Data.size())
            {
                m_Data.resize(m_Pos);
                if (m_Data.size() != m_Pos) m_Fail = true;
            }
            return *this;
        }

    public:
        const std::vector<uint8_t>& GetContainer() noexcept
        {
            return m_Data;
        }

    private:
        void EnsureDataFits(size_t size) noexcept
        {
            if (m_Data.size() >= m_Pos + size)
                return;
            m_Data.resize(m_Pos + size);
        }

    private:
        size_t m_Pos = 0;
        bool m_Fail = false;
        std::vector<uint8_t> m_Data = {};
    };
} // namespace Zibra