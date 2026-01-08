#include "PrecompiledHeader.h"

#include "NetworkRequest.h"

#if ZIB_NETWORK_REQUEST_BACKEND_WINHTTP
#pragma comment(lib, "winhttp.lib")
#elif ZIB_NETWORK_REQUEST_BACKEND_CURL
#define ZIB_CURL_FUNCTIONS(MACRO_TO_APPLY) \
    MACRO_TO_APPLY(easy_init)              \
    MACRO_TO_APPLY(easy_getinfo)           \
    MACRO_TO_APPLY(easy_setopt)            \
    MACRO_TO_APPLY(easy_perform)           \
    MACRO_TO_APPLY(easy_cleanup)

namespace curl
{
#define ZIB_CURL_FUNCTIONS_DECLARE(FUNCTION) static decltype(&curl_##FUNCTION) FUNCTION = nullptr;
    ZIB_CURL_FUNCTIONS(ZIB_CURL_FUNCTIONS_DECLARE)
#undef ZIB_CURL_FUNCTIONS_DECLARE
} // namespace curl

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userData)
{
    std::vector<char>* output = static_cast<std::vector<char>*>(userData);
    output->insert(output->end(), static_cast<char*>(contents), static_cast<char*>(contents) + size * nmemb);
    return size * nmemb;
}
#else
#error No networking backend defined
#endif

namespace Zibra::NetworkRequest
{
    template <typename T, size_t size>
    constexpr size_t ZIB_ARR_SIZE(T (&)[size])
    {
        return size;
    }

    void CreateFolderStructure(const std::string& filepath)
    {
        std::filesystem::path path(filepath);
        std::filesystem::create_directories(path.parent_path());
    }

    std::optional<std::vector<char>> GetImpl(const std::string& url)
    {
        std::vector<char> result;

#if ZIB_NETWORK_REQUEST_BACKEND_WINHTTP
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

        HINTERNET hSession = ::WinHttpOpen(L"ZibraVDB for Houdini", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                           WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_SECURE_DEFAULTS);

        if (!hSession)
        {
            assert(0);
            return std::nullopt;
        }

        // Parse the URL
        URL_COMPONENTS urlComponents{};
        urlComponents.dwStructSize = sizeof(urlComponents);

        wchar_t hostName[256]{};
        wchar_t urlPath[256]{};
        urlComponents.lpszHostName = hostName;
        urlComponents.dwHostNameLength = ZIB_ARR_SIZE(hostName);
        urlComponents.lpszUrlPath = urlPath;
        urlComponents.dwUrlPathLength = ZIB_ARR_SIZE(urlPath);

        std::wstring wideCharURL = converter.from_bytes(url.c_str());
        if (!::WinHttpCrackUrl(wideCharURL.c_str(), (DWORD)url.length(), 0, &urlComponents))
        {
            ::WinHttpCloseHandle(hSession);
            return std::nullopt;
        }

        HINTERNET hConnect = ::WinHttpConnect(hSession, urlComponents.lpszHostName, INTERNET_DEFAULT_HTTPS_PORT, 0);

        if (!hConnect)
        {
            ::WinHttpCloseHandle(hSession);
            return std::nullopt;
        }

        const wchar_t* acceptTypes[] = {L"*/*", nullptr};
        HINTERNET hRequest =
            ::WinHttpOpenRequest(hConnect, L"GET", urlComponents.lpszUrlPath, NULL, WINHTTP_NO_REFERER, acceptTypes, WINHTTP_FLAG_SECURE);

        if (!hRequest)
        {
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            return std::nullopt;
        }

        if (!::WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0))
        {
            ::WinHttpCloseHandle(hRequest);
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            return std::nullopt;
        }

        if (!::WinHttpReceiveResponse(hRequest, NULL))
        {
            ::WinHttpCloseHandle(hRequest);
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            return std::nullopt;
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if (!::WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                   &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX))
        {
            ::WinHttpCloseHandle(hRequest);
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            return std::nullopt;
        }

