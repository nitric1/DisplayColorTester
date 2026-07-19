#pragma once

#include "TestRenderer.h"

namespace DisplayColorTester
{
class GdiSrgbRenderer final : public TestRenderer
{
public:
    explicit GdiSrgbRenderer(TestPattern pattern) noexcept;
    ~GdiSrgbRenderer() override;

    bool AttachWindow(HWND window) noexcept override;
    void DetachWindow(HWND window) noexcept override;
    void PaintWindow(HWND window, size_t patchIndex, bool overlayVisible) const noexcept override;

private:
    struct WindowContext
    {
        HWND window{};
        HFONT overlayFont{};
        unsigned dpi{USER_DEFAULT_SCREEN_DPI};
    };

    [[nodiscard]] const WindowContext* FindWindowContext(HWND window) const noexcept;

    TestPattern pattern_;
    std::vector<WindowContext> windowContexts_;
};
}
