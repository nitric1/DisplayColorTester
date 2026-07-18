#include "Common.h"

#include "TestSession.h"

#include "TestColors.h"

namespace DisplayColorTester
{
namespace
{
constexpr wchar_t kTestWindowClassName[] = L"DisplayColorTester.TestWindow";
constexpr uintptr_t kOverlayTimerId = 1;
constexpr uintptr_t kCursorTimerId = 2;
constexpr unsigned kTransientDisplayMilliseconds = 1000;

int ScaleForDpi(int value, unsigned dpi) noexcept
{
    return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

HFONT CreateOverlayFont(unsigned dpi) noexcept
{
    return CreateFontW(-ScaleForDpi(40, dpi),
                       0,
                       0,
                       0,
                       FW_BOLD,
                       FALSE,
                       FALSE,
                       FALSE,
                       DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE,
                       L"Segoe UI");
}

bool UseDarkText(COLORREF color) noexcept
{
    const unsigned int luminance = 299U * GetRValue(color) + 587U * GetGValue(color) + 114U * GetBValue(color);
    return luminance >= 128000U;
}
}

TestSession::TestSession(HINSTANCE instance, HWND ownerWindow) noexcept
    : instance_(instance), ownerWindow_(ownerWindow)
{
}

TestSession::~TestSession()
{
    Close();
}

bool TestSession::RegisterWindowClass(HINSTANCE instance) noexcept
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = &TestSession::TestWindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hCursor = nullptr;
    windowClass.hbrBackground = nullptr;
    windowClass.lpszClassName = kTestWindowClassName;
    windowClass.hIconSm = windowClass.hIcon;
    return RegisterClassExW(&windowClass) != 0;
}

bool TestSession::Start()
{
    std::vector<MonitorDescriptor> monitors;
    SetLastError(ERROR_SUCCESS);
    if (!EnumDisplayMonitors(nullptr,
                             nullptr,
                             &TestSession::MonitorEnumProc,
                             reinterpret_cast<LPARAM>(&monitors)))
    {
        lastErrorCode_ = GetLastError();
        if (lastErrorCode_ == ERROR_SUCCESS)
        {
            lastErrorCode_ = ERROR_GEN_FAILURE;
        }
        return false;
    }
    if (monitors.empty())
    {
        lastErrorCode_ = ERROR_NOT_FOUND;
        return false;
    }

    try
    {
        windows_.reserve(monitors.size());
    }
    catch (const std::bad_alloc&)
    {
        lastErrorCode_ = ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }

    for (const auto& monitor : monitors)
    {
        const int width = monitor.bounds.right - monitor.bounds.left;
        const int height = monitor.bounds.bottom - monitor.bounds.top;
        HWND window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                      kTestWindowClassName,
                                      L"DisplayColorTester - sRGB",
                                      WS_POPUP,
                                      monitor.bounds.left,
                                      monitor.bounds.top,
                                      width,
                                      height,
                                      nullptr,
                                      nullptr,
                                      instance_,
                                      this);
        if (window == nullptr)
        {
            lastErrorCode_ = GetLastError();
            Stop(false, false);
            return false;
        }

        const unsigned dpi = GetDpiForWindow(window);
        windows_.push_back(TestWindow{window, CreateOverlayFont(dpi), dpi, monitor.primary});
    }

    for (const auto& testWindow : windows_)
    {
        if (!SetWindowPos(testWindow.window,
                          HWND_TOPMOST,
                          0,
                          0,
                          0,
                          0,
                          SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW))
        {
            lastErrorCode_ = GetLastError();
            Stop(false, false);
            return false;
        }
    }

    overlayVisible_ = true;
    cursorVisible_ = true;
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    if (!RestartTimer(kOverlayTimerId) || !RestartTimer(kCursorTimerId))
    {
        lastErrorCode_ = GetLastError();
        if (lastErrorCode_ == ERROR_SUCCESS)
        {
            lastErrorCode_ = ERROR_GEN_FAILURE;
        }
        Stop(false, false);
        return false;
    }

    HWND focusWindow = windows_.front().window;
    const auto primary = std::find_if(windows_.begin(), windows_.end(), [](const TestWindow& candidate) {
        return candidate.primary;
    });
    if (primary != windows_.end())
    {
        focusWindow = primary->window;
    }
    SetForegroundWindow(focusWindow);
    SetFocus(focusWindow);
    RedrawAllWindows();
    return true;
}

void TestSession::Close() noexcept
{
    Stop(false, false);
}

