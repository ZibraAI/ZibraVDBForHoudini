#include "PrecompiledHeader.h"

#include "MessageBox.h"

namespace Zibra::UI
{

    MessageBox::Result MessageBox::Show(Type type, const std::string& message, const std::string& title)
    {
#if ZIB_PLATFORM_WIN
        UINT flags = 0;
        switch (type)
        {
        case Type::OK:
            flags = MB_OK;
            break;
        case Type::YesNo:
            flags = MB_YESNO;
            break;
        default:
            assert(0);
            break;
        }

        int winResult = ::MessageBoxA(0, message.c_str(), title.c_str(), flags);
        Result result = Result::OK;

        switch (winResult)
        {
        case IDOK:
            result = Result::OK;
            break;
        case IDYES:
            result = Result::Yes;
            break;
        case IDNO:
            result = Result::No;
            break;
        default:
            assert(0);
            break;
        }

        return result;
#else
        // TODO cross-platform support
        return 0;
#endif
    }

} // namespace Zibra::UI
