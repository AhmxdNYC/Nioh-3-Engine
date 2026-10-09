# Why the MODDED run had more input delay than STOCK (PC, 2026-10-08 sessions)

Runs: MOD `captures\playsession\20261008-2059-session` (ASI 1.7.39 + OptiScaler 0.9.4: XeSS -> FSR4, its FG off; game FSR FG),
STOCK `captures\playsession\20261008-2205-session` (no ASI, no dinput8, no OptiScaler). Same user, mission and settings
(2560x1440, REN_RAT 75, cap 120, VSync ON, XeSS, FG = FSR, HDR). Play windows: MOD 21:00-21:22, STOCK 00:05-00:29.
Scripts only (PresentMon CSVs, mod log, dump disassembly); the game was not run. Tags: **M** measured, **E** estimate.

## Plain-language summary

1. **The extra delay is not rendering, it is how the frames reach the screen.** In the stock run the game handed every frame to
   Windows as "VSync off, tearing allowed" (SyncInterval 0), so with FreeSync each frame went straight to the screen
   (*independent flip*). In the modded run, from the first in-game frame on, every frame was handed over as "wait for the
   next refresh" (SyncInterval 1), so Windows had to copy it into the desktop first (*composed flip*). That costs ~7 ms
   per frame and makes Windows throw away more frames. Same GPU, same CPU, same fps.
2. **Our mod does not set this.** The ASI never touches SyncInterval, present flags or the VSync setting (checked in the
   source and in the hooks). OptiScaler's FSR4 costs a little GPU (+0.24 ms per presented frame, < 0.5 ms of delay), but
   it is not what flips the mode: the same flip happened in older runs **without** OptiScaler, and did not happen in an
   older run **with** OptiScaler and FG on.
3. **The flip is the game's own SyncInterval state.** The engine has three writers of its SyncInterval (default 1, the VSync
   setting, and a setter); which one wins changed from launch to launch and sometimes mid-run on 2026-10-06 too (with and
   without mod/OptiScaler). In the modded run it switched to 1 in the second the hub loaded and the FSR FG presenter
   started (21:00:52), and stayed there.
4. **What to do now (no mod change needed):** in-game **VSync OFF** with FreeSync on and the 120 cap. The game then always
   presents with tearing allowed -> independent flip (the stock path), and the cap + FG output (~150-175 fps) stay
   inside the 48-240 Hz FreeSync range, so no visible tearing. Expected: display latency ~7-8 ms instead of ~15, ~2 %
   dropped frames instead of ~10-20 % (from the stock run and the same-run flips below).
5. **Boss timer bug:** BossLog only measures `Mission::BossBattleMode` -> `End`. In this mission the game calls
   BossBattleMode only after a cutscene + 5 s load that spawns Jakotsu-baba (placement 76781) at 21:19:05; the combat
   before it is not counted. The fix proposal (encounter window) is in section 6.

## 1. Present mode per run (PresentMon, play windows) [M]

| run | frames | SyncInterval / tearing | PresentMode | displayed | DisplayLatency p50 / p95 | GPUBusy p50 | CPUBusy p50 | AllInputToPhoton p50 |
|---|---|---|---|---|---|---|---|---|
| MOD, gameplay + menus after 21:00:52 | 197 136 | **1 / no** | **Composed: Flip** | 90.3 % | **14.95 / 24.3 ms** | 5.66 ms | 5.93 ms | 13.3 ms (617) |
| MOD, title 21:00:00-21:00:52 | 6 011 | 0 / yes | Hardware Composed: Independent Flip | 100 % | 10.4 ms | 2.57 | 8.51 | 2.5 ms (55) |
| STOCK, play (incl. menus) | 207 677 | **0 / yes** | **Hardware Composed: Independent Flip** | 100 % | **7.57 / 14.4 ms** | 5.34 ms | 6.12 ms | 4.4 ms (330) |
| STOCK, flips not shown | 4 265 | 0 / yes | Hardware: Independent Flip | 0 % | - | 10.0 | 8.24 | - |

