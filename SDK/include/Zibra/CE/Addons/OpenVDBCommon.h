#pragma once

#include <Zibra/Math3D.h>
#include <Zibra/CE/Common.h>

namespace Zibra::CE::Addons::OpenVDBUtils
{
    enum class GridVoxelType
    {
        Float1,
        Float3,
    };

    struct VDBGridDesc
    {
        const char* gridName = "UnnamedVDB";
        GridVoxelType voxelType = GridVoxelType::Float1;
        const char* chSource[4] = {nullptr, nullptr, nullptr, nullptr};
    };

    struct EncodingMetadata
    {
        int32_t offsetX = 0;
        int32_t offsetY = 0;
        int32_t offsetZ = 0;
    };
} // namespace Zibra::CE::Addons::OpenVDBUtils
