#include "Common.h"

#include "ColorManagedRenderer.h"

namespace DisplayColorTester
{
namespace
{
constexpr float kTextSize = 40.0F;
constexpr float kShadowOffset = 2.0F;
constexpr float kWordMaximum = 65535.0F;
constexpr float kSceneReferredWhiteNits = 80.0F;
constexpr double kFixed2Dot30Scale = 1073741824.0;

struct Matrix3x3
{
    double values[3][3];
};

Matrix3x3 BuildLinearRgbToXyz(const RgbColorSpaceDefinition& definition) noexcept;

inline constexpr Matrix3x3 kXyzToLinearSrgb{{
    {3.2409699419045226, -1.5373831775700940, -0.4986107602930034},
    {-0.9692436362808796, 1.8759675015077202, 0.0415550574071756},
    {0.0556300796969937, -0.2039769588889765, 1.0569715142428786},
}};

void SetLastErrorFromHResult(HRESULT result) noexcept
{
    if (HRESULT_FACILITY(result) == FACILITY_WIN32)
    {
        SetLastError(HRESULT_CODE(result));
    }
    else
    {
        SetLastError(ERROR_GEN_FAILURE);
    }
}

bool UseDarkText(TestColorId color) noexcept
{
    return color == TestColorId::Green || color == TestColorId::Yellow ||
           color == TestColorId::Cyan || color == TestColorId::White;
}

bool IsValidChromaticity(Chromaticity value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           value.x > 0.0F && value.y > 0.0F && value.x + value.y <= 1.0F;
}

bool IsValidColorSpace(const RgbColorSpaceDefinition& definition) noexcept
{
    if (!IsValidChromaticity(definition.redPrimary) ||
        !IsValidChromaticity(definition.greenPrimary) ||
        !IsValidChromaticity(definition.bluePrimary) ||
        !IsValidChromaticity(definition.whitePoint))
    {
        return false;
    }

    const double twiceArea =
        (definition.greenPrimary.x - definition.redPrimary.x) *
            (definition.bluePrimary.y - definition.redPrimary.y) -
        (definition.greenPrimary.y - definition.redPrimary.y) *
            (definition.bluePrimary.x - definition.redPrimary.x);
    if (std::abs(twiceArea) <= 0.000001)
    {
        return false;
    }

    const Matrix3x3 rgbToXyz = BuildLinearRgbToXyz(definition);
    for (size_t row = 0; row < 3; ++row)
    {
        for (size_t column = 0; column < 3; ++column)
        {
            if (!std::isfinite(rgbToXyz.values[row][column]))
            {
                return false;
            }
        }
    }
    return rgbToXyz.values[1][0] > 0.0 &&
           rgbToXyz.values[1][1] > 0.0 &&
           rgbToXyz.values[1][2] > 0.0;
}

template <size_t Capacity>
void AppendText(wchar_t (&destination)[Capacity], size_t& length, const wchar_t* source) noexcept
{
    while (*source != L'\0' && length + 1 < Capacity)
    {
        destination[length++] = *source++;
    }
    destination[length] = L'\0';
}

long ToFixed2Dot30(double value) noexcept
{
    return static_cast<long>(value * kFixed2Dot30Scale + 0.5);
}

DWORD ToCalibratedGamma(float gamma) noexcept
{
    const unsigned fixed8Dot8 = static_cast<unsigned>(gamma * 256.0F + 0.5F);
    return static_cast<DWORD>(fixed8Dot8 << 8);
}

void SetXyz(CIEXYZ& destination, double x, double y, double z) noexcept
{
    destination.ciexyzX = ToFixed2Dot30(x);
    destination.ciexyzY = ToFixed2Dot30(y);
    destination.ciexyzZ = ToFixed2Dot30(z);
}

Matrix3x3 Invert(const Matrix3x3& matrix) noexcept
{
    const double(*m)[3] = matrix.values;
    const double determinant =
        m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
        m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
        m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    const double inverseDeterminant = 1.0 / determinant;

    Matrix3x3 inverse{};
    inverse.values[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * inverseDeterminant;
    inverse.values[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * inverseDeterminant;
    inverse.values[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inverseDeterminant;
    inverse.values[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * inverseDeterminant;
    inverse.values[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inverseDeterminant;
    inverse.values[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * inverseDeterminant;
    inverse.values[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * inverseDeterminant;
    inverse.values[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * inverseDeterminant;
    inverse.values[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inverseDeterminant;
    return inverse;
}

Matrix3x3 Multiply(const Matrix3x3& left, const Matrix3x3& right) noexcept
{
    Matrix3x3 product{};
    for (size_t row = 0; row < 3; ++row)
    {
        for (size_t column = 0; column < 3; ++column)
        {
            for (size_t index = 0; index < 3; ++index)
            {
                product.values[row][column] += left.values[row][index] * right.values[index][column];
            }
        }
    }
    return product;
}

Matrix3x3 BuildLinearRgbToXyz(const RgbColorSpaceDefinition& definition) noexcept
{
    const Chromaticity& red = definition.redPrimary;
    const Chromaticity& green = definition.greenPrimary;
    const Chromaticity& blue = definition.bluePrimary;
    const Chromaticity& white = definition.whitePoint;
    const Matrix3x3 unscaled{{
        {red.x / red.y, green.x / green.y, blue.x / blue.y},
        {1.0, 1.0, 1.0},
        {(1.0 - red.x - red.y) / red.y,
         (1.0 - green.x - green.y) / green.y,
         (1.0 - blue.x - blue.y) / blue.y},
    }};
    const double whiteXyz[3]{
        white.x / white.y,
        1.0,
        (1.0 - white.x - white.y) / white.y,
    };
    const Matrix3x3 inverse = Invert(unscaled);
    double scale[3]{};
    for (size_t row = 0; row < 3; ++row)
    {
        for (size_t column = 0; column < 3; ++column)
        {
            scale[row] += inverse.values[row][column] * whiteXyz[column];
        }
    }

    Matrix3x3 result{};
    for (size_t row = 0; row < 3; ++row)
    {
        for (size_t column = 0; column < 3; ++column)
        {
            result.values[row][column] = unscaled.values[row][column] * scale[column];
        }
    }
    return result;
}

RgbColor Transform(const Matrix3x3& matrix, RgbColor color) noexcept
{
    return {
        static_cast<float>(matrix.values[0][0] * color.red +
                           matrix.values[0][1] * color.green +
                           matrix.values[0][2] * color.blue),
        static_cast<float>(matrix.values[1][0] * color.red +
                           matrix.values[1][1] * color.green +
                           matrix.values[1][2] * color.blue),
        static_cast<float>(matrix.values[2][0] * color.red +
                           matrix.values[2][1] * color.green +
                           matrix.values[2][2] * color.blue),
    };
}

bool GetDisplayProfilePath(HMONITOR monitor, std::wstring& profilePath)
{
    MONITORINFOEXW monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(monitor, &monitorInfo))
    {
        return false;
    }

    HDC monitorDc = CreateDCW(L"DISPLAY", monitorInfo.szDevice, nullptr, nullptr);
    if (monitorDc == nullptr)
    {
        return false;
    }

    DWORD characterCount{};
    SetLastError(ERROR_SUCCESS);
    const BOOL sizeResult = GetICMProfileW(monitorDc, &characterCount, nullptr);
    if (!sizeResult && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        DeleteDC(monitorDc);
        return false;
    }
    if (characterCount == 0)
    {
        DeleteDC(monitorDc);
        SetLastError(ERROR_FILE_NOT_FOUND);
        return false;
    }

    profilePath.assign(characterCount, L'\0');
    const BOOL profileResult = GetICMProfileW(monitorDc, &characterCount, profilePath.data());
    DeleteDC(monitorDc);
    if (!profileResult)
    {
        return false;
    }

    while (!profilePath.empty() && profilePath.back() == L'\0')
    {
        profilePath.pop_back();
    }
    return !profilePath.empty();
}
}

ColorManagedRenderer::ColorManagedRenderer(ColorGamut gamut) noexcept : gamut_(gamut)
{
}

bool ColorManagedRenderer::AttachWindow(HWND window) noexcept
{
    if (FindWindowContext(window) != nullptr)
    {
        return true;
    }

    try
    {
        if (!EnsureDeviceResources())
        {
            return false;
        }

        WindowContext context{};
        if (!CreateWindowContext(window, context))
        {
            return false;
        }
        windowContexts_.push_back(std::move(context));
        return true;
    }
    catch (const std::bad_alloc&)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return false;
    }
}

void ColorManagedRenderer::DetachWindow(HWND window) noexcept
{
    const auto result = std::find_if(windowContexts_.begin(),
                                     windowContexts_.end(),
                                     [window](const WindowContext& context) {
                                         return context.window == window;
                                     });
    if (result != windowContexts_.end())
    {
        windowContexts_.erase(result);
    }
}

void ColorManagedRenderer::PaintWindow(HWND window, TestColorId color, bool overlayVisible) const noexcept
{
    PAINTSTRUCT paint{};
    HDC paintDc = BeginPaint(window, &paint);
    if (paintDc == nullptr)
    {
        return;
    }

    const WindowContext* context = FindWindowContext(window);
    if (context == nullptr)
    {
        EndPaint(window, &paint);
        return;
    }

    const RenderColor& background = context->colors[static_cast<size_t>(color)];
    ID3D11RenderTargetView* renderTarget = context->renderTarget.Get();
    d3dContext_->OMSetRenderTargets(1, &renderTarget, nullptr);
    d3dContext_->ClearRenderTargetView(renderTarget, background.data());

    if (overlayVisible)
    {
        d3dContext_->OMSetRenderTargets(0, nullptr, nullptr);
        d3dContext_->Flush();

        const D2D1_SIZE_F targetSize = context->d2dContext->GetSize();
        const D2D1_RECT_F textRect = D2D1::RectF(0.0F, 0.0F, targetSize.width, targetSize.height);
        const D2D1_RECT_F shadowRect = D2D1::RectF(kShadowOffset,
                                                   kShadowOffset,
                                                   targetSize.width + kShadowOffset,
                                                   targetSize.height + kShadowOffset);
        ID2D1SolidColorBrush* textBrush = UseDarkText(color) ? context->darkBrush.Get() : context->lightBrush.Get();
        ID2D1SolidColorBrush* shadowBrush = UseDarkText(color) ? context->lightBrush.Get() : context->darkBrush.Get();
        wchar_t overlayText[kTestOverlayTextCapacity]{};
        size_t textLength = FormatTestOverlayText(gamut_, color, overlayText);
        if (context->diagnosticText[0] != L'\0')
        {
            AppendText(overlayText, textLength, L"\n");
            AppendText(overlayText, textLength, context->diagnosticText.data());
        }

        context->d2dContext->BeginDraw();
        context->d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
        context->d2dContext->DrawTextW(overlayText,
                                      static_cast<unsigned>(textLength),
                                      textFormat_.Get(),
                                      shadowRect,
                                      shadowBrush,
                                      D2D1_DRAW_TEXT_OPTIONS_NONE,
                                      DWRITE_MEASURING_MODE_NATURAL);
        context->d2dContext->DrawTextW(overlayText,
                                      static_cast<unsigned>(textLength),
                                      textFormat_.Get(),
                                      textRect,
                                      textBrush,
                                      D2D1_DRAW_TEXT_OPTIONS_NONE,
                                      DWRITE_MEASURING_MODE_NATURAL);
        context->d2dContext->EndDraw();
    }

    context->swapChain->Present(0, 0);
    EndPaint(window, &paint);
}

bool ColorManagedRenderer::EnsureDeviceResources() noexcept
{
    if (d3dDevice_ != nullptr)
    {
        return true;
    }

    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext;
    D3D_FEATURE_LEVEL featureLevel{};
    constexpr unsigned creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT result = D3D11CreateDevice(nullptr,
                                       D3D_DRIVER_TYPE_HARDWARE,
                                       nullptr,
                                       creationFlags,
                                       nullptr,
                                       0,
                                       D3D11_SDK_VERSION,
                                       &d3dDevice,
                                       &featureLevel,
                                       &d3dContext);
    if (FAILED(result))
    {
        result = D3D11CreateDevice(nullptr,
                                   D3D_DRIVER_TYPE_WARP,
                                   nullptr,
                                   creationFlags,
                                   nullptr,
                                   0,
                                   D3D11_SDK_VERSION,
                                   &d3dDevice,
                                   &featureLevel,
                                   &d3dContext);
    }
    if (FAILED(result))
    {
        SetLastErrorFromHResult(result);
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
    if (FAILED(result = d3dDevice.As(&dxgiDevice)) ||
        FAILED(result = dxgiDevice->GetAdapter(&adapter)) ||
        FAILED(result = adapter->GetParent(IID_PPV_ARGS(&dxgiFactory))))
    {
        SetLastErrorFromHResult(result);
        return false;
    }

    D2D1_FACTORY_OPTIONS factoryOptions{};
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory;
    result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                               __uuidof(ID2D1Factory1),
                               &factoryOptions,
                               reinterpret_cast<void**>(d2dFactory.GetAddressOf()));
    if (FAILED(result))
    {
        SetLastErrorFromHResult(result);
        return false;
    }

    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice;
    if (FAILED(result = d2dFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice)))
    {
        SetLastErrorFromHResult(result);
        return false;
    }

    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory;
    result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                 __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(dwriteFactory.GetAddressOf()));
    if (FAILED(result))
    {
        SetLastErrorFromHResult(result);
        return false;
    }

    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat;
    result = dwriteFactory->CreateTextFormat(L"Segoe UI",
                                             nullptr,
                                             DWRITE_FONT_WEIGHT_BOLD,
                                             DWRITE_FONT_STYLE_NORMAL,
                                             DWRITE_FONT_STRETCH_NORMAL,
                                             kTextSize,
                                             L"",
                                             &textFormat);
    if (FAILED(result) || FAILED(result = textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER)) ||
        FAILED(result = textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER)))
    {
        SetLastErrorFromHResult(result);
        return false;
    }

    d3dDevice_ = std::move(d3dDevice);
    d3dContext_ = std::move(d3dContext);
    dxgiFactory_ = std::move(dxgiFactory);
    d2dFactory_ = std::move(d2dFactory);
    d2dDevice_ = std::move(d2dDevice);
    dwriteFactory_ = std::move(dwriteFactory);
    textFormat_ = std::move(textFormat);
    return true;
}

bool ColorManagedRenderer::CreateWindowContext(HWND window, WindowContext& context)
{
    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    const AdvancedColorState advancedColor = QueryAdvancedColorState(monitor);

    DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
    DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    float referenceWhiteScale = 1.0F;
    context.outputMode = OutputMode::LegacySdr;

    if (gamut_ == ColorGamut::DisplayNative)
    {
        if (advancedColor.active)
        {
            context.outputMode = OutputMode::AdvancedColor;
            const NativeDisplayState nativeDisplay = QueryNativeDisplayState(monitor);
            const RgbColorSpaceDefinition& sourceColorSpace = nativeDisplay.primariesValid
                ? nativeDisplay.colorSpace
                : (advancedColor.hdr ? kBt2020ColorSpace : kSrgbColorSpace);
            float nativeWhiteScale = 1.0F;
            if (advancedColor.hdr)
            {
                nativeWhiteScale = nativeDisplay.fullFrameLuminanceValid
                    ? nativeDisplay.maxFullFrameLuminance / kSceneReferredWhiteNits
                    : advancedColor.sdrWhiteScale;
            }
            context.colors = BuildAdvancedColorValues(sourceColorSpace, nativeWhiteScale);
            format = DXGI_FORMAT_R16G16B16A16_FLOAT;
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
            referenceWhiteScale = advancedColor.sdrWhiteScale;

            const wchar_t* modeName = advancedColor.hdr ? L"HDR" : L"SDR";
            int length{};
            if (nativeDisplay.primariesValid)
            {
                length = swprintf_s(
                    context.diagnosticText.data(),
                    context.diagnosticText.size(),
                    L"Advanced Color %ls | DXGI primaries (EDID/override) | %u bpc\n"
                    L"R(%.4f, %.4f)  G(%.4f, %.4f)  B(%.4f, %.4f)  W(%.4f, %.4f)",
                    modeName,
                    nativeDisplay.bitsPerColor,
                    nativeDisplay.colorSpace.redPrimary.x,
                    nativeDisplay.colorSpace.redPrimary.y,
                    nativeDisplay.colorSpace.greenPrimary.x,
                    nativeDisplay.colorSpace.greenPrimary.y,
                    nativeDisplay.colorSpace.bluePrimary.x,
                    nativeDisplay.colorSpace.bluePrimary.y,
                    nativeDisplay.colorSpace.whitePoint.x,
                    nativeDisplay.colorSpace.whitePoint.y);
            }
            else
            {
                length = swprintf_s(
                    context.diagnosticText.data(),
                    context.diagnosticText.size(),
                    L"Advanced Color %ls | reported primaries unavailable\n"
                    L"%ls estimate | best effort",
                    modeName,
                    advancedColor.hdr ? L"BT.2020" : L"sRGB");
            }
            if (length >= 0 && advancedColor.hdr)
            {
                const size_t offset = static_cast<size_t>(length);
                const int appended = nativeDisplay.fullFrameLuminanceValid
                    ? swprintf_s(context.diagnosticText.data() + offset,
                                 context.diagnosticText.size() - offset,
                                 L"\nFull-frame white target: %.1f nits",
                                 nativeDisplay.maxFullFrameLuminance)
                    : swprintf_s(context.diagnosticText.data() + offset,
                                 context.diagnosticText.size() - offset,
                                 L"\nFull-frame luminance unavailable; SDR white estimate");
                if (appended < 0)
                {
                    length = -1;
                }
            }
            if (length < 0)
            {
                context.diagnosticText[0] = L'\0';
            }
        }
        else
        {
            context.colors = BuildFallbackSdrValues();
            constexpr wchar_t diagnostic[] =
                L"Legacy SDR | full-range device RGB | ICC bypassed | best effort";
            swprintf_s(context.diagnosticText.data(),
                       context.diagnosticText.size(),
                       L"%ls",
                       diagnostic);
        }
    }
    else if (advancedColor.active)
    {
        context.outputMode = OutputMode::AdvancedColor;
        context.colors = BuildAdvancedColorValues(ColorSpaceDefinition(gamut_),
                                                  advancedColor.sdrWhiteScale);
        format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
        referenceWhiteScale = advancedColor.sdrWhiteScale;
    }
    else if (!BuildLegacySdrColors(monitor, context.colors))
    {
        context.colors = BuildFallbackSdrValues();
    }

    context.window = window;
    return CreateSwapChainResources(window, format, colorSpace, referenceWhiteScale, context);
}

bool ColorManagedRenderer::CreateSwapChainResources(HWND window,
                                                 DXGI_FORMAT format,
                                                 DXGI_COLOR_SPACE_TYPE colorSpace,
                                                 float referenceWhiteScale,
                                                 WindowContext& context) noexcept
{
    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Format = format;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    HRESULT result = dxgiFactory_->CreateSwapChainForHwnd(d3dDevice_.Get(),
                                                          window,
                                                          &description,
                                                          nullptr,
                                                          nullptr,
                                                          &swapChain);
    if (FAILED(result) || FAILED(result = swapChain.As(&context.swapChain)))
    {
        SetLastErrorFromHResult(result);
        return false;
    }

    unsigned colorSpaceSupport{};
    result = context.swapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport);
    if (FAILED(result) || (colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == 0 ||
        FAILED(result = context.swapChain->SetColorSpace1(colorSpace)))
    {
        SetLastErrorFromHResult(FAILED(result) ? result : E_NOTIMPL);
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    if (FAILED(result = context.swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) ||
        FAILED(result = d3dDevice_->CreateRenderTargetView(backBuffer.Get(), nullptr, &context.renderTarget)) ||
        FAILED(result = backBuffer.As(&surface)) ||
        FAILED(result = d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &context.d2dContext)))
    {
        SetLastErrorFromHResult(result);
        return false;
    }

    const float dpi = static_cast<float>(GetDpiForWindow(window));
    D2D1_BITMAP_PROPERTIES1 bitmapProperties{};
    bitmapProperties.pixelFormat = D2D1::PixelFormat(format, D2D1_ALPHA_MODE_IGNORE);
    bitmapProperties.dpiX = dpi;
    bitmapProperties.dpiY = dpi;
    bitmapProperties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
    result = context.d2dContext->CreateBitmapFromDxgiSurface(surface.Get(),
                                                             &bitmapProperties,
                                                             &context.targetBitmap);
    if (FAILED(result))
    {
        SetLastErrorFromHResult(result);
        return false;
    }
    context.d2dContext->SetTarget(context.targetBitmap.Get());
    context.d2dContext->SetDpi(dpi, dpi);

    const D2D1_COLOR_F dark = D2D1::ColorF(0.0F, 0.0F, 0.0F, 1.0F);
    const D2D1_COLOR_F light = D2D1::ColorF(referenceWhiteScale,
                                            referenceWhiteScale,
                                            referenceWhiteScale,
                                            1.0F);
    if (FAILED(result = context.d2dContext->CreateSolidColorBrush(dark, &context.darkBrush)) ||
        FAILED(result = context.d2dContext->CreateSolidColorBrush(light, &context.lightBrush)))
    {
        SetLastErrorFromHResult(result);
        return false;
    }

    dxgiFactory_->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
    return true;
}

ColorManagedRenderer::AdvancedColorState ColorManagedRenderer::QueryAdvancedColorState(HMONITOR monitor) const
{
    AdvancedColorState state{};
    MONITORINFOEXW monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(monitor, &monitorInfo))
    {
        return state;
    }

    unsigned pathCount{};
    unsigned modeCount{};
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
    {
        return state;
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    long queryResult = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,
                                          &pathCount,
                                          paths.data(),
                                          &modeCount,
                                          modes.data(),
                                          nullptr);
    if (queryResult != ERROR_SUCCESS)
    {
        return state;
    }
    paths.resize(pathCount);

    for (const DISPLAYCONFIG_PATH_INFO& path : paths)
    {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName{};
        sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        sourceName.header.size = sizeof(sourceName);
        sourceName.header.adapterId = path.sourceInfo.adapterId;
        sourceName.header.id = path.sourceInfo.id;
        if (DisplayConfigGetDeviceInfo(&sourceName.header) != ERROR_SUCCESS ||
            lstrcmpW(sourceName.viewGdiDeviceName, monitorInfo.szDevice) != 0)
        {
            continue;
        }

        DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2 colorInfo2{};
        colorInfo2.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2;
        colorInfo2.header.size = sizeof(colorInfo2);
        colorInfo2.header.adapterId = path.targetInfo.adapterId;
        colorInfo2.header.id = path.targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&colorInfo2.header) == ERROR_SUCCESS)
        {
            state.active = colorInfo2.advancedColorActive != 0;
            state.hdr = colorInfo2.activeColorMode == DISPLAYCONFIG_ADVANCED_COLOR_MODE_HDR;
        }
        else
        {
            DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO colorInfo{};
            colorInfo.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
            colorInfo.header.size = sizeof(colorInfo);
            colorInfo.header.adapterId = path.targetInfo.adapterId;
            colorInfo.header.id = path.targetInfo.id;
            if (DisplayConfigGetDeviceInfo(&colorInfo.header) == ERROR_SUCCESS)
            {
                state.active = colorInfo.advancedColorEnabled != 0;
                state.hdr = state.active;
            }
        }

        if (state.hdr)
        {
            DISPLAYCONFIG_SDR_WHITE_LEVEL whiteLevel{};
            whiteLevel.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
            whiteLevel.header.size = sizeof(whiteLevel);
            whiteLevel.header.adapterId = path.targetInfo.adapterId;
            whiteLevel.header.id = path.targetInfo.id;
            if (DisplayConfigGetDeviceInfo(&whiteLevel.header) == ERROR_SUCCESS && whiteLevel.SDRWhiteLevel != 0)
            {
                state.sdrWhiteScale = static_cast<float>(whiteLevel.SDRWhiteLevel) / 1000.0F;
            }
        }
        return state;
    }

