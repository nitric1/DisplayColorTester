# DisplayColorTester

DisplayColorTester is a native Win32 C++ utility that shows the same selected-gamut test color on every active monitor. It is intended for visually comparing display color alignment across multi-monitor setups.

## Features

- Five test modes: sRGB, Display-P3 (P3-D65), Adobe RGB, BT.2020, and Display Native RGB (Best effort)
- Color and Grayscale test patterns available for all five modes
- One borderless, topmost, windowed-fullscreen window per active monitor
- Per-monitor rendering paths for mixed legacy SDR, Advanced Color SDR, and HDR configurations
- Keyboard and mouse test-patch navigation
- Automatic test-session shutdown when the display configuration changes

## Color modes

| Mode | Output behavior |
| --- | --- |
| sRGB | Uses Win32 GDI `RGB(r, g, b)` and `COLORREF`. |
| Display-P3 (P3-D65) | Uses FP16 scRGB on Advanced Color displays. On legacy SDR displays, the source gamut is converted to the monitor profile through ICC/WCS. |
| Adobe RGB | Uses the Adobe RGB (1998) primaries and D65 white point with the same Advanced Color and legacy SDR paths as Display-P3. |
| BT.2020 | Uses the BT.2020 primaries and D65 white point with the same Advanced Color and legacy SDR paths as Display-P3. |
| Display Native RGB (Best effort) | On Advanced Color displays, converts the primaries and white point reported by DXGI to FP16 scRGB. On legacy SDR displays, outputs full-range device RGB without ICC/WCS conversion. |

The eight test colors are shown in this order:

Red, Green, Blue, Yellow, Magenta, Cyan, White, Black

The Grayscale pattern shows Gray 0% through Gray 100% in 10% steps. Standard modes interpret these as encoded code values. Display Native RGB uses device RGB code values on legacy SDR and a best-effort linear-light ramp on Advanced Color.

## Usage

1. Launch `DisplayColorTester.exe`.
2. Select a color mode.
3. Select the Color or Grayscale test pattern.
4. The main window is hidden and a test window opens on every active monitor.
5. Use the following controls during the test:

   - Next test patch: Right Arrow or left mouse button
   - Previous test patch: Left Arrow or right mouse button
   - Close the test and return to the main window: Escape

The overlay shows the selected mode and test patch, for example `Display-P3 (P3-D65) - Red (#F00)` or `sRGB - Gray 30%`.

If the active display configuration changes, the test closes and the main window reports that the session ended.

## Display Native RGB diagnostics and limitations

Each monitor's Display Native RGB overlay identifies the selected output path:

- Advanced Color HDR/SDR: reported bit depth and DXGI primary/white-point coordinates
- Missing or invalid Advanced Color primaries: BT.2020 estimate for HDR or sRGB estimate for SDR
- HDR luminance metadata: reported full-frame white target, or an SDR-white estimate when unavailable
- Legacy SDR: full-range device RGB with the ICC profile bypassed

Display Native RGB is a best-effort diagnostic mode. It targets digital RGB values and the reported native gamut boundary, but it cannot directly control physical subpixels or guarantee measured maximum panel luminance. Because DXGI does not report a native transfer curve, Advanced Color Grayscale is a linear-light ramp rather than a direct device code-value test. GPU/Windows transforms, display calibration data, monitor processing, tone mapping, local dimming, and the physical panel structure can all affect the result. A colorimeter or spectroradiometer is required for measurement-grade verification.

## Build

Requirements:

- Windows x64
- Visual Studio with the C++ Desktop toolchain, the v145 platform toolset, and Windows SDK 10.0

Open `DisplayColorTester.slnx` in Visual Studio and build either `Debug|x64` or `Release|x64`.

From a Visual Studio Developer PowerShell prompt, the equivalent commands are:

```powershell
msbuild DisplayColorTester.slnx /t:Build /p:Configuration=Debug /p:Platform=x64
msbuild DisplayColorTester.slnx /t:Build /p:Configuration=Release /p:Platform=x64
```

See [Docs/ImplementationPlan.md](Docs/ImplementationPlan.md) for the staged implementation history, rendering policy, limitations, and validation checklist.
