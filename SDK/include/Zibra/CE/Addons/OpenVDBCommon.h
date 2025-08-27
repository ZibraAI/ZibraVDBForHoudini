#pragma once

#include <Zibra/CE/Common.h>
#include <Zibra/Math.h>

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
} // namespace Zibra::CE::Addons::OpenVDBUtils