    return state;
}

ColorManagedRenderer::NativeDisplayState
ColorManagedRenderer::QueryNativeDisplayState(HMONITOR monitor) const noexcept
{
    NativeDisplayState state{};
    for (unsigned adapterIndex = 0;; ++adapterIndex)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        const HRESULT adapterResult = dxgiFactory_->EnumAdapters1(adapterIndex, &adapter);
        if (adapterResult == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }
        if (FAILED(adapterResult))
        {
            return state;
        }

        for (unsigned outputIndex = 0;; ++outputIndex)
        {
            Microsoft::WRL::ComPtr<IDXGIOutput> output;
            const HRESULT outputResult = adapter->EnumOutputs(outputIndex, &output);
            if (outputResult == DXGI_ERROR_NOT_FOUND)
            {
                break;
            }
            if (FAILED(outputResult))
            {
                return state;
            }

            Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
            DXGI_OUTPUT_DESC1 description{};
            if (FAILED(output.As(&output6)) || FAILED(output6->GetDesc1(&description)) ||
                description.Monitor != monitor)
            {
                continue;
            }

            state.bitsPerColor = description.BitsPerColor;
            state.maxFullFrameLuminance = description.MaxFullFrameLuminance;
            state.fullFrameLuminanceValid =
                std::isfinite(description.MaxFullFrameLuminance) &&
                std::isfinite(description.MaxLuminance) &&
                description.MaxFullFrameLuminance > 0.0F &&
                description.MaxLuminance > 0.0F &&
                description.MaxFullFrameLuminance <= description.MaxLuminance;
            state.colorSpace = {
                {description.RedPrimary[0], description.RedPrimary[1]},
                {description.GreenPrimary[0], description.GreenPrimary[1]},
                {description.BluePrimary[0], description.BluePrimary[1]},
                {description.WhitePoint[0], description.WhitePoint[1]},
                ColorTransferFunction::Srgb,
                2.2F,
            };
            state.primariesValid = IsValidColorSpace(state.colorSpace);
            return state;
        }
    }
    return state;
}

