#include "Common.h"

#include "Application.h"

namespace
{
void ShowInitializationException(const std::exception& exception) noexcept
{
    wchar_t details[768]{};
    const char* what = exception.what();
    if (what != nullptr && what[0] != '\0')
    {
        const int length = MultiByteToWideChar(CP_UTF8,
                                               MB_ERR_INVALID_CHARS,
                                               what,
                                               -1,
                                               details,
                                               static_cast<int>(std::size(details)));
        if (length == 0)
        {
            MultiByteToWideChar(CP_ACP,
                                0,
                                what,
                                -1,
                                details,
                                static_cast<int>(std::size(details)));
        }
    }

    wchar_t message[1024]{};
    swprintf_s(message,
               std::size(message),
               L"An unexpected error occurred while initializing the application.\n\n"
               L"Details: %ls",
               details[0] != L'\0' ? details : L"Unavailable");
    MessageBoxW(nullptr, message, L"DisplayColorTester", MB_OK | MB_ICONERROR);
}
}

int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, wchar_t*, int showCommand)
{
    try
    {
        DisplayColorTester::Application application(hInstance);
        return application.Run(showCommand);
    }
    catch (const std::exception& exception)
    {
        ShowInitializationException(exception);
        return 1;
    }
    catch (...)
    {
        MessageBoxW(nullptr,
                    L"An unknown error occurred while initializing the application.",
                    L"DisplayColorTester",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
}
