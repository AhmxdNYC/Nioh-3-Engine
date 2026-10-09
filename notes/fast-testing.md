# Fast testing: one-launch A/B + runtime shader hot-swap (ASI 1.7.53, 2026-10-09)

Goal (user: "find a way to do tests faster"): stop paying a game launch per A/B arm. A relaunch costs ~2.6 min of boot +
60 s warm-up + ~15 s close before anything is measured, and the launch-to-launch spread (6 % in abba2 run6) is ~30x the
live within-launch noise (0.1-0.2 %), so relaunch arms were both slow and blind to small effects
(universal-optimizer `docs/faster-cheaper-testing.md` section A).

## 1. What was built

| piece | where | what |
|---|---|---|
| LiveCtl | `src/livectl.{h,cpp}`, `[Debug] LiveCtl = 0` | control file `Nioh3EngineFix_live.ini` (game folder) polled every 100 ms. `[Live]` section: `Gen`, `Tag`, then `KEY = value` lines applied in order: live menu rows by key (`PP = 1`, same code + `MENU:` log line as a menu press, via `menu::Remote`), ShaderHot sets (`Hot.ghdm = 1`), live tuning values (`Tuning.PP_Period = 8`). `<file>.ack` = `gen= tag= t_apply_unix_ms= ok= fail= KEY=readback ...`, rewritten after each apply and every 500 ms; log line `LIVE: apply gen N ...`. A file left from an earlier run is ignored until it changes. Works the same under Proton (plain files, no pipe). |
| live tuning | `dllmain.cpp LiveTuning`, `samelook::SetTuning` | `PP_Period` (PP cave mask), `VSMA_K`, `VSMA_Front`, `LPVR_N`, `SKYR_V`, `FOGT_V`, `CLDR_V`: all read at use time by the installed code, so one aligned store switches them. Before 1.7.53 they were boot-only. |
| ShaderHot | `psoswap.{h,cpp}` + `gpuopt.cpp` / `gpuopt_thunks.asm`, `[ShaderHot] Enabled = 0` | runtime A/B of loose `shader_dx12` replacements, see section 3. DIAGNOSTIC: default off, never in a release profile. |
| hot sets | `tools/testmatrix/hotset.py` | builds `<GameDir>\Nioh3EngineFix_hot\<set>\` from a stock / replacement `.file` pair (IDRK unpack, containers paired by position, only differing records). `on_<stock digest>.dxil` when the stock file is installed, `off_<replacement digest>.dxil` when the replacement is installed, so `Hot.<set> = 1` always means "replacement active". `selftest`. |
| one-launch harness | `tools/testmatrix/fastab.ps1` | one launch via `run_tour.ps1 -Input vpad` (lock, save backup/restore, ini/ASI install/restore, Steam cloud dialog, title -> Continue) on a one-stage tour list (`-Stage`, default 7292 Mirage Fortress spawn); while the tour dwells there, per item `-Pairs` x 2 windows in ABBA order (A B, B A, A B, B A, ...): LiveCtl toggle -> ack (gen echo, read-back, fail = 0) -> `-WarmSec` warm-up (default 5 s) -> `-WindowSec` window (default 20 s, UTC span). Own PresentMon session. Refuses items that `live_keys.json` calls relaunch-only. `-DryRun` (fake game answering the control file) ends `DryRun OK`. |
| analysis | `tools/testmatrix/fastab_report.py` | paired delta per pair (B - A) for GPU busy, frame time, fps; mean, SE = sd/sqrt(n), 95 % t interval, delta %, drift of A; verdict B better / B worse / no difference; `report.md` with the table and ready `[RESULT]` lines, `results.json`, wall-time budget. `selftest`. |
| key classes | `tools/testmatrix/live_keys.py` -> `live_keys.json` | every key of the base ini, every menu row and every patches.json group classified (section 4). Re-run after every ASI change. |
| proofs | `tests/run_hot_harness.py` | real D3D12 device on this PC, real gpuopt thunks: twin bound via SetPipelineState and Reset(initial PSO), on_ and off_ polarity, unrelated PSO untouched, stream + pipeline-library PSOs get twins, the game's PSO keeps its own cached blob, LiveCtl gen/tag/ok/fail/read-backs, stale control file ignored: `ALL HOT CHECKS OK`. `test_patterns.py`: ALL 243 SITES OK. |

## 2. How to use it (PC, pc2, Ally)

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\fastab.ps1 -Items Hot.ghdm,Hot.vcpfu,PP -Tag mytest
   items:  Hot.<set>             shader A/B (ghdm esm wdo28 mpc mshlod gi fogpar vcu vcp vcpfu vcpfp; more: -HotSets name=orig|repl)
           <MenuKey>             live menu row, A = 0 / B = 1 (B, PP, GF, MQ, DQ, P1E7, BPO, AS, VSMA, LPVR, ...)
           <MenuKey>=a/b         numeric row (R1=0/2, MaxLatency=0/1)
           Tuning.<Key>=a/b      PP_Period, VSMA_K, VSMA_Front, LPVR_N, SKYR_V, FOGT_V, CLDR_V
   knobs:  -Pairs 4 -WindowSec 20 -WarmSec 5 -SettleSec 45 -Stage 7292 -UserConfig <config.xml> -KeepFG -MaxFps 240 -LockKey nioh3-pc
   output: captures\pc\fastab\<stamp>-<tag>\report.md  ([RESULT] lines at the bottom), results.json, manifest.jsonl
```
- Up to ~6 items per launch (TourDwellSec <= 1800 s; ~3.5 min per item at the defaults).
- By hand (any device, also the Ally under Proton): set `[Debug] LiveCtl = 1` (+ `[ShaderHot] Enabled = 1` and a hot folder
  from `hotset.py`), then write the control file and watch the ack:
  ```
  [Live]
  Gen = 1
  Tag = ghdm_on
  Hot.ghdm = 1
  ```
