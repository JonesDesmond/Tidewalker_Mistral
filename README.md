# Tidewalker

An open-world adventure on a coastal archipelago where **the tide goes out for
miles twice a day — and the map changes on a clock.**

At **low tide** you walk the exposed seabed to shipwrecks, sunken shrines and
flooding grottos. At **high tide** those places are underwater: you sail or
swim, and new surface routes open. Scout a spot, plan around the tide, get in,
and get out before the water comes back. At night, when you are caught out,
the archipelago gets a little bit haunted.

Tidewalker is a **native desktop game** (no browser, no engine dependencies):
a single C++17 binary with a built-in software 3D renderer, so it runs even
without a GPU. Graphics are rendered at higher-than-screen resolution and
downsampled (supersampling), with procedural terrain, animated water, day/night
cycle, fog, lantern lighting and post-processing (vignette + film grain).

![low tide](docs/screenshot-low.png) ![high tide](docs/screenshot-high.png)

## Gallery (in-game screenshots, rendered headless at 1280x720)

| | | |
| --- | --- | --- |
| [Low tide — exposed seabed](docs/gallery/01-low-tide-seabed.png) | [High tide — flooded channels](docs/gallery/02-high-tide-flooded.png) | [Shipwreck + relic at low tide](docs/gallery/03-wreck-relic.png) |
| [Grotto interior](docs/gallery/04-grotto-interior.png) | [Night, lantern light](docs/gallery/05-night-lantern.png) | [Underwater at high tide](docs/gallery/06-underwater.png) |
| [Stilt village](docs/gallery/07-stilt-village.png) | [Lighthouse isle](docs/gallery/08-lighthouse.png) | [Sailboat at high tide](docs/gallery/09-sailboat-high-tide.png) |
| [Morning ebb, 2x supersampling](docs/gallery/10-ebb-morning-2x.png) | [Flood start](docs/gallery/11-flood-start.png) | [Flood full — same camera](docs/gallery/12-flood-full.png) |

The last two are the same camera seconds apart at 40x time warp: watch the
channel fill in. All shots produced by `tidewalker --headless --out ...`.

## Animated tide cycle

[docs/tide-cycle.html](docs/tide-cycle.html) is a self-contained flipbook of
**60 frames covering one full tide cycle** (HIGH -> EBB -> LOW -> FLOOD, day
into night) from a fixed camera over the wreck channel — open it in a browser
from the cloned repo. Also included:

- [docs/tide-cycle.apng](docs/tide-cycle.apng) — the same 60 frames as one
  animated PNG (plays in Chrome/Firefox/image viewers),
- [docs/tide-cycle/](docs/tide-cycle) — the raw frames.

Reproduce or reassemble:

```sh
./build/tidewalker --headless --frames 60 --every 1 --out docs/tide-cycle \
    --width 960 --height 540 --ssaa 1 --phase high --tod 0.30 --warp 360 \
    --cam 180,55,60,2.6,-0.42
python3 tools/make_apng.py docs/tide-cycle docs/tide-cycle.apng 10
ffmpeg -framerate 10 -i docs/tide-cycle/frame_%04d.png -c:v libx264 -pix_fmt yuv420p tide-cycle.mp4
```

## How to play

| Key | Action |
| --- | --- |
| WASD | move (walk on the seabed / swim / sail) |
| Space | jump |
| Shift | sprint |
| Arrow keys | orbit camera |
| E | interact — take relic, board/disembark boat, climb lighthouse |
| T | toggle time warp x8 (watch the tide come in fast) |
| R | respawn at the stilt village |
| Esc | quit |

**The loop:** each tide cycle is HIGH (45s) → EBB (2:15 falling) → LOW (45s)
→ FLOOD (2:15 rising), repeating every 6 minutes. The tide gauge top-left shows
the water level, the phase and the countdown; a bell rings at every phase
change. Collect all **5 relics** (wreck, grotto, shrine, lighthouse, village) —
but the grotto floods from the inside, the boat beaches when the water drops,
and at low tide something pale stands on the seabed.

## Run it

### Linux — .deb installer (Debian/Ubuntu)

```sh
packaging/linux/build-deb.sh          # builds + packages tidewalker_1.0.0_amd64.deb
sudo dpkg -i tidewalker_1.0.0_amd64.deb
tidewalker                            # or launch from the applications menu
sudo dpkg -r tidewalker               # uninstall
```

Or a user-local install without root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
packaging/linux/install.sh            # installs to ~/.local/bin
tidewalker
```

### Windows

Full step-by-step guide: **[packaging/windows/BUILD-WINDOWS.md](packaging/windows/BUILD-WINDOWS.md)**.
TL;DR:

```bat
git clone https://github.com/JonesDesmond/Tidewalker_Mistral.git
cd Tidewalker_Mistral
:: in "x64 Native Tools Command Prompt for VS 2022":
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build --config Release -j
build\Release\tidewalker.exe
```

(MinGW alternative and installer packaging — Inno Setup `Tidewalker.iss` or
`install.ps1` — are in the guide. No libraries needed; no GPU required.)

### Run it right now (no display needed — autopilot demo)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/tidewalker --demo --frames 540 --every 90 --out docs/demo --warp 40
```

`--demo` plays the game by itself headless: it walks to the wreck as the tide
falls, loots the relic, reaches the boat, boards at high tide and sails out —
printing an event log and dumping frames (a recorded run is in
[docs/demo/](docs/demo)). On a machine with a display, plain `tidewalker`
(or `tidewalker.exe` on Windows) opens the interactive window.

### Options

```
tidewalker --width 1280 --height 720 --ssaa 1.25   # window + supersampling
           --tide-period 360 --tod 0.30 --phase low|high|ebb|flood
           --headless --frames 60 --out frames --every 10   # render PNG frames, no window
           --bench                                          # fps benchmark
           --selftest                                       # logic tests
           --cam x,y,z,yaw,pitch --player x,z               # scripted viewpoint
```

Lower `--ssaa` to 1.0 on weak machines. Headless mode needs no display and is
how the screenshots in `docs/` were produced.

## Verify

```sh
./build/tidewalker --selftest        # 15 logic checks: tide model, relics,
                                     # boarding/sailing/aground, grotto flood,
                                     # drowned, death/respawn
./build/tidewalker --bench --frames 300
```

## Tech

- `src/renderer.*` — software rasterizer: perspective-correct gouraud shading,
  16-bit depth buffer, near-plane clipping, alpha blending, SSAA downsample
  with vignette/grain, 5x7 bitmap font HUD.
- `src/world.*` — procedural archipelago (value-noise heightmap, 7 islands,
  carved grotto), tide state machine, POIs and prop meshes.
- `src/game.*` — player/boat physics, swim/stamina/breath, interactions,
  hazards (grotto flood, the drowned), day/night sun, camera, HUD.
- `src/platform_linux.cpp` — X11 window via `dlopen` (zero build-time deps).
- `src/platform_win32.cpp` — raw Win32 window + DIB framebuffer (`#ifdef _WIN32`).
- `src/main.cpp` — entry point, CLI, headless renderer for testing.

The full design (loop, tide model, horror layer, progression) is in
[docs/DESIGN.md](docs/DESIGN.md).
