#pragma once

#include <istream>
#include <ostream>

namespace Zibra::CE {
    static constexpr int SPARSE_BLOCK_SIZE = 8;
    static constexpr int SPARSE_BLOCK_VOXEL_COUNT = SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE;

    enum ReturnCode
    {
        // Successfully finished operation
        ZCE_SUCCESS = 0,
        // Unexpected error
        ZCE_ERROR = 100,
        // Fatal error
        ZCE_FATAL_ERROR = 110,

        ZCE_ERROR_NOT_INITIALIZED = 200,
        ZCE_ERROR_ALREADY_INITIALIZED = 201,

        ZCE_ERROR_INVALID_USAGE = 300,
        ZCE_ERROR_INVALID_ARGUMENTS = 301,
        ZCE_ERROR_NOT_IMPLEMENTED = 310,
        ZCE_ERROR_NOT_SUPPORTED = 311,

        ZCE_ERROR_NOT_FOUND = 400,
        // Out of CPU memory
        ZCE_ERROR_OUT_OF_CPU_MEMORY = 410,
        // Out of GPU memory
        ZCE_ERROR_OUT_OF_GPU_MEMORY = 411,
        // Time out
        ZCE_ERROR_TIME_OUT = 430,

    };

    struct MetadataEntry
    {
        const char* key = nullptr;
        const char* value = nullptr;
    };

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
            void(*destructor)(void*);
            void(*read)(void*, char* s, size_t count);
            size_t(*gcount)(void*);
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
    }
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
            void(*destructor)(void*);
            void(*write)(void*, const char* s, size_t count);
            bool(*fail)(void*);
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
    }
#pragma endregion OStream CAPI
}
