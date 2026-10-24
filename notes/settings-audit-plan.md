# Two-stage settings audit

This supersedes the full 3-device ? 3+3-run sweep (request #24269).
Inventory: **94 keys = 88 explicit default-OFF optimizations + 6 auto/latency
probes**. Pure diagnostics, including TS, are excluded. The shipped INI has
12 explicit Patches switches at 1; that is not a runtime activation count.

## Fixed fixture

**Yoritomo R7B35 via fight-now, 60-second fixed window, PresentMon.**
Fixture ID: `Yoritomo-R7B35-fight-now-60s-PresentMon-v1`, identical on all devices.
The planner refuses a different fixture or duration. Owners freeze the save,
player/boss positions, equipment, input route and capture start event; the ID
alone is not a reproduction. Default unmeasured warmup is 30 seconds.
Use the same build, per-device graphics/cap/upscaler/FG/power profile and cache
state in every OFF/ON pair. Results and conclusions remain device-specific.

## Stage 1: split screen

Each key belongs to exactly ONE device, with platform-restricted keys assigned
first. Each gets **one OFF and one ON** capture. Pair order alternates to reduce
systematic warming bias. A separate unchanged baseline-OFF capture runs at the
start, **after every eight keys**, and at the end of the final partial block.
These baseline anchors always use the same frozen source-baseline.ini on their
device; pair-specific prerequisites do not change those drift anchors.

| Device | Assigned keys | Key captures | Baseline anchors | Total captures | At 4 minutes/capture |
|---|---:|---:|---:|---:|---:|
| pc / Windows / RX 9070 | 32 | 64 | 5 | 69 | 4 h 36 min |
| pc2 / Windows / RX 6650 XT | 31 | 62 | 5 | 67 | 4 h 28 min |
| ally / Linux-Proton / ROG Ally | 31 | 62 | 5 | 67 | 4 h 28 min |

**203 total planned captures**, parallel across the three owners. Times are
arithmetic estimates, not measured runtimes; setup/retries are additional.
Patches.SR is retained in Ally's inventory but marked already_auto_on: its -1
baseline already enables it on Proton and Windows refuses it. Skip its two
captures as NOT_APPLICABLE rather than reporting a fictitious effect. MPCS is
fixture-dependent; ON needs its hash-verified shader package in BOTH arms.
Other auto-policy keys require log proof that the treatment activated.

```powershell
py tools/settings_audit.py --stage screen --out captures/audit-screen
```

Optional `--baseline pc=<file> --baseline pc2=<file> --baseline ally=<file>` accepts
frozen per-device INI copies. `--ini` defines the shipped inventory. No input is
modified. Output directories must be new. Copies include full OFF/ON INIs,
exact edit-line files, inventory.csv, manifest.json, schedule.csv and
results-template.csv. The planner never launches/stops the game or holds a lock.

Every OFF/ON pair changes exactly one section-qualified key. Shared prerequisites
are identical in BOTH arms (e.g. Barriers=1 for DropUndoPairs, PJ=1 for PJH,
L1OnWine=1 on Ally for TG/DS). Use the pair's OFF result for its effect; the global
baseline captures measure drift, not the pair's absolute performance difference.

## Measurements and Stage-2 gate

Fill results-template.csv with one row per scheduled run, keeping run_id, fixture
and ini_sha256. Set valid=1 ONLY after checking the actual applied INI, active
patch, complete 60-second capture and fixed fight. Metrics are:

- avg_fps: rendered frames per second (not FG/displayed FPS).
- low1_fps: 1% low FPS, using the same PresentMon postprocessor definition in all runs.
- frametime_ms: mean rendered frame time, same frame-time column throughout.

The tool joins run IDs, rejects duplicates/unknown IDs, mismatched fixture/INI
hashes, nonfinite or nonpositive metrics. Missing/invalid observations remain
INCOMPLETE. No zero or guessed result is substituted. Raw PresentMon CSVs and
logs remain evidence; these summary columns are not claimed to be raw PM headers.

For each device and each metric, measured baseline spread is:
`100 ? (maximum baseline value - minimum baseline value) / mean baseline value`.
All that device's planned baseline anchors must be present. This conservative
whole-screen range also catches long-term drift; no other device's drift is used.
Positive deltas mean improvement: FPS/1% low = `100 ? (ON/OFF - 1)`;
frame time = `100 ? (OFF - ON) / OFF`.

A key **ADVANCES only if at least one Stage-1 delta strictly exceeds that metric's
measured baseline spread**. Otherwise DROP_SCREEN (not proof of no possible
benefit on another device). There is no minimum fixed percentage threshold.

```powershell
py tools/settings_audit.py --stage confirm --plan captures/audit-screen --results screen-results.csv --out captures/audit-confirm
```

Only ADVANCE keys receive a Stage-2 schedule, on the SAME device: **3 OFF + 3 ON
runs**, alternating order. New baseline anchors run at the start, every eight
confirmed keys, and the end. Arm INIs are copied from the screen and their hashes
verified; altered frozen files fail. No winners produces an empty schedule.
No confirmation of unmeasured keys or automatic cross-device extrapolation.

## Keep / drop rule

After filling the confirmation plan's results template:

```powershell
py tools/settings_audit.py --stage evaluate --plan captures/audit-confirm --results confirm-results.csv --out captures/audit-verdicts
```

For each metric compute the delta between the arithmetic means of the three
ON and three OFF runs. **KEEP iff at least one mean improvement is strictly
> 2 ? that metric's NEW baseline spread, AND mean ON 1% low is at least mean OFF
1% low.** A 1% low regression yields DROP even if average FPS improves. Equality
at the 2? threshold does not pass. Missing data yields INCOMPLETE;
nonapplicable keys never advance. decisions.json includes means, deltas and
spreads, not just a label. `--stage evaluate` can also inspect screening decisions.

KEEP is a performance result on that device, not an automatic INI-default edit.
Owners still check crashes, latency, visual correctness, memory and activation.
Three runs do not establish arbitrarily small differences if measured drift
exceeds them. No benchmark or performance gain is claimed by this offline tool.

## Important controls

WorkerCap ON=10 and TSY ON=4 are thread counts; TSY=1 clamps to stock 3.
KL_Div ON=2; MaxFrameLatency is -1(auto) versus 2, with a latency trade-off.
W6/GF/MQ/EP/SR use -1(auto) versus 1, assigned where a treatment exists.
ExtraPerf X* ON=1 tests the least aggressive level, with Enabled=1/Preset=0 in
both arms; picture changes need review. Zero-valued queue modes, tuning values,
LM_Force's safety bypass, ShaderSwap/ShaderHot infrastructure and pure diagnostics
are excluded; SkipDebugString is included because it removes logging overhead.

PJ/PJH/PF/PL need prepared journals/traces/libraries and identical cache snapshots;
FGC/FGOnWine need supported FG ON in both arms. The fixed boss fight cannot alone
rule out boot/loading/menu benefits (LoadOnlyCpuFixes, EP/SR/RC/AQ/TSY/SH/PF/LC/LW/PL,
MenuQuantize). Such findings remain scoped to this fight screen; future targeted
intervals are not silently added to this time budget. PresentMon acquisition on
Ally remains the Linux game owner's integration responsibility; this planner
only consumes equivalent rendered-frame summaries and does not supply a collector.

## Offline validation

`py -m unittest discover -s tools -p test_settings_audit.py -v`

Nine tests PASS: unique balanced split; exactly one changed key per pair; one
OFF/ON per key; baseline spacing; fixture/duration/collector; applicability and
numeric options; drift gate/missing data; winners-only confirmation; 3+3 counts;
strict 2? threshold and 1% low regression; invalid results; frozen INI integrity;
comment preservation; CLI output and overwrite refusal. No game touched.

## Exact key ownership

**pc (32):** `Patches.RD`, `Patches.WorkerCap`, `Patches.FGC`, `Patches.P12`, `Patches.B`, `Patches.PP`, `Patches.M1`, `Patches.J1`, `Patches.E1`, `Patches.MA`, `Patches.SPF`, `Patches.MQ`, `Patches.DL`, `Patches.RWb`, `Patches.TR`, `Patches.CBS`, `Patches.VSMA`, `Patches.LPVR`, `Patches.EP`, `Patches.AQ`, `Patches.PF`, `Patches.GIMerge`, `ExtraPerf.XCLD`, `ExtraPerf.XGI`, `ExtraPerf.XTER`, `ExtraPerf.XPRT`, `VRR.MenuQuantize`, `Latency.MaxFrameLatency`, `GpuOpt.DropUndoPairs`, `GpuOpt.CbvMirror`, `GpuOpt.SceneBufMirror`, `Debug.SkipDebugString`.

**pc2 (31):** `Patches.AS`, `Patches.W6`, `Patches.M2`, `Patches.X1`, `Patches.S1`, `Patches.ED`, `Patches.GF`, `Patches.TG`, `Patches.DQ`, `Patches.P1E7`, `Patches.BPO`, `Patches.W1B`, `Patches.RPA`, `Patches.PT1`, `Patches.MPCS`, `Patches.FOGT`, `Patches.CLDR`, `Patches.PJ`, `Patches.PL`, `Patches.GL1`, `Patches.LC`, `Patches.LW`, `ExtraPerf.XFOG`, `ExtraPerf.XSHD`, `ExtraPerf.XLOD`, `ExtraPerf.XAO`, `Latency.VrrPresent`, `GpuOpt.FrameOverlap`, `GpuOpt.AnyCaller`, `GpuOpt.CbvMirrorAllSites`, `Memory.LM`.

**ally (31):** `Patches.MW`, `Patches.AS0`, `Patches.L1OnWine`, `Patches.FGOnWine`, `Patches.FPC`, `Patches.AABB`, `Patches.M2S`, `Patches.IK`, `Patches.WG`, `Patches.DS`, `Patches.LK`, `Patches.CP`, `Patches.ESM1`, `Patches.RTP`, `Patches.PCM`, `Patches.CBF`, `Patches.SKYR`, `Patches.PJH`, `Patches.LoadOnlyCpuFixes`, `Patches.SR`, `Patches.RC`, `Patches.TSY`, `Patches.SH`, `ExtraPerf.XSHF`, `ExtraPerf.XPOST`, `Tuning.KL_Div`, `GpuOpt.Barriers`, `GpuOpt.TileBatch`, `GpuOpt.R2a`, `GpuOpt.R2b`, `GpuOpt.R1`.

## Candidate inventory

The table below is generated from the shipped INI. A listed OFF=-1 is the auto
baseline described above, not a forced 0. Pair prerequisites are in the manifest.

| Section | Key | OFF edit | ON edit | INI line |
|---|---|---|---|---:|
| Patches | MW | `MW = 0` | `MW = 1` | 117 |
| Patches | RD | `RD = 0` | `RD = 1` | 119 |
| Patches | WorkerCap | `WorkerCap = 0` | `WorkerCap = 10` | 145 |
| Patches | AS0 | `AS0 = 0` | `AS0 = 1` | 152 |
| Patches | AS | `AS = 0` | `AS = 1` | 162 |
| Patches | L1OnWine | `L1OnWine = 0` | `L1OnWine = 1` | 168 |
| Patches | FGOnWine | `FGOnWine = 0` | `FGOnWine = 1` | 173 |
| Patches | FGC | `FGC = 0` | `FGC = 1` | 179 |
| Patches | P12 | `P12 = 0` | `P12 = 1` | 183 |
| Patches | W6 | `W6 = -1` | `W6 = 1` | 191 |
| Patches | B | `B = 0` | `B = 1` | 193 |
| Patches | PP | `PP = 0` | `PP = 1` | 195 |
| Patches | FPC | `FPC = 0` | `FPC = 1` | 197 |
| Patches | AABB | `AABB = 0` | `AABB = 1` | 199 |
| Patches | M1 | `M1 = 0` | `M1 = 1` | 201 |
| Patches | M2 | `M2 = 0` | `M2 = 1` | 203 |
| Patches | M2S | `M2S = 0` | `M2S = 1` | 205 |
| Patches | J1 | `J1 = 0` | `J1 = 1` | 207 |
| Patches | X1 | `X1 = 0` | `X1 = 1` | 209 |
| Patches | E1 | `E1 = 0` | `E1 = 1` | 211 |
| Patches | S1 | `S1 = 0` | `S1 = 1` | 213 |
| Patches | IK | `IK = 0` | `IK = 1` | 218 |
| Patches | ED | `ED = 0` | `ED = 1` | 228 |
| Patches | GF | `GF = -1` | `GF = 1` | 234 |
| Patches | WG | `WG = 0` | `WG = 1` | 244 |
| Patches | TG | `TG = 0` | `TG = 1` | 250 |
| Patches | DS | `DS = 0` | `DS = 1` | 254 |
| Patches | DQ | `DQ = 0` | `DQ = 1` | 259 |
| Patches | P1E7 | `P1E7 = 0` | `P1E7 = 1` | 263 |
| Patches | LK | `LK = 0` | `LK = 1` | 266 |
| Patches | MA | `MA = 0` | `MA = 1` | 272 |
| Patches | BPO | `BPO = 0` | `BPO = 1` | 278 |
| Patches | W1B | `W1B = 0` | `W1B = 1` | 284 |
| Patches | RPA | `RPA = 0` | `RPA = 1` | 288 |
| Patches | CP | `CP = 0` | `CP = 1` | 291 |
| Patches | SPF | `SPF = 0` | `SPF = 1` | 293 |
| Patches | MQ | `MQ = -1` | `MQ = 1` | 298 |
| Patches | PT1 | `PT1 = 0` | `PT1 = 1` | 301 |
| Patches | DL | `DL = 0` | `DL = 1` | 304 |
| Patches | RWb | `RWb = 0` | `RWb = 1` | 310 |
| Patches | ESM1 | `ESM1 = 0` | `ESM1 = 1` | 315 |
| Patches | MPCS | `MPCS = 0` | `MPCS = 1` | 322 |
| Patches | TR | `TR = 0` | `TR = 1` | 329 |
| Patches | RTP | `RTP = 0` | `RTP = 1` | 336 |
| Patches | PCM | `PCM = 0` | `PCM = 1` | 343 |
| Patches | CBS | `CBS = 0` | `CBS = 1` | 355 |
| Patches | CBF | `CBF = 0` | `CBF = 1` | 364 |
| Patches | VSMA | `VSMA = 0` | `VSMA = 1` | 373 |
| Patches | LPVR | `LPVR = 0` | `LPVR = 1` | 380 |
| Patches | SKYR | `SKYR = 0` | `SKYR = 1` | 381 |
| Patches | FOGT | `FOGT = 0` | `FOGT = 1` | 382 |
| Patches | CLDR | `CLDR = 0` | `CLDR = 1` | 383 |
| Patches | PJ | `PJ = 0` | `PJ = 1` | 394 |
| Patches | PJH | `PJH = 0` | `PJH = 1` | 409 |
| Patches | PL | `PL = 0` | `PL = 1` | 421 |
| Patches | GL1 | `GL1 = 0` | `GL1 = 1` | 431 |
| Patches | LC | `LC = 0` | `LC = 1` | 439 |
| Patches | LW | `LW = 0` | `LW = 1` | 446 |
| Patches | LoadOnlyCpuFixes | `LoadOnlyCpuFixes = 0` | `LoadOnlyCpuFixes = 1` | 457 |
| Patches | EP | `EP = -1` | `EP = 1` | 466 |
| Patches | SR | `SR = -1` | `SR = 1` | 478 |
| Patches | RC | `RC = 0` | `RC = 1` | 482 |
| Patches | AQ | `AQ = 0` | `AQ = 1` | 489 |
| Patches | TSY | `TSY = 0` | `TSY = 4` | 493 |
| Patches | SH | `SH = 0` | `SH = 1` | 496 |
| Patches | PF | `PF = 0` | `PF = 1` | 506 |
| Patches | GIMerge | `GIMerge = 0` | `GIMerge = 1` | 522 |
| ExtraPerf | XCLD | `XCLD = 0` | `XCLD = 1` | 536 |
| ExtraPerf | XFOG | `XFOG = 0` | `XFOG = 1` | 538 |
| ExtraPerf | XGI | `XGI = 0` | `XGI = 1` | 540 |
| ExtraPerf | XSHD | `XSHD = 0` | `XSHD = 1` | 542 |
| ExtraPerf | XSHF | `XSHF = 0` | `XSHF = 1` | 544 |
| ExtraPerf | XLOD | `XLOD = 0` | `XLOD = 1` | 546 |
| ExtraPerf | XTER | `XTER = 0` | `XTER = 1` | 548 |
| ExtraPerf | XPRT | `XPRT = 0` | `XPRT = 1` | 550 |
| ExtraPerf | XAO | `XAO = 0` | `XAO = 1` | 552 |
| ExtraPerf | XPOST | `XPOST = 0` | `XPOST = 1` | 554 |
| VRR | MenuQuantize | `MenuQuantize = 0` | `MenuQuantize = 1` | 572 |
| Tuning | KL_Div | `KL_Div = 0` | `KL_Div = 2` | 606 |
| Latency | MaxFrameLatency | `MaxFrameLatency = -1` | `MaxFrameLatency = 2` | 650 |
| Latency | VrrPresent | `VrrPresent = 0` | `VrrPresent = 1` | 659 |
| GpuOpt | Barriers | `Barriers = 0` | `Barriers = 1` | 664 |
| GpuOpt | FrameOverlap | `FrameOverlap = 0` | `FrameOverlap = 1` | 669 |
| GpuOpt | TileBatch | `TileBatch = 0` | `TileBatch = 1` | 673 |
| GpuOpt | DropUndoPairs | `DropUndoPairs = 0` | `DropUndoPairs = 1` | 679 |
| GpuOpt | R2a | `R2a = 0` | `R2a = 1` | 685 |
| GpuOpt | R2b | `R2b = 0` | `R2b = 1` | 689 |
| GpuOpt | R1 | `R1 = 0` | `R1 = 1` | 692 |
| GpuOpt | AnyCaller | `AnyCaller = 0` | `AnyCaller = 1` | 695 |
| GpuOpt | CbvMirror | `CbvMirror = 0` | `CbvMirror = 1` | 709 |
| GpuOpt | CbvMirrorAllSites | `CbvMirrorAllSites = 0` | `CbvMirrorAllSites = 1` | 711 |
| GpuOpt | SceneBufMirror | `SceneBufMirror = 0` | `SceneBufMirror = 1` | 730 |
| Memory | LM | `LM = 0` | `LM = 1` | 935 |
| Debug | SkipDebugString | `SkipDebugString = 0` | `SkipDebugString = 1` | 960 |
