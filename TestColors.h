#pragma once

namespace DisplayColorTester
{
struct TestColor
{
    COLORREF value;
    const wchar_t* name;
};

inline constexpr std::array<TestColor, 8> kSrgbTestColors{{
    {RGB(255, 0, 0), L"Red (#F00)"},
    {RGB(0, 255, 0), L"Green (#0F0)"},
    {RGB(0, 0, 255), L"Blue (#00F)"},
    {RGB(255, 255, 0), L"Yellow (#FF0)"},
    {RGB(255, 0, 255), L"Magenta (#F0F)"},
    {RGB(0, 255, 255), L"Cyan (#0FF)"},
    {RGB(255, 255, 255), L"White (#FFF)"},
    {RGB(0, 0, 0), L"Black (#000)"},
}};
}
