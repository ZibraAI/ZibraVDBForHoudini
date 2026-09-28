#include "PrecompiledHeader.h"

namespace Zibra
{
    bool IsInteractiveSession()
    {
        std::string appName = HOM().applicationName();

        return appName == "houdinifx";
    }
} // namespace Zibra
