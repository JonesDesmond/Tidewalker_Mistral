# Building Tidewalker on Windows

Tidewalker is portable C++17 with no external library dependencies.
The Windows build uses a raw Win32 window and a 32-bit DIB framebuffer
(`src/platform_win32.cpp`), so only a C++ compiler is needed — no SDL,
no OpenGL, no vcpkg.

## Option A: Visual Studio (recommended)

1. Install **Visual Studio 2022** with the *Desktop development with C++* workload.
2. Open a **x64 Native Tools Command Prompt** (or run `vcvars64.bat`).
3. From the repo root:

   ```bat
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release -j
   build\Release\tidewalker.exe
   ```

## Option B: MinGW-w64

```bat
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
build\tidewalker.exe
```

## Packaging an installer

With [Inno Setup 6](https://jrsoftware.org/isinfo.php) installed, compile
`packaging\windows\Tidewalker.iss` — it produces `Tidewalker_Setup_1.0.0.exe`
with a Start-menu shortcut and an uninstaller entry in *Add/Remove Programs*.

Or run `packaging\windows\install.ps1` in PowerShell (as Administrator) to
copy the exe to `C:\Program Files\Tidewalker` and create shortcuts directly.

## Controls

| Key | Action |
| --- | --- |
| WASD | move (walk / swim / sail) |
| Space | jump (on land) |
| Shift | sprint |
| Arrow keys | orbit camera |
| E | interact / board / disembark / climb |
| T | toggle time warp x8 |
| R | respawn at the village |
| Esc | quit |
