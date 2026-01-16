#include "PrecompiledHeader.h"

#include "CURLBackend.h"

#ifdef ZIB_NETWORK_REQUEST_BACKEND_CURL

#define ZIB_CURL_FUNCTIONS(MACRO_TO_APPLY) \
    MACRO_TO_APPLY(easy_init)              \
    MACRO_TO_APPLY(easy_getinfo)           \
    MACRO_TO_APPLY(easy_setopt)            \
    MACRO_TO_APPLY(easy_perform)           \
    MACRO_TO_APPLY(easy_cleanup)           \
    MACRO_TO_APPLY(slist_append)           \
    MACRO_TO_APPLY(slist_free_all)

namespace curl
{
#define ZIB_CURL_FUNCTIONS_DECLARE(FUNCTION) static decltype(&curl_##FUNCTION) FUNCTION = nullptr;
    ZIB_CURL_FUNCTIONS(ZIB_CURL_FUNCTIONS_DECLARE)
#undef ZIB_CURL_FUNCTIONS_DECLARE
} // namespace curl

static size_t DataCallback(void* contents, size_t size, size_t nmemb, void* userData)
{
    static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(DataCallback)>);

    std::vector<char>* vec = static_cast<std::vector<char>*>(userData);
    char* dataBegin = (char*)contents;
    char* dataEnd = dataBegin + size * nmemb;
    vec->insert(vec->end(), dataBegin, dataEnd);
    return size * nmemb;
}

static size_t HeaderCallback(char *buffer, size_t size, size_t nitems, void *userData)
{
    static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(HeaderCallback)>);

    std::vector<std::pair<std::string, std::string>>* headers = static_cast<std::vector<std::pair<std::string, std::string>>*>(userData);
    std::string headerData(buffer, size * nitems - 2); // drop last two bytes (/r/n)
    
    // Split headerData by ": "
    size_t colonPos = headerData.find(": ");
    if (colonPos == std::string::npos)
    {
        return size * nitems;
    }

    std::string headerName = headerData.substr(0, colonPos);
    std::string headerValue = headerData.substr(colonPos + 2);

    headers->emplace_back(headerName, headerValue);
    return size * nitems;
}

namespace Zibra::NetworkRequest::CURLBackend
{
    Response Perform(const Request& request)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(Perform)>);

        Response result;

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
            result.errorMessage = "Failed to load libcurl";
            return result;
        }

#define ZIB_CURL_FUNCTIONS_LOAD(FUNCTION)                                                           \
    curl::FUNCTION = reinterpret_cast<decltype(curl::FUNCTION)>(dlsym(curlLib, "curl_" #FUNCTION)); \
    if (!curl::FUNCTION)                                                                            \
    {                                                                                               \
        dlclose(curlLib);                                                                           \
        result.errorMessage = "Failed to load libcurl functions";                                   \
        return result;                                                                              \
    }

        ZIB_CURL_FUNCTIONS(ZIB_CURL_FUNCTIONS_LOAD)
#undef ZIB_CURL_FUNCTIONS_LOAD

        CURL* curl;
        curl = curl::easy_init();
        if (!curl)
        {
            dlclose(curlLib);
            result.errorMessage = "Failed to initialize curl";
            return result;
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
            result.errorMessage = "Failed to initialize ssl";
            return result;
        }

        curl::easy_setopt(curl, CURLOPT_URL, request.URL.c_str());
        
        switch (request.method)
        {
        case Method::GET:
            curl::easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            break;
        case Method::POST:
            curl::easy_setopt(curl, CURLOPT_HTTPPOST, 1L);
            break;
        default:
            assert(0);
            curl::easy_setopt(curl, CURLOPT_HTTPGET, 1L);
            break;
        }

        if (request.method == Method::POST)
        {
            curl::easy_setopt(curl, CURLOPT_POSTFIELDS, request.data.data());
            curl::easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request.data.size());
        }

        curl::easy_setopt(curl, CURLOPT_WRITEFUNCTION, &DataCallback);
        curl::easy_setopt(curl, CURLOPT_WRITEDATA, &result.data);
        curl::easy_setopt(curl, CURLOPT_HEADERFUNCTION, &HeaderCallback);
        curl::easy_setopt(curl, CURLOPT_HEADERDATA, &result.headers);
        curl::easy_setopt(curl, CURLOPT_USERAGENT, request.userAgent.c_str());
        struct curl_slist* headers = nullptr;
        
        headers = curl::slist_append(headers, ("Accept: " + request.acceptTypes).c_str());
        
        if (request.method == Method::POST)
        {
            headers = curl::slist_append(headers, ("Content-Type: " + request.contentType).c_str());
        }
        
        for (const auto& [headerName, headerValue] : request.additionalHeaders)
        {
            headers = curl::slist_append(headers, (headerName + ": " + headerValue).c_str());
        }
        
        curl::easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl::easy_perform(curl);

        if (res != CURLE_OK)
        {
            result.errorMessage = "Failed to perform request";
            return result;
        }

        result.success = true;

        long http_code = 0;
        curl::easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        result.statusCode = (int)http_code;

        curl::slist_free_all(headers);
        curl::easy_cleanup(curl);

        dlclose(curlLib);

        return result;
    }
} // namespace Zibra::NetworkRequest::CURLBackend
#endif
