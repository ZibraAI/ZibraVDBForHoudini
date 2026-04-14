#include "PrecompiledHeader.h"

#if defined(_DEBUG) && ZIB_NO_DEBUG_PYTHON
#error Debug Python library was not found, can't build debug configuration. Please install appropriate version of python with debug library and re-configure project to build Debug configuration.
#endif
