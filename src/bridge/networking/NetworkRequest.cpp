#include "PrecompiledHeader.h"

#include "NetworkRequest.h"

#if ZIB_PLATFORM_WIN
#pragma comment(lib, "winhttp.lib")
#else
// TODO cross-platform support
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

    bool DownloadFile(const std::string& url, const std::string& filepath)
    {
        CreateFolderStructure(filepath);

        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open())
        {
            return false;
        }

#if ZIB_PLATFORM_WIN
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

        HINTERNET hSession = ::WinHttpOpen(L"ZibraVDB for Houdini", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                           WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_SECURE_DEFAULTS);

        if (!hSession)
        {
            assert(0);
            return false;
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
            return false;
        }

        HINTERNET hConnect = ::WinHttpConnect(hSession, urlComponents.lpszHostName, INTERNET_DEFAULT_HTTPS_PORT, 0);

        if (!hConnect)
        {
            ::WinHttpCloseHandle(hSession);
            return false;
        }

        const wchar_t* acceptTypes[] = {L"*/*", nullptr};
        HINTERNET hRequest =
            ::WinHttpOpenRequest(hConnect, L"GET", urlComponents.lpszUrlPath, NULL, WINHTTP_NO_REFERER, acceptTypes, WINHTTP_FLAG_SECURE);

        if (!hRequest)
        {
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            return false;
        }

        if (!::WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0))
        {
            ::WinHttpCloseHandle(hRequest);
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            return false;
        }

        if (!::WinHttpReceiveResponse(hRequest, NULL))
        {
            ::WinHttpCloseHandle(hRequest);
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            return false;
        }

        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        if (!::WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                   &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX))
        {
            ::WinHttpCloseHandle(hRequest);
            ::WinHttpCloseHandle(hConnect);
            ::WinHttpCloseHandle(hSession);
            return false;
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
                return false;
            }

            file.write(responseBuffer.data(), bytesRead);
            if (file.fail())
            {
                ::WinHttpCloseHandle(hRequest);
                ::WinHttpCloseHandle(hConnect);
                ::WinHttpCloseHandle(hSession);
                return false;
            }

        } while (bytesRead > 0);

        ::WinHttpCloseHandle(hRequest);
        ::WinHttpCloseHandle(hConnect);
        ::WinHttpCloseHandle(hSession);
#else
        // TODO cross-platform support
        return false;
#endif

        file.close();
        if (file.fail())
        {
            return false;
        }

        return true;
    }

} // namespace Zibra::NetworkRequest
