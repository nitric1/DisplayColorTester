#pragma once

#include "TestRenderer.h"

namespace DisplayColorTester
{
inline constexpr unsigned kTestSessionEndedMessage = WM_APP + 1;

class TestSession final
{
public:
    TestSession(HINSTANCE instance,
                HWND ownerWindow,
                ColorGamut gamut,
                TestPattern pattern) noexcept;
    ~TestSession();

    TestSession(const TestSession&) = delete;
    TestSession& operator=(const TestSession&) = delete;

    static bool RegisterWindowClass(HINSTANCE instance) noexcept;

    bool Start();
    void Close() noexcept;
    [[nodiscard]] DWORD LastErrorCode() const noexcept;

private:
    struct MonitorDescriptor
    {
        HMONITOR monitor{};
        RECT bounds{};
        bool primary{};
    };

    struct TestWindow
    {
        HWND window{};
        bool primary{};
    };

    static BOOL __stdcall MonitorEnumProc(HMONITOR monitor,
                                          HDC monitorDc,
                                          RECT* monitorRect,
                                          LPARAM context);
    static LRESULT __stdcall TestWindowProc(HWND window, unsigned message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleTestWindowMessage(HWND window, unsigned message, WPARAM wParam, LPARAM lParam);
    void ChangePatch(int direction) noexcept;
    void ShowCursorTemporarily() noexcept;
    void RequestClose(bool displayConfigurationChanged) noexcept;
    void Stop(bool notifyOwner, bool displayConfigurationChanged) noexcept;
    void DestroyTestWindows() noexcept;
    void RedrawAllWindows() const noexcept;
    bool RestartTimer(uintptr_t timerId) noexcept;
    bool RescheduleEarlyTimer(HWND window, uintptr_t timerId, uint64_t deadline) const noexcept;
    void HideCursorIfOverTestWindow() const noexcept;
    [[nodiscard]] bool IsTestWindow(HWND window) const noexcept;
    [[nodiscard]] HWND CoordinatorWindow() const noexcept;

    HINSTANCE instance_{};
    HWND ownerWindow_{};
    ColorGamut gamut_{ColorGamut::Srgb};
    TestPattern pattern_{TestPattern::Color};
    std::unique_ptr<TestRenderer> renderer_;
    std::vector<TestWindow> windows_;
    size_t patchIndex_{};
    bool overlayVisible_{};
    bool cursorVisible_{true};
    bool stopping_{};
    uint64_t overlayDeadline_{};
    uint64_t cursorDeadline_{};
    DWORD lastErrorCode_{};
};
}
