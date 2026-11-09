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

## Graphics approach (v2 — renderer overhaul)

No GPU dependency: a custom **multithreaded** software rasterizer (5 cores,
per-band worker buffers + z-merge) renders at `window × --ssaa` (default
1.25×) and downsamples. Features:

- perspective-correct **bilinear texture mapping** with procedurally
  generated textures (sand/grass/rock/wood/stone/foliage/clouds) and
  **3-way terrain splatting** (sand/grass/rock by height+slope), terrain
  vertex AO and real normals, scattered grass tufts / rocks / kelp
  (alpha-tested), blob shadows, animated limb-swing on the player;
- **per-pixel lighting**: sun diffuse + specular, lantern point light,
  distance fog; **water shader** with analytic wave normals, fresnel sky
  reflection, sun glints (Blinn pow 220), depth-based color, shoreline foam;
- **sky pass**: gradient + procedural cloud layer + sun/moon glow + stars;
  emissive hut windows and lighthouse lamp/beam at night;
- **post**: ACES tonemap + bloom (1/4-res bright pass, separable blur) +
  saturation + vignette + film grain; underwater tint when submerged.

Measured in the build sandbox (5 CPU cores): ~28 fps at 1280×720 window
(1600×900 internal, ssaa 1.25); ~34 fps at ssaa 1. Self-test: 15/15.

## Possible next steps

- GPU backend (OpenGL/Vulkan) behind the same renderer interface, with FSR/DLSS
  upscaling on machines that have one.
- More POIs, quests that only exist at certain tides, diving with air pockets,
  a surface predator at high tide, proper audio (bell, waves, whispers).
- Save games, gamepad input, settings menu.
