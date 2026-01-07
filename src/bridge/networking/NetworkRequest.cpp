#include "PrecompiledHeader.h"

#include "NetworkRequest.h"

#ifdef ZIB_NETWORK_REQUEST_BACKEND_WINHTTP
#include "WinHTTPBackend.h"
#endif
#ifdef ZIB_NETWORK_REQUEST_BACKEND_CURL
#include "CURLBackend.h"
#endif

namespace Zibra::NetworkRequest
{

    Response Perform(const Request& request)
    {
#ifdef ZIB_NETWORK_REQUEST_BACKEND_WINHTTP
        return WinHTTPBackend::Perform(request);
#elif defined(ZIB_NETWORK_REQUEST_BACKEND_CURL)
        return CURLBackend::Perform(request);
#else
#error "No network request backend defined."
#endif
    }

} // namespace Zibra::NetworkRequest
