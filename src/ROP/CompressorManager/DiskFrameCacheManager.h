#pragma once

#include <Zibra/CE/Compression.h>
#include <filesystem>

namespace Zibra::ZibraVDBCompressor
{
    class DiskFrameCacheManager : public CE::Compression::CacheManager
    {
    protected:
        struct CacheItem
        {
            std::filesystem::path filepath = {};
            std::ifstream* read = nullptr;
            IStream* readWrapper = nullptr;
            std::ofstream* write = nullptr;
            OStream* writeWrapper = nullptr;
        };

    public:
        explicit DiskFrameCacheManager(const std::filesystem::path& basePath) noexcept : m_BasePath(basePath)
        {
            std::filesystem::create_directories(basePath);
        }

    public:
        ReturnCode StartCacheStore(const char* id, OStream** outStream) noexcept override
        {
            auto it = m_Cache.find(id);
            if (it != m_Cache.end() && (it->second.readWrapper || it->second.writeWrapper))
            {
                return ZCE_ERROR;
            }

            CacheItem cache{};
            cache.filepath = MakePathFromID(id);
            cache.write = new std::ofstream{cache.filepath, std::ios::binary};
            if (!cache.write->is_open())
            {
                delete cache.write;
                return ZCE_ERROR;
            }
            cache.writeWrapper = new STDOStreamWrapper{*cache.write};

            if (it != m_Cache.end())
            {
                it->second = cache;
            }
            else
            {
                m_Cache.emplace(id, cache);
            }
            *outStream = cache.writeWrapper;
            return ZCE_SUCCESS;
        }

        ReturnCode FinishCacheStore(const char* id) noexcept override
        {
            auto it = m_Cache.find(id);
            if (it == m_Cache.end())
                return ZCE_ERROR_NOT_FOUND;

            delete it->second.writeWrapper;
            it->second.writeWrapper = nullptr;
            delete it->second.write;
            it->second.write = nullptr;
            return ZCE_SUCCESS;
        }

        ReturnCode StartCacheRead(const char* id, IStream** outStream) noexcept override
        {
            auto it = m_Cache.find(id);
            if (it == m_Cache.end())
                return ZCE_ERROR_NOT_FOUND;

            if (it->second.readWrapper || it->second.writeWrapper)
                return ZCE_ERROR;

            it->second.read = new std::ifstream{it->second.filepath, std::ios::binary};
            if (!it->second.read->is_open())
            {
                delete it->second.read;
                it->second.read = nullptr;
                return ZCE_ERROR;
            }
            it->second.readWrapper = new STDIStreamWrapper{*it->second.read};
            *outStream = it->second.readWrapper;
            return ZCE_SUCCESS;
        }

        ReturnCode FinishCacheRead(const char* id) noexcept override
        {
            auto it = m_Cache.find(id);
            if (it == m_Cache.end())
                return ZCE_ERROR_NOT_FOUND;

            delete it->second.readWrapper;
            it->second.readWrapper = nullptr;
            delete it->second.read;
            it->second.read = nullptr;
            return ZCE_SUCCESS;
        }

        ReturnCode ReleaseCache(const char* id) noexcept override
        {
            auto it = m_Cache.find(id);
            if (it == m_Cache.end())
                return ZCE_ERROR_NOT_FOUND;

            if (it->second.readWrapper || it->second.writeWrapper)
                return ZCE_ERROR;

            // dummy error code for noexcept function call.
            std::error_code errorCode = {};
            auto status = std::filesystem::remove(it->second.filepath, errorCode);
            m_Cache.erase(it);

            return status ? ZCE_SUCCESS : ZCE_ERROR;
        }

        void Release() noexcept
        {
            for (const auto& [id, cache] : m_Cache)
            {
                delete cache.write;
                delete cache.writeWrapper;
                delete cache.read;
                delete cache.readWrapper;
                // dummy error code for noexcept function call.
                std::error_code errorCode = {};
                std::filesystem::remove(cache.filepath, errorCode);
            }
            m_Cache.clear();
        }

    protected:
        virtual std::filesystem::path MakePathFromID(const std::string& id) noexcept
        {
            using namespace std::string_literals;
            return "FrameCache."s + id + ".tmp"s;
        }

    protected:
        std::map<std::string, CacheItem> m_Cache{};
        std::filesystem::path m_BasePath{};
    };
}