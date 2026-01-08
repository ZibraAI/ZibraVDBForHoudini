#pragma once

#include "NetworkRequest.h"

#ifdef ZIB_NETWORK_REQUEST_BACKEND_WINHTTP
namespace Zibra::NetworkRequest::WinHTTPBackend
{
    Response Perform(const Request& request);
}
#endif
