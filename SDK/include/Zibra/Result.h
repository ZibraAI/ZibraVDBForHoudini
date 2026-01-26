#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

struct ZibraNamespaceFlag
{
};

namespace Zibra
{
    struct ZibraNamespaceFlag
    {
    };

    using Result = uint32_t;

#define ZIB_RESULT_ERROR_FLAG (uint32_t(0x80000000))
#define ZIB_RESULT_CATEGORY_MASK (uint32_t(0x7FFF0000))

#define ZIB_RESULT_DEFINE_CATEGORY(category, value)                          \
    constexpr uint32_t RESULT_CATEGORY_##category = uint32_t((value) << 16); \
    static_assert((value) < (1 << 15), "Value needs to fit into 15 bits");   \
    static_assert(std::is_same_v<ZibraNamespaceFlag, ::Zibra::ZibraNamespaceFlag>, "Result macros must only be used in Zibra namespace");
#define ZIB_RESULT_DEFINE(name, category, value, description, isError)                                                             \
    constexpr uint32_t RESULT_##name = uint32_t((RESULT_CATEGORY_##category) | (value) | ((isError) ? ZIB_RESULT_ERROR_FLAG : 0)); \
    constexpr char RESULT_##name##_DESCRIPTION[] = description;                                                                    \
    static_assert((value) < (1 << 16), "Value needs to fit into 16 bits");                                                         \
    static_assert(std::is_same_v<ZibraNamespaceFlag, ::Zibra::ZibraNamespaceFlag>, "Result macros must only be used in Zibra namespace");

#define ZIB_SUCCEEDED(return_code) ((return_code) < ZIB_RESULT_ERROR_FLAG)
#define ZIB_FAILED(return_code) ((return_code) >= ZIB_RESULT_ERROR_FLAG)
#define ZIB_RESULT_CATEGORY(return_code) ((return_code) & ZIB_RESULT_CATEGORY_MASK)

    ZIB_RESULT_DEFINE_CATEGORY(GENERIC, 0);

    ZIB_RESULT_DEFINE(SUCCESS, GENERIC, 0x0, "Operation succeeded.", false);
    ZIB_RESULT_DEFINE(TIMEOUT, GENERIC, 0x1, "Operation timed out.", false);

    ZIB_RESULT_DEFINE(ERROR, GENERIC, 0x0, "General failure.", true);

    ZIB_RESULT_DEFINE(INVALID_ARGUMENTS, GENERIC, 0x100, "One or multiple function arguments were not valid.", true);
    ZIB_RESULT_DEFINE(INVALID_USAGE, GENERIC, 0x101, "Operation cannot be perfored due to API misuse.", true);
    ZIB_RESULT_DEFINE(UNSUPPORTED, GENERIC, 0x102, "Operation not supported on current platoform/in current built/etc.", true);
    ZIB_RESULT_DEFINE(UNINITIALIZED, GENERIC, 0x103, "Opearion requires initialization, but it was not performed successfully.", true);
    ZIB_RESULT_DEFINE(ALREADY_INITIALIZED, GENERIC, 0x104, "Initalization was already performed.", true);
    ZIB_RESULT_DEFINE(NOT_FOUND, GENERIC, 0x105, "Requested item was not found.", true);
    ZIB_RESULT_DEFINE(ALREADY_PRESENT, GENERIC, 0x106, "Submitted item is already present.", true);
    ZIB_RESULT_DEFINE(OUT_OF_MEMORY, GENERIC, 0x107, "Out of memory.", true);
    ZIB_RESULT_DEFINE(OUT_OF_BOUNDS, GENERIC, 0x108, "Specified position is out of bounds.", true);
    ZIB_RESULT_DEFINE(IO_ERROR, GENERIC, 0x109, "I/O Error.", true);

    ZIB_RESULT_DEFINE(UNIMPLEMENTED, GENERIC, 0x1000, "Unimplemented.", true);
    ZIB_RESULT_DEFINE(UNEXPECTED_ERROR, GENERIC, 0x1001, "Unexpected error. If you ever get it, contact ZibraAI.", true);

} // namespace Zibra
