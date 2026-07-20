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
        return L"An unknown error occurred.";
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
        return L"Error code: " + std::to_wstring(errorCode);
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

Application::Application(HINSTANCE instance) noexcept
    : instance_(instance), selectedPattern_(TestPattern::Color)
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
                    (L"Unable to create the application window.\n\n" + details).c_str(),
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

        if (mainWindow_ != nullptr && IsWindowVisible(mainWindow_) &&
            IsDialogMessageW(mainWindow_, &message))
        {
            continue;
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
    RECT windowRect{0, 0, ScaleForDpi(520, dpi), ScaleForDpi(420, dpi)};
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
    if (gamutButtons_[0] != nullptr && IsWindowVisible(mainWindow_))
    {
        SetFocus(gamutButtons_[0]);
    }
    UpdateWindow(mainWindow_);
    return true;
}

bool Application::CreateButtons()
{
    struct PatternDefinition
    {
        int id;
        const wchar_t* text;
    };

    constexpr std::array<PatternDefinition, 2> patternDefinitions{{
        {IDC_PATTERN_COLOR, L"Color"},
        {IDC_PATTERN_GRAYSCALE, L"Grayscale"},
    }};
    for (size_t index = 0; index < patternDefinitions.size(); ++index)
    {
        const auto& definition = patternDefinitions[index];
        const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON |
                            (index == 0 ? WS_GROUP : 0);
        patternButtons_[index] = CreateWindowExW(
            0,
            L"BUTTON",
            definition.text,
            style,
            0,
            0,
            0,
            0,
            mainWindow_,
            reinterpret_cast<HMENU>(static_cast<intptr_t>(definition.id)),
            instance_,
            nullptr);
        if (patternButtons_[index] == nullptr)
        {
            return false;
        }
    }
    SendMessageW(patternButtons_[0], BM_SETCHECK, BST_CHECKED, 0);

    struct ButtonDefinition
    {
        int id;
        const wchar_t* text;
    };

    constexpr std::array<ButtonDefinition, 5> definitions{{
        {IDC_GAMUT_SRGB, L"sRGB"},
        {IDC_GAMUT_DISPLAY_P3, L"Display-P3 (P3-D65)"},
        {IDC_GAMUT_ADOBE_RGB, L"Adobe RGB"},
        {IDC_GAMUT_BT2020, L"BT.2020"},
        {IDC_GAMUT_DISPLAY_NATIVE, L"Display Native RGB (Best effort)"},
    }};

    for (size_t index = 0; index < definitions.size(); ++index)
    {
        const auto& definition = definitions[index];
        const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON |
                            (index == 0 ? WS_GROUP : 0);
        gamutButtons_[index] = CreateWindowExW(
            0,
            L"BUTTON",
            definition.text,
            style,
            0,
            0,
            0,
            0,
            mainWindow_,
            reinterpret_cast<HMENU>(static_cast<intptr_t>(definition.id)),
            instance_,
            nullptr);
        if (gamutButtons_[index] == nullptr)
        {
            return false;
        }
    }

    RecreateMainFont();
    LayoutButtons();
    return true;
}

void Application::LayoutButtons() const noexcept
{
    if (mainWindow_ == nullptr || patternButtons_[0] == nullptr || gamutButtons_[0] == nullptr)
    {
        return;
    }

    RECT clientRect{};
    GetClientRect(mainWindow_, &clientRect);
    const unsigned dpi = GetDpiForWindow(mainWindow_);
    const int padding = ScaleForDpi(24, dpi);
    const int gap = ScaleForDpi(12, dpi);
    const int patternHeight = ScaleForDpi(30, dpi);
    const int patternToGridGap = ScaleForDpi(18, dpi);
    const int buttonHeight = ScaleForDpi(58, dpi);
    const int clientWidth = clientRect.right - clientRect.left;
    const int clientHeight = clientRect.bottom - clientRect.top;
    const int availableWidth = (std::max)(0, clientWidth - (padding * 2) - gap);
    const int buttonWidth = availableWidth / 2;
    const int rowCount = static_cast<int>((gamutButtons_.size() + 1) / 2);
    const int gridHeight = (buttonHeight * rowCount) + (gap * (rowCount - 1));
    const int contentHeight = patternHeight + patternToGridGap + gridHeight;
    const int top = (std::max)(padding, (clientHeight - contentHeight) / 2);

    const int patternWidth = availableWidth / 2;
    for (size_t index = 0; index < patternButtons_.size(); ++index)
    {
        const int left = padding + static_cast<int>(index) * (patternWidth + gap);
        SetWindowPos(patternButtons_[index],
                     nullptr,
                     left,
                     top,
                     patternWidth,
                     patternHeight,
                     SWP_NOACTIVATE | SWP_NOZORDER);
    }

    const int gridTop = top + patternHeight + patternToGridGap;
    for (size_t index = 0; index < gamutButtons_.size(); ++index)
    {
        const int column = static_cast<int>(index % 2);
        const int row = static_cast<int>(index / 2);
        const bool lastOddButton = index + 1 == gamutButtons_.size() && (gamutButtons_.size() % 2) != 0;
        const int left = lastOddButton ? padding : padding + column * (buttonWidth + gap);
        const int y = gridTop + row * (buttonHeight + gap);

        SetWindowPos(gamutButtons_[index],
                     nullptr,
                     left,
                     y,
                     lastOddButton ? availableWidth + gap : buttonWidth,
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
    for (HWND button : patternButtons_)
    {
        if (button != nullptr)
        {
            SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(fontToUse), TRUE);
        }
    }
    for (HWND button : gamutButtons_)
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

    auto session = std::make_unique<TestSession>(instance_, mainWindow_, gamut, selectedPattern_);
    ShowWindow(mainWindow_, SW_HIDE);
    if (!session->Start())
    {
        const std::wstring details = FormatSystemError(session->LastErrorCode());
        ShowWindow(mainWindow_, SW_SHOW);
        SetForegroundWindow(mainWindow_);
        MessageBoxW(mainWindow_,
                    (L"Unable to create the monitor test windows.\n\n" + details).c_str(),
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
    SetFocus(gamutButtons_[lastStartedButtonIndex_]);

    if (displayConfigurationChanged)
    {
        MessageBoxW(mainWindow_,
                    L"The display configuration changed, so the test was closed.\n"
                    L"Restart the test to use the current monitor configuration.",
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
        if (LOWORD(wParam) == IDC_PATTERN_COLOR && HIWORD(wParam) == BN_CLICKED)
        {
            selectedPattern_ = TestPattern::Color;
            return 0;
        }
        if (LOWORD(wParam) == IDC_PATTERN_GRAYSCALE && HIWORD(wParam) == BN_CLICKED)
        {
            selectedPattern_ = TestPattern::Grayscale;
            return 0;
        }
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
        if (LOWORD(wParam) == IDC_GAMUT_ADOBE_RGB && HIWORD(wParam) == BN_CLICKED)
        {
            StartTest(ColorGamut::AdobeRgb, 2);
            return 0;
        }
        if (LOWORD(wParam) == IDC_GAMUT_BT2020 && HIWORD(wParam) == BN_CLICKED)
        {
            StartTest(ColorGamut::Bt2020, 3);
            return 0;
        }
        if (LOWORD(wParam) == IDC_GAMUT_DISPLAY_NATIVE && HIWORD(wParam) == BN_CLICKED)
        {
            StartTest(ColorGamut::DisplayNative, 4);
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
        minimums->ptMinTrackSize.y = ScaleForDpi(370, dpi);
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
