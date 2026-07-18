#include "Common.h"

#include "TestRenderer.h"

#include "DisplayP3Renderer.h"
#include "GdiSrgbRenderer.h"

namespace DisplayColorTester
{
std::unique_ptr<TestRenderer> CreateTestRenderer(ColorGamut gamut)
{
    switch (gamut)
    {
    case ColorGamut::Srgb:
        return std::make_unique<GdiSrgbRenderer>();

    case ColorGamut::DisplayP3:
        return std::make_unique<DisplayP3Renderer>();

    case ColorGamut::AdobeRgb:
    case ColorGamut::Bt2020:
        return nullptr;
    }

    return nullptr;
}
}
