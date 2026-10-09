# Meshlet front-end: probe, MPC tail skip, LOD tail collapse (2026-10-09, ASI 1.7.47)

Input: render-architecture.md §0/§4.1 (the meshlet front-end = 6.5 ms of a 29.9 ms dense pc2 frame, "structural") and
gpu-rewrite-round3.md §3.1 (mod-side steps: 1 probe, 2 MPC tail skip, 3 LOD tail collapse, ~0.6-1.3 ms total). Static work + offline
proofs + a real-device harness on this PC; no game run here (the PC game lock `nioh3-pc` was held by the TR/PJH agent the whole
session). New data used: pc2 GP2 **MinRun 1** split (bridge #643, file #140, `captures/pc2/gp2_minrun1_140/`).

## 0. Verdict so far (before the probe has run in the game)

The pc2 MinRun 1 split already changes the picture of §3.1. In the 13 dense windows (31.4 ms span) the meshlet culling family is
5.9 ms, but it is **not** spread over ~100 cheap stages - five kernels carry 4.5 ms of it:

| kernel (record) | role | ms/frame | dispatches/frame |
|---|---|---|---|
| MshLod#1 e6a62bda | MRLOD root-list init (one thread per root: 2 loads, filter, 2 stores) | **1.51** | 1.5 |
| MIC#0 e31c389f | instance culling (Hi-Z footprint + VSM SAT) | 0.99 | 3.3 |
| MPC#27 1b4b5b75 | meshlet arg / index build (indirect) | 0.81 | 3.5 |
| MshLod#2 63c07c21 levels 0..5 | per-cluster LOD | 0.63 / 0.29 / 0.19 / 0.11 / 0.05 / 0.02 | ~1 per level |
| MPC#20 18b6fee7 | meshlet culling | 0.52 | 2.7 |
| MCLRC#4/#5, MPC#22 | streaming codes, scan stage 0 | 0.22 + 0.18 + 0.20 | |

The stages §3.1 steps 2 and 3 target are small on pc2: MPC #23 0.022, #24 0.022 ms/frame, **#28-#31 and #26 are not in the top-100
rows at all** (< ~0.005 ms each); MeshletPreculling self time 1.78 ms minus the named rows leaves ~0.16 ms for #0, #26, #28-#31 and
their barriers over 3.5 chains. LOD levels >= 4 (GP1 markers "4".."13") total ~0.11 ms/frame. So, honestly:

- **MPCS (step 2) is worth ~0.05-0.1 ms on pc2 dense**, not 0.2-0.5. It is built, proven and queued anyway (it is free once built).
- **LOD tail collapse (step 3) is worth <= ~0.1 ms on pc2 dense** and cannot keep identical buffers under the engine's bindings
  (§4). Not built.
- **MshLod#1 at ~1 ms per dispatch is the anomaly to explain first**: its DXIL is an 8-byte-per-thread copy with a level filter.
  Either the interval absorbs a wait (the first meshlet dispatch after the frame-begin tile-mapping / streaming sync) or the root
  list is enormous. The probe settles it (thread count from the stats frames, dispatch vs barrier split). If it is real per-root
  work, a kernel fix is cheap; if it is a wait, the cost belongs to the frame-begin sync, not the front-end.
- Whether the remaining "structure" (launch + drain per stage) is recoverable mod-side is exactly what the probe's floor x dispatch
  count vs above-floor split answers. The harness (§1.4) already shows on this RX 9070 that 6 dependent one-wave stages with UAV
  barriers cost ~22 us (~3.7 us per stage); on pc2 the same chain shape is ~2.5-3x slower (latency).

## 1. Probe (MLP) - BUILT, harness-verified, queued (pc2 Phase M)

### 1.1 GPU half (gpuprof.cpp, `[Profiler] MeshletProbe = 1`, needs GPUProf = 2 and `[GpuOpt] Barriers = 0`)
- Regions = the engine markers MeshletResolveLOD, MeshletPreculling, MeshletInstanceCulling, MeshletCheckRequestLoad,
  MeshletRequestLoad, GenerateSceneHiZ, GenerateShadowMapHiZ (per recording thread, opened at BeginEvent, closed at the matching
  EndEvent; LOD levels stay visible as the innermost marker "0".."13").
- **Timing frames** (every other GP2-sampled frame): a timestamp before every Dispatch / ExecuteIndirect and before + after every
  ResourceBarrier call (gpuopt's pass-through barrier path). The region time splits exactly into dispatch (pre-dispatch stamp ->
  next stamp: launch + execution + drain), barrier (pre -> post: the flush itself) and gap (post-barrier / region start -> next
  command: state setup, list switches). `MeshletProbe_Barriers = 0` = dispatch stamps only (each stage = dispatch + its barrier).
- **Stats frames** (the other sampled frames, `MeshletProbe_Stats = 1`): no timestamps; each dispatch is wrapped in a
  PIPELINE_STATISTICS query -> CS invocations = threads launched (0 = empty dispatch, <= 64 = at most one wave). Kept out of the
  timing frames so the counts never perturb the times.
- Per 30 s window, `GP: MLP:` lines: headline (front-end ms/frame = dispatch + barrier + gap; Dispatch / EI / barrier calls per frame;
  <= 1-wave and empty dispatches and their time; **floor** = median us of the one-wave dispatches; **structural = barrier + gap +
  floor x dispatches**, **work above the floor**), region rows (region#ordinal @recorder-caller [parent marker]) and the top
  dispatch rows (op index, CS digest, level marker, ms/frame, us/call, groups or EI max count, avg threads, % one-wave, % empty).
- `MeshletProbe_Dump = N`: raw events of N sampled frames -> `Nioh3EngineFix_mlp.csv` next to the ini.
- Covers every PSO variant: rows are keyed by the CS container digest of whatever is bound (pc2 shows different variants per spot,
  e.g. MPC#20 18b6fee7 vs the a09d4223 rows of #125; MshLod#2 has 24 permutation records, MSV `all` covers every meshlet VS,
  lodu rewrites only 63c07c21 - the variant all pc2 windows used).

### 1.2 CPU half (mlprobe.cpp, same key; v2.02 entry thunks at 531948 / 5301FC / 53FD3C, patches.json group MLP check sites)
Per recorder call: caller RVA, thread, MeshletSys, LOD level / root / cull-code counts + camera-block hash + param_11, MPC chain
`cnt` (param_5+20h|1Ch), the chain gate, the commit function (param_3 vtbl+C8h). `MLPC:` lines: calls/frame per (recorder,
caller), **duplicate-resolve test** (two LOD calls in one frame on the same MeshletSys with identical camera block and counts =
§3.1 step 4 candidate; different MeshletSys = its own buffers, not skippable), **cnt histogram** of the MPC chains (0 / 1..1024 /
> 1024 = how often MPCS applies), the three most frequent per-frame call sequences. GPU regions get the caller of the k-th call
of the same recorder in the last CPU frame; "caller match" = the region's first UAV-barrier resource equals that call's
`*(*(MeshletSys+16A170h)+18h)` (tok 0x8A carries the ID3D12Resource pointer itself, SynTokUavBarrier 113A0C).

### 1.3 Cost
Timing frames: ~4 timestamps per stage (~1000 per frame in the dense windows) in 1 of 2xSampleEvery frames; stats frames:
~100 pipeline-statistics queries. CPU hooks: ~10 calls/frame. Measurement only, default off.

### 1.4 Real-device harness (tests/gp_harness.cpp mode 5, this PC's RX 9070, no game)
A fake MeshletPreculling region (#23, #28 x2, #24, #26, #27 = six one-wave compute dispatches with a UAV barrier after each):
1.00 regions/frame, 6.00 dispatches, 6.00 barrier calls, 6.00 one-wave dispatches (64 threads each, from the stats frames).
Region 21.5-23.5 us with the probe (full or dispatch-only stamps) vs 22 us GP marker time with the probe off -> **the stamps do
not inflate the region time** (a cold-clock first run read 70-160 us: compare like with like). Barrier column ~3 us, dispatch
column ~19 us: on AMD the drain of a dependent dispatch lands in the dispatch column, the barrier column is the flush.

### 1.5 Queued (pc2, Phase M of pc2_pc_gpu_session.ps1)
`-Phases M,B -Items mpc,mpcs -GP2MinRun 1`: M1 (shipped MPC file) and M2 (`mpc` file) at the Mirage Fortress spawn (7292), 3 x 30 s
each, MeshletProbe + MpcCheck 400 chains + raw dump; digest `re\scripts_gpu\mlp_report.py` -> `M_report.txt`. Results: **pending**.

## 2. MPC tail skip (MPCS) - BUILT, proven offline, harness-verified, queued

**What.** With the `mpc` build of shader_dx12\0x29ee0902.file (sha256 3947f3d7..., gpu-culling-rewrite.md), MPC #23 leaves the
final state of the #23->#31 prefix-scan chain when nVSDrawPrm.y = cnt <= 1024, the replaced #28-#31 return at once, and #23 also
writes the value #26 writes (u9[1]). MPCS drops the recording of those no-op stages in MeshletPreculling 5301FC by jumping to the
engine's own bind-failed paths (`[Patches] MPCS = 1`, next launch, refused unless that exact file is installed):
- MPCSa 530FD3 (end of the #23 block, #23 recorded): mark the invocation (rsp) when 1 <= cnt <= 32768, then decide;
- MPCSb 530FE4 (#28 block, now only the #23-bind-failed path): drop a stale mark, then decide;
- decision: switch on and 1 <= ebx = cnt <= 1024 -> 5313C5 (the #24 section = the engine's #31-bind-failed path), else stock;
- MPCSc 531520 (#26 block): this invocation's mark present -> 5315C5 (the engine's #26-bind-failed path), else stock.
Removed per chain: 4 dispatches + 8 UAV barrier tokens (#28-#31) + 1 dispatch + 1 barrier (#26); 3.5 chains/frame on pc2 dense.

**Proof.**
1. Chain: `gpu-shaders\culling\verify_culling.py` check B ("#23 alone [+ #28-#31 only if cnt > 1024], no #26" == shipped chain, all
   UAV contents, DXIL reference interpreter): re-run this session, 94 scenes (quick set; the full set has 95 incl. cnt 33000), 0 failures.
   cnt 0 is outside B's domain (with 0 groups #23 writes nothing; #26 would still write u9[1]) -> the caves never skip there.
2. Caves: `tests\run_mpcs_harness.py` runs the real mpcskip.asm the way 5301FC reaches it (one frame per invocation, constant rsp,
   rbx = cnt, entry by jmp) against the model "#28-#31 skipped <=> on and 1 <= cnt <= 1024; #26 skipped <=> on and #23 recorded in
   THIS invocation and 1 <= cnt <= 32768": 272 edge cases + 200,000 random invocations incl. 20k A-path invocations that never reach
   #26 (stale marks) at shared stack depths; registers the stock code needs at each landing (rax = [rsi], edx = 0Eh/0Bh), rbx / rsi /
   rdi / rsp untouched, token pointer +8 on the A path: ALL OK. Mutation check: `cmp eax,1023 -> 1024` = 2,231 failures, "always
   skip #26" = 84,449 failures.
3. Sites: patches.json group MPCS (3 jmp sites + 9 deps: cnt store, both ebx reloads, both resumes, both skip targets and the
   engine's own branches to them); `tests\test_patterns.py`: ALL 237 SITES OK. Branch scan of 5301FC: nothing jumps into the
   patched ranges except 530FE4 from the replaced jmp. Liveness at every target: only volatile "arg?" (not real arguments).
4. The one assumption the static proof cannot close (gpu-culling-rewrite §3): CPU cnt == cbuffer nVSDrawPrm.y. **Runtime self-check
   (MpcCheck)** below; MPCS is not to ship before M2 reports 0 mismatches over >= 400 chains in dense scenes.

**Self-check (mpccheck.cpp, `[Profiler] MpcCheck = N`, run with MPCS = 0).** In the stats frames, inside MeshletPreculling: the
buffers the engine UAV-barriers after #23 (before #28 runs) are copied to READBACK memory before #28 and after #31 (before the next
dispatch), and again before / after #26 (transition UAV -> COPY_SOURCE -> UAV around the copies, on the game's own list; a
WriteBufferImmediate marker tells the Present thread the copies are done). Equal = skipping would give identical buffers on that
frame's real data. Applicability mirrors the caves (#23 as Dispatch(1,1,1) <=> 1 <= cnt <= 1024; #26: 1..32 groups). Barriered
buffers in the tail that the snapshot set misses are counted. Negative control: with the SHIPPED file #28-#31 do real work and
MpcCheck must report mismatches (M1). Harness (gp_harness mode 5, RX 9070): no-op K28 chains 50 equal, changing K28 chains
50 MISMATCH, #26 100 equal / 0 mismatch - the check detects a one-dword change.

**Expected gain.** pc2 dense ~0.05-0.1 ms (from the MinRun 1 residual above); Ally x2-4 latency; RX 9070 ~0.02. Risk: none once
MpcCheck is clean (missing geometry if .y != cnt - which MpcCheck would show as MISMATCH). Status: **queued (pc2 Phase B item
`mpcs` = mpc file + MPCS = 1 vs `mpc` vs BASE)**.

## 3. LOD tail collapse - NOT BUILT (not recoverable with identical buffers; worth <= ~0.1 ms on pc2)

Design in §3.1: when a level fits one group, one MshLod#2 group finishes the remaining levels in-kernel. The bindings rule it out:
per level the recorder (531948) binds, for MshLod#2, the payload buffer, the NEXT level's counter (lVar16, u3) and the selected-
cluster outputs, but **not the current level's argument buffer** (lVar18, the ExecuteIndirect args), which MshLod#3 of the next
level converts (count -> groups) and the level after resets ({0,1,1,0}). A kernel that runs level L+1 in-group cannot leave those
two ping-pong buffers as the stock chain does (their final values belong to the last two levels), and the stock #3/#2 records of
the collapsed levels still run afterwards and would redo or undo the work. Identical buffers need a recorder change (bind both
argument buffers, or stop recording the collapsed levels) plus a new kernel per MshLod#2 permutation (24 records) - and a
draw-set-equivalent variant (counters not compared) needs proof that nothing reads the argument buffers after the resolve.
The pc2 MinRun 1 numbers cap the prize at ~0.11 ms/frame (GP1 levels "4".."13" incl. their #3 and barriers, dense windows), of
which the work itself stays. Not worth 1-2 weeks; the probe's per-level rows (marker "0".."13", threads per level) will confirm.
Empty-tail note: once a level is empty, every later level's #3/#2 rewrites identical values (idempotent), so an ASI-only skip of
empty tail levels would be same-picture, but the CPU cannot know emptiness at record time and GPU predication does not skip the
barriers: ~0 gain. Not built.

## 4. Second resolve (§3.1 step 4)
Decided by the CPU half's duplicate-resolve test (`MLPC: ... identical inputs on the same MeshletSys N`). Nothing built until the
probe shows identical inputs on the same buffers.

## 5. Files
| file | what |
|---|---|
| mod/Nioh3EngineFix/src/gpuprof.cpp / .h | MLP GPU half (regions, timing / stats frames, aggregation, log, dump), MpcCheck hooks |
| mod/Nioh3EngineFix/src/gpuopt.cpp | stamps around pass-through ResourceBarrier calls (only while MLP / MpcCheck is on) |
| mod/Nioh3EngineFix/src/mlprobe.cpp / .h | MLP CPU half (recorder entry thunks), SHA-256 file guard for MPCS |
| mod/Nioh3EngineFix/src/mpccheck.cpp / .h | MpcCheck self-check |
| mod/Nioh3EngineFix/src/mpcskip.asm | MPCS caves |
| mod/Nioh3EngineFix/patches.json | groups MPCS (3 jmp + 9 deps) and MLP (3 check sites) |
| mod/Nioh3EngineFix/tests/run_mpcs_harness.py + mpcs_harness.* | caves / kernel map / SHA-256 harness |
| mod/Nioh3EngineFix/tests/gp_harness.cpp mode 5 | MLP + MpcCheck on a real D3D12 device (`gp_harness.exe 5 [ctl|light]`) |
| re/scripts_gpu/mlp_report.py | log / CSV digest |
| tools/testmatrix/pc_gpu_session.ps1 (+ pc2_ copy), pc_gpu_report.py | Phase M; items mpc / mpcs |
| captures/pc2/gp2_minrun1_140/ | pc2 MinRun 1 split used in §0 |
