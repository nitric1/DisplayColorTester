#pragma once

#include "TestRenderer.h"

namespace DisplayColorTester
{
class DisplayP3Renderer final : public TestRenderer
{
public:
    DisplayP3Renderer() = default;
    ~DisplayP3Renderer() override = default;

    bool AttachWindow(HWND window) noexcept override;
    void DetachWindow(HWND window) noexcept override;
    void PaintWindow(HWND window, TestColorId color, bool overlayVisible) const noexcept override;

private:
    using RenderColor = std::array<float, 4>;

    enum class OutputMode
    {
        LegacySdr,
        AdvancedColor,
    };

    struct AdvancedColorState
    {
        bool active{};
        bool hdr{};
        float sdrWhiteScale{1.0F};
    };

    struct WindowContext
    {
        HWND window{};
        OutputMode outputMode{OutputMode::LegacySdr};
        std::array<RenderColor, 8> colors{};
        Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTarget;
        Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dContext;
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> darkBrush;
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> lightBrush;
    };

    bool EnsureDeviceResources() noexcept;
    bool CreateWindowContext(HWND window, WindowContext& context);
    bool CreateSwapChainResources(HWND window,
                                  DXGI_FORMAT format,
                                  DXGI_COLOR_SPACE_TYPE colorSpace,
                                  float referenceWhiteScale,
                                  WindowContext& context) noexcept;
    [[nodiscard]] AdvancedColorState QueryAdvancedColorState(HMONITOR monitor) const;
    [[nodiscard]] bool BuildLegacySdrColors(HMONITOR monitor,
                                            std::array<RenderColor, 8>& colors) const;
    [[nodiscard]] static std::array<RenderColor, 8> BuildAdvancedColorValues(float referenceWhiteScale) noexcept;
    [[nodiscard]] static std::array<RenderColor, 8> BuildFallbackSdrValues() noexcept;
    [[nodiscard]] const WindowContext* FindWindowContext(HWND window) const noexcept;

    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext_;
    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory_;
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory_;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
    std::vector<WindowContext> windowContexts_;
};
}
