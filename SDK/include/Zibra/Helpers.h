#pragma once

#include <cstddef>

#define ZIB_INTERNAL_CONCAT_HELPER(a, b) a##b
#define ZIB_INTERNAL_STRINGIFY_HELPER(x) #x

#define ZIB_CONCAT(a, b) ZIB_INTERNAL_CONCAT_HELPER(a, b)
#define ZIB_STRINGIFY(x) ZIB_INTERNAL_STRINGIFY_HELPER(x)

template <typename T, size_t size>
constexpr size_t ZIB_ARRAY_SIZE(T (&)[size])
{
	return size;
}
