#include "Common.h"

#include "DisplayP3Renderer.h"

namespace DisplayColorTester
{
namespace
{
constexpr float kTextSize = 40.0F;
constexpr float kShadowOffset = 2.0F;
constexpr float kWordMaximum = 65535.0F;
constexpr double kFixed2Dot30Scale = 1073741824.0;

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

long ToFixed2Dot30(double value) noexcept
{
    return static_cast<long>(value * kFixed2Dot30Scale + 0.5);
}

void SetXyz(CIEXYZ& destination, double x, double y, double z) noexcept
{
    destination.ciexyzX = ToFixed2Dot30(x);
    destination.ciexyzY = ToFixed2Dot30(y);
    destination.ciexyzZ = ToFixed2Dot30(z);
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

bool DisplayP3Renderer::AttachWindow(HWND window) noexcept
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

void DisplayP3Renderer::DetachWindow(HWND window) noexcept
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

void DisplayP3Renderer::PaintWindow(HWND window, TestColorId color, bool overlayVisible) const noexcept
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
        const wchar_t* text = TestColorName(color);
        const unsigned textLength = static_cast<unsigned>(lstrlenW(text));

        context->d2dContext->BeginDraw();
        context->d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
        context->d2dContext->DrawTextW(text,
                                      textLength,
                                      textFormat_.Get(),
                                      shadowRect,
                                      shadowBrush,
                                      D2D1_DRAW_TEXT_OPTIONS_NONE,
                                      DWRITE_MEASURING_MODE_NATURAL);
        context->d2dContext->DrawTextW(text,
                                      textLength,
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

bool DisplayP3Renderer::EnsureDeviceResources() noexcept
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

bool DisplayP3Renderer::CreateWindowContext(HWND window, WindowContext& context)
{
    const HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    const AdvancedColorState advancedColor = QueryAdvancedColorState(monitor);

    DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
    DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    float referenceWhiteScale = 1.0F;
    context.outputMode = OutputMode::LegacySdr;

    if (advancedColor.active)
    {
        context.outputMode = OutputMode::AdvancedColor;
        context.colors = BuildAdvancedColorValues(advancedColor.sdrWhiteScale);
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

bool DisplayP3Renderer::CreateSwapChainResources(HWND window,
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

DisplayP3Renderer::AdvancedColorState DisplayP3Renderer::QueryAdvancedColorState(HMONITOR monitor) const
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

bool DisplayP3Renderer::BuildLegacySdrColors(HMONITOR monitor,
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
    SetXyz(source.lcsEndpoints.ciexyzRed, 0.4865709486482162, 0.2289745640697488, 0.0);
    SetXyz(source.lcsEndpoints.ciexyzGreen, 0.2656676931690931, 0.6917385218365064, 0.0451133818589026);
    SetXyz(source.lcsEndpoints.ciexyzBlue, 0.1982172852343625, 0.0792869140937450, 1.0439443689009760);
    source.lcsGammaRed = 0x00023300;
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

std::array<DisplayP3Renderer::RenderColor, 8>
DisplayP3Renderer::BuildAdvancedColorValues(float referenceWhiteScale) noexcept
{
    std::array<RenderColor, 8> colors{};
    for (size_t index = 0; index < colors.size(); ++index)
    {
        const RgbColor p3 = TestColorRgb(kTestColorSequence[index]);
        colors[index] = {
            (1.22494017628F * p3.red - 0.224940176281F * p3.green) * referenceWhiteScale,
            (-0.0420569547097F * p3.red + 1.04205695471F * p3.green) * referenceWhiteScale,
            (-0.0196375545903F * p3.red - 0.0786360455506F * p3.green + 1.09827360014F * p3.blue) * referenceWhiteScale,
            1.0F,
        };
    }
    return colors;
}

std::array<DisplayP3Renderer::RenderColor, 8> DisplayP3Renderer::BuildFallbackSdrValues() noexcept
{
    std::array<RenderColor, 8> colors{};
    for (size_t index = 0; index < colors.size(); ++index)
    {
        const RgbColor rgb = TestColorRgb(kTestColorSequence[index]);
        colors[index] = {rgb.red, rgb.green, rgb.blue, 1.0F};
    }
    return colors;
}

const DisplayP3Renderer::WindowContext* DisplayP3Renderer::FindWindowContext(HWND window) const noexcept
{
    const auto result = std::find_if(windowContexts_.begin(),
                                     windowContexts_.end(),
                                     [window](const WindowContext& context) {
                                         return context.window == window;
                                     });
    return result != windowContexts_.end() ? &*result : nullptr;
}
}
