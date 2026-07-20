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

enum class TestPattern
{
    Color,
    Grayscale,
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

struct TestPatch
{
    RgbColor encodedRgb;
    const wchar_t* name;
};

struct TestPatchSequence
{
    const TestPatch* patches;
    size_t size;

    [[nodiscard]] constexpr const TestPatch& operator[](size_t index) const noexcept
    {
        return patches[index];
    }
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

[[nodiscard]] inline float DecodeColorChannel(
    float encodedValue,
    const RgbColorSpaceDefinition& definition) noexcept
{
    if (encodedValue <= 0.0F)
    {
        return 0.0F;
    }
    if (encodedValue >= 1.0F)
    {
        return 1.0F;
    }

    const double encoded = encodedValue;
    switch (definition.transferFunction)
    {
    case ColorTransferFunction::Srgb:
        if (encoded <= 0.04045)
        {
            return static_cast<float>(encoded / 12.92);
        }
        return static_cast<float>(std::pow((encoded + 0.055) / 1.055, 2.4));

    case ColorTransferFunction::Power:
        return static_cast<float>(std::pow(encoded, definition.calibratedGamma));

    case ColorTransferFunction::Bt2020:
    {
        constexpr double alpha = 1.09929682680944;
        constexpr double beta = 0.018053968510807;
        constexpr double linearScale = 4.5;
        constexpr double exponent = 0.45;
        if (encoded < linearScale * beta)
        {
            return static_cast<float>(encoded / linearScale);
        }
        return static_cast<float>(
            std::pow((encoded + alpha - 1.0) / alpha, 1.0 / exponent));
    }
    }

    return encodedValue;
}

[[nodiscard]] inline RgbColor DecodeColor(
    RgbColor encoded,
    const RgbColorSpaceDefinition& definition) noexcept
{
    return {
        DecodeColorChannel(encoded.red, definition),
        DecodeColorChannel(encoded.green, definition),
        DecodeColorChannel(encoded.blue, definition),
    };
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

[[nodiscard]] constexpr const wchar_t* TestPatternName(TestPattern pattern) noexcept
{
    switch (pattern)
    {
    case TestPattern::Color:
        return L"Color";
    case TestPattern::Grayscale:
        return L"Grayscale";
    }

    return L"Unknown";
}

inline constexpr std::array<TestPatch, 8> kColorTestPatches{{
    {{1.0F, 0.0F, 0.0F}, L"Red (#F00)"},
    {{0.0F, 1.0F, 0.0F}, L"Green (#0F0)"},
    {{0.0F, 0.0F, 1.0F}, L"Blue (#00F)"},
    {{1.0F, 1.0F, 0.0F}, L"Yellow (#FF0)"},
    {{1.0F, 0.0F, 1.0F}, L"Magenta (#F0F)"},
    {{0.0F, 1.0F, 1.0F}, L"Cyan (#0FF)"},
    {{1.0F, 1.0F, 1.0F}, L"White (#FFF)"},
    {{0.0F, 0.0F, 0.0F}, L"Black (#000)"},
}};

inline constexpr std::array<TestPatch, 11> kGrayscaleTestPatches{{
    {{0.0F, 0.0F, 0.0F}, L"Gray 0%"},
    {{0.1F, 0.1F, 0.1F}, L"Gray 10%"},
    {{0.2F, 0.2F, 0.2F}, L"Gray 20%"},
    {{0.3F, 0.3F, 0.3F}, L"Gray 30%"},
    {{0.4F, 0.4F, 0.4F}, L"Gray 40%"},
    {{0.5F, 0.5F, 0.5F}, L"Gray 50%"},
    {{0.6F, 0.6F, 0.6F}, L"Gray 60%"},
    {{0.7F, 0.7F, 0.7F}, L"Gray 70%"},
    {{0.8F, 0.8F, 0.8F}, L"Gray 80%"},
    {{0.9F, 0.9F, 0.9F}, L"Gray 90%"},
    {{1.0F, 1.0F, 1.0F}, L"Gray 100%"},
}};

inline constexpr std::array<unsigned, 11> kExpectedGrayscaleByteCodes{{
    0U, 26U, 51U, 77U, 102U, 128U, 153U, 179U, 204U, 230U, 255U,
}};

inline constexpr size_t kTestOverlayTextCapacity = 384;

[[nodiscard]] constexpr TestPatchSequence TestPatches(TestPattern pattern) noexcept
{
    switch (pattern)
    {
    case TestPattern::Color:
        return {kColorTestPatches.data(), kColorTestPatches.size()};
    case TestPattern::Grayscale:
        return {kGrayscaleTestPatches.data(), kGrayscaleTestPatches.size()};
    }

    return {};
}

[[nodiscard]] constexpr unsigned Unorm8ByteValue(float value) noexcept
{
    if (value <= 0.0F)
    {
        return 0U;
    }
    if (value >= 1.0F)
    {
        return 255U;
    }
    return static_cast<unsigned>(value * 255.0F + 0.5F);
}

[[nodiscard]] constexpr float Unorm8Value(float value) noexcept
{
    return static_cast<float>(Unorm8ByteValue(value)) / 255.0F;
}

[[nodiscard]] constexpr bool UseDarkOverlayText(const TestPatch& patch) noexcept
{
    const RgbColor& rgb = patch.encodedRgb;
    return 0.299F * rgb.red + 0.587F * rgb.green + 0.114F * rgb.blue >= 0.5F;
}

template <size_t Capacity>
size_t FormatTestOverlayText(ColorGamut gamut,
                             const TestPatch& patch,
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
    append(patch.name);
    buffer[length] = L'\0';
    return length;
}

[[nodiscard]] constexpr bool ValidateGrayscaleByteCodes() noexcept
{
    for (size_t index = 0; index < kGrayscaleTestPatches.size(); ++index)
    {
        const RgbColor& rgb = kGrayscaleTestPatches[index].encodedRgb;
        if (Unorm8ByteValue(rgb.red) != kExpectedGrayscaleByteCodes[index] ||
            Unorm8ByteValue(rgb.green) != kExpectedGrayscaleByteCodes[index] ||
            Unorm8ByteValue(rgb.blue) != kExpectedGrayscaleByteCodes[index])
        {
            return false;
        }
    }
    return true;
}

static_assert(kColorTestPatches.size() == 8);
static_assert(kGrayscaleTestPatches.size() == 11);
static_assert(ValidateGrayscaleByteCodes());
}