- One swapchain per run (MOD `0x1F3C3F4A650`, STOCK `0x27614C697D0`), never recreated. PresentFlags column 0 in both.
- Timeline: both runs start with ~20 s of SyncInterval 1 at boot (ctor default), then 0 at the title. STOCK stays 0 for
  the whole 2 h 24 min (5-min buckets: 0.0 % SI1 after 22:06:10). MOD switches to 1 at **21:00:52** (PresentMon 17:00:52;
  mod log `21:00:52.670 OSD: Present called from thread "AMD FSR Presenter Thread"`, first in-game frames after the hub
  load) and stays 100 % SI1 in every later minute, including FG-paused menus and the 67 s options visit.
- Render side is equal: present rate 152 vs 147 fps, CPUBusy p50 5.93 vs 6.12 ms. GPU +0.24 ms per presented frame in MOD
  = FSR4 instead of XeSS plus the OSD (E: not separable from this data).
- **Dropped before display:** STOCK 2-3 % in every play bucket. MOD 3-6 % until 21:12:50, then a step to **18-20 %** that
  lasts until the boss kill (21:19:40) with no change in present cadence (presents < 4.17 ms apart: 5-8 % before and
  after) - a composition-side effect (E: cause unknown, no mod-log event at that second). Composed flip is the
  precondition: independent flip showed no such step in 24 min.

### Same-run A/B of the flip alone (older PresentMon `MsUntilDisplayed` = present -> display) [M]
The flip happened mid-run in three 2026-10-06 benchmarks with nothing else changed, which isolates its cost:

| capture | before | after | change |
|---|---|---|---|
| `bench-final3-mod-fg` (FG on, OptiScaler installed) | SI0, 2.3 ms, 99 % shown | SI1, 8.5-8.8 ms, 95-96 % shown | **+6.3 ms** |
| `bench-r3-G1b` (FG off, OptiScaler on) | SI0, 6.6 ms | SI1, 15.2 ms, 92-98 % shown | **+8.6 ms** |
| `bench-r3-G0b` (FG off, OptiScaler on) | SI1, 18.4 ms | SI0, 7.6 ms | **-10.8 ms** |

That matches the MOD-STOCK gap here: DisplayLatency +7.4 ms p50, all-input-to-photon +8.5 ms p50.

## 2. Who sets SyncInterval 1? (ours / OptiScaler / game) 

