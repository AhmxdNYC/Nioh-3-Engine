# Training: Time of day slider (+ weather) - feasibility (#22942 / #22943)

2026-10-22, offline only (static: re/Nioh3.dump.exe via tools/n3dis.py, D:/nioh3-re/decomp, re/out/renderconfig_fields.tsv,
package env DBs read with data/tools n3rdb/n3kod). Game not launched, no memory written. Scripts: scratch/tod22942/.

**Verdict: feasible (yes for time of day, maybe-yes for weather).** The game has a continuous, per-frame sky state (a
physically based Sky2 sky with sun, moon and stars). Time of day is one float, `SkylightDateTime` (0..1 of a day), in the
active camera/RenderConfig object. We can override it every frame from a 5-byte call-site hook. The game freezes the clock
itself (no auto-advance), so a written value stays put.

Confidence tags: **[dec]** read in disasm/decompile, **[data]** read from shipped data, **[inf]** inference, not yet run.

## 1. Where the time of day lives

| claim | evidence | conf |
|---|---|---|
| The active environment block is `S = [[B+0x4751490]]+0x28` (`GetActiveSessionObj` 164188). It is the same object GameUpdateTick passes as r8 to `RenderFrameSettingsApply` 39B570 (call at 2339F7, after `164188` at 2339C6) | disasm 233940..2339F7 | high [dec] |
| S uses the Camera::Perspective / RenderConfig field layout (re/out/renderconfig_fields.tsv): +7C SkylightCloudy, +80 Temperature, +84 Humidity, +88 Precipitation, +8C Foggy, +90 Hazy, +98 SkyClearly, +AC SkylightMonthDay, **+B0 SkylightDateTime**, +B4 Longitude, +B8 Latitude, +C0 NorthDir, +C4 MoonAge, +CC TimeSpanPerDay, +158 KeyLightSunMargin, +174 AutoDateTimeStepType, +1D4 TimeOfDay, +1D8 Weather, +15C8 RainDropPrecipitation | field table + the reads below | high [dec] |
| `SkySunMoonDirections` 5EBD08 is called (e.g. from 5EBB30) with S+B4/B8/C0 in degrees, **arg7 = S+AC (MonthDay)** and **arg8 = max(S+B0, 0) (DateTime)**. Season angle = (MonthDay - 3.7)*pi/6 (3.7 is about the March equinox). Hour angle = (2*DateTime - 1)*pi + season - longitude. So DateTime is the fraction of a day: 0.5 = noon, 0 / 1 = midnight. The moon uses MoonAge. 5EBB30 returns "sun.y <= S+158" (sun below the key-light margin, i.e. night) | disasm 5EBB30..5EBBE0, decompile 0x5ebd08.c | high [dec] |
| Same layout read by 4D4524 (via the resource of type 38D04576 -> +28) and by the param-table copy in 5EDAC4 (+200..+218) | disasm | high [dec] |
| **No clock runs.** Every frame `RenderFrameSettingsApply` sets S+CC TimeSpanPerDay = 0x60AD78EC (~1e20 s per day) and S+174 AutoDateTimeStepType = 0 | 39B684 / 39B691 | high [dec] |
| `TimeOfDay` (+1D4) and `Weather` (+1D8) are dead fields: 0 in all 8,494 shipped presets. Do not use them | propstats over all 137 env DBs | high [data] |
| Sky ambient, clouds and VSM already handle a changing sun: SkyAmbientDetectChange 1467EC resets its time slicing when the sky params change, NatureSkyUpdatePatternParams 5F1628 re-renders the pattern map on change, and the VSM page cache invalidates on a light-direction change (notes/shadows-culling.md) | earlier notes | medium [dec] |
| Exposure, bloom, fog colours and key-light scale follow the sun through `SkylightDependingParams` ("KeysAnd...Values" tables keyed per preset; flags on in 6,025 / 8,494 presets), so night should get night exposure, not a black screen | data/dumps/0x396b9210 field list + propstats | medium [data/inf] |

