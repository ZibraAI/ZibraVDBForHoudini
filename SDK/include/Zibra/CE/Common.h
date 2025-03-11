#pragma once

#include <Zibra/Foundation.h>

#define ZCE_CONCAT_HELPER(A, B) A##B
#define ZCE_PFN(name) ZCE_CONCAT_HELPER(PFN_, name)

namespace Zibra::CE {
    static constexpr int SPARSE_BLOCK_SIZE = 8;
    static constexpr int SPARSE_BLOCK_VOXEL_COUNT = SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE;
    static constexpr size_t ZCE_MAX_CHANNEL_COUNT = 8;

    enum ReturnCode
    {
        // Successfully finished operation
        ZCE_SUCCESS = 0,
        // Unexpected error
        ZCE_ERROR = 100,
        // Fatal error
        ZCE_FATAL_ERROR = 110,

        ZCE_ERROR_NOT_INITIALIZED = 200,
        ZCE_ERROR_ALREADY_INITIALIZED = 201,

        ZCE_ERROR_INVALID_USAGE = 300,
        ZCE_ERROR_INVALID_ARGUMENTS = 301,
        ZCE_ERROR_NOT_IMPLEMENTED = 310,
        ZCE_ERROR_NOT_SUPPORTED = 311,

        ZCE_ERROR_NOT_FOUND = 400,
        // Out of CPU memory
        ZCE_ERROR_OUT_OF_CPU_MEMORY = 410,
        // Out of GPU memory
        ZCE_ERROR_OUT_OF_GPU_MEMORY = 411,
        // Time out
        ZCE_ERROR_TIME_OUT = 430,

    };

    struct MetadataEntry
    {
        const char* key = nullptr;
        const char* value = nullptr;
    };
}
