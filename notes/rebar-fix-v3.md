# ReBAR v3: the notice (shipped) and a targeted scene-buffer mirror for PCs that cannot enable ReBAR (2026-10-09)

Input: bridge #1091 (pc2 RX 6650 XT, Resizable BAR switched ON in the BIOS): DXGI non-local 919 -> 212 MB, GP2 span 32.5 ->
24.0 ms (-26 %) at spawn 7356, MIC / MshLod#1 / MshLod#2 out of the top 100, VCLOUD -80 %, MPC 0.37 -> 0.09 ms. #1096 (save
spot): GPUBusy 27.2 -> 19.1 ms (-29 %) but fps only +6 % there (pc2 turns CPU-bound). Earlier mod-side copies: CbvMirror v1/v2
(per-draw graphics CBVs, #988 / #1030: +10 to +36 ms), CbvMirrorSites = 2 (compute dispatch constants, #1080: meshlet rows
flat, VCLOUD -1.3 ms, span +0.5 ms). Tags: **M** measured, **S** static (capstone over `re\Nioh3.dump.exe` + the D:\nioh3-re
decompile cache; no Ghidra while the PC game ran), **E** estimate.

## 0. Verdict

1. **(A) shipped, ASI 1.7.56 (7d6f180):** `[General] ReBarNotice = 1` - one check ~40 s after the game first renders, a log
   line `RB:` and a one-time 25 s overlay toast when a desktop GPU's UPLOAD heaps sit in system memory. README / USER_GUIDE
   "Biggest free fps boost: Resizable BAR", notes/user-settings.md has the measured gain.