Native game-side controls (useful later, not needed for v1):
- Mission script `WorldDateTime::*` (name table 0x3982600.., wrappers 23B97xx..23B9BB4): `LoadEnviromentSet(id)` (23B9858 -> 1FCCA6C on `[g_pSceneDrawCfg 0x45B9E58]+0x450`), `IsLoadEnviromentSet`, `SetSceneProgress` / `...NoEnvChange` (23B9A54), `Set/ClearForceLocationRate` (writes +4C0..+4D0), `SetSkylightPrecipitation` / `Reset...` (23B9BB4 -> 2239EEC). These switch between authored **discrete environment sets** (asset load, async). They are not a slider. [dec]
- World-env state `[B+0x4757710]`: +1C/+20 precipitation override {value, on} (set by the script call above), +24/+28 NorthDir override (Graphics::SetSkylightNorthDir, applied by 97BB50 at 39B64C), **+2C storm weight**. `39BED0` (called via 39BEA4 at 39B67D) lerps S toward a storm by that weight: Precipitation -> 0.06, Cloudy -> 1, VolumetricFogKeyLightScatterScale (+13C8) -> 12, GustPower (+F70) -> 750, GustIntervalPower (+F84) -> 550, Tree2WindScaler (+1968) -> 2. Who writes +2C was not found. [dec]
- That in-place lerp running every frame implies S is re-filled from the preset each frame. samelook (notes/same-look-package.md) measured a per-frame re-fill for +144 / +7C0 / +1394. For +B0 it is **expected, not proven** (the probe checks it). [inf]

## 2. Do the Practice arenas have it?

Practice / Training fights run in the boss's own mission and arena (notes/training-stage-contract.md). The arenas use the
stage's world environment DB. Shipped values, per env (data, `scratch/tod22942/perenv.py`; each env has 62 area presets):

| env | SkylightDateTime values (most common first) | Precipitation | SkyRenderType |
|---|---|---|---|
| S00_EDOWORLD_00 / _01 | 0.43, 0.51, 0.33, 0.47, 0.72 / 0.5, 0.4, 0.43, 0.35, 0.55 | 0..0.2 | 1 |
| S01_SENGOKUWORLD_00 / _01 / _02 | 0.4, 0.51, 0.43 / 0.43, 0.5, 0.35 / **0.825, 0.86** (dusk), 0.43 | 0..0.25 | 1 |
| S02_HEIANWORLD_00 | 0.33, 0.55, 0.51, 0.4, 0.43 | 0..0.1 | 1 |
| S03_KODAIWORLD_00, S04_BAKUMATSUWORLD_00 | 0.4, 0.43, 0.55, 0.51 / 0.4, 0.43, 0.5, 0.51 | 0..0.2 | 1 |
| S90_DLC1, S91_DLC2 | 0.43, 0.62, 0.37, 0.68 / 0.67 | 0..0.1 | 1 |
| A01_HAZAMA (hub) | 0.43, 0.51, 0.37, 0.33, 0.47 | 0..0.2 | 1 |
| A01_DOUJOU (dojo) | 0.62, 0.5, **0.15, 0.065** (night), 0 | 0..0.2 | 1 |

- **Every gameplay env has SkyRenderType 1 (Sky2 sky) and a real DateTime.** Only 2 cutscene envs (EVT_EV2103, EVT_PRM_EV1101B)
  have no sky fields. So no Practice arena has "fixed lighting" in the data sense. [data, high]
- Caveat for interiors (castle halls, caves, the Hazama interior): the sky is the same, but you only see the sun through openings.
  There the slider mostly changes ambient / exposure. Some interiors may look nearly unchanged. Local lights (torches) never
  change. [inf]
- The authored values already include night (0.065 = 01:34, 0.15 = 03:36) and dusk (0.825 = 19:48). So the renderer and
  presets support night, and a slider just moves an existing input. [data]

## 3. Implementation plan (Nioh3Training + Rematch menu, like KiDamageDealtPct)

**Hook:** retarget the call at **39B67D** `48 8B CE E8 22 08 00 00` (`mov rcx,rsi; call 39BEA4`) using the existing
`NfPatchCall(site, B+0x39BEA4, stub, "TOD 39B67D")` pattern (same as the Ki damage hook at 329E9D). Detour
`void TodApply(void* S)`:
1. Call the original `39BEA4(S)`, so the native storm lerp runs first.
2. If Practice is active (same gate as the other PS_* rows: session running or a Training boss alive) and S is non-null and
   readable: write overrides into S.
3. Otherwise write nothing. S is re-filled each frame (to be confirmed in §4), so turning the option off restores the stock look
   on the next frame. Nothing needs to be saved or restored.

This runs on the game thread inside RenderFrameSettingsApply. It is after ApplyGraphicsSettings, 39BFA8 and the NorthDir
fix-up, and before the renderer vfuncs +4B8/+4B0 consume S. It does not touch Nioh3EngineFix's RCO1 site at 39B7CA.
Skip it while `IsEventScene` holds, so cutscenes keep their authored light.

