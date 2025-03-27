#pragma once

#include <string>

namespace Zibra
{

    class TrialManager
    {
    public:
        static bool RequestTrialCompression();
        static void CheckoutTrialCompression();
        static int GetRemainingTrialCompressions();
    };

} // namespace Zibra
