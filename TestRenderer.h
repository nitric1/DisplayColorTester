#pragma once

#include "TestColors.h"

namespace DisplayColorTester
{
class TestRenderer
{
public:
    virtual ~TestRenderer() = default;

    TestRenderer(const TestRenderer&) = delete;
    TestRenderer& operator=(const TestRenderer&) = delete;

    virtual bool AttachWindow(HWND window) noexcept = 0;
    virtual void DetachWindow(HWND window) noexcept = 0;
    virtual void PaintWindow(HWND window, size_t patchIndex, bool overlayVisible) const noexcept = 0;

protected:
    TestRenderer() = default;
};

[[nodiscard]] std::unique_ptr<TestRenderer> CreateTestRenderer(ColorGamut gamut, TestPattern pattern);
}
