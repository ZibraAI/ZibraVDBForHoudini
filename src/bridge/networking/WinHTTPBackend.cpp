#include "PrecompiledHeader.h"

#include "WinHTTPBackend.h"

#ifdef ZIB_NETWORK_REQUEST_BACKEND_WINHTTP

#pragma comment(lib, "winhttp.lib")

namespace Zibra::NetworkRequest::WinHTTPBackend
{

    template <typename T, size_t size>
    constexpr size_t ZIB_ARR_SIZE(T (&)[size])
    {
        return size;
    }

    std::wstring ToWideString(const std::string str)
    {
        if (str.empty())
            return {};

        const int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
        if (size == 0)
            return {};

        std::vector<wchar_t> buf((size_t)size);
        const int result = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), buf.data(), size);
        if (result == 0)
            return {};

        return {buf.data(), buf.size()};
    }

    std::string ToNarrowString(const std::wstring str)
    {
        if (str.empty())
            return {};

        const int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0, NULL, NULL);
        if (size == 0)
            return {};

        std::vector<char> buf((size_t)size);
        const int result = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.length(), buf.data(), size, NULL, NULL);
        if (result == 0)
            return {};

        return {buf.data(), buf.size()};
    }

    const wchar_t* MethodStr(Method method)
    {
        switch (method)
        {
        case Method::GET:
            return L"GET";
        case Method::POST:
            return L"POST";
        default:
            assert(0);
            return L"GET";
        }
    }

    Response Perform(const Request& request)
    {
        Response result;

        const std::wstring wideUserAgent = ToWideString(request.userAgent);
        HINTERNET hSession = ::WinHttpOpen(wideUserAgent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                           WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession)
        {
            result.errorMessage = "Failed to initialize network library.";
            return result;
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

        std::wstring wideCharURL = ToWideString(request.URL.c_str());
        if (!::WinHttpCrackUrl(wideCharURL.c_str(), (DWORD)request.URL.length(), 0, &urlComponents))
        {
            ::WinHttpCloseHandle(hSession);
            result.errorMessage = "Failed to parse URL.";
            return result;
        }

        HINTERNET hConnect = ::WinHttpConnect(hSession, urlComponents.lpszHostName, urlComponents.nPort, 0);

        if (!hConnect)
        {
            ::WinHttpCloseHandle(hSession);
            result.errorMessage = "Failed to initialize connection.";
            return result;
        }

        const std::wstring wideAcceptType = ToWideString(request.acceptTypes);
        const wchar_t* acceptTypes[] = {wideAcceptType.c_str(), nullptr};
        HINTERNET hRequest = ::WinHttpOpenRequest(hConnect, MethodStr(request.method), urlComponents.lpszUrlPath, NULL, WINHTTP_NO_REFERER,
                                                  acceptTypes, urlComponents.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);

        if (!hRequest)
        {
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            result.errorMessage = "Failed to initialize connection.";
            return result;
        }

        for (const auto& header : request.additionalHeaders)
        {
            const std::wstring headerStr = ToWideString(header.first + ": " + header.second);
            if (!::WinHttpAddRequestHeaders(hRequest, headerStr.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD))
            {
                ::WinHttpCloseHandle(hRequest);
                ::WinHttpCloseHandle(hConnect);
                ::WinHttpCloseHandle(hSession);
                result.errorMessage = "Failed to parse headers.";
                return result;
            }
        }

        if (request.method == Method::POST)
        {
            const std::wstring headers = ToWideString("Content-Type: " + request.contentType);
            if (!::WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD))
            {
                ::WinHttpCloseHandle(hRequest);
                ::WinHttpCloseHandle(hConnect);
                ::WinHttpCloseHandle(hSession);
                result.errorMessage = "Failed to parse headers.";
                return result;
            }
        }

        LPVOID dataToSend = nullptr;
        DWORD dataSize = 0;

        if (request.method == Method::POST)
        {
            dataToSend = (LPVOID)request.data.data();
            dataSize = (DWORD)request.data.size();
        }

        if (!::WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, dataToSend, dataSize, dataSize, 0))
        {
            ::WinHttpCloseHandle(hRequest);
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            result.errorMessage = "Failed to send request.";
            return result;
        }

        if (!::WinHttpReceiveResponse(hRequest, NULL))
        {
            ::WinHttpCloseHandle(hRequest);
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            result.errorMessage = "Failed to receive response.";
            return result;
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if (!::WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                   &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX))
        {
            ::WinHttpCloseHandle(hRequest);
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            result.errorMessage = "Failed to read status code.";
            return result;
        }

        // Read headers
        DWORD headerSize = 0;
        ::WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &headerSize,
                              WINHTTP_NO_HEADER_INDEX);

        std::vector<wchar_t> headerData(headerSize);
        if (::WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS, WINHTTP_HEADER_NAME_BY_INDEX, headerData.data(), &headerSize,
                                     WINHTTP_NO_HEADER_INDEX))
        {
            wchar_t* current = headerData.data();
            while (true)
            {
                if (*current == L'\0')
                {
                    break;
                }
                std::wstring headerData(current);
                current += headerData.length() + 1;

                // Split headerData by ": "
                size_t colonPos = headerData.find(L": ");
                if (colonPos == std::wstring::npos)
                {
                    // Case of status header which we intentionally skip
                    continue;
                }

                std::wstring headerName = headerData.substr(0, colonPos);
                std::wstring headerValue = headerData.substr(colonPos + 2);

                result.headers.emplace_back(ToNarrowString(headerName), ToNarrowString(headerValue));
            }
        }

        // Saves response body to a buffer
        DWORD bytesRead = 0;
        DWORD bytesToRead = 0;
        DWORD newBytes = 0;
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

            result.data.resize(result.data.size() + bytesToRead);
            if (!::WinHttpReadData(hRequest, result.data.data() + bytesRead, bytesToRead, &newBytes))
            {
                assert(0);
                break;
            }
            bytesRead += newBytes;
        } while (newBytes > 0);

        ::WinHttpCloseHandle(hRequest);
        ::WinHttpCloseHandle(hConnect);
        ::WinHttpCloseHandle(hSession);

        result.success = true;
        result.statusCode = (int)statusCode;

        return result;
    }
} // namespace Zibra::NetworkRequest::WinHTTPBackend

#endif
