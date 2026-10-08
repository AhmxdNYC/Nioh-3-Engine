# Ally X input-latency procedure (240 fps phone slow-motion)

Protocol item 3. No Linux tool reads end-to-end latency, so we film button press -> screen change. Absolute numbers include finger
travel and panel scan-out; **only the differences between the three configs are the result**. Same everything else in all three.

## Configs (one filming session each, same save, same spot, same time of day, battery or plugged in the same way)
| tag in CSV | game | mod | FG |
|---|---|---|---|
| STOCK | Steam game alone | `Nioh3EngineFix` asi/ini removed (or `[Patches]` all 0 and no ShaderSwap, no shader replacements) | game FG as the user set it for "stock" (record it) |
| OPTIMIZED | mod ALL-KEPT ini + kept shader swaps | on | FG OFF |
| OPTIMIZED_FG | as OPTIMIZED | on | FG ON (same FG type/mode as the PC run) |
Keep REN_FPS, VSYNC, SCR_REZ, RESOLUTION_SCALING, REN_RAT identical across configs except where STOCK/FG require otherwise; write
what you changed in the CSV `note` of press 1. Use `cfgswitch.sh show` and paste it in the run notes. Set the phone/camera, not the Ally.

## Filming
1. Phone on a tripod (or clamp), 240 fps slow-motion (iPhone "Slo-mo 1080p 240" / Android 240 fps). Lock exposure and focus
   (AE/AF lock), turn OFF stabilisation, torch off, room lit steadily (no 50/60 Hz flicker bands: a daylight or DC lamp).
2. Frame so both are visible and sharp: the Ally X right-hand face buttons (the finger and the pressed button) AND the whole screen.
   Camera slightly above and in front, ~35 cm away. The finger-press must be unambiguous: use the **A button with a bare finger**,
   nail visible, pressing straight down (no rolling). Do not film the controller by reflection.
3. One clip per config, 24+ presses in the clip, ~3 s between presses (the game must be back at idle). Say the config out loud at the start.
4. Run MangoHud logging for the same period (`run_log.sh tag LAT_<cfg> <variant> USER`, then `run_log.sh window lat`): gives
   frametime p50/p99 + temps for the present-to-display part. For `LAT_STOCK` without the mod, MangoHud alone is fine.

## What to press and what to watch
- **Button: Light attack (X on the Ally X) from standing idle**, character in the
  open, weapon sheathed-ready stance, camera not moving, no enemies, no UI. Same weapon in all configs.
- **Event: the first video frame where the character's weapon / arm visibly starts the swing** (any pixel change of the character
  body relative to idle, not the sound, not the effect trail). Pick this rule once, use it for all configs.
- Fallback if the swing start is ambiguous on the first clips: open the in-game Pause (Menu button): the first frame where the dark
  pause overlay appears. Use ONE event for all three configs; do not mix.

## Frame counting
1. Transfer the 240 fps clips to the PC unconverted (no re-encode). Open in a frame-stepping player (VLC `E` key; mpv `.` / `,`; or
   `ffmpeg -i clip.mp4 -vf "select=..." ` -> or simply `ffplay -vf "drawtext=text=%{n}:fontsize=40"` to burn frame numbers).
   Easiest: `ffmpeg -i clip.mp4 -vf "drawtext=text='%{n}':x=10:y=10:fontsize=48:fontcolor=yellow:box=1" numbered.mp4`.
2. For each press:
   - `frame_press` = the first video frame where the button is visibly depressed (the finger has touched and the button cap moved
     down; the frame BEFORE it shows no movement). Use the same criterion every time.
   - `frame_change` = the first video frame with the screen change defined above.
   - Write both numbers in `latency.csv` (copy `latency_template.csv`, fill `frame_press`, `frame_change`; mark a bad press with
     `bad` in `note`: finger bounced, missed, screen already moving).
3. Count in two passes, or have a second person check 5 random presses: they must agree within +-1 frame (4.2 ms).

## Rules
- **>= 20 valid presses per config** (record 24, drop the bad ones). The script flags fewer than 20 as NOT VALID.
- Report the **median** (primary), plus mean, p90, min, max and stdev. Resolution is 4.17 ms; a median difference below ~4 ms is noise.
- Order of configs: STOCK, OPTIMIZED, OPTIMIZED_FG, then repeat STOCK once; the two STOCK medians differing by more than 1 frame
  (4 ms) means the setup drifted, redo the session.
- Do not change TDP / governor / env vars / gamescope between configs (game-side rule). The Ally must be at the same battery/plug
  state and display refresh.

## Compute
```
python3 latency_calc.py latency.csv
```
Prints the per-config table and the delta vs STOCK. Add the result and the three MangoHud frametime summaries
(`summary_ally_hw.csv`, ids `LAT_STOCK`, `LAT_OPTIMIZED`, `LAT_OPTIMIZED_FG`) to the benchmark report. FG latency is listed
separately and never counts toward the goal.
