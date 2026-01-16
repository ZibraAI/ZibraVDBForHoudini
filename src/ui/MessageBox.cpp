#include "PrecompiledHeader.h"

#include "MessageBox.h"

#include <SI/AP_Interface.h>
#include <UI/UI_Value.h>

namespace Zibra::UI
{

    class MessageBoxDialog : public AP_Interface
    {
    public:
        MessageBoxDialog(const char* uiFile);

        const char* className() const final
        {
            static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&MessageBoxDialog::className)>);

            return "MessageBoxDialog";
        }

        void Show(const std::string& message, void (*callback)(MessageBox::Result));

    private:
        static constexpr size_t MAX_CHARS_PER_LINE = 95;
        static constexpr size_t LINE_COUNT = 4;

        bool ParseUIFile();
        void HandleClick(UI_Event* event);
        std::array<std::string, LINE_COUNT> SplitMessage(const std::string& message);

        const char* m_UIFile;
        bool m_IsParsed = false;
        void (*m_Callback)(MessageBox::Result) = nullptr;
    };

    MessageBoxDialog::MessageBoxDialog(const char* uiFile)
        : m_UIFile(uiFile)
    {
    }

    void MessageBoxDialog::Show(const std::string& message, void (*callback)(MessageBox::Result))
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&MessageBoxDialog::Show)>);

        if (!ParseUIFile())
        {
            return;
        }

        m_Callback = callback;

        auto lines = SplitMessage(message);

        (*getValueSymbol("message1.val")) = lines[0].c_str();
        (*getValueSymbol("message2.val")) = lines[1].c_str();
        (*getValueSymbol("message3.val")) = lines[2].c_str();
        (*getValueSymbol("message4.val")) = lines[3].c_str();

        (*getValueSymbol("dialog.val")) = true;
        getValueSymbol("dialog.val")->changed(this);
    }

    bool MessageBoxDialog::ParseUIFile()
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&MessageBoxDialog::ParseUIFile)>);

        if (m_IsParsed)
        {
            return true;
        }

        if (!readUIFile(m_UIFile))
        {
            return false;
        }

        getValueSymbol("result.val")->addInterest(this, static_cast<UI_EventMethod>(&MessageBoxDialog::HandleClick));

        m_IsParsed = true;

        return true;
    }

    void MessageBoxDialog::HandleClick(UI_Event* event)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&MessageBoxDialog::HandleClick)>);

        (*getValueSymbol("dialog.val")) = false;
        getValueSymbol("dialog.val")->changed(this);

        int32 result = (*getValueSymbol("result.val"));
        if (m_Callback)
        {
            m_Callback(static_cast<MessageBox::Result>(result));
        }
    }

    std::array<std::string, MessageBoxDialog::LINE_COUNT> MessageBoxDialog::SplitMessage(const std::string& message)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&MessageBoxDialog::SplitMessage)>);

        std::array<std::string, LINE_COUNT> result;

        size_t currentOffset = 0;
        size_t totalLength = message.size();

        for (std::string& line : result)
        {
            if (currentOffset == totalLength)
            {
                break;
            }

            size_t afterSpaces = message.find_first_not_of(' ', currentOffset);
            if (afterSpaces != std::string::npos)
            {
                currentOffset = afterSpaces;
            }

            if (currentOffset + MAX_CHARS_PER_LINE >= totalLength)
            {
                line = message.substr(currentOffset);
                break;
            }

            size_t nextOffset = message.find_last_of(' ', currentOffset + MAX_CHARS_PER_LINE);
            if (nextOffset == std::string::npos || nextOffset <= currentOffset)
            {
                nextOffset = std::min(currentOffset + MAX_CHARS_PER_LINE, totalLength);
            }
            line = message.substr(currentOffset, nextOffset - currentOffset);
            currentOffset = nextOffset;
        }

        return result;
    }

    void MessageBox::Show(Type type, const std::string& message, void (*callback)(MessageBox::Result) /*= nullptr*/)
    {
        static_assert(Zibra::is_all_func_arguments_acceptable_v<decltype(&MessageBoxDialog::Show)>);

        static MessageBoxDialog okDialog("ZibraVDBMessageBoxOK.ui");
        static MessageBoxDialog yesNoDialog("ZibraVDBMessageBoxYesNo.ui");
        switch (type)
        {
        case Type::OK:
            okDialog.Show(message, callback);
            break;
        case Type::YesNo:
            yesNoDialog.Show(message, callback);
            break;
        }
    }

} // namespace Zibra::UI
