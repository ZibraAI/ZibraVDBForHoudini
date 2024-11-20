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
            return "MessageBoxDialog";
        }

        void Show(const std::string& message, void (*callback)(MessageBox::Result));

    private:
        bool ParseUIFile();
        void HandleClick(UI_Event* event);

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
        if (!ParseUIFile())
        {
            return;
        }

        m_Callback = callback;

        (*getValueSymbol("message.val")) = message.c_str();
        volatile const char* test2 = (*getValueSymbol("message.val"));

        (*getValueSymbol("dialog.val")) = true;
        getValueSymbol("dialog.val")->changed(this);
    }

    bool MessageBoxDialog::ParseUIFile()
    {
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
        (*getValueSymbol("dialog.val")) = false;
        getValueSymbol("dialog.val")->changed(this);

        int32 result = (*getValueSymbol("result.val"));
        if (m_Callback)
        {
            m_Callback(static_cast<MessageBox::Result>(result));
        }
    }

    void MessageBox::Show(Type type, const std::string& message, void (*callback)(MessageBox::Result) /*= nullptr*/)
    {
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