2. **The meshlet kernels read the per-frame SCENE BUFFERS, not constants (S + M).** MeshletFrameBegin 5D214 swaps
   `MeshletSys+16A0B0` between two buffer sets (`+169FB0` / `+16A030`) every frame and MeshletUpdateSceneBuffers 4BBEC maps
   every slot of the current set and writes this frame's data once (section 2). Slot `+58h` is the MRLOD root list that
   MshLod#1 reads as `tDispatcherMRLOD` (t20, StructuredBuffer of 8-byte payloads, one thread per root). pc2 #1070: MshLod#1 =
   53,166 threads x 8 B = **425 KB per call in 602 us = 0.71 GB/s** - a pure streaming read of system memory. Its only other
   input is cb0 (`nCullMode`), a compute constant that CMC (#1080) moved to VRAM with no effect. With ReBAR the same kernel is
   < ~10 us. That slot `+58h` is a usage-2 (UPLOAD, CPU-mapped) buffer is inferred from this; the SceneBufProbe census (queued)
   prints the usage, heap type and bytes of every slot.
3. **Why the earlier copies cost 10-15 ms (S + M):** volume, placement and synchronisation, not the idea of copying.
   - Volume: CM moved ~7-8 MB/frame (2,400 per-draw CBV views x 2.9 KB average, mostly padding) - an order of magnitude more
     than the scene buffers (~0.5-2 MB/frame, E, the census below measures it).
   - Placement: v2 recorded the copies as CP-DMA `CopyBufferRegion`s in a prologue list in front of EVERY game
     ExecuteCommandLists (dozens per frame), serialized ahead of the game's work on the same queue, plus a whole-mirror
     COPY_DEST -> constant-buffer transition each time; async COMPUTE queues re-copied the same ranges (+36 ms with them).
   - Synchronisation: v1 used its own COPY queue but made every game submission wait on its fence (+15 ms, #988).
   - Throughput: #1030 C copied ~7.5 MB/frame for a copy bucket of 1.36 -> 17.48 ms = ~0.45 GB/s on pc2. Whether that is pc2's
     PCIe read rate or the many-copies pattern is open; the RX 9070 harness (below) gets 24-26 GB/s from system memory for one
     4 MB copy per frame on either queue. The pc2 harness run (queued) settles it.
4. **(B) built behind default-OFF keys, ASI 1.7.57: SBM = scene-buffer mirror** (`[GpuOpt] SceneBufMirror`, scenemirror.cpp):
   one hook after MeshletUpdateSceneBuffers, ONE copy list per frame on its own COPY queue (DMA engine) with the current set's
   written bytes, ONE fence signal, every game DIRECT / COMPUTE queue waits once at its next submission, and the engine's own
   buffer SRVs of those buffers re-created on VRAM twins. Harness-proven on the RX 9070 (correct bytes per set across 43
   frames, only the scene views redirected, late-write detection + restore, no device removal). **Not run in the game yet.**
5. **Size of the prize (E):** the ReBAR-ON drop of the scene-buffer readers on pc2 is MIC 1.66-1.72 + MshLod#1 1.2-1.4 +
   MshLod#2 (main L0-2) 1.0-1.5 + MPC 0.3-0.6 = **~4-5 ms of the 8.5 ms ReBAR gain** (the rest: per-draw graphics CBVs ~4 ms,
   VCLOUD's compute constants ~2 ms - neither is in SBM). SBM's cost is the copy: ~1 MB/frame (E) on the DMA engine, async to
   the graphics queue. At >= 3 GB/s that is < 0.4 ms of DMA time that should overlap the ~5-10 ms of graph nodes before the
   first meshlet pass -> **net ~3-4 ms (~10-13 % on a GPU-bound no-ReBAR RDNA2 frame)**. If pc2's DMA also runs at ~0.5 GB/s
   the copy takes ~2 ms - still off the graphics queue, but it shares PCIe with the remaining system-memory readers, so the
   gain may shrink to ~1-2 ms or vanish. **Decision rule (pc2 A/B): ship SceneBufMirror = 1 (auto, no-ReBAR only) if MIC +
   MshLod#1 + MshLod#2 drop >= ~2 ms, GPUBusy drops >= ~3 %, the census shows no LATE write and shotdiff is within noise;
   otherwise leave it at 0 and say so here.** ReBAR stays the real fix either way (free, -26..-29 % GPU time).

## 1. What is in system memory without ReBAR, and who reads it (all sources)

| data | where | per frame | readers | mirrored by |
|---|---|---|---|---|
| per-draw graphics constants | 700 MB UPLOAD ring, CBV tables (118F89) | ~2,400 views, avg 2.9 KB (~7 MB) | every draw | CbvMirror (slower than it saves) |
| compute dispatch constants | same ring (495B2E / 117F20) | ~250 views < 1 KB | meshlet kernels (cb0), VCLOUD, fog | CbvMirrorSites = 2 (VCLOUD -1.3 ms, net +0.5) |
| **meshlet scene buffers** | UPLOAD buffer objects (usage 2) of the two sets | ~0.5-2 MB (E; census) | MIC, MshLod#1/#2, MPC, meshlet draws | **SBM (new)** |
| particle / UI vertex + index streams | ring | ~200 VB / ~44 IB binds | particles, UI | - |
| meshlet / texture staging | ring placements (4E2C0, 6305F5, ...) | MB while streaming | copies only (read once) | not needed |
| streaming feedback | 100 MB READBACK | ~30 KB | GPU writes, CPU reads | stays in system memory even with ReBAR |

## 2. The scene buffers (S)

- **Set swap** (MeshletFrameBegin 5D214, decompiled): `idx = ([sys+16A0B8] - 1) & 1; [sys+16A0B0] = sys + 169FB0 + idx * 80h`,
  then MeshletUpdateSceneBuffers(sys, ...) at the call 5D417 (rcx = MeshletSys; second caller 33DCD58 passes param_4 = 1
  without the swap). MeshletSys = renderer + 18D0h (RenderFrameBegin 4A529C).
- **Writes** (MeshletUpdateSceneBuffers 4BBEC, 6.6 KB): each slot is mapped (86F53C / 86F564 / 86F58C / 86F5B4 / 86F5DC ->
  `[obj+18h]->Map`, CPU pointer + `[obj+80h]`), filled with Memcpy from the scene arrays (`param_2+9F0h..A20h`, whole-array
  fast path, or per object), unmapped (45D828 / 953310 / ...). Slots: `+18h` instance data (`[obj+48h] * 3` float4 = 48 B per
  instance), `+28h` / `+40h` 4-byte per-object offsets (count `sys+16A24C`), `+30h` 16-byte part entries (`sys+16A250`, with
  rebased offsets), `+50h` 12-byte entries, **`+58h` the MRLOD root list, 8 B per root, padded to a multiple of 8 / 32 with
  `FFFFFFFF00000000`**, `+20h` / `+38h` prefix tables (`(n+1) * 4`), `+70h` a counter reset to 0, `+78h` read back (READBACK).
- **Buffer object** (map 86F5DC / 4DF64, unmap 4DE8C, create 4E828): `+18h` ID3D12Resource, `+68h` usage index into the
  12-byte table `3BEA260` {heap type, initial state, CPU-mappable}: **0/1 DEFAULT (map goes through an upload-ring staging copy:
  4D874 -> 4D8E0 -> 4E250, unmap 62EAC8 submits a copy list), 2 UPLOAD (Map on the resource itself, state GENERIC_READ AC3h),
  3 READBACK**; `+70h` capacity, `+80h` offset inside the resource, `+98h` map state (4 = unmapped), `+A0h` / `+A8h` written
  range of the last map.
- **Binding:** MeshletResolveLOD 531948 PSO #1 = MshLod#1: `52F5D4(ctx, [[sys+16A0B0]+58h])` puts the refcounted buffer object
  in the context's t20 slot (`ctx+1398h`), `942C88(ctx, [sys+16A1A0])` the u4 output; groups = roots/8 or /32 by wave size.
  The views are the buffer objects' own SRVs (created once at buffer creation through 4E828's SRV callback 915AEC: the VS
  census row `S 915B0B <- 4EC3B <- 4C8BFD`, 343 creations, max 27.8 MB, is that path), copied into the shader-visible tables
  at record time - so re-creating those SRVs on a twin redirects every later frame, and anything recorded before still reads
  the (identical) original.
- Compute root descriptors 0 and no root SRVs (VS binds census), so descriptor tables are the only path to check.

## 3. SBM design (scenemirror.cpp, ASI 1.7.57)

- Hook: the call at 5D417 (patches.json SBM1, check site; `test_patterns` ALL 244 SITES OK) retargeted to a relay (`mov rax,
  HkUpdate; jmp rax`) that calls 4BBEC with the 7 original arguments and then runs the mirror step. hook_liveness: no volatile
  register is live after 5D41C (only `arg?` for the next vtable call, which 4BBEC clobbers anyway).
- After the CPU wrote the current set: for every slot with usage 2: a DEFAULT twin of its resource (once, same width, cap 256 MB
  total; the resource is AddRef'd while a twin exists), copy `[+80h, +80h + +A8h)` (whole capacity if the last map was
  partial). All copies of the frame in ONE list on our COPY queue (`SceneBufMirrorQueue = 1`) or on the game's DIRECT queue
  (`= 0`, CP DMA, no Wait on that queue - a Wait queued before our own Signal on the same queue would deadlock), ONE Signal.
- Every game DIRECT / COMPUTE submission (osd queue hook) waits on the fence once per new value. The mirror stays inactive
  until that hook has seen a game DIRECT submission.
- Views: memstats' CreateShaderResourceView hook records buffer SRVs on UPLOAD-like resources (UPLOAD, CPU-visible CUSTOM L0 =
  VSForceSysUpload, placed in a system-memory heap) with their byte range. In the frame a set is copied, its SRVs whose range
  lies inside that set's buffer are re-created on the twin through the unhooked call (with the device object the hook saw: it
  can be a proxy). New SRVs inside an already-copied range are redirected at creation. Other SRVs on the same resource stay.
