#pragma once

#include <Zibra/Foundation.h>
#include <Zibra/Math3D.h>
#include "Addons/ZCEDecompressionShaderTypes.hlsli"

#define ZCE_CONCAT_HELPER(A, B) A##B
#define ZCE_PFN(name) ZCE_CONCAT_HELPER(PFN_, name)

namespace Zibra::CE
{
    static constexpr int SPARSE_BLOCK_SIZE = 8;
    static constexpr int SPARSE_BLOCK_VOXEL_COUNT = SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE;
    static constexpr size_t MAX_CHANNEL_COUNT = 8;

    using ChannelMask = uint32_t;

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

    struct ChannelBlock
    {
        /**
         * Dense voxels container.
         * @range [-INF; INF]
         */
        float voxels[SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE] = {};
    };

    struct SpatialBlockInfo
    {
        /**
         * Position of spatial block in space.
         * Stored in blocks. To get position in voxels must be multiplied by sparseBlockSize.
         * @range [0; INF]
         */
        int32_t coords[3];

        /**
         * Offset to first channel block of this spatial block.
         * @range [0; INF]
         */
        uint32_t channelBlocksOffset;

        /**
         * Mask of active channels in this spatial block.
         * @range [0x0; 0xFFFFFF]
         */
        ChannelMask channelMask;

        /**
         * Count of active channels in this spatial block.
         * @range [0; 8]
         */
        uint32_t channelCount;
    };
    inline bool operator==(const SpatialBlockInfo& a, const SpatialBlockInfo& b) noexcept
    {
        const bool coords = a.coords[0] == b.coords[0] && a.coords[1] == b.coords[1] && a.coords[2] == b.coords[2];
        return coords && (a.channelBlocksOffset == b.channelBlocksOffset) && (a.channelMask == b.channelMask) &&
               (a.channelCount == b.channelCount);
    }

    /**
     * Packs 3 coords into 32bit
     * @param [in] coords uint3 coords
     * @return packed 32-bit value
     */
    inline uint32_t PackCoords(Math3D::uint3 coords) noexcept
    {
        return coords.x & 1023 | (coords.y & 1023) << 10 | (coords.z & 1023) << 20;
    }
    /**
     * Unpacks 32bit-packed by PackCoords coords
     * @param [in] packedCoords packed 32-bit value
     * @return uint3 coords
     */
    inline Math3D::uint3 UnpackCoords(uint32_t packedCoords) noexcept
    {
        return {packedCoords & 1023, (packedCoords >> 10) & 1023, (packedCoords >> 20) & 1023};
    }

    inline int ModBlockSize(int value) noexcept
    {
        return value & (SPARSE_BLOCK_SIZE - 1);
    }

    inline int FloorToBlockSize(int value) noexcept
    {
        return value - ModBlockSize(value);
    }

    inline int CeilToBlockSize(int value) noexcept
    {
        return FloorToBlockSize(value + SPARSE_BLOCK_SIZE - 1);
    }

    template <class T>
    constexpr void NormalizeRange(T* data, const size_t size, const T minValue, const T maxValue, const T newMinValue,
                                         const T newMaxValue) noexcept
    {
        for (size_t i = 0; i < size; i++)
        {
            data[i] = (data[i] - minValue) / (maxValue - minValue) * (newMaxValue - newMinValue) + newMinValue;
        }
    };

    inline uint32_t CountBits(uint32_t x) noexcept
    {
        uint32_t count = 0;
        while (x)
        {
            x &= (x - 1); // Clears the lowest set bit
            count++;
        }
        return count;
    }

    inline float Float16ToFloat32(uint16_t float16Value) noexcept
    {
        uint32_t t1, t2, t3;

        t1 = float16Value & 0x7fffu; // Non-sign bits
        t2 = float16Value & 0x8000u; // Sign bit
        t3 = float16Value & 0x7c00u; // Exponent

        t1 <<= 13u; // Align mantissa on MSB
        t2 <<= 16u; // Shift sign bit into position

        t1 += 0x38000000; // Adjust bias

        t1 = (t3 == 0 ? 0 : t1); // Denormals-as-zero

        t1 |= t2; // Re-insert sign bit

        uint32_t float32_value = t1;

        return *(reinterpret_cast<float*>(&float32_value));
    }

    inline SpatialBlockInfo UnpackPackedSpatialBlock(const ZCEDecompressionPackedSpatialBlock& packedBlock) noexcept {
        SpatialBlockInfo result{};
        result.coords[0] = static_cast<int32_t>((packedBlock.packedCoords >> 0u) & 1023u);
        result.coords[1] = static_cast<int32_t>((packedBlock.packedCoords >> 10u) & 1023u);
        result.coords[2] = static_cast<int32_t>((packedBlock.packedCoords >> 20u) & 1023u);
        result.channelBlocksOffset = packedBlock.channelBlocksOffset;
        result.channelMask = packedBlock.channelMask;
        result.channelCount = CountBits(packedBlock.channelMask);
        return result;
    }
} // namespace Zibra::CE
