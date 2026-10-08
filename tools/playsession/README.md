# Nioh 3 play session kit (PC)

Records everything about one normal play session (for example the boss mission) while you play by hand.
Nothing presses keys or buttons for you; the kit only measures.

## What you do

1. Double-click **Nioh3 Session START** on the desktop. A window shows a short checklist; press Enter to close it.
2. Start Nioh 3 from Steam and play as usual. Don't use the Ally at the same time.
3. When you are done: quit to the title screen, then exit the game to the desktop.
4. Double-click **Nioh3 Session STOP**. It puts your mod files back and writes the report.

If the game crashes, just do step 4. If you click STOP while the game is still open, it tells you to quit first and changes nothing.

## What START does

- Refuses if a session is already running, if Nioh 3 is open, or if the Ally holds the game (bridge game lock).
- Backs up your saves and settings (`%LOCALAPPDATA%\KoeiTecmo\NIOH3`) to `D:\nioh3-re\save-backups\<date-time>-session`.
- Takes the game lock for the PC (renewed every 20 min while the session runs, released by STOP).
- Keeps a copy of the game folder's `Nioh3EngineFix.ini` and `.asi`, installs the current build from `mod\Nioh3EngineFix\bin`
  (1.7.35, needed for FnProbe and PadLatency), and writes **your own ini** plus these measurement-only keys:
  `[Debug] FnProbe BossLog RematchLog PadLatency`, `[Profiler] MemStats LoadStats LoadGates`, `[Patches] CpuStats TS SPR` (all = 1).
  Everything else in your ini stays as you set it (AutoContinueLoad included).
- Starts PresentMon (records every frame of Nioh3.exe, follows relaunches) and a small background monitor (Windows counters every
  ~5 s, keeps the mod log of every launch, notes crashes/exit codes).

## What STOP does

Stops PresentMon and the monitor, keeps the last mod log, restores your ini and ASI byte-for-byte, releases the lock, and runs
`summarize.py`. Everything lands in `captures\playsession\<date-time>-session\`; the report is `report.md` there
(numbers also in `summary.json`).

## What the report contains

| section | numbers | source |
|---|---|---|
| Frames | avg fps, 1 % and 0.1 % lows (two definitions), frame time p50/p95/p99/p99.9/max, spikes (> 2x local median and > +8 ms) and per minute, frames > 33/50/100 ms, pacing (stdev, mean and p99 frame-to-frame change, animation error), displayed time | PresentMon 2.6 `--v2_metrics` |
| CPU / GPU per frame | CPU busy / wait, GPU busy / wait / latency, share of GPU-bound frames | PresentMon |
| Input latency | all-input-to-photon, click-to-photon, display latency, present -> display (DisplayLatency - CPUBusy), PC latency (if the game sends PCL markers) | PresentMon |
| Controller latency | pad changes (buttons / triggers / sticks), input->present and input->GPU-done p50/p95/p99, the game's pad poll interval p50/p95/p99, and an estimate pad press -> on screen = poll interval / 2 + input->present + PresentMon's present -> display (the controller path PresentMon cannot see) | mod PadLatency (`notes\pad-latency.md`) + PresentMon |
| Thread timing | pad sample age, update + limiter, logic -> waitable, render, logic frame, present interval, sample -> present (mod TS probes) | mod log `stats latency` |
| System | CPU total %, CPU performance % (clock vs base), Nioh3 CPU %, GPU 3D engine % (total and Nioh3), VRAM (Nioh3 and adapter), Nioh3 working set / private bytes, RAM available, commit | Windows counters (monitor.ps1) |
| Memory | working set, commit, VRAM local used / budget, non-local, system RAM | mod `stats mem` (MemStats) |
| Loads | seconds, frames, CPU cores, stage ready, MB read, PSOs, which load gate held it, auto-continue | mod LoadStats / LoadGates |
| Stutter | runtime PSO creates, how many took >= 1 ms, worst; upload-heap fallbacks | mod `stats spr` (SPR) |
| Boss fight | each fight's start / end / duration, HP-bar actor, mission flags, clear id, rewards; fps, lows and spikes inside the fight | BossLog + PresentMon |
| Rematch | RematchLog events by tag (Battle Scroll, replay, retry, saves) | RematchLog |
| FnProbe | per probed function: hits, rate, peak per second, first / last hit, thread, top callers with symbol names, first-hit arguments with hash-looking values | FnProbe (remaining-work 10.1) |

Not captured: GPU clock, temperature and board power (no source on this PC without HWiNFO or AMD's SDK).
With frame generation on (your config.xml has FSR), PresentMon fps counts generated frames too; the report says so.

## Checked without the game (`test_session.ps1`: DRY RUN OK; `summarize.py --selftest`: SELFTEST OK)

Confirmed:
- START refuses while the Ally holds the lock (real bridge status), refuses a second START; STOP refuses while Nioh3.exe runs.
- Save backup is complete; the ini keeps every line of yours and adds only the capture keys; the ASI from `bin\` is installed;
  STOP restores ini and ASI byte-identical and stops PresentMon and the monitor.
- The monitor sees a Nioh3.exe start and exit (a stand-in process), copies its mod log, and logs real CPU / GPU 3D / VRAM / RAM
  counters from this PC.
- PadLatency lines are rendered from the format strings in `padlat.cpp` and parsed back (window + total), the estimate checked;
  the hook and the frame matching have their own offline harness (`mod\Nioh3EngineFix\tests\run_padlat_harness.py`).
- PresentMon column names are the real header this PC's PresentMon 2.6.0 writes with `--v2_metrics --date_time`.
  Its date column was 4 h off local time here (it applies the UTC offset twice); the summary corrects that and
  checks the fix against the session times.
- Log parsers: LoadStats, LoadGates, MemStats, TS latency and CpuStats lines are real lines from earlier sessions.
  BossLog, SPR and FnProbe lines are rendered from the format strings in the mod's source.
- FnProbe hook: register-exact thunk test (all volatile registers incl. xmm0-5 and flags), exact counts from 4 threads,
  sampling, arguments, prologue decoder vs capstone on 6614 game functions, no branch into patched bytes, test_patterns ALL OK.

Only the game can confirm:
- FnProbe running live (log line `FnProbe: 14/14 functions hooked`), its hit rates and that the cost is invisible.
- Real Nioh3 frames in PresentMon, and the input latency columns: they fill only on mouse/keyboard input
  (a controller is probably not tracked - that is what PadLatency covers). PC latency appears only if the game sends PCL markers.
- PadLatency running live (log line `PadLatency: hooked the game's XInputGetState`, then `PadLatency 30s:` lines with matched ~= changes).
  A DualShock/DualSense used without Steam Input goes through the game's own HID path and is not counted.
- Per-process GPU % / VRAM for Nioh3, boss-fight frame windows (needs a BossLog fight start/end in the log).

## Files

`start_session.ps1`, `stop_session.ps1`, `monitor.ps1`, `session_lib.ps1` (paths, capture keys), `summarize.py`,
`test_session.ps1` (dry run), `make_shortcuts.ps1` (recreates the two desktop shortcuts). `state\active.json` exists only
while a session runs. Re-run a report: `py -I tools\playsession\summarize.py captures\playsession\<session>`.