- Synchronisation is inherited from the engine: it rewrites a set only after the GPU finished the frame that read it (otherwise
  its own UPLOAD writes would race), so the twin region of that set is free at the same moment.
- Probe (`[Profiler] SceneBufProbe = 1`): `SBM set` lines (per slot: usage, resource, offset, capacity, bytes written, heap
  type, views inside / redirected) and, while the mirror runs, a late-write check every ~900 frames (hash of the copied ranges
  right after the copy and again at the next update of the other set): a mismatch logs `LATE CPU WRITE` and switches SBM off
  for the session, putting every redirected SRV back on its original resource.
- `stats/s SBM` every 30 s: copy lists / bytes per frame, GPU copy time and GB/s (timestamps on the copy list), queue waits,
  twins, views redirected, retired twins (released once only we hold the resource).
- Auto (`= 1`) follows rebar.cpp's verdict (discrete GPU, UPLOAD heaps in system memory); refused under Wine.

## 4. Harness (tests/run_sbm_harness.py, RX 9070 on this PC, no game) - all OK

| run | result |
|---|---|
| COPY queue, 43 frames, two sets in one 1 MB UPLOAD resource + a non-scene view | only the scene views redirected, each in the frame of its set; the game list reads this frame's bytes from the twin; the other set's range keeps its bytes; a later view inside a copied range redirected at creation, outside not |
| DIRECT queue (CP DMA), same | same |
| late write (probe on) | `LATE CPU WRITE` caught, view put back on the original |
| bw: 16 MB CUSTOM write-back L0 (system memory), 4 MB/frame, 60 frames | COPY queue 0.163 ms/frame = **25.7 GB/s**; DIRECT queue 0.175 ms = 24.0 GB/s |

