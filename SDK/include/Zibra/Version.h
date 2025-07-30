#pragma once

#include <cstdint>
#include <string>

namespace Zibra
{
    struct Version
    {
        uint32_t major;
        uint32_t minor;
        uint32_t patch;
        uint32_t build;

        [[nodiscard]] constexpr bool operator==(const Version& other)
        {
            return major == other.major && minor == other.minor && patch == other.patch && build == other.build;
        }

        [[nodiscard]] constexpr bool operator!=(const Version& other)
        {
            return major != other.major || minor != other.minor || patch != other.patch || build != other.build;
        }

        [[nodiscard]] constexpr bool operator>(const Version& other)
        {
            if (major != other.major)
                return major > other.major;
            if (minor != other.minor)
                return minor > other.minor;
            if (patch != other.patch)
                return patch > other.patch;
            if (build != other.build)
                return build > other.build;
            return false;
        }

        [[nodiscard]] constexpr bool operator<(const Version& other)
        {
            if (major != other.major)
                return major < other.major;
            if (minor != other.minor)
                return minor < other.minor;
            if (patch != other.patch)
                return patch < other.patch;
            if (build != other.build)
                return build < other.build;
            return false;
        }

        [[nodiscard]] constexpr bool operator>=(const Version& other)
        {
            return *this == other || *this > other;
        }

        [[nodiscard]] constexpr bool operator<=(const Version& other)
        {
            return *this == other || *this < other;
        }

        [[nodiscard]] constexpr bool IsCompatibleVersion(const Version& expectedVersion)
        {
            return *this >= expectedVersion && *this < Version{expectedVersion.major + 1, 0, 0, 0};
        }

        [[nodiscard]] std::string ToString()
        {
            return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch) + "." + std::to_string(build);
        }
    };
} // namespace Zibra
