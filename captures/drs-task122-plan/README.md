# Board122 DRS OFF/ON/ON/OFF - offline recipe (prepared by cx-pc2, #27901). NOT run, nothing installed or edited.

Source: origin/cx-pc/drs-matched-27690:notes/drs-test-asi-27690.md. Build: `captures/drs-task122-build.asi`
sha256 `01f54b794b06b6f10164d8c119d04aa137010c776ca50ce88e638bd44ecede14` (verified against the local file).
Scope: PC2, FG OFF only (no FG-ON / resize extras).

## Fixed settings (identical in all 4 windows; only hook `Enabled` differs)
| where | key | value |
|---|---|---|
| game config.xml (native, leave as the lead has it; verify, do not rewrite) | `DYNAMIC_RESOLUTION` | non-OFF (recipe: 60FPS; engine slot 0x45BAFEC, 0 = OFF -> hook inert, menu shows "enable game dynamic resolution") |
| | `DYNAMIC_RENDER_RATIO_MIN` / `_MAX` | 50 / 65 (bounds index 5 / 8; percent = 25 + 5*index) |
| | `REN_FPS` | 120 |
| | `FRAME_GENERATION` | OFF |
| | `RESOLUTION_SCALING`, sharpness, resolution, every other toggle | unchanged between windows |
| Nioh3EngineFix.ini `[DynamicResolution]` | `NativeTargetFPS` | 40 (clamped 20..120) |
| | `Enabled` | OFF/ON per window via live control (ini starts 0) |
| Nioh3EngineFix.ini `[Live]`-related | `LiveCtl` | 1 (same in all windows; `LiveCtlFile = Nioh3EngineFix_live.ini`) |

## Window plan: OFF, ON, ON, OFF (one launch, live toggle, ABBA-style order)
`schedule.csv` + `live-W*-*.ini` (Gen strictly increasing 1..4; key `DRS` is the menu row key = `menu set DRS 0/1`).
Per window: write the control file atomically (`.w` then Move-Item, as gpu_sweep `Apply-SweepControl`), wait for `<file>.ack`
with `gen=<n>` and `fail=0` and `DRS=<0|1>` read back, settle 60 s, measure 300 s. 2 repeats per arm = the 4 windows.
(settle/measure lengths are my assumption - change in schedule.csv if the lead wants others.)
Same scene/fight each window; no death, power plan fixed, same thermal start (idle gap between windows is the lead's call).

## Per-window pass/fail evidence (a window with any miss is INVALID, not "no effect")
1. Installed hook: startup log `DRS: target hook installed` (once, before W1). Missing -> abort, nothing measured.
2. Override counters from the 30 s `DRS:` log line (`calls`, `overrides`, `modeOff`, `ratio=a->b`, `boundsIndex=min..max`):
   take the delta across the measure window. ON: `overrides` delta > 0 and ~= `calls` delta; `modeOff` delta = 0;
   boundsIndex must read 5..8 (50..65%). OFF: `overrides` delta == 0 while `calls` delta > 0 (hook alive, passthrough).
3. Ack read-back `DRS=` equals the wanted value for the window.
4. Metrics (same tools as the other final benches): GPU ms, avg fps, 1% low, game speed (stays 1.0x; target40 must not
   change sim speed), player HP (no death), plus temps/power/usage and latency per the standing final-bench order.
5. Render dimensions / time at scale (NOT ratio-only) - see gap 1.

## Exact keys and instrumentation that exist
- Native: config.xml `DYNAMIC_RESOLUTION` (settings.md #47), `DYNAMIC_RENDER_RATIO_MAX/MIN` (#48/49, 25..100 step 5), `REN_FPS`.
  Engine slots read by the hook: mode 0x45BAFEC, max-index 0x45BAFF4 (as logged first), min-index 0x45BAFF0, published ratio enum 0x45BBCE8.
- Mod: `[DynamicResolution] Enabled`, `NativeTargetFPS`; menu row `DRS`; `DRS:` log line every 30 s (fields above);
  startup `DRS: target hook installed`; menu status string ("OFF (native)" / "ON / 40 native fps" / "enable game dynamic resolution").
- Effective dimensions: the engine picks them at 0x5AC428 (not hooked). Ratio enum 0x45BBCE8 percent = 25+5*index gives the
  REQUESTED scale only.

## Gaps (cannot be closed offline; lead must decide before launch)
1. **No time-at-scale and no accepted render dimensions.** The build logs only ratio before/after of the last controller call per
   30 s line. Options: (a) a tiny sampler reading 0x45BBCE8 every 100-250 ms from outside (needs a memory-read route; none found
   in tools/rematch RMP - only status/ping), (b) add a ratio histogram + 0x5AC428 result log to the ASI (new build => new hash,
   the task fixes hash 01f54b79...), (c) derive from GPU-pass timing/RGP capture. Until one exists, report "requested ratio from 30 s
   snapshots", explicitly NOT proof of dimensions.
2. `DYNAMIC_RESOLUTION` value on PC2 is not read here (config untouched); lead must confirm non-OFF and bounds 50..65 in config.xml.
3. Under live toggling the notes say history/resize is owned by the engine: a ratio change after ON->OFF may take time to settle;
   the 60 s settle is a guess.
4. The existing gpu_boot/gpu_sweep runner needs a DRS-aware row type (control = `DRS=<v>` is already accepted by the live
   control path, but there is no schedule consumer for this CSV); I did not touch any runner.
