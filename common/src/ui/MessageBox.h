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

        static void Show(Type type, const std::string& message, void (*callback)(MessageBox::Result) = nullptr);
    };
} // namespace Zibra::UI
