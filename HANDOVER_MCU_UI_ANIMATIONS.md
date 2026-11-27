# Handover: MCU home screen animations (branch `repetren/mcu-ui-animations`)

Written 2026-11-27 for the next agent picking this branch up. Everything below
is the state of the working tree at that moment: commits up to `309f13e` plus
the uncommitted dolphin work (see "What is not committed"). The code owner is
Vlad; he commits himself — never commit or push, leave changes in the tree.

Reply to Vlad in at most three sentences. He hands over mock-ups as PNGs and
expects pixel-perfect placement.

## 1. The screen

- Display is 258×144, 8-bit gray (0x00 black … 0xFF white), Clay layout engine
  on top of a software canvas (`applications/services/gui/`).
- The home screen (`applications/services/desktop/scenes/desktop_scene.c`) is a
  parallax scene plus floating elements: header, version text, three tabs.
- Settings and every other screen keep the classic compact header and the
  classic softkeys; only the home screen has the new look.

## 2. Parallax scene (`applications/services/gui/modules/parallax.[ch]`)

Scene coordinates are the artist's 256×144 canvas; the scene origin sits on the
screen at `HOME_SCENE_X = 1`, `HOME_SCENE_Y = -2` (so the whole scene is 2 px
higher than the mock-up and centred on the 258 px display). Layers are drawn
bottom to top. `offset += (speed * speed_mult + swim) * dt`; a layer with
`frame_count > 1` shows `frames[floor(t * fps) % n]`, phase-locked to the
clock. Redraw is 30 fps (`PARALLAX_FRAME_PERIOD_MS 33`) while anything moves.

| # | layer | asset(s) in `assets/home_screen/` | size | x | y | mult | swim | wrap | frames/fps |
|---|-------|-----------------------------------|------|---|---|------|------|------|------------|
| 1 | bg | home_bg | 256×65 | 0 | 42 | 0.8 | — | tile 256 | 1 |
| 2 | ship | home_ship | 73×58 | 99 | 44 | 0.7 | — | period 256+73 | 1 |
| 3 | fish right | home_fishr1/2 | 35×19 / 34×19 | −35 | 41 | 0.9 | +10 px/s | period, w 35 | 2 @ 3 fps |
| 4 | fish left | home_fishl1/2 | 35×19 / 34×19 | 256 | 73 | 0.9 | −10 px/s | period, w 35 | 2 @ 3 fps |
| 5 | ground | home_ground | 256×16 | 0 | 99 | 0.8 | — | tile 256 | 1 |
| 6 | water top | home_water_top | 256×15 | 0 | 27 | 0.8 | — | tile 256 | 1 |
| 7 | foliage | home_foliage_1/2 | 256×31 | 0 | 84 | 1.0 | — | tile 256 | 2 @ 2 fps |
| 8 | dolphin asleep | see §3 | 113 px box | 71 | 39 | 0 | — | none | 14 @ 2 fps, `ParallaxShowStill` |
| 9 | dolphin swimming | see §4 | 130 px box | 62 | 47 | 0 | — | none | 4 @ 4 fps, `ParallaxShowMoving` |
| 10 | dolphin swim-in | same as 9 | 130 px box | 62 | 47 | 0 | — | none | `ParallaxShowArriving`, from −192 px at 120 px/s |

Wrap arithmetic: `parallax_period_x`, `parallax_tile_first_x`,
`parallax_mirror_x` are inline in the header and covered by
`applications/debug/unit_test_app/parallax_test/`.

Scene speed (`desktop_update_scene_speed`): while the charger reports
*charging*, `speed = +100 px/s × input_W / 36`, at least 30 px/s — the scene
drifts **right**. Idle or full: 0. The speed is refreshed once a second from
`DesktopSceneEventTypePowerUpdate` and only while the Linux host is off
(`power_host_shares_bus()`: the power chips sit on an I²C bus Linux drives
too, and the MCU must not touch it while the host runs).

`face_against_motion = true` mirrors a layer in place (within its box) while
`speed > 0`, so the dolphin faces left while charging. The source art faces
right.

## 3. Sleeping dolphin (composite frames, `ParallaxComposite`)

Artist's canvas is 113 px wide; placed at scene (71, 39), so `sleepd_1` lands
at scene (71, 43). 14 frames at 2 fps, each a list of sprites `{image, x, y,
alpha}` on that canvas (alpha 255 = 100 %). Assets in `assets/desktop/`:
`sleepd_1` 113×55, `sleepd_2` 112×57, `sleepd_3` 112×54, `bubble_mid` 4×4,
`bubble_small` 3×3.

| frame | body | bubbles (asset x y opacity) |
|---|---|---|
| 1 | sleepd_1 (0,4) | — |
| 2 | sleepd_2 (1,5) | mid 99 19 100 % |
| 3 | sleepd_3 (1,6) | mid 102 13 75 %, small 100 21 100 % |
| 4 | sleepd_2 (1,5) | mid 100 6 55 %, small 103 16 50 % |
| 5 | sleepd_1 (0,4) | mid 97 0 15 %, small 102 5 5 % |
| 6 | sleepd_2 (1,5) | small 100 21 100 % |
| 7 | sleepd_3 (1,6) | small 104 13 60 %, mid 100 20 100 % |
| 8 | sleepd_2 (1,5) | small 102 3 20 %, mid 102 13 60 % |
| 9 | sleepd_1 (0,4) | mid 98 6 25 % |
| 10 | sleepd_2 (1,5) | mid 6 32 100 % |
| 11 | sleepd_3 (1,6) | mid 8 27 100 % |
| 12 | sleepd_2 (1,5) | mid 5 18 75 % |
| 13 | sleepd_3 (1,6) | mid 8 8 50 % |
| 14 | sleepd_2 (1,5) | mid 4 1 15 % |