- Relaunch-only keys (code bytes, boot lists, hooks) still need `abba2_pc.ps1 -Arms`, with >= 3 BASE launches and a >= 2 % bar.

## 3. ShaderHot design (runtime shader hot-swap)

- The engine reads `shader_dx12\*.file` at boot and builds its PSOs from those blobs; a loose-file A/B needed a relaunch.
- psoswap already hooks PSO creation (device vtable via the `D3D12CreateDevice` IAT slot, plus pipeline-library loads).
  With ShaderHot, after the game's PSO is created normally, psoswap plans a TWIN from the game's own desc in which the
  listed shader's bytecode is replaced by the set's file (the `CachedPSO` blob is dropped, mixed DXBC/DXIL refused, stage
  must match) and creates it straight on the runtime's ORIGINAL create method (not through the vtable), so no other hook
  - the PJ journal, Streamline - ever sees a twin. psoswap takes one reference on the game's PSO (its address can never
  be reused while it is mapped) and files `game PSO -> twin` in a lock-free 8192-slot table.
- Binding: gpuopt's command-list thunks route `SetPipelineState` (25), `Reset(allocator, initial PSO)` (10) and
  `ClearState` (11) to `GpuOptSlow` (mask `gpuopt_hook4`). The slow routine now passes a 6th argument, a pointer to the
  saved argument registers; `GpuOptSlow` calls every observer first (GP2 / SkipFx attribution stay on the game's PSO) and
  then, if the set's live switch says so, writes the twin into the saved `rdx` / `r8`, which the routine restores before
  tail-jumping to the runtime. The runtime binds the twin; the game's PSO object is unchanged.
- PSO caches: the game only ever holds its own PSOs, so `GetCachedBlob`, `ID3D12PipelineLibrary::StorePipeline` and the
  PJ journal keep the bytecode of the installed files; the twin has no cached blob of its own; the driver's shader cache
  is keyed by bytecode. Nothing on disk can end up holding the other variant.
- A set starts in the state of the installed files (binding unchanged until the first switch).
- Cost: every SetPipelineState takes the slow path (a C call + a 64-probe-max hash lookup) whether the set is ON or OFF,
  so both arms pay it equally; it is CPU-side on the render-recording thread only.
- Limits: a PSO built from shaders of two sets gets a twin for the first set only (logged); bundles (ExecuteBundle) are
  not rebound (the engine records none); GIMerge holding a Dispatch replays it with the game's PSO (warned at boot;
  fastab sets GIMerge = 0); the shipped change is still the `.file` swap (ShaderHot is for measuring only).

## 4. Key classes (`live_keys.json`, 280 keys)

| class | keys (LiveCtl syntax) |
|---|---|
| live (menu rows, 53) | VRR, W3, W3MODE, R1 (+ R1_TimeoutMs), L1, F1, WK, MaxLatency, W6, B, PP, FPC, AABB, M1, X1, E1, IK, TS, WakeStats, ED, GF, WL, WG, TG, DS, CP, SPF, MQ, PT1, DL, DQ, P1E7, BPO, W1B, RPA, AS, PJH, TR, VSMA, LPVR, SKYR, FOGT, CLDR, GPU (Barriers), R2A, R2B, R1GPU, FPS, FRAMETIME, SPEED, AUTOCONTINUELOAD, PAUSEHINT |
| live-tuning (7, new in 1.7.53) | Tuning.PP_Period, VSMA_K, VSMA_Front, LPVR_N, SKYR_V, FOGT_V, CLDR_V |
| live-shader (11, new in 1.7.53) | Hot.ghdm, esm, wdo28, mpc, mshlod, gi, fogpar, vcu, vcp, vcpfu, vcpfp (any other .file pair with -HotSets) |
| live-diag (2) | SkipFx control file (PSO / Marker skip lists) |
| relaunch (207) | everything read once at boot: [Patches] AC AQ AS0 BN CpuStats EP ESM1 FGC FGP FP GIMerge GL1 GP J1 JT LC LG LK LW LoadOnlyCpuFixes M2 MA MLP MPCS P12 PF PJ PL PM RC RCO RP RWb S1 SH SPR SR TSY VP W1 W2 W6W WC WS WorkerCap (+ their tuning keys), [Memory], [Profiler], [Debug], [GpuOpt] other than the rows above, [Latency] other than MaxLatency, [Menu]/[Overlay] layout keys |

Not made live, on purpose: M2, J1, S1, LK, MA rewrite instruction bytes inside hot loops that other threads execute
(lock-prefixed loads -> plain loads). A live switch there needs a cave + flag per site (the design every live CPU row
already uses) or a cross-modifying-code protocol with all threads parked; until one of those is built they stay
relaunch-only. Boot lists (PJ, PL, WorkerCap, P12), loader / IO hooks (EP SR RC AQ TSY SH PF RP LG) and profilers are
consumed at boot by design.

## 5. Measured on the PC (RX 9070)

(see below - filled from the in-game run)