bool ColorManagedRenderer::BuildLegacySdrColors(HMONITOR monitor,
                                                std::array<RenderColor, 8>& colors) const
{
    std::wstring profilePath;
    if (!GetDisplayProfilePath(monitor, profilePath))
    {
        return false;
    }

    PROFILE profileDescriptor{};
    profileDescriptor.dwType = PROFILE_FILENAME;
    profileDescriptor.pProfileData = profilePath.data();
    profileDescriptor.cbDataSize = static_cast<DWORD>((profilePath.size() + 1) * sizeof(wchar_t));
    HPROFILE profile = OpenColorProfileW(&profileDescriptor,
                                         PROFILE_READ,
                                         FILE_SHARE_READ,
                                         OPEN_EXISTING);
    if (profile == nullptr)
    {
        return false;
    }

    LOGCOLORSPACEW source{};
    source.lcsSignature = LCS_SIGNATURE;
    source.lcsVersion = 0x400;
    source.lcsSize = sizeof(source);
    source.lcsCSType = LCS_CALIBRATED_RGB;
    source.lcsIntent = LCS_GM_GRAPHICS;
    const RgbColorSpaceDefinition& definition = ColorSpaceDefinition(gamut_);
    const Matrix3x3 sourceToXyz = BuildLinearRgbToXyz(definition);
    SetXyz(source.lcsEndpoints.ciexyzRed,
           sourceToXyz.values[0][0],
           sourceToXyz.values[1][0],
           sourceToXyz.values[2][0]);
    SetXyz(source.lcsEndpoints.ciexyzGreen,
           sourceToXyz.values[0][1],
           sourceToXyz.values[1][1],
           sourceToXyz.values[2][1]);
    SetXyz(source.lcsEndpoints.ciexyzBlue,
           sourceToXyz.values[0][2],
           sourceToXyz.values[1][2],
           sourceToXyz.values[2][2]);
    source.lcsGammaRed = ToCalibratedGamma(definition.calibratedGamma);
    source.lcsGammaGreen = source.lcsGammaRed;
    source.lcsGammaBlue = source.lcsGammaRed;

    HTRANSFORM transform = CreateColorTransformW(&source, profile, nullptr, BEST_MODE);
    if (transform == nullptr)
    {
        CloseColorProfile(profile);
        return false;
    }

    std::array<COLOR, 8> inputColors{};
    std::array<COLOR, 8> outputColors{};
    for (size_t index = 0; index < kTestColorSequence.size(); ++index)
    {
        const RgbColor rgb = TestColorRgb(kTestColorSequence[index]);
        inputColors[index].rgb.red = rgb.red > 0.0F ? 0xFFFF : 0;
        inputColors[index].rgb.green = rgb.green > 0.0F ? 0xFFFF : 0;
        inputColors[index].rgb.blue = rgb.blue > 0.0F ? 0xFFFF : 0;
    }

    const BOOL translated = TranslateColors(transform,
                                             inputColors.data(),
                                             static_cast<DWORD>(inputColors.size()),
                                             COLOR_RGB,
                                             outputColors.data(),
                                             COLOR_RGB);
    DeleteColorTransform(transform);
    CloseColorProfile(profile);
    if (!translated)
    {
        return false;
    }

    for (size_t index = 0; index < colors.size(); ++index)
    {
        colors[index] = {
            static_cast<float>(outputColors[index].rgb.red) / kWordMaximum,
            static_cast<float>(outputColors[index].rgb.green) / kWordMaximum,
            static_cast<float>(outputColors[index].rgb.blue) / kWordMaximum,
            1.0F,
        };
    }
    return true;
}

