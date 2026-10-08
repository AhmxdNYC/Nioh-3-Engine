# PC CPU: what is left on the critical path, ranked (2026-10-08, ASI 1.7.39)

Inputs: `captures\cpuprof-r5..r9` (+ `bench-*.csv` frame times; r5 8.85, r6/r7 8.6-8.8, r9-all 11.0 ms), abba2 `run5` / `run6`
(1.7.32, FG off, REN_RAT 50 FSR, 1080p), `mainthread-drawprep.md`, `mainthread-logic.md`, `mainthread-25.md`,
`parallel-drawprep.md`, `cpu100-opportunities.md`. Nothing was run in the game. "ms" = share x frame time of that capture.
**est.** = estimate (offline benchmark or reasoning), everything else is measured.

## 1. PP sanity check: the KEEP is real
- **The counter is right.** `stats/s exp: ... PP scans X skipped Y` is a per-second average over 30 s. `pp_ran` counts every
  `PagePoolScanAll` call that runs (the cave's `ppRun` path also counts while the switch is off), `pp_skipped` the skipped
  ones. run5: every OFF window reads `scans 106-111 skipped 0` (= fps, a scan every frame), every ON window `scans 28 skipped
  84-85` (1:3 at `PP_Period = 4`); BASE (PP = 1 in the base ini) reads `29 / 88`. The quoted `scans 110 skipped 0` lines are
  the OFF windows. A 20 s window holds 0 or 1 of these 30 s lines, so some windows show none.
- **The gain matches the mechanism.** Valid quads q2 / q3: +2.91 / +2.95 % (q1 +4.0 %, rejected for an OFF-OFF step);
  8.98 -> 8.72 ms = **-0.26 ms/frame**, CPU busy -2.6 %, GPU busy flat (0.1-1.1 %). The removed work: `PagePoolCountUsed
  2523FC` = 2.0-3.0 % of [App]Game self in r5-r9 = 0.18-0.33 ms/frame; skipping 3 of 4 = 0.13-0.25 ms, plus the cache
  footprint of walking every 64-byte page-pool entry each frame (est.). -0.26 ms sits at the top of that range: consistent.
- **No hidden side effect.** On skipped frames the cave publishes `B30h = B2Ch`, `B38h = B34h`. The two readers
  (`PagePoolOccupancyCheck 126C64`: > 85 % -> mip-bias step with a 0.3 s cooldown; `PagePoolStatsReader 28B1B1C`: settled
  state machine) use the absolute value, not a delta, so no downstream work is skipped. Values are <= 3 frames stale.
- Open: the texture-sharpness visual gate (fast travel, time-to-sharp ON vs OFF) has still not been run. Perf KEEP stands.

## 2. Where the PC frame goes (gameplay, CPU-bound)
**Two serial stages.** [App]Game is a pure render-graph executor (draw prep + token recording); the game logic runs on
**KIDSTaskScheduler0** (frame step 5ACF80 + object-update jobs + job joins). [App]Game waits for the logic side's frame
packet in the blind 1 ms QPC spin at 1A74BD0 (D), and for the submit thread (SynergisticDispatcher) in 7D9E90.

| what [App]Game waits on | r5 | r6 / r7 / r9 | ms/frame |
|---|---|---|---|
| logic packet (D spin, attr 1A74130) | 0 | 6.7-11.9 % (r7-B1 1.1 %) | **0.6-1.05** |
| submit thread, present flag (7D9E90, Sleep(0) loop) | 6.1 % | 0.9-3.8 % | 0.08-0.54 |
| dispatcher drain (418F40) | 2.1 % | 0-1.4 % | <= 0.19 |

**In the abba2 test scene the main thread waits for the logic thread every frame:** run6 DQ launches count ~420 early retries
per second = **~3.6 per frame** (`stats/s drawprep`), i.e. **~0.6-0.7 ms/frame** of [App]Game idles on the logic packet (est.
from the 200 us quantum). PresentMon `MsCPUBusy` (8.35 ms ~ frame time) cannot show this: spins count as busy.
So the frame = render work + logic wait; **both** threads must shrink. PP (render) gained because in its OFF windows (111 fps)
the render side was the longer stage.

**[App]Game self, median of r5-r7 (min-max), ms/frame:** DrawEmit 1E80B0 0.57 (0.49-0.75); QPC (D spin) 0.55 (0-0.97);
MemcpyErms B94230 0.25 + Memcpy B94250 0.22; MeshletCheckRequestLoad 4AA34 0.21; PagePoolCountUsed 2523FC 0.21 (PP); MainLoop
1A74130 0.19 (D loop); TextureStreamUpdate 1E7050 0.18 + 128EF0 0.16 + 1290B8 0.09 (P1E7 target); StageSlotTableUpdate
1D8720 0.16 + 1D84A0 0.15 + 1D8220 0.08 + 1D7EB0 0.07; DrawListWalk 39E20 0.16; ZwDelayExecution 0.15; WcUploadZeroFill 61B1C
0.14; UiQuadBuild 1CD690 0.12; DrawListGroupLength 3BC0C 0.11 (B). Inclusive (corrected for the 53 KB frame of 535188): draw
walker 535188 ~35 %, render-frame-begin 4A529C 13-19 % (meshlet streaming 5D214 ~10 %), UI 6-7 %, effects 365AA98 6-7 %.

**KIDSTaskScheduler0 (logic) self, median, ms/frame:** job-join wait NtWaitForAlertByThreadId **1.44** (1.18-1.99; it waits
for the collision / stage / character workers); **ObbToAabb family 6EA50 + Vec4AddInPlace 6EFD0 + Vec4Scale 6F010 + Vec3Dot
6F060 = 0.20 + 0.33 + 0.12 + 0.08 = 0.73**; BoundsFromPartObbs 71C60 0.27 (+ 174610 0.12, SinCos 54360 0.14, 74 % from 71C60);
JobQueuePoll 411C0 0.17; ZwSetEvent 0.16; UiNodeUpdateWorldMatrix 66D00 0.16; RollingAvgListUpdate 12B440 0.11.

**SynergisticDispatcher** (submit, 48-60 % busy, off the critical path except in the 7D9E90 wait): ZwWaitForSingleObject 4.0 ms
(idle), driver amdxc64 0.3 + 0.12 + 0.1, PsoCacheBTreeFind 119820 0.23, memcpy 0.29.

## 3. Ranking: wins >= 0.2 ms/frame (measured target; saving est. unless stated)
| # | fix | thread | measured target | saving | status / next |
|---|---|---|---|---|---|
| 1 | **AABB** ObbToAabb in SSE, bit-exact (`[Patches] AABB`, live row) | logic | 0.6-0.8 ms (7-9 % of Scheduler0) | **0.5-0.6 ms est.** (offline: ~87 % cheaper per box); fps only where [App]Game waits on logic - the test scene does (~0.6-0.7 ms/frame) | built 1.1.0, **never A/B'd on PC**. Test first |
| 2 | **DQ** retry quantum 1 ms -> 200 us (live row since 1.7.39) | main | spin 0.08-0.96 (median 0.58) + loop 0-0.31 | the overshoot only: 0.1-0.4 ms est. in frames that wait (every frame in the test scene) | built 1.7.32; run6 INVALID (drift + cap). Re-test |
| 3 | **P1E7** texture-loop prefetch (live row since 1.7.39) | main | 1E7050 + 128EF0 + 1290B8 = 0.28-0.46 ms | 0.05-0.2 ms est. | built 1.7.32; run6 INVALID, but its launches had the lowest CPU busy (8.34-8.37 vs DQ 8.40-8.48 ms at equal GPU 6.61) - a hint, not a result |
| 4 | **MA** 25 main-thread `lock cmpxchg` loads -> plain loads (**new, `[Patches] MA`**, next launch) | main | 4AA34 0.18-0.25 + 419A8 0.07-0.09 + 4B104 ~0.05 + 1 per draw in 1E80B0 = 0.3-0.4 ms | 0.05-0.15 ms est. (r9 live A was neutral within its old-harness noise) | **built 1.7.39** (below) |
| 5 | PP page-pool sweep every 4th frame | main | 0.18-0.33 ms | **-0.26 ms measured** (+2.9 %) | KEEP (run5) |
| - | BoundsFromPartObbs 71C60 own loop + 174610 + 54360 | logic | 0.27 + 0.12 + 0.10 | unknown (needs RE; per-part OBB build, half-float unpack) | next RE target after AABB is measured |
| - | logic join wait (workers: collision / stage / character jobs) | workers | 1.4 ms of Scheduler0 waiting | depends on the slowest job; CP / SPF / DL built (untested), MQ DROP | test CP/SPF live once AABB is decided |
| - | state-commit slot tables 1D8720/1D84A0/1D8220/1D7EB0 | main | 0.46 ms | unknown; already dirty-tracked | RE "D2" (which stages really change) - no patch yet |
| - | memcpy into the KTGL stream B94230/B94250 | main | 0.47 ms | none safe (design: copied again by the dispatcher) | no |
| - | DrawEmit 1E80B0 per-draw state filter | main | 0.57 ms | none safe (necessary work, cache misses) | no |
| - | PsoCacheBTreeFind 119820 | dispatcher | 0.23 ms | 0 unless the 7D9E90 wait shows | SF1 PSO memo after the 2.2 trace |
Below 0.2 ms (not pursued): glyph atlas C 0.14, UI alpha RMW E 0.12 (r9 0.24), FPC 0.1-0.17, RollingAvg 0.11, B 0.11 (DROP), K1 <= 0.19.

## 4. Built in 1.7.39 (worktree `pccpu`, all default off)
- **MA** (`[Patches] MA = 0`, code bytes, next launch, menu row shows the ini value): the `xor eax,eax; lock cmpxchg [m],rZ`
  atomic-load idiom -> `mov eax,[m]` + NOP at 4AC14 4AC2B 4AC42 4AECF 4AFAC (4AA34), 4B319 4B32C 4B33F (4B104), 419E8 41A10
  41A28 41A38 41A4A 41A5A 41A6A 41A7A 41A9A 41AAC 41AC3 41ACC 41AED 41AF6 41B12 41B7B (419A8), 1E835B (DrawEmit). Same
  values (aligned load is atomic, acquire on x64); drops the full fence + forced line ownership that serialise those
  pointer-chasing loops. Real CAS loops and refcounts untouched. All-or-nothing (patches.json group MA, 25 sites, deps =
  FNV of the three function bodies + the `xor edx,edx` before 1E835B).
  Register safety: `re\scripts_cpu\ma_zero.fsx` (new) proves with an Iced reaching-definition pass over every chained piece of
  each function that rZ (r10d / ebp / edx) is zero on **every** path to every site, so the original could only ever store 0
  over 0; `mainthread_patches.fsx` proves operand equality and that flags are re-set before use; `test_patterns.py` checks
  each replacement mechanically (lock cmpxchg with the zero register -> mov eax,<same mem> + standard NOP).
- **Live menu rows for DQ and P1E7** (`Shorter no-frame retry (DQ)`, `Texture-loop prefetch (P1E7)`): the 1.7.32 caves already
  had runtime switches (`2` = installed + off); now they can be A/B'd inside one launch like PP, and Save keeps `2`.
  VirtualPad `menu set DQ 1` works too. `matrix.json`: DQ / P1E7 mode live (installed value 2), new MA item (launch).
- Harness: `abba2_pc.ps1 -Arms BASE,DQ,P1E7,BASE,P1E7,DQ,BASE` (explicit order), `-MaxFps 240` ([VRR] MaxFps for the run),
  live items missing from the base ini are installed switched off (2), MA boot line read. `abba2_report.py`: 3+ BASE launches
  -> interpolated baseline, live DQ / P1E7 switch read-back from `stats/s drawprep`, cap-compressed flag (selftest OK).
- Offline: test_patterns ALL 261 SITES OK (MA 25/25), drawprep + vpad harnesses ALL PASSED, menu_map selftest OK, ASI builds.

## 5. Test plan (next PC turn; ASI 1.7.39; one-time: user's ini unchanged, the harness writes its own)
**Test condition.** FG off, REN_RAT 50 FSR 1080p (harness default), same save / spawn, `-Input vpad`, no input, 60 s warm-up.
**New: `-MaxFps 240`.** run5/run6 sat at 116-118.5 fps against the 120 cap (45-65 % of frames < 8.6 ms): at most +1.3-3 % was
reachable, so B / GF / P1E7 "DROP" there is partly the cap. The engine's internal tier stays 120 above 120 (VRR design A).
Gate: the report must not print `cap-compressed`.

**Block A - one launch, live quads (primary evidence; in-launch noise ~0.2 %)**
`abba2_pc.ps1 -Live AABB,DQ,P1E7 -LaunchFlags none -Quads 3 -MaxFps 240 -Input vpad`
- Order AABB (logic thread, biggest), DQ, P1E7. ON OFF OFF ON quads, 20 s windows, report rules unchanged (quad rejected on
  OFF-OFF > 1 %, ON-ON > 1.5 %, GPU busy move > 3 %; KEEP = median >= +1 %, every valid quad > 0, p99 not worse > 2 %).
- Mechanism read-backs: DQ `early retries` rise only while ON; P1E7 via the MENU line; AABB via the MENU line.
- If AABB is KEEP: re-measure DQ after it (a shorter logic stage changes how long the main thread waits).

**Block B - the requested cross-launch re-measure of DQ and P1E7 (drift-bracketed)**
`abba2_pc.ps1 -Arms BASE,DQ,P1E7,BASE,P1E7,DQ,BASE -Live none -MaxFps 240 -Input vpad` (7 launches, ~35 min)
- Each arm launch is compared with the BASE fps **interpolated** between the BASE launches before and after it (linear drift
  cancels: run6 drifted 116.2 -> 109.2 = 6 % across 6 launches, with GPU busy 6.55 -> 6.91, a scene-state change, not a flag).
- INVALID: BASE range > 8 % or an inner BASE > 1.5 % off the line through its neighbours. KEEP: both paired d > 0 and
  mean >= max(1 %, 2 x that noise). Block A decides when the two disagree (it has the lower noise floor).

**Block C - MA (code bytes, launch only)**
`abba2_pc.ps1 -Arms BASE,MA,BASE,MA,BASE -Live none -MaxFps 240 -Input vpad` (boot line `MA: 25 main-thread locked atomic
loads` = installed). Same interpolated rule. Optional mechanism proof: `[Profiler] SP = 1` one window each, 4AA34 / 419A8 self
should fall (r9 with A + caves: 4AA34 loop 2.3 % -> ~0.9 %).

**Decisions after the run.** KEEP items go into `release\profiles.json` (PC); AABB KEEP -> next RE target 71C60; DQ KEEP only
matters below the cap (keep it on, it costs nothing). Update `pc-keep-candidates.md` and `remaining-work.md` 2.4 / 2.5.
