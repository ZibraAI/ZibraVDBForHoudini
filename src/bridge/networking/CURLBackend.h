#pragma once

#include "NetworkRequest.h"

#ifdef ZIB_NETWORK_REQUEST_BACKEND_CURL
namespace Zibra::NetworkRequest::CURLBackend
{
    Response Perform(const Request& request);
}
#endif
