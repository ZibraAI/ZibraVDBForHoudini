#include "PrecompiledHeader.h"

namespace Zibra
{
    bool IsInteractiveSession()
    {
        std::string appName = HOM().applicationName();

        if (appName == "houdinifx")
        {
            return true;
        }
        return false;
    }
} // namespace Zibra
