# Ally GFX-queue gap `[GFM_TextureClear -> App_CopyMRLROcnScene]`: root cause (2026-10-09)

Question: the Ally's DIRECT queue idles 0.52-0.81 ms once per frame, mid-frame, between the lists ending in `GFM_TextureClear`
and starting with `App_CopyMRLROcnScene` (bridge #391; game-logic-audit.md #2; gq-jt-probes.md). Is it a CPU-late submit,
vkd3d work at the boundary, or a resource transition, and is it worth ~+2 % fps?
Inputs: Ally GQ log 1.7.43 (`captures/ally/gq_1743/gq.log`, #119), the Ally fogsync log 1.7.49
(`captures/ally/fogsync_1749/fogsync.log.gz`, bridge file #161 for result #884), the PROTON_LOG summary (#120 for result #393),
pc2 SCHED1 (#842), and Ghidra on the v2.02 dump. Tags: **M** measured, **S** static, **E** estimate. No new game run.

## 0. Verdict

**The gap is the async fog join. It is not a CPU-late submit, not vkd3d overhead, and not a transition. The GPU is not
idle in it: the compute queue is running the fog alone. Expected gain from removing it on the Ally is about 0 (measured).**
No probe is needed and nothing was queued on the Ally.

- **What is recorded there (S).** `StaticNatureRenderView` 108590 calls the SSFog binding helper `4F9AD8` (call at 108C52).
  That helper sets SSFog constants and textures (4F9C74 x3, 4F9570, 107040, 10708C, 1070D8, 107104: CPU-side state writes
  that only set a dirty bit at ctx+12E8h, with no GPU commands). Then, gated by `RS+968h & 0x20000`, it peeks async slot 4
  (`3C5D0C(RS+1130h, &f, 4)`) and calls `GfxWaitAsyncFence 4F8F38`. That function asks the fence `IsComplete` (vf+48h) and, if
  not, records the wait through 4F8F84. The ECL thread (4F9114) replays it as tok6 `ExecuteCommandLists(list k)`, tok8
  `queue->Wait(fence, value)` (`EclTokQueueWait` 4F93C8, `ID3D12CommandQueue` vtbl+78h), then tok6 for list k+1.
  So the boundary is a GPU-side cross-queue Wait. There is no Signal, flush, CPU fence wait or `SetEventOnCompletion` on that
  path. GPU-bound, the CPU records ~1 frame ahead, so the fence is never complete at record time and the wait is emitted every
  frame (GQ: 0.94-1.00 gaps/frame).
- **CPU-late is excluded (M).** The two lists have consecutive submission numbers on the same ECL call site
  (e.g. 119263 -> 119264, `4F9114 < 4F908B`): nothing else is submitted between them. More importantly, the gap goes away
  when only the queue Wait goes away (next point), with the same CPU recording.
- **The A/B that decides it (M, #884, fogsync log).** Same launch, same spot, `[Patches] AS = 2`, `AS_Slots = -4`
  (fog kept on DIRECT while the row is ON):

  | window (30 s) | fog | DIRECT lists/frame | GQ gaps ms/frame | top edge | lists busy ms | span ms | async-compute ms | fog CS 5473bfb7 |
  |---|---|---|---|---|---|---|---|---|
  | 14:18:17 OFF | async (stock) | 17 | 0.898 | TextureClear->Ocn 0.591 | 27.715 | 29.107 | 3.23 | 1.604 ms async wall |
  | 14:19:17 ON | DIRECT | 15 | **0.352** | edge gone | **28.176** | **29.042** | 1.52 | **0.717 ms on DIRECT** |
  | 14:19:47 OFF | async | 16 | 0.876 | 0.542 | 27.763 | 29.127 | 2.87 | 1.256 async |
  | 14:20:17 OFF | async | 17 | 1.039 | 0.708 | 27.604 | 29.105 | 3.22 | (async) |
  | 14:21:18 ON | DIRECT | 15 | **0.327** | edge gone | **28.235** | **29.068** | 1.71 | **0.713 on DIRECT** |

  (14:18:47 and 14:20:48 are switch windows, mixed.) fps 34.9 both arms; GPU 2116-2129 MHz, 21.8-22.0 W.
  - Sync removes the edge (-0.55 ms gaps) and 2 lists/frame (the kick split and the join split), and adds +0.5 ms of DIRECT
    busy time. **Span is identical within 0.06 ms.**
  - Async, the fog dispatch takes 1.25-1.60 ms of wall time. Synchronous, it needs 0.71 ms. Roughly 1.0 ms of its async
    wall overlaps DIRECT work and makes little progress there (the graphics queue has priority on a saturated GPU). The
    remaining ~0.6 ms it runs alone while DIRECT waits at the join. **The "idle" gap is the fog running on the compute queue.**
    GQ only sees DIRECT stamps, so it reports this as idle.
- **vkd3d (S + M).** tok8 maps to a vkd3d-proton queue Wait, which becomes a timeline-semaphore wait on the next submit
  (GPU-side). The compute Signal was submitted at the kick, so the wait-before-signal path is not taken. PROTON_LOG (#120)
  shows no vkd3d submission or wait warnings. The only vkd3d cost here is the per-split class (~40 us per boundary,
  gq-jt-probes §5, gpu-scheduling-sync §3), which the sync arm also removes. It is already inside the measured span-neutral result.
- **Transition (S).** List k+1 starts with HEADS 0.013-0.015 ms/frame for the whole frame (start -> first stamp, which
  includes list-start COMMON->X barriers). A transition at this boundary would land there, and it is negligible.

## 1. Why there is no +2 %

0.6 ms of 28.5 ms is +2 % only if the GPU were idle. It is not: the same span with fog on either queue means the Ally's
25 W iGPU is throughput-bound (power and clock), not parallelism-bound. The fog costs ~0.7 ms of GPU however it is
scheduled. Moving it to DIRECT is neutral (#884). Kicking it earlier or joining it later (engine reorder) gives more overlap,
but on a saturated GPU the overlapped fog is starved as it is now, so the expected gain is **0 to +0.2 ms (0 to +0.7 %), E**.
It could only pay if the fog overlapped a pass that leaves CUs idle (raster-only shadow tiles, the barrier-bound meshlet
front-end). Fog needs the shadow maps and light lists, so it cannot move before them. That is an engine-level reorder of
VolumetricFogScatter (node 162) relative to its inputs, with high risk for <= 0.2 ms. **Not recommended.**

Do not ship `AS_Slots = -4` on the Ally either: neutral, and it costs nothing to keep stock.

## 2. Windows (pc2 / PC)

- pc2 SCHED1 fogsync (#842): GPUBusy -0.164 / +0.069 ms vs BASE (noise 0.061), fps +0.3 %, neutral. On pc2 the fog join sits
  after ~3-4 ms of DIRECT work against fog's 1.0-1.3 ms wall (gpu-scheduling-sync §0.2), so the join should never wait.
- There is no PC or pc2 GQ capture yet (Phase U pending). The same pattern can only cost on Windows where fog's async wall exceeds
  the DIRECT work between kick and join, i.e. a weak iGPU. Measured cost there is ~0 in every arm.

## 3. Corrections to older notes

- STATUS.md "0.65 ms = CPU-late submit after GFM_TextureClear (bridge #391)": wrong. It is the async-fog (slot 4) join,
  GPU-side, and the compute queue is busy during it (#884 + this note).
- game-logic-audit.md #2 "fits the ocean slot-6 wait": falsified by #536 (ocean producer 0 calls). The fog join is the
  one in that region (gpu-scheduling-sync §1.1) and #884 confirmed it.
- gq-jt-probes.md §5: a regular once-per-frame edge with mean >> 150 us is not necessarily CPU-late. Check for a
  `GfxWaitAsyncFence` join first. When an async slot is involved, GQ "gaps" are DIRECT-idle time, not GPU-idle time. Compare
  the edge with GP2's async-compute wall for that slot before reading it as wasted.

## 4. Reproduce

```
py -I - <<EOF   # per-window rows above
import gzip,re; L=gzip.open(r'captures/ally/fogsync_1749/fogsync.log.gz','rt').read().splitlines()
for l in L:
    if re.search(r'stats/s AS:|GQ: frame|GQ:   1 \[|5473bfb7', l): print(l[:220])
EOF
re\decomp.cmd 0x4F9AD8 c:0x4F9AD8 0x4F8F38 0x4F93C8      # join helper, its caller 108590, the wait, ECL tok8
```
