#ifndef __ZCE_DECOMPRESSION_SHADER_TYPES_HLSLI_
#define __ZCE_DECOMPRESSION_SHADER_TYPES_HLSLI_

#ifdef __cplusplus
#include <Zibra/Math.h>
#endif // __cplusplus


#ifdef __cplusplus
namespace Zibra::CE::Decompression::Shaders {
    using namespace Math;
    using uint = uint32_t;
#endif // __cplusplus

struct ZCEDecompressionPackedSpatialBlock {
    uint packedCoords;
    uint channelBlocksOffset;
    uint channelMask;
};

struct ZCEDecompressionPackedChannelBlockInfo {
    uint packed2flaot16;
};

#ifdef __cplusplus
using PackedChannelBlockInfo = ZCEDecompressionPackedChannelBlockInfo;
using PackedSpatialBlock = ZCEDecompressionPackedSpatialBlock;
} // namespace Zibra::CE::Decompression::Shaders
#endif // __cplusplus

#endif // __ZCE_DECOMPRESSION_SHADER_TYPES_HLSLI_
