#pragma once

namespace DisplayColorTester
{
enum class ColorGamut;
enum class TestPattern;
class TestSession;

class Application final
{
public:
    explicit Application(HINSTANCE instance) noexcept;
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int Run(int showCommand);

private:
    static LRESULT __stdcall MainWindowProc(HWND window, unsigned message, WPARAM wParam, LPARAM lParam);

    bool RegisterWindowClasses() const noexcept;
    bool CreateMainWindow(int showCommand);
    bool CreateButtons();
    void LayoutButtons() const noexcept;
    void RecreateMainFont();
    void StartTest(ColorGamut gamut, size_t buttonIndex);
    void FinishTestSession(bool displayConfigurationChanged);
    void CloseTestSession() noexcept;
    LRESULT HandleMainWindowMessage(HWND window, unsigned message, WPARAM wParam, LPARAM lParam);

    HINSTANCE instance_{};
    HWND mainWindow_{};
    std::array<HWND, 2> patternButtons_{};
    std::array<HWND, 5> gamutButtons_{};
    HFONT mainFont_{};
    TestPattern selectedPattern_;
    size_t lastStartedButtonIndex_{};
    std::unique_ptr<TestSession> testSession_;
};
}
