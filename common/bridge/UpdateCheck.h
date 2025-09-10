#pragma once

namespace Zibra::UpdateCheck
{
    enum class Status
    {
        Latest,
        UpdateAvailable,
        UpdateCheckFailed,
        NotInstalled,
        Count
    };

    Status Run() noexcept;
} // namespace Zibra::UpdateCheck
