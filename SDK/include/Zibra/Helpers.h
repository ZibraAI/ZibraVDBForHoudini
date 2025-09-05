#pragma once

#define ZIB_INTERNAL_CONCAT_HELPER(a, b) a##b
#define ZIB_CONCAT(a, b) ZIB_INTERNAL_CONCAT_HELPER(a, b)