## 5. Tests queued

1. **pc2, no game (bridge file `sbm_harness.exe`):** `sbm_harness.exe 1 bw`, `sbm_harness.exe 0 bw`, `sbm_harness.exe 1`,
   `sbm_harness.exe 0` - system-memory copy throughput of the RX 6650 XT on the DMA engine vs CP DMA. If both are ~0.5 GB/s
   like CM's copies, the link/driver is the limit and SBM's prize shrinks (section 0.5); if multiple GB/s, CM's cost was its
   pattern.
2. **pc2 in game (ASI 1.7.57 file):** spawn 7356 (or the GPU-bound Continue save spot), `Profiler.MemStats=2;
   Profiler.VSForceSysUpload=2` (the no-ReBAR layout on the now-ReBAR pc2), GP2 MinRun 1, PresentMon, per launch: A =
   `Profiler.SceneBufProbe=1` | B = A + `GpuOpt.SceneBufMirror=2` | C = B + `GpuOpt.SceneBufMirrorQueue=0`; order A B A B C.
   Post: run01.logs (`SBM` lines, `RB:` line), GP2 rows MIC e31c389f, MshLod#1 e6a62bda, MshLod#2 63c07c21, MPC 18b6fee7 /
   1b4b5b75, VCLOUD b76874bb, span, GPUBusy, shotdiff B vs A. Decision rule in section 0.5.
3. PC (when free): the same A/B with `VSForceSysUpload=2` (cheap mechanism check: MIC should return from ~17 to ~8 us,
   MshLod#1 from ~35 to ~7 us per dispatch if the scene buffers are the whole story).

## 6. Files

| file | what |
|---|---|
| mod/Nioh3EngineFix/src/rebar.cpp / .h | RB check + notice (1.7.56) |
| mod/Nioh3EngineFix/src/osd.cpp / .h | `ShowNotice` / `NoticeDone` toast; queue hook calls `scenemirror::OnEcl` |
| mod/Nioh3EngineFix/src/scenemirror.cpp / .h | SBM (1.7.57) |
| mod/Nioh3EngineFix/src/memstats.cpp | SRV hook -> `scenemirror::OnSrv`; device hooks also when SBM is wanted |
| mod/Nioh3EngineFix/patches.json | SBM1 check site 5D417 |
| mod/Nioh3EngineFix/tests/sbm_harness.cpp, run_sbm_harness.py | harness (section 4) |