**Ours - ruled out [M].** `mod\Nioh3EngineFix\src`: no SyncInterval, ALLOW_TEARING, `0x424C` or VSync-slot (`0x45BAF3C`)
write anywhere; `HkPresent/HkPresent1` pass `sync, flags` through unchanged; `[Latency] MaxFrameLatency = 0` (log: "the
game's maximum frame latency is 1", untouched); `[GpuOpt] Barriers = 0`; `FG = 1` (F1) only sleeps until 1 ms before the
presenter's own deadline and spins the rest (`L1_MarginUs 1000`), so FG pacing precision is unchanged; `VRR = 1` changes
the simulation dt only; PadLatency adds one fence Signal per Present. None of these touches the present mode.

**OptiScaler - not the trigger, costs a little GPU [M/E].** `OptiScaler.ini` (identical in repo and game folder):
`Dx12Upscaler=fsr31`, `Fsr4Update=true`, XeSS inputs only, `[FrameGen] FGInput/FGOutput = nofg`, `[V-Sync]
OverrideVsync=false`, `ForceVsync/SyncInterval = auto`, `LoadAsiPlugins=false`. Its FG DLLs are not used: the game's
FG is statically linked, and the MOD run's presenter is the game's own (F1 timer sleeps 64-110 per second during FG
prove the static `FsrFiPresenterLoop` 0x37778B4 ran). History (`patcher\round3.ps1` sets OptiScaler per launch, all with
the same live patches): G0a/G1a (OptiScaler on) SI1, G0b/G1b (on) flipped mid-run both ways, **P0a/P1a (OptiScaler
removed) SI1**, S1a/P0b/P1b/S1b (removed) SI0; `bench-fg-stock` (OptiScaler installed since 14:10, FG on) SI0. So SI1 occurs
with and without OptiScaler, and OptiScaler + FG can run SI0.

**Game's own FG / engine - where the value comes from [M].**
- `FfxFiPresentFrame_37773E4` (FFX passthrough path, E: role read from the disasm; it stores vsync byte 0) calls `Present(edx = caller's SyncInterval, r8d
  = caller's flags)` unchanged (disasm 377741F / 3777833). The presenter (`37772FC`) calls `Present(vsync byte, (tearing
  supported && !vsync && !exclusive) ? 0x200 : 0)`. So FFX forwards whatever the engine asked for.
- Engine `GfxDevicePresent` 3D62E4 presents with SyncInterval = `GfxDevice+0x424C` (unless its param bit 0). Writers of
  +0x424C (byte search of the dump): ctor `2EDFBBD` = 1 (the boot frames), `DisplaySettingsWatcher` `13503A3` = VSync
  slot `0x45BAF3C` (only when the slot changes and the `+0x2538/+0x2544` guard is clear), setter `2EEBB50` (`max(si, 0)`,
  no direct caller: called through a pointer). Which writer wins differed per launch and even mid-run on 2026-10-06.
- Not yet proven: whether the 1 that reached DXGI was produced above FFX (engine) or rewritten below it (Streamline
  `sl.interposer` / OptiScaler `dxgi.dll` sit between FFX and DXGI). Our OSD hook sits exactly there (SL swapchain
  vtable) and can tell: see fix A.

## 3. Fixes

**User setting (now, measured basis):** in-game VSync OFF + FreeSync on + keep the 120 cap (user-settings.md PC row 12,
updated). Expected DisplayLatency ~7-8 ms p50, ~2 % dropped (STOCK numbers). No tearing while presented fps < 240 (cap 120,
FG output 150-175).

**A. Ours, diagnostic (proposal for the next ASI that owns src; not built):** log the present mode in `osd.cpp`
`HkPresent/HkPresent1`: on every change of `(sync, flags & 0x200, presenting thread)` one line `OSD: present mode SI x
flags y thread "name"` plus the engine's `GfxDevice+0x424C` and VSync slot `0x45BAF3C`, and a 30 s count. Cost: one
compare per Present. Result: SI1 in our hook = engine/FFX chose it; SI0 in our hook but SI1 in PresentMon = Streamline
or OptiScaler rewrote it below us.

**B. Ours, optional mitigation (proposal, default 0):** `[Latency] VrrPresent = 1`: in the same hook, when `sync == 1`,
the swapchain was created with `DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` (game flags 0x842 have it), the window is not
exclusive fullscreen and the user's VSync slot is ON, call the original with `(0, DXGI_PRESENT_ALLOW_TEARING)` - the
exact call the stock run made. Same look inside the VRR range; above the panel's max refresh it would tear (not
reachable with the 120 cap + FSR FG at 240 Hz). Only worth building if A shows the value is produced above our hook.

**OptiScaler (recommendation):** no setting change is needed for latency. For one diagnostic session set `[Log]
LogToFile=true`, `LogLevel=1` to record its swapchain/present handling, then revert. Keep it only for the FSR4 image
(+0.24 ms GPU per presented frame, E: < 0.5 ms delay).

### Test plan (one 10-min replay of mission 52 each, PresentMon on, compare play windows)
| # | setup | expect / decides |
|---|---|---|
| T1 | user's setup, VSync OFF in game | PresentMode Hardware Composed: Independent Flip >= 95 %, DisplayLatency p50 <= 9 ms, dropped <= 3 % |
| T2 | `start_session.ps1 -Stock -KeepOptiScaler`, VSync ON | does SI flip to 1 at hub load with OptiScaler alone? |
| T3 | mod session with OptiScaler reverted (`optiscaler\apply.ps1 -Revert`), VSync ON | does SI flip with the ASI alone? |
| T4 | ASI with fix A, VSync ON | which layer produces SI 1 |
Pass/fail is the PresentMon `SyncInterval` / `PresentMode` columns per minute (scratch scripts `tl.py` logic: 60 s buckets,
% SI1, % displayed, DisplayLatency p50).

