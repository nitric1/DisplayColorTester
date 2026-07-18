# DisplayColorTester

DisplayColorTester is a native Win32 C++ utility for showing the same sRGB test color on every active monitor.

## Current scope

- sRGB output through Win32 GDI `RGB(r, g, b)`
- One borderless windowed-fullscreen window per active monitor
- Color sequence: `#F00`, `#0F0`, `#00F`, `#FF0`, `#F0F`, `#0FF`, `#FFF`, `#000`
- Previous color: Left Arrow or right mouse button
- Next color: Right Arrow or left mouse button
- Exit the test: Escape
- Color-name and hex-code overlay for one second after each change
- Mouse cursor hidden after one second without mouse movement

Display-P3, Adobe RGB, BT.2020, ICC/WCS, and Windows Advanced Color output are planned but are not enabled yet.

## Build

Open `DisplayColorTester.slnx` in Visual Studio and build either the x64 Debug or x64 Release configuration.

See [Docs/ImplementationPlan.md](Docs/ImplementationPlan.md) for the staged implementation plan and validation checklist.