        // Saves response body to a buffer
        std::vector<char> responseBuffer;
        DWORD bytesRead = 0;
        DWORD bytesToRead = 0;
        do
        {
            if (!::WinHttpQueryDataAvailable(hRequest, &bytesToRead))
            {
                assert(0);
                break;
            }

            if (bytesToRead == 0)
            {
                break;
            }

            responseBuffer.resize(bytesToRead);
            if (!::WinHttpReadData(hRequest, responseBuffer.data(), bytesToRead, &bytesRead))
            {
                ::WinHttpCloseHandle(hRequest);
                ::WinHttpCloseHandle(hConnect);
                ::WinHttpCloseHandle(hSession);
                return std::nullopt;
            }

            result.insert(result.end(), responseBuffer.begin(), responseBuffer.begin() + bytesRead);

        } while (bytesRead > 0);

        ::WinHttpCloseHandle(hRequest);
        ::WinHttpCloseHandle(hConnect);
        ::WinHttpCloseHandle(hSession);
#elif ZIB_NETWORK_REQUEST_BACKEND_CURL
        // Dynamically load libcurl and load functions
        
#if ZIB_TARGET_OS_LINUX
        void* curlLib = dlopen("libcurl.so.4", RTLD_LAZY);
#elif ZIB_TARGET_OS_MAC
        void* curlLib = dlopen("libcurl.4.dylib", RTLD_LAZY);
#else
#error Unexpected OS
#endif
        
        if (!curlLib)
        {
            return std::nullopt;
        }

#define ZIB_CURL_FUNCTIONS_LOAD(FUNCTION)                                                           \
    curl::FUNCTION = reinterpret_cast<decltype(curl::FUNCTION)>(dlsym(curlLib, "curl_" #FUNCTION)); \
    if (!curl::FUNCTION)                                                                            \
    {                                                                                               \
        dlclose(curlLib);                                                                           \
        return std::nullopt;                                                                               \
    }
        ZIB_CURL_FUNCTIONS(ZIB_CURL_FUNCTIONS_LOAD)
#undef ZIB_CURL_FUNCTIONS_LOAD

        CURL* curl;
        curl = curl::easy_init();
        if (!curl)
        {
            dlclose(curlLib);
            return std::nullopt;
        }

        // List of default CAINFO/CAPATH for different distros
        const char* defaultCAINFO[] = {
            "/etc/ssl/certs/ca-certificates.crt",
            "/etc/pki/tls/certs/ca-bundle.crt",
            "/etc/ssl/cert.pem",
            "/etc/ssl/certs/ca-bundle.crt",
        };

        // Iterate ovoer the list and set the first one that exists
        bool found = false;
        for (const char* path : defaultCAINFO)
        {
            if (std::filesystem::exists(path))
            {
                curl::easy_setopt(curl, CURLOPT_CAINFO, path);
                found = true;
                break;
            }
        }

        // If no default CAINFO was found, return an error
        if (!found)
        {
            curl::easy_cleanup(curl);
            dlclose(curlLib);
            return std::nullopt;
        }

        curl::easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl::easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteCallback);
        curl::easy_setopt(curl, CURLOPT_WRITEDATA, &result);

        CURLcode res = curl::easy_perform(curl);

        long http_code = 0;
        curl::easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        curl::easy_cleanup(curl);

        dlclose(curlLib);

        if (res != CURLE_OK || http_code != 200)
        {
            return std::nullopt;
        }
#else
#error No networking backend defined
#endif
        return result;
    }

    std::string Get(const std::string& url)
    {
        auto response = GetImpl(url);
        if (!response)
        {
            return "";
        }
        return std::string(response->data(), response->size());
    }

    bool DownloadFile(const std::string& url, const std::string& filepath)
    {
        CreateFolderStructure(filepath);

        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open())
        {
            return false;
        }

        auto response = GetImpl(url);

        if (!response)
        {
            return false;
        }

        file.write(response->data(), response->size());

        file.close();
        if (file.fail())
        {
            return false;
        }

        return true;
    }

} // namespace Zibra::NetworkRequest
