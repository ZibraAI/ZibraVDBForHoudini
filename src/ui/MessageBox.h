#pragma once

namespace Zibra::UI
{
    class MessageBox
    {
    public:
        enum class Type
        {
            OK,
            YesNo
        };

        enum class Result
        {
            OK = 0,
            Yes = 0,
            No = 1
        };

        static Result Show(Type type, const std::string& message, const std::string& title);
    };
} // namespace Zibra::UI