## 4. Other ASI settings in this session's `ini_used.ini` (no latency effect found)
`VRR = 1` (MaxFps 120), `FG = 1`, `IK = 0`, `R1 = 1` (2 ms), `L1 = 1`, `W6` hybrid, `[Latency] MaxFrameLatency = 0`,
`[GpuOpt] Barriers = 0`, `PadLatency = 1`, `TS = 1`, FpsCounter OSD on. TS medians in play: waitable wait ~2 ms (GPU-bound),
sample->present 26-31 ms; the same engine path runs in both runs, so these do not explain a display-side gap.

## 5. FG engagement (both runs)
Game FSR FG ran in both (presented > 120 cap; same static presenter). MOD OSD: rendered 100-118 fps -> presented
150-178 (ratio 1.42-1.69). STOCK has no mod log, so no real-rate split; present rate 147-168 fps per 5 min in play, similar.

## 6. BossLog "fight 26.8 s" for Jakotsu-baba (proposal for `mod\Nioh3BossRematch`, src not edited)

**What the log shows [M]** (`modlog_run1.log`):
- 21:17:45.6 FG paused 0.5 s (event/cutscene flicker), then combat input: PadLatency buttons per 30 s 4 / 13 before,
  **75 (21:17:56-21:18:26) and 58 (-21:18:56)**.
- 21:18:53 stat-counter trophy call (id 36), FG pauses 21:18:53 / 21:19:00, event signal `D63C0 = 1` 21:19:01.8.
- **21:19:02.5-21:19:07.5 load** (LoadStats #3, 5.0 s, 118 MB read); `Character::Create 76781` at 21:19:05.1 (the only
  creation in the 60 s before); **`BossBattleMode` 21:19:07.546** (same ms the load flag clears); defeat flag 118 at
  21:19:32.4; End 21:19:34.4 -> "26.8 s".
- PresentMon: GPU per presented frame 5.2-5.8 ms until 21:19:00, **6.6-6.8 ms after the load** (the arena), drop-rate step
  at 21:12:50 (section 1, cause unknown).
- The catalogue row for mission 0x34 (`Nioh3BossRematch_bosses.tsv` R0034) lists **Enenra & Jakotsu-baba** with the same
  placement 76781.

**Why the start fired late:** `HkBegin` stamps the fight at `Mission::BossBattleMode`, and the mission script calls it only
for the actor spawned by the mid-encounter cutscene + load (76781). Everything before that load (the first boss / first
part of the encounter) runs without BossBattleMode, so it is outside the window. The duration also includes no load time
because the load is just before the stamp. Real start: **no later than 21:17:46** (event pause + combat burst; 108 s
encounter) and possibly 21:12:50 (drop step; 6.7 min). Ask the user which matches; the 60 s placement ring cannot reach back
far enough to show an earlier boss spawn.

**Proposed fix (bosslog.cpp):**
1. Encounter window separate from the BossBattleMode window: start = earliest of (a) `Character::Create` of any placement in
   the catalogue row for the current mission, (b) first `Player::IsEnterPlacementBox*` true for a boss box (boss-exe-strings.md),
   (c) first `Duel::IsAliveBoss` / `Duel::GetBossPlacementID` poll (boss-fights.md: the boss script loops on it), (d) the
   first BossBattleMode of the mission. End = last `BossBattleModeEnd` / defeat flag / `SetMissionClearFlag`.
2. Do not reset the clock on a second BossBattleMode within the same mission; log each as a phase:
   `BossLog #1: phase 2 start +81.2 s`.
3. Log every `Character::Create` with a timestamp while a scroll/replay mission is active, for catalogue ids at least (here
   `RmBoss()` was false during the mission, so none were printed), and keep a mission-scope list of catalogue placements
   instead of the 60 s ring.
4. Subtract loads (`LoadStats` 58A set/clear) and menu time inside the window; print both:
   `fight 26.8 s (BossBattleMode) | encounter 108.8 s, of it load 5.0 s, menu 0 s`.
Offline test: feed this session's log lines (create/begin/end/flag/load times) through the encounter logic in the add-on's
harness and expect start 21:17:4x (or the box/poll time when hooked), phase start 21:19:07.5, end 21:19:34.4.
