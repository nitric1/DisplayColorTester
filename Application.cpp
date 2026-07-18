#include "Common.h"

#include "Application.h"

#include "Resource.h"
#include "TestSession.h"

namespace DisplayColorTester
{
namespace
{
constexpr wchar_t kMainWindowClassName[] = L"DisplayColorTester.MainWindow";
constexpr wchar_t kApplicationTitle[] = L"DisplayColorTester";

std::wstring FormatSystemError(DWORD errorCode)
{
    if (errorCode == ERROR_SUCCESS)
    {
        return L"알 수 없는 오류입니다.";
    }

    wchar_t buffer[512]{};
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                        nullptr,
                                        errorCode,
                                        0,
                                        buffer,
                                        static_cast<DWORD>(std::size(buffer)),
                                        nullptr);
    if (length == 0)
    {
        return L"오류 코드: " + std::to_wstring(errorCode);
    }

    std::wstring message(buffer, length);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n'))
    {
        message.pop_back();
    }
    return message;
}

int ScaleForDpi(int value, unsigned dpi) noexcept
{
    return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}
}

Application::Application(HINSTANCE instance) noexcept : instance_(instance)
{
}

Application::~Application()
{
    CloseTestSession();
    if (mainFont_ != nullptr)
    {
        DeleteObject(mainFont_);
        mainFont_ = nullptr;
    }
}

