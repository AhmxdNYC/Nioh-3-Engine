# What keeps the screen black after cross-world Fight now (#9182, offline analysis, game not run)

Inputs: captures/pc/cover/ training_{OFF1,ON2,cOFF,cON,ccov,ccov2}.log, lum_*.csv (screen mean luminance, ~4 Hz),
shot_*.png; Ally bridge #732 (ev.tgz: A1-3 CoverReleaseMs -1, B1-3 CoverReleaseMs 0, Shingen; logs only, no lum csv).
Script: per click, the log events (FADE, COVER, CONTROL, GAMESTATE, XSTREAM seq transitions) vs. the first lum sample
> 15 that stays bright (dark = 2.5). ON1 / ccOFF / cFIX have no usable Training log for a cross-world click.

## pc table (seconds after the click = `XWORLD ... requested`; seq = mission sequencer state, decimal as logged)

| run (arm) | boss | click | control | FADE back in | GS2 -> 0 | seq 5 | seq 6 | seq 8 | seq 0xB (11) | seq 17 | **lum bright** | bright - seq 0xB | bright - FADE in |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| OFF1-1 (CR -1) | Shingen | 15:13:46.416 | 21.50 | 22.32 | 23.05 | 1.17 | 16.81 | 17.21 | 22.49 | 23.05 | **23.52** | 1.03 | 1.20 |
| OFF1-2 (CR -1) | Otoroshi | 15:15:57.976 | 15.28 | 15.78 | 15.08 | 1.56 | 12.74 | 13.55 | 14.55 | 15.08 | **16.30** | 1.75 | 0.52 |
| ON2-1 (CR 0) | Shingen | 15:25:25.514 | 6.33 | 6.83 | 6.14 (hand) | 1.26 | 14.79 | 22.35 | 33.29 | 33.84 | **34.38** | 1.09 | 27.55 |
| ON2-2 (CR 0) | Otoroshi | 15:27:51.058 | 9.54 | 10.04 | 9.36 (hand) | 1.68 | 9.84 | 16.12 | 22.26 | 22.80 | **23.19** | 0.93 | 13.15 |
| cOFF-1 (CR -1) | Shingen | 19:25:59.937 | 21.40 | 21.90 | 25.70 | 1.29 | 17.02 | 17.42 | 25.12 | 25.70 | **26.03** | 0.91 | 4.13 |
| cOFF-2 (CR -1) | Otoroshi | 19:28:24.734 | 17.23 | 17.74 | 17.03 | 1.74 | 14.69 | 15.58 | 16.49 | 17.02 | **17.93** | 1.44 | 0.19 |
| cON-1 (Near 1) | Shingen | 19:32:49.602 | 21.39 | 22.32 | 23.13 | 1.12 | 14.74 | 15.16 | 22.57 | 23.13 | **23.59** | 1.02 | 1.27 |
| cON-2 (Near 1) | Otoroshi | 19:35:15.019 | 23.36 | 23.87 | 23.15 | 1.78 | 19.43 | 20.71 | 22.62 | 23.15 | **24.35** | 1.73 | 0.48 |
| ccov-1 (CR -1) | Shingen | 19:41:51.035 | 21.66 | 22.17 | 23.02 | 1.20 | 17.86 | 18.29 | 22.47 | 23.02 | **23.92** | 1.45 | 1.75 |
| ccov-2 (CR -1) | Otoroshi | 19:44:14.678 | 16.76 | 17.27 | 16.56 | 1.80 | 14.06 | 14.92 | 16.02 | 16.55 | **17.37** | 1.35 | 0.10 |
| ccov2-1 (CR 0, EC72C2C4) | Shingen | 19:54:49.718 | 8.44 | 8.94 | 8.25 (hand) | 2.97 | 14.42 | 22.09 | 39.94 | 40.49 | **40.91** | 0.97 | 31.97 |
| ccov2-2 (CR 0, EC72C2C4) | Otoroshi | 19:57:21.332 | 6.26 | 6.76 | 6.07 (hand) | 1.92 | 14.59 | 20.93 | 25.30 | 25.83 | **26.68** | 1.38 | 19.92 |

Lum goes dark +0.3..0.6 s after every click (FADE to black at +0.00, OutMs 250; GAMESTATE[2]=1 and seq 22->1 at +0.55).

