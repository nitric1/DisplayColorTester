#pragma once

namespace DisplayColorTester
{
enum class ColorGamut
{
    Srgb,
    DisplayP3,
    AdobeRgb,
    Bt2020,
};

enum class TestColorId
{
    Red,
    Green,
    Blue,
    Yellow,
    Magenta,
    Cyan,
    White,
    Black,
};

[[nodiscard]] constexpr const wchar_t* ColorGamutName(ColorGamut gamut) noexcept
{
    switch (gamut)
    {
    case ColorGamut::Srgb:
        return L"sRGB";
    case ColorGamut::DisplayP3:
        return L"Display-P3 (P3-D65)";
    case ColorGamut::AdobeRgb:
        return L"Adobe RGB";
    case ColorGamut::Bt2020:
        return L"BT.2020";
    }

    return L"Unknown";
}

inline constexpr std::array<TestColorId, 8> kTestColorSequence{{
    TestColorId::Red,
    TestColorId::Green,
    TestColorId::Blue,
    TestColorId::Yellow,
    TestColorId::Magenta,
    TestColorId::Cyan,
    TestColorId::White,
    TestColorId::Black,
}};

inline constexpr std::array<const wchar_t*, 8> kTestColorNames{{
    L"Red (#F00)",
    L"Green (#0F0)",
    L"Blue (#00F)",
    L"Yellow (#FF0)",
    L"Magenta (#F0F)",
    L"Cyan (#0FF)",
    L"White (#FFF)",
    L"Black (#000)",
}};

[[nodiscard]] constexpr const wchar_t* TestColorName(TestColorId color) noexcept
{
    return kTestColorNames[static_cast<size_t>(color)];
}
}