int Application::Run(int showCommand)
{
    if (!RegisterWindowClasses() || !CreateMainWindow(showCommand))
    {
        const std::wstring details = FormatSystemError(GetLastError());
        MessageBoxW(nullptr,
                    (L"프로그램 창을 만들 수 없습니다.\n\n" + details).c_str(),
                    kApplicationTitle,
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    MSG message{};
    for (;;)
    {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == 0)
        {
            return static_cast<int>(message.wParam);
        }
        if (result == -1)
        {
            return 1;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

bool Application::RegisterWindowClasses() const noexcept
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &Application::MainWindowProc;
    windowClass.hInstance = instance_;
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kMainWindowClassName;
    windowClass.hIconSm = windowClass.hIcon;

    if (RegisterClassExW(&windowClass) == 0)
    {
        return false;
    }
    return TestSession::RegisterWindowClass(instance_);
}

bool Application::CreateMainWindow(int showCommand)
{
    const unsigned dpi = GetDpiForSystem();
    RECT windowRect{0, 0, ScaleForDpi(520, dpi), ScaleForDpi(300, dpi)};
    if (!AdjustWindowRectExForDpi(&windowRect, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi))
    {
        return false;
    }

    mainWindow_ = CreateWindowExW(0,
                                  kMainWindowClassName,
                                  kApplicationTitle,
                                  WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  windowRect.right - windowRect.left,
                                  windowRect.bottom - windowRect.top,
                                  nullptr,
                                  nullptr,
                                  instance_,
                                  this);
    if (mainWindow_ == nullptr)
    {
        return false;
    }

    ShowWindow(mainWindow_, showCommand);
    UpdateWindow(mainWindow_);
    return true;
}

bool Application::CreateButtons()
{
    struct ButtonDefinition
    {
        int id;
        const wchar_t* text;
        bool enabled;
    };

    constexpr std::array<ButtonDefinition, 4> definitions{{
        {IDC_GAMUT_SRGB, L"sRGB", true},
        {IDC_GAMUT_DISPLAY_P3, L"Display-P3 (P3-D65)", true},
        {IDC_GAMUT_ADOBE_RGB, L"Adobe RGB", false},
        {IDC_GAMUT_BT2020, L"BT.2020", false},
    }};

    for (size_t index = 0; index < definitions.size(); ++index)
    {
        const auto& definition = definitions[index];
        buttons_[index] = CreateWindowExW(0,
                                          L"BUTTON",
                                          definition.text,
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                          0,
                                          0,
                                          0,
                                          0,
                                          mainWindow_,
                                          reinterpret_cast<HMENU>(static_cast<intptr_t>(definition.id)),
                                          instance_,
                                          nullptr);
        if (buttons_[index] == nullptr)
        {
            return false;
        }
        EnableWindow(buttons_[index], definition.enabled ? TRUE : FALSE);
    }

    RecreateMainFont();
    LayoutButtons();
    return true;
}

void Application::LayoutButtons() const noexcept
{
    if (mainWindow_ == nullptr || buttons_[0] == nullptr)
    {
        return;
    }

    RECT clientRect{};
    GetClientRect(mainWindow_, &clientRect);
    const unsigned dpi = GetDpiForWindow(mainWindow_);
    const int padding = ScaleForDpi(24, dpi);
    const int gap = ScaleForDpi(12, dpi);
    const int buttonHeight = ScaleForDpi(58, dpi);
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    const int availableWidth = (std::max)(0, clientWidth - (padding * 2) - gap);
    const int buttonWidth = availableWidth / 2;
    const int gridHeight = (buttonHeight * 2) + gap;
    const int top = (std::max)(padding, (clientHeight - gridHeight) / 2);

    for (size_t index = 0; index < buttons_.size(); ++index)
    {
        const int column = static_cast<int>(index % 2);
        const int row = static_cast<int>(index / 2);
        const int left = padding + column * (buttonWidth + gap);
        const int y = top + row * (buttonHeight + gap);

        SetWindowPos(buttons_[index],
                     nullptr,
                     left,
                     y,
                     buttonWidth,
                     buttonHeight,
                     SWP_NOACTIVATE | SWP_NOZORDER);
    }
}

void Application::RecreateMainFont()
{
    if (mainWindow_ == nullptr)
    {
        return;
    }

    const unsigned dpi = GetDpiForWindow(mainWindow_);
    HFONT newFont = CreateFontW(-ScaleForDpi(10, dpi),
                                0,
                                0,
                                0,
                                FW_NORMAL,
                                FALSE,
                                FALSE,
                                FALSE,
                                DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE,
                                L"Segoe UI");
    if (newFont != nullptr)
    {
        if (mainFont_ != nullptr)
        {
            DeleteObject(mainFont_);
        }
        mainFont_ = newFont;
    }

    HFONT fontToUse = mainFont_;
    if (fontToUse == nullptr)
    {
        fontToUse = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    for (HWND button : buttons_)
    {
        if (button != nullptr)
        {
            SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(fontToUse), TRUE);
        }
    }
}

void Application::StartTest(ColorGamut gamut, size_t buttonIndex)
{
    if (testSession_ != nullptr)
    {
        return;
    }

    auto session = std::make_unique<TestSession>(instance_, mainWindow_, gamut);
    ShowWindow(mainWindow_, SW_HIDE);
    if (!session->Start())
    {
        const std::wstring details = FormatSystemError(session->LastErrorCode());
        ShowWindow(mainWindow_, SW_SHOW);
        SetForegroundWindow(mainWindow_);
        MessageBoxW(mainWindow_,
                    (L"모니터 테스트 창을 만들 수 없습니다.\n\n" + details).c_str(),
                    kApplicationTitle,
                    MB_OK | MB_ICONERROR);
        return;
    }

    lastStartedButtonIndex_ = buttonIndex;
    testSession_ = std::move(session);
}

void Application::FinishTestSession(bool displayConfigurationChanged)
{
    testSession_.reset();
    if (mainWindow_ == nullptr)
    {
        return;
    }

    ShowWindow(mainWindow_, SW_SHOW);
    SetForegroundWindow(mainWindow_);
    SetFocus(buttons_[lastStartedButtonIndex_]);

    if (displayConfigurationChanged)
    {
        MessageBoxW(mainWindow_,
                    L"디스플레이 구성이 변경되어 테스트를 종료했습니다.\n다시 시작하면 현재 모니터 구성을 사용합니다.",
                    kApplicationTitle,
                    MB_OK | MB_ICONINFORMATION);
    }
}

void Application::CloseTestSession() noexcept
{
    if (testSession_ != nullptr)
    {
        testSession_->Close();
        testSession_.reset();
    }
}

LRESULT Application::HandleMainWindowMessage(HWND window, unsigned message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        return CreateButtons() ? 0 : -1;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_GAMUT_SRGB && HIWORD(wParam) == BN_CLICKED)
        {
            StartTest(ColorGamut::Srgb, 0);
            return 0;
        }
        if (LOWORD(wParam) == IDC_GAMUT_DISPLAY_P3 && HIWORD(wParam) == BN_CLICKED)
        {
            StartTest(ColorGamut::DisplayP3, 1);
            return 0;
        }
        break;

    case WM_SIZE:
        LayoutButtons();
        return 0;

    case WM_DPICHANGED:
    {
        const auto* suggestedRect = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(window,
                     nullptr,
                     suggestedRect->left,
                     suggestedRect->top,
                     suggestedRect->right - suggestedRect->left,
                     suggestedRect->bottom - suggestedRect->top,
                     SWP_NOACTIVATE | SWP_NOZORDER);
        RecreateMainFont();
        LayoutButtons();
        return 0;
    }

    case WM_GETMINMAXINFO:
    {
        auto* minimums = reinterpret_cast<MINMAXINFO*>(lParam);
        const unsigned dpi = GetDpiForWindow(window);
        minimums->ptMinTrackSize.x = ScaleForDpi(420, dpi);
        minimums->ptMinTrackSize.y = ScaleForDpi(250, dpi);
        return 0;
    }

    case kTestSessionEndedMessage:
        FinishTestSession(wParam != 0);
        return 0;

    case WM_CLOSE:
        DestroyWindow(window);
        return 0;

    case WM_DESTROY:
        CloseTestSession();
        PostQuitMessage(0);
        return 0;

    case WM_NCDESTROY:
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        if (mainWindow_ == window)
        {
            mainWindow_ = nullptr;
        }
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT __stdcall Application::MainWindowProc(HWND window, unsigned message, WPARAM wParam, LPARAM lParam)
{
    Application* application = nullptr;
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        application = static_cast<Application*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<intptr_t>(application));
        application->mainWindow_ = window;
    }
    else
    {
        application = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (application != nullptr)
    {
        return application->HandleMainWindowMessage(window, message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}
