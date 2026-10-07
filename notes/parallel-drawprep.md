# Main-thread draw prep: can it go parallel, and what can be eliminated instead

Static analysis plus offline native tests only (dump, Ghidra export `D:\nioh3-re\decomp`, the r5 / r7 / r9 sampling
profiles, the round-9 benches). Nothing was attached to the game, read live, written or applied. Date: 2026-10-07.
Script: `patcher\drawprep_elim.fsx` (offline run: **ALL PASSED**, output `re\scripts_draw\drawprep_elim_offline.txt`).
Symbols: `re\symbols\incoming\parallel-drawprep.tsv` (24 rows, validated with `merge_incoming.py --dry`).

## TL;DR

The goal changed during this work: the ROG Ally (power-bound, 25 W shared) is now the MVP. So the verdict is split.

| | what | verdict | expected gain | built |
|---|---|---|---|---|
| **(a) parallelisation** (PC latency) | Record draws on the KIDS job workers instead of the main thread | **NO-GO** as a live patch. KTGL has exactly one recording context and one ordered token stream with a shared state-shadow cache, and DrawEmit has side effects outside the stream on every draw (§4) | Even if the engine supported it: at most about +15–20 % fps on the 7600X before the GPU (7.2–7.6 ms) and the single dispatcher thread cap it. **On the Ally it is ≤ 0**: the work only moves, and spreading it costs power | no |
| (a') helper-thread runahead prefetch | A worker walks the draw lists ahead of the main thread to warm L3 | **NO-GO**. The same idea done in-line (meshlet prefetch caves P4AA/P1E7) measured neutral to negative in round 9. It adds CPU work, which is the wrong direction on the Ally | ~0 | no |
| (a'') texture streaming 128EF0/1290B8 as a job | The only draw-prep piece that writes no tokens | Possible in principle, but invasive: allocator critical section, the visited array needs a sync point, and the main thread is not a KIDS worker. Worth 2–3 % of the main thread on PC; **0 on the Ally** | no |
| **(b) work elimination** (Ally power + PC fps) | **PP**: run the page-pool usage sweep every 4th frame instead of every frame | **GO** (default OFF, A/B pending) | 2523FC is 2.2–3.0 % of the main thread → **saves ~1.6–2.3 %**, plus a full sweep of the page-pool table (memory traffic) on 3 of 4 frames | `drawprep_elim.fsx` PP |
| (b) | **FPC**: per-frame cache for material `FindParam(name)` (per draw "Meshlet", per group "IndexedRim" …) | **GO** (default OFF, A/B pending) | The lookup family is ~1.1–1.6 % of the main thread → **saves ~0.7–1.3 %** at a 60–90 % hit rate (measured live by `status`) | `drawprep_elim.fsx` FPC |

**Realistic total for (b): about 2.3–3.6 % of the main thread.** That is roughly +2–3.5 % fps on the 7600X where it is
CPU-bound (r9: CPU 9.0 ms vs GPU 7.5 ms per frame). On the Ally the CPU work removed is about 0.03 of a core. It is
real, but small. The draw-prep path has no single large redundant computation left; the earlier big wins (W1, gallop B
+5 %) have taken them. What remains costs cache misses per draw, so it scales with how much is drawn. The large Ally
levers are outside draw prep: the worker-side patches (WC/WK/J1/M1/AABB) and draw count.

---

## 1. The serial main-thread draw-prep path (cost shares from the captures)

[App]Game is a render task-graph executor (`notes\mainthread-logic.md` §2). Shares below are % of main-thread samples
(r5 / r7-A1 / r9-all). Inclusive figures use the corrected accounting: 535188's 53 KB stack frame hides its callers.

```
1A74130 MainLoopFrame → 3D1F20 → 112DD8 RenderTaskGraphRun (node list, strictly sequential) → 113204 per node
 ├ 4A529C RenderFrameBegin (13–19 % incl)
 │   ├ 7D9B6C → 7D9E90 wait for Present (1–6 %), → 252510 PagePoolFrameUpdate → 2522C4 → 2523FC   2.2 / 2.1 / 3.0 self   ← PP
 │   ├ 96A2C0 glyph-atlas reset (candidate C, needs a live read)
 │   └ 5D214 meshlet/texture streaming bookkeeping (~10 %): 4BBEC→1E7050→128EF0/1290B8, 4AA34, 419A8
 ├ 535188 SceneRenderView (≈35 % incl; 25 call sites of 39E20, one per render bucket / stencil layer / shadow cascade)
 │   └ 39E20 DrawListWalk (self 1.8 %): per group: material by pass, FindParam filters, technique bind 3C77F4 (6.7 % incl)
 │        ├ 3BC0C group length (gallop B, +5 % in r9)
 │        ├ 3C768C MaterialFindParam → 3C774C/3C7750 per node (0.8–1.0 % self + loop)                          ← FPC
 │        └ 1DC2F0 → 1E80B0 DrawEmit (self 6.1 / 6.4 / 6.8 %) → 1E6664 params, 1D2F60 DrawSubmit ("Meshlet" FindParam per draw)
 ├ state commit family 1D5F20/1D6780/1D69CC/1D6E64 → 1D8720/1D84A0/1D8220/1D7EB0, 1DAF6C, 1E0B64/1E55D4/1E5854 (≈8–10 % self)
 ├ constants into the stream: B94230/B94250 memcpy from 242EB8/242FC4/1D7340 (≈5 %)
 └ UI 253C68 (6–7 %), 365AA98 chain (6–7 %), other render-graph nodes (18–26 %)
3D62E4 GfxDevicePresent → 418B94 → 418C70 final kick + 418F40 drain spin (0.7–2.1 %)
```

The profile is flat. In r9 the top main-thread self items are 1E80B0 6.8 %, B94230 3.2 %, 2523FC 3.0 %,
1CD690 2.2 %, B94250 2.1 %, 39E20 1.8 %, 1D84A0 1.6 %, then a long tail of ~1 % functions, all miss-bound.

## 2. What is data-parallel, and what is not

- **Already parallel and off the main thread.** Visibility, registration and sorting are already split:
  - the frame task graph's RegisterLight/RegisterScenePlaceable nodes run with `Param::Parallel` 32 units (`notes\game-data.md` §4.2);
  - 3D::ViewParam has `ParallelSortPrimitiveUnitNum 128` and `ParallelSortLightUnitNum 128`;
  - meshlet LOD runs on the GPU (`MeshletResolveLOD`).
  
  None of these shows up on [App]Game. So the main thread does **no culling** that a job could take over.
- **Data-parallel in shape, but not in effect.** Draw recording has the right shape: 25 independent-looking draw-list
  walks per view (buckets, stencil layers, `Cascade %d` shadow cascades), and inside each walk independent groups and
  draws. But every draw's effect lands in shared, order-dependent state (§4), so the shape does not make it parallel.
- **Pure, so parallel-safe:** FindParam lookups, group-boundary searches (3BC0C) and the texture-streaming stamps per
  texture (128EF0/1290B8, under their own critical section). Each of these is small (0.5–3 %).

## 3. What the job system offers (KIDS, `notes\job-system.md`)

| what | RVA | notes |
|---|---|---|
| JobPushToOwner(group, job) | 3FAD8 | lock-free per-priority stack on the owner worker; never wakes anyone |
| JobPushLocal(worker, job) | 417A0 | continuation push |
| JobForwardToWorker | 48DF8 | push to a specific worker and SetEvent it (the only push that wakes a worker) |
| JobGroupWait(group, mode) | 3E7CC | help-while-waiting join: pops and steals jobs, Sleep(0) when idle (404B9) |
| Job layout | +20h prio, +22h worker mask, +24h flags/pool, +28h next | JobGroup +8 owner worker, +34h pending+1 |

**The main thread is not a KIDS worker.** [App]Game has 0 samples in 403A0/3E7CC/40800/411C0 in every capture.
JobGroupWait needs `group->owner` to be a JobWorker, so the main thread would need a private join: push with 48DF8 to
chosen workers, then spin or block on the group counter. Nothing signals that counter (`notes\job-system.md` §2d), so the
join would be a spin on the critical thread. A live patch could do this, but it is new machinery.

## 4. Hazards: why recording cannot simply be split

1. **One recording context, one stream.**
   - `KtglContext == GfxDevice == [45BA588]`. Every DrawEmit writes tokens through `*(ctx+3C40h)`, and ctx is reached as
     `(*drawState)+70h`.
   - Only one such object exists, and no second recording context or deferred-context path exists. The only "Deferred
     context" string is a D3D11-style error message.
   - The stream is handed to the dispatcher ring (`GfxDevice+3BE0h`) every `CmdFlushLimit` draws (+3C70h) **in order**,
     and replayed into **one** ID3D12GraphicsCommandList.
2. **State-shadow filtering is order-dependent.** Each token is emitted only when the state differs from the shadow copy:
   ctx+2550h/2558h/25B8h/25BCh/26ABh/29A3h/2604h, the slot tables of the commit family, and the dirty bytes +13Bh/+13Ch.
   A stream recorded in parallel would start from an unknown shadow. Every segment would need a full state reset, and
   the shadow at each join would need a reconcile.
3. **Side effects outside the stream on every draw (1E80B0 / 1D2F60):**
   - per-frame stamps written into shared material objects (`mat+20h = g+6EB0h`, 1E80B0 line ~1378);
   - the refcounted meshlet pointer swap at ctx+1A80h (`lock inc` / `lock xadd`);
   - meshlet residency flag reads and writes (A3);
   - the draw counters ctx+3C74h/3C78h and the dispatch counter +2A70h;
   - the per-walk state in the draw-state object (`param_1+FEh`, `+10h`, `+18h`).
4. **Constants are copied from mutable CPU shadows.** 242EB8/242FC4/1D7340 copy constant blobs that are rewritten between
   draws (`notes\d3d12-submission.md` §4). Recording out of order would read the wrong values.
5. **Allocators.** 4DD4C/695328 hand out double-buffered upload space by frame parity, and stream space comes from
   +3C50h/+3C68h with a wrap path (1910CAF). None of them is thread-safe.
6. **Walk state lives on a 53 KB stack frame.** 535188's frame is 53 KB and 39E20 takes 52 parameters. A job would have to
   capture all of it.
7. **The next bottleneck.** SynergisticDispatcherTask is single-threaded and ~60 % busy, and it replays everything into
   one list. Halving the main thread would make the dispatcher and the 418F40 drain co-limiting. On the 7600X the GPU
   (7.2–7.6 ms) also caps any gain at about +20 %.

Making this parallel means writing per-worker KtglContexts, stream stitching, shadow reconciliation and thread-safe
allocators. That is an engine redesign, not a patch.

**(a') Runahead prefetch on a helper thread.**
- It is semantically harmless if every load is guarded: a helper fault would need a VEH to recover.
- But the in-line version of the same idea did not pay. Round 9 measured the meshlet caves (with P4AA/P1E7):
  - mesh-off 103.6 / 108.4 fps;
  - mesh-on 96.0 / 104.9 fps.
- A helper thread also burns a second core walking the same graph, which is the opposite of what the Ally needs.

## 5. (b) Work elimination: built

Both live in `patcher\drawprep_elim.fsx`.
- **One cave per game process.** Hooks are installed with their switches **OFF**; `on` / `off` flip one dword, and no
  code is touched.
- **Offline proof.** Original-byte checks for every dependency, plus native tests against the original bytes (FPC) or a
  model (PP).

### 5.1 PP: page-pool usage sweep every N frames (N = 4)

**What it is.**
- `PagePoolFrameUpdate 252510(pool = g+61A0h)` runs at every render-frame begin (7D9B6C).
- When `pool+D21h` is set, it copies the stats block, calls `PagePoolScanAll 2522C4`, then increments `pool+D10h`.
  pool+D10h **is g+6EB0h**, the global frame stamp that texture and meshlet streaming read.
- On the hot path (`pool+B28h != 0`), 2522C4 calls `PagePoolCountUsed 2523FC`. It walks every bucket × way × segment
  64-byte entry of the streaming page pool and counts entries with `lastUsedFrame >= D10h`.
- It writes `B2Ch` = used count and `B34h` = full buckets, and shifts the old values to `B30h` / `B38h`.
- The `B3Ch..` block is zero-filled on this path.

**Who reads the result.** The image was scanned for displacements 6CCC–6CD8h, and every reader was decompiled:
- `PagePoolOccupancyCheck 126C64` reads g+6CD0h (= B30h). If occupancy is above 85 %, it steps a mip bias, with a
  **0.3 s cooldown**.
- `PagePoolStatsReader 28B1B1C` reads g+6CD8h (= B38h). It is a streaming-settled state machine with a float threshold.
- Two trivial getters, 2EE8FE8/2EE8FF0. No code or vtable references to them were found.
- Nothing uses the value as an allocator truth. It is a lagging statistic: it is already one frame old when read.

**Patch.**
- Call site `252677: E8 48 FC FF FF` becomes `call cave`.
- The cave runs the original (tail `jmp [2522C4]`) when any of these holds:
  - the switch is off;
  - `pool+B28h == 0` (the unanalysed branch);
  - `(pool+D10h & (N−1)) == 0`.
- Otherwise it publishes the latest measurement as "previous" (`B30h = B2Ch`, `B38h = B34h`) and returns.
- The caller's `inc [rdi+D10h]` and everything after it are unchanged. rax and the flags are dead after the call
  (checked in the disassembly at 25267C..25268D).

**Effect on the heuristics.** The value readers see is at most N−1 frames old: 50 ms at 60 fps with N = 4, against a
300 ms cooldown.

**Test.** 20,000 random pool states × switch × N ∈ {2, 4, 8, 16} × B28h, run against a model: **0 mismatches**.

**Expected saving.** 75 % of 2.2–3.0 % = **1.6–2.3 % of the main thread**. On the Ally it also removes 3 of every 4
sweeps of the page-pool entry table (memory traffic).

**Risk: low.** In the worst case, mip-bias steps and the post-load "streaming settled" transition react up to N−1 frames
later.

### 5.2 FPC: per-frame cache for material FindParam

**What it is.**
- `MaterialFindParam 3C768C` is reached through **material vtbl+F8h**: vtable at 38FB8E0, slot 38FB9D8. That slot is the
  only pointer to 3C768C in the image.
- It hashes the name, then walks `mat+1330h` → `next +8`.
- Each node costs an indirect call through node vtbl+60h = 3C774C (`r9 = [node+10h]` record, then the hash compare and
  strcmp in 3C7750). That is 3 dependent loads plus a call per node.

**Who calls it.**
- per draw: 1D2F60 ("Meshlet"); 1E80B0 ("WritePositionBuffer", "SoftBody", "ControlPointMeshDeformation");
- per group per pass: 39E20 (IndexedRim, LightMap, SSSTransmit, Refraction*).

The same (material, name) pair therefore recurs in every pass, cascade and sub-pass that draws the material.

**Patch.** Re-point the vtable slot at the cave. This is an aligned 8-byte data write with the process frozen; no code
bytes change. The cave:
- **Cached** only when all of these hold:
  - the calling thread is [App]Game (`gs:[48h]` vs the TID recorded at apply);
  - the name pointer lies inside `.rdata`, so it is a string literal (no stack buffers);
  - `g != 0`.
- **Key:** material, name pointer, `mat+1338h` (first link of the list) and epoch.
- **Epoch:** the cave's own 64-bit counter. It is bumped whenever `g+3C60h` (stream generation) changes. That counter
  is incremented at every frame end (418D3D) and on stream wrap (1910CAF), so entries never outlive a frame. Using its
  own counter makes the cave immune to the engine counter ever repeating.
- **Table:** 2048 entries × 64 B, direct-mapped. A miss calls the original and fills the entry. Everything else jumps to
  the original.

**Why it is safe.**
- FindParam is a pure function of the list and the name.
- Within one rendered frame, a material that the main thread draws cannot be freed and reallocated. The GPU still
  references it, so KTGL must defer its release past the frame fences.
- The first-link key additionally catches a list rebuilt at the same address.

**Residual risk (documented and tested).** If a parameter is appended *inside* a frame, behind the first link, a cached
negative stays stale until the next frame. No code doing that was seen, and the stock code would race the same way if it
happened on another thread.

**Test.**
- The **original 3C768C and 3C774C bytes from the dump** are run natively against the cave.
- Setup: 48 materials, random lists of 1–9 nodes, corrupted hashes, empty names, heap (non-.rdata) names. Materials are
  rebuilt at the **same addresses** across frames, the epoch sometimes jumps by 2, and the switch is toggled.
- Result: 240,000 lookups, **0 mismatches**, 87k cache hits.
- The mid-frame-append limitation shows up as expected: stale in the same frame, correct in the next.
- Timing on a 31-node list with warm caches: original 44 ns, cave hit 8 ns. In the game the original misses on cold
  nodes, so the gap per hit is larger.

**Expected saving.** 3C7750 is 0.8–1.0 % self. Adding the loop and the node-vtable misses in 3C768C gives ~1.1–1.6 %.
At a 60–90 % hit rate that saves **~0.7–1.3 %** of the main thread. `status` prints the real hit rate.

**Risk: low–medium.** It rests on the per-frame lifetime argument above.

### 5.3 (b) candidates not built (ranked)

| id | what | main-thread share | why not now |
|---|---|---|---|
| C | Glyph atlas wiped and rebuilt every frame (96A2C0 at 4A5420) | ~2–3 % | needs one live read of `atlas+40h → +A8h` first (`notes\mainthread-drawprep.md` C) |
| E | UI alpha read-modify-write on write-combined memory (1CD690) | 1.1–2.2 % | confirm with `ripprobe.fsx` first; the fix is a medium-size cave |
| R | Meshlet ref swap at ctx+1A80h: skip `lock inc` / `lock xadd` when new == old | ≤ 0.3 % | small |
| F2 | Make FPC cross-frame (hit rate → ~100 %) | +0.2–0.4 % over FPC | needs proof that material frees are fenced; ABA risk across frames |
| D2 | State-commit family (1D8720/1D84A0, ≈5 % self) refreshes 5+5 slot tables on every program change | ≈5 % | dirty-tracked already; needs RE of which stages change. This is the largest remaining eliminable block, if any redundancy exists |

## 6. How to test PP / FPC (game running, in a real level)

```
cd C:\Users\Ahmad Hamza\nioh3-engine\patcher
dotnet fsi drawprep_elim.fsx offline                  # dump only: must print OFFLINE CHECKS: ALL PASSED
dotnet fsi drawprep_elim.fsx dry                      # read-only: sites original, deps match, epoch advances ~fps, [App]Game TID
$env:FT_ALLOW_WRITE=1
dotnet fsi drawprep_elim.fsx apply PP,FPC             # hooks in, switches OFF (stock behaviour)
dotnet fsi drawprep_elim.fsx on PP,FPC
dotnet fsi drawprep_elim.fsx status                   # PP: scans ≈ fps/4, skipped ≈ 3·fps/4; FPC: hit rate, bypass/s
dotnet fsi drawprep_elim.fsx off PP,FPC               # A/B without code writes
dotnet fsi drawprep_elim.fsx ppn 8                    # optional: 8-frame period
dotnet fsi drawprep_elim.fsx revert PP,FPC
```

Pass criteria:
- **cpuprof** at the r9 spot, ON against OFF:
  - 2523FC self 2.2–3.0 % → ≤ 0.8 %;
  - 3C7750 (leaf) + 3C768C self down by about the hit rate;
  - new `<anon>` cave samples ≤ 0.3 %.
- **PresentMon**: avg fps up, or at least not down, over ≥ 3 ABAB pairs. Run-to-run noise in r9 was ±8 %
  (cpu-base 94.3 vs 112.4), so the profile is the primary signal and fps the confirmation.
- **Visual (PP):** fast-travel, then time-to-sharp textures with PP on vs off; no mip shimmer in a texture-heavy area.
- **Visual (FPC):** none possible unless the stale-negative case occurs. Watch for a meshlet object drawn as non-meshlet
  (missing or flickering geometry) or rim/lightmap pass changes.
- **Ally:** package power at a fixed 60 fps cap, ON vs OFF, 3 pairs each.

**Entry for a future round script** (uses `patcher\common.ps1`; in-session A/B, gameplay-gated):
```powershell
# --- drawprep_elim A/B (PP + FPC), one game session, switches only ---
$de = "$ws\patcher\drawprep_elim.fsx"
dotnet fsi $de dry 2>$null | Select-String "PP site|deps|epoch|TID"
dotnet fsi $de apply PP,FPC 2>$null | Select-String "applied|fail|error"
foreach ($i in 1..3) {
    dotnet fsi $de off PP,FPC 2>$null | Out-Null; Bench-Checked "r11-DE-off$i"
    dotnet fsi $de on  PP,FPC 2>$null | Out-Null; Bench-Checked "r11-DE-on$i"
    dotnet fsi $de status 2>$null | Select-Object -Last 3
}
if (Ensure-InGame) { dotnet fsi cpuprof.fsx 20 "$ws\captures\cpuprof-r11-DE-on.txt" 1000 2 24 131072 2>&1 | Select-Object -Last 1 }
dotnet fsi $de off PP,FPC 2>$null | Out-Null
if (Ensure-InGame) { dotnet fsi cpuprof.fsx 20 "$ws\captures\cpuprof-r11-DE-off.txt" 1000 2 24 131072 2>&1 | Select-Object -Last 1 }
# separate legs if the pair is positive: PP alone, FPC alone
dotnet fsi $de on PP 2>$null | Out-Null;  Bench-Checked "r11-DE-PPonly"
dotnet fsi $de off PP 2>$null | Out-Null; dotnet fsi $de on FPC 2>$null | Out-Null; Bench-Checked "r11-DE-FPConly"
dotnet fsi $de revert PP,FPC 2>$null | Select-String "revert"
```
Compare `2523FC`, `3C7750(leaf?)`, `3C768C` and `<anon>` self % between the two cpuprof files.

## 7. Files
- `patcher\drawprep_elim.fsx`: PP and FPC caves, offline verifier (bytes, dependency instructions, single-pointer check
  for the slot, overlap list, native tests), and the live modes dry / apply / on / off / ppn / status / revert.
- `re\scripts_draw\drawprep_elim_offline.txt`: the last offline run (ALL PASSED), with the cave disassembly.
- `re\symbols\incoming\parallel-drawprep.tsv`: 24 rows. They include the corrections that `g+6EB0h` is PagePool+D10h,
  and that `g+6CD0h` / `g+6CD8h` are PagePool+B30h / +B38h.
- Inputs reused: `D:\nioh3-re\parallel\dec_535188.c` (535188 decompile), `re\scripts_draw\dec_39E20.txt`, and
  `D:\nioh3-re\decomp\0x252510.c`, `0x2522c4.c`, `0x2523fc.c`, `0x126c64.c`, `0x28b1b1c.c`, `0x3c768c.c`, `0x1e80b0.c`,
  `0x1dc2f0.c`.
