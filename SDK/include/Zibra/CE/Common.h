#pragma once

#include <mutex>

#include <Zibra/Math.h>
#include <Zibra/Result.h>
#include <Zibra/Version.h>
#include <Zibra/Stream.h>

namespace Zibra
{
    ZIB_RESULT_DEFINE_CATEGORY(COMPRESSION_ENGINE, 0x100);

    ZIB_RESULT_DEFINE(INVALID_SOURCE, COMPRESSION_ENGINE, 0x0, "Provided source data is not valid .zibravdb sequence.", true);
    ZIB_RESULT_DEFINE(INCOMPATIBLE_SOURCE, COMPRESSION_ENGINE, 0x1,
                      "Specified .zibravdb sequence file version is not compatible with current SDK.", true);
    ZIB_RESULT_DEFINE(CORRUPTED_SOURCE, COMPRESSION_ENGINE, 0x2, ".zibravdb sequence is corrupted.", true);
    ZIB_RESULT_DEFINE(MERGE_VERSION_MISMATCH, COMPRESSION_ENGINE, 0x3,
                      "Specified .zibravdb sequence does not use the current file version and can't be merged.", true);
    ZIB_RESULT_DEFINE(BINARY_FILE_SAVED_AS_TEXT, COMPRESSION_ENGINE, 0x4,
                      "Specified .zibravdb sequence was corrupted by saving it as text.", true);
    ZIB_RESULT_DEFINE(BYTE_RANGE_IS_TOO_SMALL, COMPRESSION_ENGINE, 0x5,
                      "Passed byte range was too small to perform requested operation.", true);

    ZIB_RESULT_DEFINE(COMPRESSION_LICENSE_ERROR, COMPRESSION_ENGINE, 0x100, "ZibraVDB compression requires an active license.", true);
    ZIB_RESULT_DEFINE(DECOMPRESSION_LICENSE_ERROR, COMPRESSION_ENGINE, 0x101, "Decompression of this file requires an active license.", true);
    ZIB_RESULT_DEFINE(DECOMPRESSION_LICENSE_TIER_TOO_LOW, COMPRESSION_ENGINE, 0x102,
                      "Your license does not allow decompression of this effect.", true);
} // namespace Zibra

#ifdef ZIB_DEBUG_BUILD
#define ZIB_DEBUG_ONLY(x) x
#else
#define ZIB_DEBUG_ONLY(x)
#endif

#ifdef ZIB_PROFILE_BUILD
#define ZIB_PROFILE_ONLY(x) x
#else
#define ZIB_PROFILE_ONLY(x)
#endif

#if defined(ZIB_DEBUG_BUILD) || defined(ZIB_PROFILE_BUILD)
#define ZIB_DEBUG_OR_PROFILE_ONLY(x) x
#else
#define ZIB_DEBUG_OR_PROFILE_ONLY(x)
#endif


namespace Zibra::CE
{
    static constexpr int SPARSE_BLOCK_SIZE = 8;
    static constexpr int SPARSE_BLOCK_VOXEL_COUNT = SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE * SPARSE_BLOCK_SIZE;
    static constexpr size_t MAX_CHANNEL_COUNT = 32;
    static constexpr size_t CHUNK_SIZE_IN_BYTES = 128 * 1024 * 1024;

    using ChannelMask = uint32_t;

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
        float voxels[SPARSE_BLOCK_VOXEL_COUNT] = {};
    };

    struct SpatialBlock
    {
        /**
         * Position of spatial block in space.
         * Stored in blocks. To get position in voxels must be multiplied by sparseBlockSize.
         * @range [0; 2^9]
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
         * @range [0; 32]
         */
        uint8_t channelCount;
    };
    inline bool operator==(const SpatialBlock& a, const SpatialBlock& b) noexcept
    {
        const bool coords = a.coords[0] == b.coords[0] && a.coords[1] == b.coords[1] && a.coords[2] == b.coords[2];
        return coords && (a.channelBlocksOffset == b.channelBlocksOffset) && (a.channelMask == b.channelMask) &&
               (a.channelCount == b.channelCount);
    }

    struct ChannelVoxelStatistics
    {
        /**
         * Min voxel value for entire channel gird.
         * @range [-INF; INF]
         */
        float minValue = 0.f;
        /**
         * Max voxel value for entire channel gird.
         * @range [-INF; INF]
         */
        float maxValue = 0.f;
        float meanPositiveValue = 0.f;
        float meanNegativeValue = 0.f;
        /**
         * Total voxels count per channel grid.
         * @range [0; INF]
         */
        uint32_t voxelCount = 0;
    };

    struct ChannelInfo
    {
        /**
         * Channel name.
         */
        const char* name = nullptr;
        /**
         * Axis aligned bounding box. Cannot be 0.
         */
        Math::AABB aabb = {};
        /**
         * Affine transformation matrix. Cannot be 0.
         */
        Math::Transform gridTransform = {};
        ChannelVoxelStatistics voxelStatistics = {};
    };

    struct FrameInfo
    {
        /**
         * Count of channels present in frame.
         * @range [0; MAX_CHANNEL_COUNT]
         */
        uint8_t channelsCount = 0;
        /**
         * Per channel information.
         * @range Length: =channelsCount
         */
        ChannelInfo channels[MAX_CHANNEL_COUNT] = {};
        /**
         * Spatial info count
         * @range [1; INF]
         */
        uint32_t spatialBlockCount = 0;
        /**
         * Channel blocks count
         * @range [1; INF]
         */
        uint32_t channelBlockCount = 0;
        /**
         * Total frame AABB.
         * @range Volume must be greater than 0.
         */
        Math::AABB aabb = {};
    };

    struct PlaybackInfo
    {
        uint32_t framerateNumerator = 30;
        uint32_t framerateDenominator = 1;
        uint32_t sequenceIndexIncrement = 1;
    };

    /**
     * Packs 3 coords into 32bit
     * @param [in] coords uint3 coords
     * @return packed 32-bit value
     */
    inline uint32_t PackCoords(Math::uint3 coords) noexcept
    {
        return coords.x & 1023 | (coords.y & 1023) << 10 | (coords.z & 1023) << 20;
    }
    /**
     * Unpacks 32bit-packed by PackCoords coords
     * @param [in] packedCoords packed 32-bit value
     * @return uint3 coords
     */
    inline Math::uint3 UnpackCoords(uint32_t packedCoords) noexcept
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
} // namespace Zibra::CE
