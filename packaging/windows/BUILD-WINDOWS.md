# Tidewalker on Windows — step-by-step

Tidewalker is portable C++17 with **no external libraries** (no SDL, no OpenGL,
no vcpkg). You only need a C++ compiler. The result is a single `tidewalker.exe`
that runs on any 64-bit Windows 10/11 — no GPU required.

Pick **Option A** (Visual Studio) if you have it or want the easiest setup;
pick **Option B** (MinGW) if you want a single static exe with zero runtime
dependencies.

---

## Option A — Visual Studio 2022 (recommended)

### 1. Install the prerequisites

1. Download **Visual Studio 2022 Community** (free): https://visualstudio.microsoft.com/downloads/
2. Run the installer and on the *Workloads* tab tick:
   **Desktop development with C++**
   (this includes CMake, MSVC and the Windows SDK).
3. Install **CMake** if it's not already on your PATH — the VS installer usually
   includes it; otherwise grab it from https://cmake.org/download/ and tick
   "Add CMake to the system PATH" during its install.

### 2. Get the source

```bat
:: open "x64 Native Tools Command Prompt for VS 2022" from the Start menu, then:
git clone https://github.com/JonesDesmond/Tidewalker_Mistral.git
cd Tidewalker_Mistral
```

(No git? Click **Code → Download ZIP** on the repo page and unzip instead.)

### 3. Build

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build --config Release -j
```

`-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` links the C++ runtime **statically**,
so the exe runs on any Windows PC **without installing the VC++ Redistributable**.
(If you skip that flag, players need https://aka.ms/vs/17/release/vc_redist.x64.exe.)

### 4. Run

```bat
build\Release\tidewalker.exe
```

A 1280×720 window opens. Controls:

| Key | Action |
| --- | --- |
| WASD | move (walk / swim / sail) |
| Space | jump |
| Shift | sprint |
| Arrow keys | camera |
| E | interact / take relic / board boat / climb lighthouse |
| T | time warp ×8 |
| R | respawn |
| Esc | quit |

Tune performance if needed: `tidewalker.exe --ssaa 1` (faster), `--width 1920 --height 1080 --ssaa 1.5` (sharper).

---

## Option B — MinGW-w64 (single static exe)

1. Install **MinGW-w64** (e.g. via MSYS2: https://www.msys2.org/ — `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake`) or grab a standalone build and add its `bin` to PATH.
2. Clone the repo (see above).
3. Build:

   ```bat
   cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j
   build\tidewalker.exe
   ```

MinGW builds link statically by default — no redistributable needed.

---

## Make an installer (optional but recommended)

### With Inno Setup (proper .exe installer + uninstaller)

1. Install **Inno Setup 6**: https://jrsoftware.org/isdl.php
2. Build the game first (Option A step 3 or B step 3) so the exe exists.
3. In Inno Setup Compiler, open `packaging\windows\Tidewalker.iss` and press **Compile**.
4. The installer is written to `packaging\windows\Output\Tidewalker_Setup_1.0.0.exe`.
   It installs to `C:\Program Files\Tidewalker`, creates Start Menu + Desktop
   shortcuts, and registers an uninstaller in *Settings → Apps*.

### Without Inno Setup (PowerShell only)

Run PowerShell **as Administrator**:

```powershell
powershell -ExecutionPolicy Bypass -File packaging\windows\install.ps1
```

This copies the built exe to `C:\Program Files\Tidewalker` and creates Start
Menu + Desktop shortcuts. (Pass `-ExePath build\Release\tidewalker.exe` if your
exe is elsewhere.)

---

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| `tidewalker.exe` won't start, "VCRUNTIME140.dll missing" | You built with Option A *without* the static-runtime flag — install the [VC++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe), or rebuild with `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`. |
| Windows SmartScreen warning | Normal for unsigned indie exes — click **More info → Run anyway**, or sign the exe with your own certificate. |
| `cmake` not recognized | Use the *x64 Native Tools Command Prompt for VS 2022* (Start menu), or add CMake's `bin` to PATH. |
| Window opens but is slow | Lower supersampling: `tidewalker.exe --ssaa 1`. It's a CPU renderer, so very weak CPUs may prefer 960×540: `--width 960 --height 540`. |
| Antivirus flags the exe | The renderer is a self-contained software rasterizer; add an exclusion or build it yourself from source (this repo) to verify. |
| Want to verify the code builds the same game | `tidewalker.exe --selftest` runs 15 logic checks in a console window; `tidewalker.exe --bench --frames 300` prints fps. |
