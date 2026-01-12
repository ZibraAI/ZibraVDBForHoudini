#pragma once

namespace Zibra::Analytics
{
    struct OSInfo
    {
        std::string Name;
        std::string Version;
    };
    OSInfo GetOSInfo();
} // namespace Zibra::Analytics