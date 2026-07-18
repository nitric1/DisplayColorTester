#include "Common.h"

#include "Application.h"

int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, wchar_t*, int showCommand)
{
    try
    {
        DisplayColorTester::Application application(hInstance);
        return application.Run(showCommand);
    }
    catch (const std::exception&)
    {
        MessageBoxW(nullptr,
                    L"프로그램을 초기화하는 중 예기치 않은 오류가 발생했습니다.",
                    L"DisplayColorTester",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    catch (...)
    {
        MessageBoxW(nullptr,
                    L"프로그램을 초기화하는 중 알 수 없는 오류가 발생했습니다.",
                    L"DisplayColorTester",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
}
