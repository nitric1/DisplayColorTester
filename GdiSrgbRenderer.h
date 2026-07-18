#pragma once

#include "TestRenderer.h"

namespace DisplayColorTester
{
class GdiSrgbRenderer final : public TestRenderer
{
public:
    GdiSrgbRenderer() = default;
    ~GdiSrgbRenderer() override;

    bool AttachWindow(HWND window) noexcept override;
    void DetachWindow(HWND window) noexcept override;
    void PaintWindow(HWND window, TestColorId color, bool overlayVisible) const noexcept override;

private:
    struct WindowContext
    {
        HWND window{};
        HFONT overlayFont{};
        unsigned dpi{USER_DEFAULT_SCREEN_DPI};
    };

    [[nodiscard]] const WindowContext* FindWindowContext(HWND window) const noexcept;

    std::vector<WindowContext> windowContexts_;
};
}