Translucent sprites use `canvas_draw_image_alpha()` (per-pixel blend
`dst += (src − dst) × alpha / 255`), added to `clay_render.c` for this.

## 4. Swimming dolphin and the swim-in

Assets `swim1s` 127×47, `swim2s` 126×43, `swim3s` 120×44. **No artist
coordinates exist for these**: I aligned the three frames on the eye myself —
swim1 at (0,0), swim2 at (4,4), swim3 at (8,2) on a 130 px canvas, canvas at
scene (62, 47) so it is centred on the sleeping dolphin (71 + 113/2). Frame
order 1-3-2-3 at 4 fps (tail down, mid, up, mid). If the artist supplies
positions, replace these.

Rules:
- scene still → sleeping layer only;
- scene moving (charging) → swimming layer only, mirrored (faces left);
- boot → swim-in layer only: starts at `arrive_from = −(62 + 130)` px (fully
  off the left edge), closes in at 120 px/s (~1.6 s), stops, then the sleeping
  layer takes over. Plays once per boot: the `Parallax` is allocated in
  `desktop_on_alloc`, and the arrival state lives in it.

## 5. Header (`applications/services/desktop/scenes/header.c`)

Two heights: compact 11 px (every screen but home: old battery icon + percent)
and home 22 px. All x values are screen pixels plus `HEADER_X_OFFSET 2`.

- Battery `ElementsBattery` (41×17) at (212, 3); fill 34×13 at (2, 2) inside,
  `fill = ceil(34 × % / 100)`, percent text centred on x 19, y 5; bolt at
  (15, 3). Looks: Light (idle), Dark (charging, bolt), DarkStill (full).
- Idle: `Discharge: X.XXW` right-aligned to x 210, y 8 (FontBody).
- Charging: black plate from x 153, gray plate from x 66 (whole watts) or 58
  (fractional); line 1 `Charging: 5V 4.63W` at x 75/67, y 3; line 2 `Full in:
  0h 0m` at x 83/72, y 14; big percent `%.2f` right edge x 200, y 6 (FontBig),
  `%` sign as image `header_percent_big` at (199, 6). Plates slide in from 84
  px right over 200 ms: steps {84,77,71,63,56,50,42,35,29,21,14,8,0} at 16 ms.
- Full: plate 171 / gray 84, `Fully charged!` at (103, 8), percent `100`.
- Percent = gauge RemCap / FullCap; "full" = charger termination, not SoC
  (Vlad: showing full on SoC alone would lie to the user).
- Refresh 1 s (`DESKTOP_POWER_UPDATE_PERIOD_MS` in desktop.c).

## 6. Version text and tabs

- Version `[A] branch hash` FontBody colour 0xAA, box top-left (5, 112) so the
  glyphs start at y 113; home screen only.
- Tabs (`elements_home_tab_element`, `elements.c`): x {2, 52, 99, 154, 206} + 1,
  y 0 at the bottom edge; label padding top 4, slant nudge 4; Power tab has a
  9 px label box plus the icon and zIndex 1 (drawn over its neighbours).
  Assets `desktop_button_{help,power,settings}_{inactive,pressed}` and
  `desktop_button_power_icon{,_pressed}`. Power press → power menu, tab stays
  inactive. Settings is Help mirrored.
- The classic softkeys (`elements_softkey_button_element`) are untouched and
  still used by DFU/rollback/popup menus; `popup_menu_set_softkey_style` picks
  HomeTab only for the power menu.

## 7. Power meter LEDs (`applications/services/power_meter/`)

Consumption bars from charger ADC IBAT×VBAT (same figure Linux shows):
thresholds 0 / 300 / 1500 / 3000 / 5000 mW → 0…4 bars (green, yellow, orange,
red). Input thresholds 1 / 7.5 / 15 / 27 W. Tick 1 s. Under a running host no
bus reads at all (`POWER_METER_SHARED_BUS_READS 0`), so the LEDs freeze.

## 8. Assets, conversion, tests

- PNG → C via `scripts/image_convert.py` (CMake glob `assets/*/*.png`); a PNG
  with alpha becomes `ImageFormatRawGray8Mask` (1-bit mask, alpha ≥ 128 is
  opaque). Folders under `assets/animations/` become `ImageFormatAnimation`
  via `scripts/animation_convert.py` (unused now).
- Unit tests: `animation_test`, `parallax_test` in
  `applications/debug/unit_test_app/`.

## 9. What is not committed

Uncommitted in the tree (dolphin work): `parallax.[ch]`, `clay_render.[ch]`,
`desktop_scene.c`, `parallax_test.c`, new `assets/desktop/bubble_mid.png` and
`bubble_small.png`. Everything else up to `309f13e` is committed.

## 10. Open items from the artist's spec

- Swim-frame coordinates from the artist (replace my eye alignment, §4).
- `USB-C1 Power In` status line and the dolphin shift while charging.
- Discharging "Output / Left" header state and the `∞` glyph (shows `--`).
- Power meter alive under Linux (needs Linux to hand the figures over).

Good luck with it, and have a great weekend when you get there.
