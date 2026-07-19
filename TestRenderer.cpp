#include "Common.h"

#include "TestRenderer.h"

#include "ColorManagedRenderer.h"
#include "GdiSrgbRenderer.h"

namespace DisplayColorTester
{
std::unique_ptr<TestRenderer> CreateTestRenderer(ColorGamut gamut, TestPattern pattern)
{
    if (pattern == TestPattern::Grayscale && gamut != ColorGamut::Srgb)
    {
        return nullptr;
    }

    switch (gamut)
    {
    case ColorGamut::Srgb:
        return std::make_unique<GdiSrgbRenderer>(pattern);

    case ColorGamut::DisplayP3:
    case ColorGamut::AdobeRgb:
    case ColorGamut::Bt2020:
    case ColorGamut::DisplayNative:
        return std::make_unique<ColorManagedRenderer>(gamut, pattern);
    }

    return nullptr;
}
}