**Settings** (`[Practice]` in Nioh3Training.ini, new PS_* ids; menu rows on the Practice hub, built with PstStep / PstClamp /
PstLabel / PstFooter like PS_KI_DEALT):
- `TimeOfDayMin = -1`: -1 = Stock (no write). 0..1410 in 30-min steps (Left/Right; hold = 2 h). Write
  `S+0xB0 = min/1440.0f`. Label "Time of day: Stock / 06:00 / ...". Suggested step list: Stock, 00:00, 03:00, 05:00, 06:00, 07:00,
  09:00, 12:00, 15:00, 17:00, 18:00, 19:00, 20:00, 22:00, or every 30 min if the menu supports long lists.
- Optional `SeasonMonth = 0` (0 = stock, 1..12 -> `S+0xAC = month + 0.5`). This changes sun height (winter low sun) and is cheap
  to add. Defer unless asked.
- `Weather = 0`: 0 Stock, 1 Clear (Cloudy 0.3, SkyClearly 1, Foggy 0, Hazy 0, Precip 0), 2 Overcast (Cloudy 1, SkyClearly 0,
  Humidity 0.7), 3 Rain (Overcast + Precipitation 0.2, RainDropPrecipitation 1), 4 Storm (Rain + the 39BED0 storm targets:
  Precip 0.06 .. 0.25, FogKeyLightScatter 12, GustPower 750, GustIntervalPower 550, Tree2WindScaler 2), 5 Snow (Rain +
  Temperature -20). Values are taken from the shipped distributions (Cloudy 0.3..1, Precip 0..0.25, Temperature -50..20).
  Write the S fields directly. Do not rely on world+2C (writer unknown).
- Pure decision code goes in practice_stats.h (or a new tod.h) with unit tests: step list, clamp, minutes->fraction, weather
  preset table. Glue in practice_stats_game.h: hook install on first non-stock value, `__try` around S reads/writes.

**Ranges / safety:** DateTime must stay in [0,1). The engine clamps below 0 with maxss but not above 1. Floats only, aligned
4-byte stores, game thread. Expected costs on a change: a VSM re-render burst (whole-page invalidation on sun move) and a
sky-ambient re-slice (43 steps). Both are by design, so dragging the slider may stutter for a frame or two. A constant value
costs nothing.

Effort: about 0.5 day code + tests (one call-site hook, two settings, six menu strings), then 1 in-game session.

## 4. In-game verification plan (read-only first)

1. **Probe (read-only, no hook, no writes):** from the Training tick, once per second while in a mission, read
   `p = [[B+0x4751490]]+0x28` (guarded) and log p plus floats +7C, +80, +84, +88, +8C, +90, +98, +AC, +B0, +B4, +B8, +C0, +C4,
   +15C8, and `[B+0x4757710]+0x2C`, with the mission id. Pass: p is stable within a stage. +B0 is in [0,1) and matches a value
   from the §2 table for that env. +AC / +B4 / +B8 match (e.g. Latitude 35.553 or 50, Longitude 139.644). +B0 is constant
   over 60 s.
2. **Re-fill check (read-only):** log +B0 and +88 in two consecutive frames near an area boundary (walk between presets).
   Values should step or blend there, which shows the per-frame re-fill from presets.
3. **Write test (INI-gated, Practice only):** enable the hook with TimeOfDayMin = 0 (midnight), then 720 (noon), then Stock.
   Screenshot each with the existing capture tools in one arena. Pass: sun / moon / stars visibly change. Stock restores the
   same image as before (shotdiff at the noise floor). No hitch beyond 1-2 frames on change. Log that the value read back
   at the next frame's probe equals the written one.
4. Weather presets: same A/B with Overcast / Rain / Snow in an open-sky arena (an S01 world boss) and in one interior
   arena to note the limitation.
5. Regression: cutscene (event scene) untouched, the title / hub unaffected when Practice is off, and Nioh3EngineFix
   RCO knobs still log normally (different site).

## 5. Open questions
- Whether +B0 is re-filled every frame (expected; probe step 2). If not, the detour must restore the stock value when switched off
  (keep the last stock value read before the first write).
- Who writes world+2C (storm weight), and whether Rain needs RainDrop resources loaded per stage. RainDrop objects are in
  the shared system DB 0xdab638b2, so probably global.
- Interiors: how much a sun change shows there; decide whether to hide the row in sky-less arenas.
