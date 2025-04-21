#ifndef __ZCE_DECOMPRESSION_SHADER_TYPES_HLSLI_
#define __ZCE_DECOMPRESSION_SHADER_TYPES_HLSLI_

struct ZCEDecompressionPackedSpatialBlock {
    uint packedCoords;
    uint channelBlocksOffset;
    uint channelMask;
};

#endif // __ZCE_DECOMPRESSION_SHADER_TYPES_HLSLI_