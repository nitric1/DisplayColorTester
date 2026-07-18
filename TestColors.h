#pragma once

namespace DisplayColorTester
{
enum class ColorGamut
{
    Srgb,
    DisplayP3,
    AdobeRgb,
    Bt2020,
    DisplayNative,
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

enum class ColorTransferFunction
{
    Srgb,
    Power,
    Bt2020,
};

struct Chromaticity
{
    float x;
    float y;
};

struct RgbColorSpaceDefinition
{
    Chromaticity redPrimary;
    Chromaticity greenPrimary;
    Chromaticity bluePrimary;
    Chromaticity whitePoint;
    ColorTransferFunction transferFunction;
    float calibratedGamma;
};

struct RgbColor
{
    float red;
    float green;
    float blue;
};

inline constexpr RgbColorSpaceDefinition kSrgbColorSpace{
    {0.6400F, 0.3300F},
    {0.3000F, 0.6000F},
    {0.1500F, 0.0600F},
    {0.3127F, 0.3290F},
    ColorTransferFunction::Srgb,
    2.2F,
};

inline constexpr RgbColorSpaceDefinition kDisplayP3ColorSpace{
    {0.6800F, 0.3200F},
    {0.2650F, 0.6900F},
    {0.1500F, 0.0600F},
    {0.3127F, 0.3290F},
    ColorTransferFunction::Srgb,
    2.2F,
};

inline constexpr RgbColorSpaceDefinition kAdobeRgbColorSpace{
    {0.6400F, 0.3300F},
    {0.2100F, 0.7100F},
    {0.1500F, 0.0600F},
    {0.3127F, 0.3290F},
    ColorTransferFunction::Power,
    2.19921875F,
};

inline constexpr RgbColorSpaceDefinition kBt2020ColorSpace{
    {0.7080F, 0.2920F},
    {0.1700F, 0.7970F},
    {0.1310F, 0.0460F},
    {0.3127F, 0.3290F},
    ColorTransferFunction::Bt2020,
    2.4F,
};

[[nodiscard]] constexpr const RgbColorSpaceDefinition& ColorSpaceDefinition(ColorGamut gamut) noexcept
{
    switch (gamut)
    {
    case ColorGamut::Srgb:
        return kSrgbColorSpace;
    case ColorGamut::DisplayP3:
        return kDisplayP3ColorSpace;
    case ColorGamut::AdobeRgb:
        return kAdobeRgbColorSpace;
    case ColorGamut::Bt2020:
        return kBt2020ColorSpace;
    case ColorGamut::DisplayNative:
        return kSrgbColorSpace;
    }

    return kSrgbColorSpace;
}

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
    case ColorGamut::DisplayNative:
        return L"Display Native RGB (Best effort)";
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

inline constexpr size_t kTestOverlayTextCapacity = 384;

[[nodiscard]] constexpr const wchar_t* TestColorName(TestColorId color) noexcept
{
    return kTestColorNames[static_cast<size_t>(color)];
}

template <size_t Capacity>
size_t FormatTestOverlayText(ColorGamut gamut,
                             TestColorId color,
                             wchar_t (&buffer)[Capacity]) noexcept
{
    static_assert(Capacity > 0);
    size_t length{};
    const auto append = [&buffer, &length](const wchar_t* source) noexcept {
        while (*source != L'\0' && length + 1 < Capacity)
        {
            buffer[length++] = *source++;
        }
    };

    append(ColorGamutName(gamut));
    append(L" - ");
    append(TestColorName(color));
    buffer[length] = L'\0';
    return length;
}

[[nodiscard]] constexpr RgbColor TestColorRgb(TestColorId color) noexcept
{
    switch (color)
    {
    case TestColorId::Red:
        return {1.0F, 0.0F, 0.0F};
    case TestColorId::Green:
        return {0.0F, 1.0F, 0.0F};
    case TestColorId::Blue:
        return {0.0F, 0.0F, 1.0F};
    case TestColorId::Yellow:
        return {1.0F, 1.0F, 0.0F};
    case TestColorId::Magenta:
        return {1.0F, 0.0F, 1.0F};
    case TestColorId::Cyan:
        return {0.0F, 1.0F, 1.0F};
    case TestColorId::White:
        return {1.0F, 1.0F, 1.0F};
    case TestColorId::Black:
        return {0.0F, 0.0F, 0.0F};
    }

    return {};
}
}
