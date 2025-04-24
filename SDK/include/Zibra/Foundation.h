#pragma once

#include <istream>
#include <ostream>

namespace Zibra
{
    struct Version
    {
        uint32_t major;
        uint32_t minor;
        uint32_t patch;
        uint32_t build;
    };
    inline bool IsCompatibleVersion(const Version& runtime, const Version& expected) noexcept
    {
        if (runtime.major != expected.major)
            return false;
        if (runtime.minor != expected.minor)
            return runtime.minor >= expected.minor;
        if (runtime.patch != expected.patch)
            return runtime.patch >= expected.patch;
        return runtime.build >= expected.build;
    }

    class IStream
    {
    public:
        virtual ~IStream() noexcept = default;

    public:
        virtual void read(char* s, size_t count) noexcept = 0;
        [[nodiscard]] virtual size_t gcount() const noexcept = 0;
    };

    class STDIStreamWrapper : public IStream
    {
    public:
        STDIStreamWrapper(std::istream& stream)
            : m_IStream(stream)
        {
        }

        void read(char* s, size_t count) noexcept final
        {
            m_IStream.read(s, count);
        }

        [[nodiscard]] size_t gcount() const noexcept final
        {
            return m_IStream.gcount();
        }

    private:
        std::istream& m_IStream;
    };

#pragma region IStream CAPI
    namespace CAPI
    {
        struct IStreamVTable
        {
            void* obj;
            void (*destructor)(void*);
            void (*read)(void*, char* s, size_t count);
            size_t (*gcount)(void*);
        };

        inline IStreamVTable VTConvert(IStream* obj) noexcept
        {
            using T = IStream;
            IStreamVTable vt{};
            vt.obj = obj;

            vt.destructor = [](void* o) { delete static_cast<T*>(o); };
            vt.read = [](void* o, char* s, size_t c) { return static_cast<T*>(o)->read(s, c); };
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
        virtual void write(const char* s, size_t count) noexcept = 0;
        [[nodiscard]] virtual bool fail() const noexcept = 0;
    };

    class STDOStreamWrapper : public OStream
    {
    public:
        STDOStreamWrapper(std::ostream& os)
            : m_OStream(os)
        {
        }

        void write(const char* s, size_t count) noexcept final
        {
            m_OStream.write(s, count);
        }

        [[nodiscard]] bool fail() const noexcept final
        {
            return m_OStream.fail();
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
            void (*write)(void*, const char* s, size_t count);
            bool (*fail)(void*);
        };

        inline OStreamVTable VTConvert(OStream* obj) noexcept
        {
            using T = OStream;
            OStreamVTable vt{};
            vt.obj = obj;

            vt.destructor = [](void* o) { delete static_cast<T*>(o); };
            vt.write = [](void* o, const char* s, size_t c) { return static_cast<T*>(o)->write(s, c); };
            vt.fail = [](void* o) { return static_cast<T*>(o)->fail(); };

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
            void write(const char* s, size_t count) noexcept final
            {
                m_VT.write(m_VT.obj, s, count);
            }
            [[nodiscard]] bool fail() const noexcept final
            {
                return m_VT.fail(m_VT.obj);
            }

        private:
            OStreamVTable m_VT;
        };
    } // namespace CAPI
#pragma endregion OStream CAPI

} // namespace Zibra