Ally (#732, no luminance): A1-3 control +29.4..31.0, FADE back in +29.9..31.5, seq 0xB +47.9 / +48.7 / +48.6, seq 17
+49.2..50.0 -> matches "black until ~47-50 s". B1-3 (CR 0): control +13.8..17.2, FADE in +14.4..17.8, seq 6 held
**36 s** (6 -> 7 at +57..59.5), the XSTREAM monitor stops at 60 s before 0xB -> matches "black until ~60-65 s".

## Conclusion: the last releaser is the game's own loading-screen widget, closed only by sequencer state 0xB

- In all 12 pc clicks the screen turns bright **0.9-1.75 s after seq 0xB** (223BE08 -> UiLoadingScreenEnd 3B1B6C),
  i.e. at seq 17-20 (0xF 223C308 30-dt60 timer + widget fade-out; same 0.59 s close tail as notes/loading-bottleneck.md).
  Never earlier, whatever we do with GAMESTATE[2] or our overlay.
- **Not our FADE overlay.** With CoverReleaseMs 0 the overlay fades in at +6.8..10.0 s but the screen stays black
  13-32 s longer. shot_ccov2_1+10s / +40s, shot_ON2_*+10s (all ~47 KB vs 33 KB for overlay-black frames) show a black
  frame **with the game's loading coin icon** bottom right: our overlay is gone and the game's loading screen
  (widget [45C84A8]+748h, opened by UiLoadingScreenStart 6372A0 in seq 1 223BDB0) is what is drawn.
  The overlay is the last releaser only in the Otoroshi hold runs, and only by 0.1-0.5 s (fade starts ~0.6 s after seq 0xB).
- **Not GAMESTATE[2].** Clearing it gives control at +0.5 s after ready, but the loading widget is tied to the sequencer,
  not to GS2. cOFF-1: GS2 -> 0 at +25.70, bright +26.03, but that is just seq 17 (GS2 is cleared by 0xF itself).
- No other black source seen: each lum trace has exactly one dark span (click+0.4 -> bright) and no second dark dip
  after it (no camera cut / render gap); the +40 s shots taken after the bright point (9-10 MB) show the scene.
  loading-bottleneck.md: the full scene renders behind the loading cover.
- Clearing GS2 early makes the black LONGER: seq 6 (actors ready, 223C53C) holds 6.3-7.7 s (pc) / 36 s (Ally) instead
  of 0.4-1.3 s, and seq 8 (G9 loader idle) 4.4-17.8 s instead of 0.9-7.7 s, because gameplay streaming resumes and
  queues more loader work. Shingen: 34.4 / 40.9 s (CR 0) vs 23.5-26.0 s (CR -1).
- Side observation: in 3 of 4 Shingen hold runs seq 8 -> 9 happens 0.15-0.3 s after our hold ends (CONTROL/FADE in,
  +21.4..22.3), i.e. something our ARENA hold does (player pin / XFORCE centre / hold timer) may keep G9 busy until
  AreaWaitCrossMs (15000) expires. Cheap ini test of that: AreaWaitCrossMs 5000 with CR -1.

## Cheapest next probe / patch

Close the loading widget ourselves when the arena is ready: on the game thread (pump tick), at the existing
"COVER +312 ms ... cleared by hand" site (or "FADE back in"), call **UiLoadingScreenEnd 3B1B6C** (`void(void)`, closes
[[45C84A8]+748h] via vfunc +38h(0)) once per cross-world Fight now, and keep CoverReleaseMs **-1** (do NOT clear GS2,
so seq 6/8 are not slowed). Log the lum/shot as now. Expected: bright ~+1 s after the call (Shingen ~+7..22 s instead of
23-26 s; with GS2 held the fight is still frozen until seq 17, so pair it with the control release only if the probe shows
the scene). Risks: seq 0xB calls it again (likely a no-op close); the tip page byte 45BC467 path is not involved for
mode-1/2 warps (unverified). Fallback ini-only probe if no code change is wanted: AreaWaitCrossMs 5000 (tests the side
observation, may shave ~10 s off Shingen OFF).

## Probe result (pc, RX 9070, asi d3aa82c8 = LoadingScreenEndMs 0, CoverReleaseMs -1, 1 run per boss, 21:17-21:23)
UiLoadingScreenEnd called ~6.3 s (Shingen) / 9.6 s (Otoroshi) after the fade started, but luminance stayed black: Shingen 25.5 s total (OFF ~22-26), Otoroshi 14.8 s (OFF 17.8). Bright came ~4 s after our 'FADE back in' / control release, ~19 s (Shingen) after the call. Calling UiLoadingScreenEnd by hand does NOT close the screen: the loading screen is re-asserted or driven by state until the sequencer reaches 0xB. Not a win.
