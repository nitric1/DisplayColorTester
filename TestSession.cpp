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
}

TestSession::TestSession(HINSTANCE instance, HWND ownerWindow, ColorGamut gamut) noexcept
    : instance_(instance), ownerWindow_(ownerWindow), gamut_(gamut)
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
    try
    {
        renderer_ = CreateTestRenderer(gamut_);
    }
    catch (const std::bad_alloc&)
    {
        lastErrorCode_ = ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }
    if (renderer_ == nullptr)
    {
        lastErrorCode_ = ERROR_NOT_SUPPORTED;
        return false;
    }

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

    std::wstring windowTitle;
    try
    {
        windowTitle = std::wstring(L"DisplayColorTester - ") + ColorGamutName(gamut_);
    }
    catch (const std::bad_alloc&)
    {
        lastErrorCode_ = ERROR_NOT_ENOUGH_MEMORY;
        renderer_.reset();
        return false;
    }

    for (const auto& monitor : monitors)
    {
        const int width = monitor.bounds.right - monitor.bounds.left;
        const int height = monitor.bounds.bottom - monitor.bounds.top;
        HWND window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                      kTestWindowClassName,
                                      windowTitle.c_str(),
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

        windows_.push_back(TestWindow{window, monitor.primary});
        if (!renderer_->AttachWindow(window))
        {
            lastErrorCode_ = GetLastError();
            if (lastErrorCode_ == ERROR_SUCCESS)
            {
                lastErrorCode_ = ERROR_GEN_FAILURE;
            }
            Stop(false, false);
            return false;
        }
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
        if (renderer_ != nullptr)
        {
            renderer_->PaintWindow(window, kTestColorSequence[colorIndex_], overlayVisible_);
        }
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

void TestSession::ChangeColor(int direction) noexcept
{
    if (stopping_)
    {
        return;
    }

    if (direction > 0)
    {
        colorIndex_ = (colorIndex_ + 1) % kTestColorSequence.size();
    }
    else
    {
        colorIndex_ = (colorIndex_ + kTestColorSequence.size() - 1) % kTestColorSequence.size();
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
        if (renderer_ != nullptr)
        {
            renderer_->DetachWindow(testWindow.window);
        }
        if (testWindow.window != nullptr && IsWindow(testWindow.window))
        {
            DestroyWindow(testWindow.window);
        }
    }
    windows_.clear();
    renderer_.reset();
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

HWND TestSession::CoordinatorWindow() const noexcept
{
    return windows_.empty() ? nullptr : windows_.front().window;
}
}