std::array<ColorManagedRenderer::RenderColor, 8>
ColorManagedRenderer::BuildAdvancedColorValues(const RgbColorSpaceDefinition& colorSpace,
                                               float referenceWhiteScale) noexcept
{
    std::array<RenderColor, 8> colors{};
    const Matrix3x3 sourceToScRgb = Multiply(kXyzToLinearSrgb,
                                             BuildLinearRgbToXyz(colorSpace));
    for (size_t index = 0; index < colors.size(); ++index)
    {
        // Every test component is an endpoint (0 or 1), so all supported transfer
        // functions map it to the same linear endpoint before matrix conversion.
        const RgbColor source = TestColorRgb(kTestColorSequence[index]);
        const RgbColor scRgb = Transform(sourceToScRgb, source);
        colors[index] = {
            scRgb.red * referenceWhiteScale,
            scRgb.green * referenceWhiteScale,
            scRgb.blue * referenceWhiteScale,
            1.0F,
        };
    }
    return colors;
}

std::array<ColorManagedRenderer::RenderColor, 8> ColorManagedRenderer::BuildFallbackSdrValues() noexcept
{
    std::array<RenderColor, 8> colors{};
    for (size_t index = 0; index < colors.size(); ++index)
    {
        const RgbColor rgb = TestColorRgb(kTestColorSequence[index]);
        colors[index] = {rgb.red, rgb.green, rgb.blue, 1.0F};
    }
    return colors;
}

const ColorManagedRenderer::WindowContext* ColorManagedRenderer::FindWindowContext(HWND window) const noexcept
{
    const auto result = std::find_if(windowContexts_.begin(),
                                     windowContexts_.end(),
                                     [window](const WindowContext& context) {
                                         return context.window == window;
                                     });
    return result != windowContexts_.end() ? &*result : nullptr;
}
}
