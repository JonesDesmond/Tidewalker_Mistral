# Tidewalker — design doc

## Premise

On a coastal archipelago, the tide goes out for miles twice a day.

## Hook

The map changes on a clock. At low tide you walk the seabed to shipwrecks,
sunken shrines and caves. At high tide those places flood, so you sail or swim,
and new routes open on the surface.

## Loop

1. **Scout** a spot (from a headland, the lighthouse, or by boat).
2. **Plan around the tide** — the gauge shows phase + countdown; every POI
   has a time cost.
3. **Get in** — walk the seabed, swim, or sail.
4. **Get out before the water comes back** — the FLOOD phase is a visible,
   audible race: channels turn into rapids.

## Tone

Salty, moody, a bit of horror when you are caught out at the wrong time:
fog, lantern light at night, muffled underwater view, pale figures on the
seabed, a grotto that floods from the inside while you are still in it.

## Tide model (implemented)

Cycle = `--tide-period` seconds (default 360):

| Phase | Duration | Sea level |
| --- | --- | --- |
| HIGH | 12.5% (45s) | +3.0 m |
| EBB | 37.5% (135s) | +3.0 → −2.0 (smoothstep) |
| LOW | 12.5% (45s) | −2.0 m |
| FLOOD | 37.5% (135s) | −2.0 → +3.0 (smoothstep) |

World geometry is static; a single sea level drives walk/swim/sail eligibility,
rendering (animated wave surface, shoreline foam), the grotto flood, drowned
visibility and boat grounding.

## World (implemented vertical slice)

1600×1600 m archipelago, 7 procedural islands:

- **Stilt village** (main island) — spawn, safehouse, chest relic, dock + boat.
- **Wreck** in the channel — hull, mast, ribs; relic on deck. Walkable at
  low tide, swimmable at high.
- **Grotto** on the east isle — rock walls + overhead slab; dry at low tide,
  floods completely at high tide (breath drains → drowning).
- **Sunken shrine** on the seabed — torii gate, pillars, altar relic.
- **Lighthouse isle** — climbable tower (E at base), relic at the lamp room,
  rotating beam at night.
- **The Drowned** — pale figures standing on the seabed; they only exist at
  low tide and damage you if you stand too close.

## Controls of the slice

Walk / swim (stamina) / sail (boat grounds in shallow water — wait for the
flood), jump, camera orbit, interact, time-warp toggle, respawn. Win state:
all 5 relics.

## Graphics approach

No GPU dependency: a custom software rasterizer (perspective-correct gouraud,
16-bit z-buffer, alpha-blended water, near-plane clipping) renders at
`window × --ssaa` (default 1.25×) and downsamples — supersampled edges, the
cheapest high-quality "upscale" possible without a GPU. Add per-vertex sun +
lantern lighting, distance fog, day/night sky with sun/moon/stars, animated
Gerstner-ish wave surface with depth-based color and shoreline foam, then
vignette + film grain in the downsample pass. Post: underwater tint when the
camera is submerged. Target: 30+ fps at 1280×720 on a CPU (measured ~35 fps
at 1600×900 internal in the sandbox).

## Possible next steps

- GPU backend (OpenGL/Vulkan) behind the same renderer interface, with FSR/DLSS
  upscaling on machines that have one.
- More POIs, quests that only exist at certain tides, diving with air pockets,
  a surface predator at high tide, proper audio (bell, waves, whispers).
- Save games, gamepad input, settings menu.