DWORD TestSession::LastErrorCode() const noexcept
{
    return lastErrorCode_;
}

BOOL __stdcall TestSession::MonitorEnumProc(HMONITOR monitor, HDC, RECT*, LPARAM context)
{
    auto* monitors = reinterpret_cast<std::vector<MonitorDescriptor>*>(context);
    MONITORINFOEXW monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(monitor, &monitorInfo))
    {
        return FALSE;
    }

    try
    {
        monitors->push_back(MonitorDescriptor{
            monitor,
            monitorInfo.rcMonitor,
            (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) != 0,
        });
    }
    catch (const std::bad_alloc&)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    return TRUE;
}

LRESULT __stdcall TestSession::TestWindowProc(HWND window, unsigned message, WPARAM wParam, LPARAM lParam)
{
    TestSession* session = nullptr;
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        session = static_cast<TestSession*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<intptr_t>(session));
    }
    else
    {
        session = reinterpret_cast<TestSession*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (session != nullptr)
    {
        return session->HandleTestWindowMessage(window, message, wParam, lParam);
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT TestSession::HandleTestWindowMessage(HWND window, unsigned message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
        PaintWindow(window);
        return 0;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_KEYDOWN:
        if (wParam == VK_RIGHT)
        {
            ChangeColor(1);
            return 0;
        }
        if (wParam == VK_LEFT)
        {
            ChangeColor(-1);
            return 0;
        }
        if (wParam == VK_ESCAPE)
        {
            RequestClose(false);
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        SetFocus(window);
        ChangeColor(1);
        return 0;

    case WM_RBUTTONDOWN:
        SetFocus(window);
        ChangeColor(-1);
        return 0;

    case WM_CONTEXTMENU:
        return 0;

    case WM_MOUSEMOVE:
        ShowCursorTemporarily();
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(cursorVisible_ ? LoadCursorW(nullptr, IDC_ARROW) : nullptr);
            return TRUE;
        }
        break;

    case WM_TIMER:
        if (wParam == kOverlayTimerId)
        {
            if (RescheduleEarlyTimer(window, kOverlayTimerId, overlayDeadline_))
            {
                return 0;
            }
            KillTimer(window, kOverlayTimerId);
            overlayVisible_ = false;
            RedrawAllWindows();
            return 0;
        }
        if (wParam == kCursorTimerId)
        {
            if (RescheduleEarlyTimer(window, kCursorTimerId, cursorDeadline_))
            {
                return 0;
            }
            KillTimer(window, kCursorTimerId);
            cursorVisible_ = false;
            HideCursorIfOverTestWindow();
            return 0;
        }
        break;

    case WM_DISPLAYCHANGE:
        RequestClose(true);
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0U) == SC_SCREENSAVE || (wParam & 0xFFF0U) == SC_MONITORPOWER)
        {
            return 0;
        }
        break;

    case WM_CLOSE:
        RequestClose(false);
        return 0;

    case WM_NCDESTROY:
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        break;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

void TestSession::PaintWindow(HWND window) const noexcept
{
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    if (dc == nullptr)
    {
        return;
    }

    RECT clientRect{};
    GetClientRect(window, &clientRect);
    const TestColor& color = kSrgbTestColors[colorIndex_];
    SetDCBrushColor(dc, color.value);
    FillRect(dc, &clientRect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    if (overlayVisible_)
    {
        const TestWindow* testWindow = FindTestWindow(window);
        HFONT font = testWindow != nullptr ? testWindow->overlayFont : nullptr;
        if (font == nullptr)
        {
            font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        }
        const HGDIOBJ previousFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);

        const bool darkText = UseDarkText(color.value);
        const COLORREF textColor = darkText ? RGB(0, 0, 0) : RGB(255, 255, 255);
        const COLORREF shadowColor = darkText ? RGB(255, 255, 255) : RGB(0, 0, 0);
        const unsigned dpi = testWindow != nullptr ? testWindow->dpi : USER_DEFAULT_SCREEN_DPI;
        const int shadowOffset = (std::max)(1, ScaleForDpi(2, dpi));
        constexpr unsigned textFormat = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;

        RECT shadowRect = clientRect;
        OffsetRect(&shadowRect, shadowOffset, shadowOffset);
        SetTextColor(dc, shadowColor);
        DrawTextW(dc, color.name, -1, &shadowRect, textFormat);

        SetTextColor(dc, textColor);
        DrawTextW(dc, color.name, -1, &clientRect, textFormat);
        SelectObject(dc, previousFont);
    }

    EndPaint(window, &paint);
}

void TestSession::ChangeColor(int direction) noexcept
{
    if (stopping_)
    {
        return;
    }

    if (direction > 0)
    {
        colorIndex_ = (colorIndex_ + 1) % kSrgbTestColors.size();
    }
    else
    {
        colorIndex_ = (colorIndex_ + kSrgbTestColors.size() - 1) % kSrgbTestColors.size();
    }

    overlayVisible_ = RestartTimer(kOverlayTimerId);
    RedrawAllWindows();
}

void TestSession::ShowCursorTemporarily() noexcept
{
    if (stopping_)
    {
        return;
    }

    cursorVisible_ = true;
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    if (!RestartTimer(kCursorTimerId))
    {
        cursorVisible_ = false;
        HideCursorIfOverTestWindow();
    }
}

void TestSession::RequestClose(bool displayConfigurationChanged) noexcept
{
    Stop(true, displayConfigurationChanged);
}

void TestSession::Stop(bool notifyOwner, bool displayConfigurationChanged) noexcept
{
    if (stopping_)
    {
        return;
    }
    stopping_ = true;

    if (HWND coordinator = CoordinatorWindow(); coordinator != nullptr)
    {
        KillTimer(coordinator, kOverlayTimerId);
        KillTimer(coordinator, kCursorTimerId);
    }

    cursorVisible_ = true;
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    DestroyTestWindows();

    if (notifyOwner && IsWindow(ownerWindow_))
    {
        PostMessageW(ownerWindow_, kTestSessionEndedMessage, displayConfigurationChanged ? 1 : 0, 0);
    }
}

void TestSession::DestroyTestWindows() noexcept
{
    for (auto& testWindow : windows_)
    {
        if (testWindow.window != nullptr && IsWindow(testWindow.window))
        {
            DestroyWindow(testWindow.window);
        }
        if (testWindow.overlayFont != nullptr)
        {
            DeleteObject(testWindow.overlayFont);
            testWindow.overlayFont = nullptr;
        }
    }
    windows_.clear();
}

void TestSession::RedrawAllWindows() const noexcept
{
    for (const auto& testWindow : windows_)
    {
        InvalidateRect(testWindow.window, nullptr, FALSE);
    }
    for (const auto& testWindow : windows_)
    {
        UpdateWindow(testWindow.window);
    }
}

bool TestSession::RestartTimer(uintptr_t timerId) noexcept
{
    HWND coordinator = CoordinatorWindow();
    if (coordinator == nullptr)
    {
        return false;
    }
    KillTimer(coordinator, timerId);
    const uint64_t deadline = GetTickCount64() + kTransientDisplayMilliseconds;
    if (timerId == kOverlayTimerId)
    {
        overlayDeadline_ = deadline;
    }
    else if (timerId == kCursorTimerId)
    {
        cursorDeadline_ = deadline;
    }
    return SetTimer(coordinator, timerId, kTransientDisplayMilliseconds, nullptr) != 0;
}

bool TestSession::RescheduleEarlyTimer(HWND window, uintptr_t timerId, uint64_t deadline) const noexcept
{
    const uint64_t now = GetTickCount64();
    if (now >= deadline)
    {
        return false;
    }

    const unsigned remaining = static_cast<unsigned>(deadline - now);
    KillTimer(window, timerId);
    return SetTimer(window, timerId, (std::max)(1U, remaining), nullptr) != 0;
}

void TestSession::HideCursorIfOverTestWindow() const noexcept
{
    POINT cursorPosition{};
    if (!GetCursorPos(&cursorPosition))
    {
        return;
    }
    if (IsTestWindow(WindowFromPoint(cursorPosition)))
    {
        SetCursor(nullptr);
    }
}

bool TestSession::IsTestWindow(HWND window) const noexcept
{
    return std::any_of(windows_.begin(), windows_.end(), [window](const TestWindow& candidate) {
        return candidate.window == window;
    });
}

const TestSession::TestWindow* TestSession::FindTestWindow(HWND window) const noexcept
{
    const auto result = std::find_if(windows_.begin(), windows_.end(), [window](const TestWindow& candidate) {
        return candidate.window == window;
    });
    return result != windows_.end() ? &*result : nullptr;
}

HWND TestSession::CoordinatorWindow() const noexcept
{
    return windows_.empty() ? nullptr : windows_.front().window;
}
}
