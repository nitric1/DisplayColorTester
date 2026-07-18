# Project Instructions

## C++ and Win32 conventions

- Spell the Win32 callback calling convention as `__stdcall`; do not use the `CALLBACK` macro.
- Put all C and C++ standard library headers included with angle brackets in `Common.h`. Do not include standard library headers directly from other project headers or source files.
- Prefer C++ fundamental or standard fixed-width integer types over Win32 aliases that only rename the same underlying integer type. Examples include `UINT` to `unsigned`, `INT` to `int`, `LONG` to `long`, `UINT_PTR` to `uintptr_t`, `INT_PTR` and `LONG_PTR` to `intptr_t`, and `ULONGLONG` to `uint64_t`.
- Keep semantically meaningful Win32 API and ABI types such as `BOOL`, `DWORD`, `WPARAM`, `LPARAM`, `LRESULT`, `COLORREF`, and handle types when they communicate API meaning or are part of a Windows signature.
