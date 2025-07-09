#pragma once

#include <istream>
#include <ostream>
#include <cstring>

namespace Zibra
{
    template<class T>
    constexpr auto ZCE_CEIL_TO_MULTIPLE_OF(T intValue, T multiple) noexcept
    {
        return ((intValue + multiple - 1) / multiple) * multiple;
    }

    struct Version
    {
        uint32_t major;
        uint32_t minor;
        uint32_t patch;
        uint32_t build;
    };
    inline bool operator==(const Version& l, const Version& r)
    {
        return l.major == r.major && l.minor == r.minor && l.patch == r.patch && l.build == r.build;
    }
    inline bool operator!=(const Version& l, const Version& r)
    {
        return l.major != r.major || l.minor != r.minor || l.patch != r.patch || l.build != r.build;
    }
    inline bool operator>(const Version& l, const Version& r)
    {
        if (l.major != r.major) return l.major > r.major;
        if (l.minor != r.minor) return l.minor > r.minor;
        if (l.patch != r.patch) return l.patch > r.patch;
        if (l.build != r.build) return l.build > r.build;
        return false;
    }
    inline bool operator>=(const Version& l, const Version& r)
    {
        return l == r || l > r;
    }
    inline bool operator<(const Version& l, const Version& r)
    {
        if (l.major != r.major) return l.major < r.major;
        if (l.minor != r.minor) return l.minor < r.minor;
        if (l.patch != r.patch) return l.patch < r.patch;
        if (l.build != r.build) return l.build < r.build;
        return false;
    }
    inline bool operator<=(const Version& l, const Version& r)
    {
        return l == r || l < r;
    }
    inline bool IsCompatibleVersion(const Version& runtime, const Version& expected) noexcept
    {
        return runtime >= expected && runtime < Version{expected.major + 1, 0, 0, 0};
    }

    class IStream
    {
    public:
        virtual ~IStream() noexcept = default;

    public:
        virtual void read(void* s, size_t count) noexcept = 0;
        virtual bool fail() const noexcept = 0;
        virtual bool good() const noexcept = 0;
        virtual bool bad() const noexcept = 0;
        virtual bool eof() const noexcept = 0;
        virtual IStream& seekg(size_t pos) noexcept = 0;
        virtual size_t tellg() noexcept = 0;
        [[nodiscard]] virtual size_t gcount() noexcept = 0;
    };

    class STDIStreamWrapper : public IStream
    {
    public:
        STDIStreamWrapper(std::istream& stream)
            : m_IStream(stream)
        {
        }

        void read(void* s, size_t count) noexcept final
        {
            m_IStream.read(static_cast<char*>(s), count);
        }

        bool fail() const noexcept final
        {
            return m_IStream.fail();
        }

        bool good() const noexcept final
        {
            return m_IStream.good();
        }

        bool bad() const noexcept final
        {
            return m_IStream.bad();
        }

        bool eof() const noexcept final
        {
            return m_IStream.eof();
        }

        IStream& seekg(size_t pos) noexcept final
        {
            m_IStream.seekg(pos);
            return *this;
        }

        size_t tellg() noexcept final
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
            : m_Mem(static_cast<const char*>(memory)), m_MemSize(size), m_GPos(0), m_GCount(0)
        {
        }

    public:
        void read(void* s, size_t count) noexcept override
        {
            m_Fail = (m_GPos + m_GCount) > m_MemSize;
            if (m_Fail)
                return;
            m_GCount = std::min(count, m_MemSize - m_GPos);
            memcpy(s, &m_Mem[m_GPos], m_GCount);
            m_GPos += m_GCount;
        }
        bool fail() const noexcept override
        {
            return m_Bad || m_Fail;
        }
        bool good() const noexcept override
        {
            return !eof() && !m_Bad || !m_Fail;
        }
        bool bad() const noexcept override
        {
            return m_Bad;
        }
        bool eof() const noexcept override
        {
            return m_GPos >= m_MemSize;
        }
        IStream& seekg(size_t pos) noexcept override
        {
            m_Fail = pos > m_MemSize;
            m_GPos = std::min(pos, m_MemSize);
            return *this;
        }
        size_t tellg() noexcept override
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

#pragma region IStream CAPI
    namespace CAPI
    {
        struct IStreamVTable
        {
            void* obj;
            void (*destructor)(void*);
            void (*read)(void*, void* s, size_t count);
            size_t (*gcount)(void*);
        };

        inline IStreamVTable VTConvert(IStream* obj) noexcept
        {
            using T = IStream;
            IStreamVTable vt{};
            vt.obj = obj;

            vt.destructor = [](void* o) { delete static_cast<T*>(o); };
            vt.read = [](void* o, void* s, size_t c) { return static_cast<T*>(o)->read(s, c); };
            vt.gcount = [](void* o) { return static_cast<T*>(o)->gcount(); };

            return vt;
        }
    } // namespace CAPI
#pragma endregion IStream CAPI

    class OStream
    {
    public:
        virtual ~OStream() noexcept = default;

    public:
        virtual void write(const void* s, size_t count) noexcept = 0;
        [[nodiscard]] virtual bool fail() const noexcept = 0;
        [[nodiscard]] virtual size_t tellp() noexcept = 0;
        virtual OStream& seekp(size_t pos) noexcept = 0;
    };

    class STDOStreamWrapper : public OStream
    {
    public:
        STDOStreamWrapper(std::ostream& os)
            : m_OStream(os)
        {
        }

        void write(const void* s, size_t count) noexcept final
        {
            m_OStream.write(static_cast<const char*>(s), count);
        }

        [[nodiscard]] bool fail() const noexcept final
        {
            return m_OStream.fail();
        }

        size_t tellp() noexcept final
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

#pragma region OStream CAPI
    namespace CAPI
    {
        struct OStreamVTable
        {
            void* obj;
            void (*destructor)(void*);
            void (*write)(void*, const void* s, size_t count);
            bool (*fail)(void*);
            size_t (*tellp)(void*);
            OStream& (*seekp)(void*, size_t pos);
        };

        inline OStreamVTable VTConvert(OStream* obj) noexcept
        {
            using T = OStream;
            OStreamVTable vt{};
            vt.obj = obj;

            vt.destructor = [](void* o) { delete static_cast<T*>(o); };
            vt.write = [](void* o, const void* s, size_t c) { return static_cast<T*>(o)->write(s, c); };
            vt.fail = [](void* o) { return static_cast<T*>(o)->fail(); };
            vt.tellp = [](void* o) { return static_cast<T*>(o)->tellp(); };
            vt.seekp = [](void* o, size_t p) -> OStream& { return static_cast<T*>(o)->seekp(p); };

            return vt;
        }

        class OStreamCAPIWrapper : public OStream
        {
        public:
            explicit OStreamCAPIWrapper(OStreamVTable vt)
                : m_VT(vt)
            {
            }

        public:
            void write(const void* s, size_t count) noexcept final
            {
                m_VT.write(m_VT.obj, s, count);
            }

            [[nodiscard]] bool fail() const noexcept final
            {
                return m_VT.fail(m_VT.obj);
            }

            size_t tellp() noexcept final
            {
                return m_VT.tellp(m_VT.obj);
            }

            OStream& seekp(size_t pos) noexcept final
            {
                return m_VT.seekp(m_VT.obj, pos);
            }

        private:
            OStreamVTable m_VT;
        };
    } // namespace CAPI
#pragma endregion OStream CAPI

} // namespace Zibra
