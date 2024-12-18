#pragma once

#include "ui/MessageBox.h"

namespace Zibra::UI
{

    class LibraryDownloadManager
    {
    public:
        static void DownloadLibrary();

    private:
        static void MessageBoxCallback(MessageBox::Result result);
    };

} // namespace Zibra::UI
