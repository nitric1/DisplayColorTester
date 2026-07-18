#include "Common.h"

#include "GdiSrgbRenderer.h"

namespace DisplayColorTester
{
namespace
{
inline constexpr std::array<COLORREF, 8> kSrgbColors{{
    RGB(255, 0, 0),
    RGB(0, 255, 0),
    RGB(0, 0, 255),
    RGB(255, 255, 0),
    RGB(255, 0, 255),
    RGB(0, 255, 255),
    RGB(255, 255, 255),
    RGB(0, 0, 0),
}};

int ScaleForDpi(int value, unsigned dpi) noexcept
{
    return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

HFONT CreateOverlayFont(unsigned dpi) noexcept
{
    return CreateFontW(-ScaleForDpi(40, dpi),
                       0,
                       0,
                       0,
                       FW_BOLD,
                       FALSE,
                       FALSE,
                       FALSE,
                       DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE,
                       L"Segoe UI");
}

COLORREF SrgbColor(TestColorId color) noexcept
{
    return kSrgbColors[static_cast<size_t>(color)];
}

bool UseDarkText(COLORREF color) noexcept
{
    const unsigned luminance = 299U * GetRValue(color) + 587U * GetGValue(color) + 114U * GetBValue(color);
    return luminance >= 128000U;
}
}

GdiSrgbRenderer::~GdiSrgbRenderer()
{
    for (auto& context : windowContexts_)
    {
        if (context.overlayFont != nullptr)
        {
            DeleteObject(context.overlayFont);
            context.overlayFont = nullptr;
        }
    }
}

bool GdiSrgbRenderer::AttachWindow(HWND window) noexcept
{
    if (FindWindowContext(window) != nullptr)
    {
        return true;
    }

    const unsigned dpi = GetDpiForWindow(window);
    HFONT overlayFont = CreateOverlayFont(dpi);
    try
    {
        windowContexts_.push_back(WindowContext{window, overlayFont, dpi});
    }
    catch (const std::bad_alloc&)
    {
        if (overlayFont != nullptr)
        {
            DeleteObject(overlayFont);
        }
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return false;
    }
    return true;
}

void GdiSrgbRenderer::DetachWindow(HWND window) noexcept
{
    const auto result = std::find_if(windowContexts_.begin(),
                                     windowContexts_.end(),
                                     [window](const WindowContext& context) {
                                         return context.window == window;
                                     });
    if (result == windowContexts_.end())
    {
        return;
    }

    if (result->overlayFont != nullptr)
    {
        DeleteObject(result->overlayFont);
    }
    windowContexts_.erase(result);
}

void GdiSrgbRenderer::PaintWindow(HWND window, TestColorId color, bool overlayVisible) const noexcept
{
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(window, &paint);
    if (dc == nullptr)
    {
        return;
    }

    RECT clientRect{};
    GetClientRect(window, &clientRect);
    const COLORREF backgroundColor = SrgbColor(color);
    SetDCBrushColor(dc, backgroundColor);
    FillRect(dc, &clientRect, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

    if (overlayVisible)
    {
        const WindowContext* context = FindWindowContext(window);
        HFONT font = context != nullptr ? context->overlayFont : nullptr;
        if (font == nullptr)
        {
            font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        }
        const HGDIOBJ previousFont = SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);

        const bool darkText = UseDarkText(backgroundColor);
        const COLORREF textColor = darkText ? RGB(0, 0, 0) : RGB(255, 255, 255);
        const COLORREF shadowColor = darkText ? RGB(255, 255, 255) : RGB(0, 0, 0);
        const unsigned dpi = context != nullptr ? context->dpi : USER_DEFAULT_SCREEN_DPI;
        const int shadowOffset = (std::max)(1, ScaleForDpi(2, dpi));
        constexpr unsigned textFormat = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
        wchar_t overlayText[kTestOverlayTextCapacity]{};
        FormatTestOverlayText(ColorGamut::Srgb, color, overlayText);

        RECT shadowRect = clientRect;
        OffsetRect(&shadowRect, shadowOffset, shadowOffset);
        SetTextColor(dc, shadowColor);
        DrawTextW(dc, overlayText, -1, &shadowRect, textFormat);

        SetTextColor(dc, textColor);
        DrawTextW(dc, overlayText, -1, &clientRect, textFormat);
        SelectObject(dc, previousFont);
    }

    EndPaint(window, &paint);
}

const GdiSrgbRenderer::WindowContext* GdiSrgbRenderer::FindWindowContext(HWND window) const noexcept
{
    const auto result = std::find_if(windowContexts_.begin(),
                                     windowContexts_.end(),
                                     [window](const WindowContext& context) {
                                         return context.window == window;
                                     });
    return result != windowContexts_.end() ? &*result : nullptr;
}
}
