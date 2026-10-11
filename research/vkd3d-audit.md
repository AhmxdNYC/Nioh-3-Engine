# Nioh 3 under vkd3d-proton: which D3D12 patterns cost more on the Ally, and game-side fixes

Status 2026-10-07. Static work only: vkd3d-proton `<refs>\vkd3d-proton` (commit 2230755, `libs/vkd3d/command.c`,
`resource.c`), DirectX-Specs, the dump/decomp, and the existing Ally logs (`captures\ally\*.log`, GP2 digests) plus the
PC RGP r5 barrier digest (`<re-scratch>\d3d12\bcost\summary_r5.txt`). The game was not run and nothing in `src` changed.
RADV statements are from vkd3d's code paths plus knowledge of RADV (RADV source is not on this PC): marked *(RADV)*.

## TL;DR

The CPU side of the translation is not the Ally's problem: the dispatcher thread is 81 % parked (69 % in the ECL-ring
backpressure wait 4930C8 -> EclRingPush), and vkd3d (`d3d12core`) + winevulkan are 9.3 % of its samples. All
fps-relevant costs are GPU waits that vkd3d emits for the engine's barrier pattern.

| rank | id | pattern (engine) | what vkd3d does (command.c) | per frame | Ally GPU (model) | fix | status |
|---|---|---|---|---|---|---|---|
| 1 | **R2** | Per-list state reset: `SynTransitionResource` 115E24 sets the tracked state to COMMON when the resource's stamp != `SynCtx+0x6D0` (fence value), so the first use per list is a `COMMON -> X` barrier | `vk_access_and_stage_flags_from_d3d12_resource_state`: COMMON = `ALL_COMMANDS` + `MEMORY_READ/WRITE` -> *(RADV)* full wait-for-idle (CS+VS+PS partial flush) + CB/DB flush, mid-list | unknown (which of the 45 callers pass a stamp is not decoded; EI argument buffers do); PC proxy: 485 EOP waits of 1360 barriers | up to ~0.2-2 ms if many are textures with false dependencies; buffers are excluded | rewrite/drop when the real prior state is known (design 2) | **measure first**; see Addendum B: stamped = buffers only, COMMON is their true state; R2a (buffers without ALLOW_UNORDERED_ACCESS) is exact, `P -> X` is unsafe |
| 2 | **R1** | Texture bind 115B9C ORs read states: `new = (cur & 0xAC3) | PSR/NPSR` (115D94..115DA9), e.g. `NPSR -> NPSR|PSR` | read->read transitions are not elided (`barrier_batch_add_layout_transition`; no layout change, both use `common_layout`): one `vkCmdPipelineBarrier2` *(RADV: partial flush of the src stage + VCACHE invalidate)* and `ResourceBarrier()` ends the render pass | unknown | 0.05-0.4 ms if 50-150/frame | drop strict-superset read->read transitions (design 1) | design; same result on RADV, not spec-exact |
| 3 | RP | Every barrier call, OMSetRenderTargets (always ends, no compare), Dispatch, Copy*, partial clear, ClearUAV ends the dynamic render pass | `d3d12_command_list_end_current_render_pass` at the top of `ResourceBarrier`, `OMSetRenderTargets`, ... | ~1270 barrier calls (Ally, gpuopt off) -> ~520 with gpuopt on | small *(RADV: no tiler; with unified layouts End/BeginRendering emits no layout barrier)*: <= 0.1-0.3 ms | gpuopt `Barriers = 1` already removes ~60 % of the splits | exists (ASI 1.7.x, default off; it measured slower, not faster, in fps on the PC, docs/BENCHMARKS.md) |
| 4 | E1 | 21-28 GFX lists per frame, one `ExecuteCommandLists(1)` each (ECL thread tok 6, 4F9214) | one `vkQueueSubmit2` per ECL; on RADV no barrier command buffer but a serializing binary semaphore between submissions (`vkd3d_driver_implicitly_syncs_host_readback`, `serializing_semaphore`) -> *(RADV/amdgpu)* same-ring pipeline sync + cache flush per submission | 21-28 | PC: 1.07 ms idle at CB boundaries (~38 us each, mostly gated stalls, gpu-scheduling B2); Ally unknown | merge consecutive ECLs (design 3) | design; D3D12 ECL-boundary semantics make it risky |
| - | - | ExecuteIndirect (tok 6E/6F/81, MaxCount 1) | signatures are plain: created at 2EFF231 (`CreateCommandSignature`, NULL root sig): `SynCtx+0x6E8` DRAW stride 0x10, `+0x6F0` DRAW_INDEXED stride 0x14 -> no state template, no DGC, direct `vkCmdDraw*Indirect` | - | none | - | not a cost |
| - | - | Queries | no BeginQuery/EndQuery/ResolveQueryData in the dispatcher handler table (`<re-scratch>\d3d12\vcalls.tsv`); only the frame-end ResolveQueryData on the main thread. The ~1400 GP2 timestamps were the drain (ASI 1.7.8) | 1 | none | - | not a cost |
| - | - | CBV per draw (118E28: upload-ring memcpy + CreateConstantBufferView), CopyDescriptorsSimple (115DE8), root tables, PSO set | CPU only (descriptor-buffer writes); root CBVs would need root-signature/shader changes | dispatcher 9.3 % vkd3d+winevulkan | 0 fps (dispatcher blocked); watts only | none | not worth it |
| - | - | Clears / copies (see below) | full ClearRTV -> deferred `loadOp CLEAR`; ClearUAV -> `vkCmdFillBuffer`/`vkCmdClearColorImage` when trivial, else meta compute + implicit barrier (`clear_uav_pending`); copies batched until the next non-copy command | 34-35 copy/clear calls | inside the GP2 bucket | none found | no redundancy provable |

**Bottom line:** the only big lever is barrier *quality*, not count: COMMON-sourced transitions (full drains) and
read->read transitions that native AMD drivers can skip but vkd3d+RADV cannot. Both need the trace run below before
any change; nothing was implemented because neither drop is provably result-identical by the spec (reasons in each design).

## Numbers used

- Ally in-game (v71 MAX, 175-L1/L2 logs): 1240-1470 ResourceBarrier calls/frame (~9 multi-struct); gpuopt on:
  1274 -> 520 calls (-59 %), 43 duplicate UAV barriers dropped and 18.6 undo pairs per frame.
- Ally GP2 in-game windows (PSO stamps on, so spans are inflated by the stamps): 2187-2877 draws+dispatches/frame,
  async compute 5-8 ms, covered 91-97 % of the span; copy/resolve/clear 34.5 intervals, 1.56 ms/frame (2.3 %).
- Dispatcher SP (v71, 146 windows, 19 572 samples): ntdll 81.3 %, Nioh3 7.6 %, winevulkan 5.8 %, d3d12core 3.5 %.
  vkd3d+winevulkan by game caller: 1180D0 1.82 %, 1167D4 1.04, 115DE8 0.78, 6BE12C 0.49, 58BE90 0.47, 1188AC 0.33,
  118E28 0.29, 115E24 0.24.
- PC RGP r5 (proxy for the barrier mix): 1360 barriers in 478 chains, EOP waits 485, CS partial 619, PS partial 568,
  CB/DB flush 255, no wait 48; GPU fully idle inside barriers 0.56 ms, at CB boundaries 1.07 ms.

## Copy / resolve / clear bucket

Engine sources (vcalls.tsv): CopyTextureRegion tok 03 45A020, tok 7A 45C880/45A190 (generic copy), tok 7B 45DDC8 (depth
copy), tok 91 9E2BA4; CopyBufferRegion tok 68 2F0075C, tok 8C 2F0D858; ClearRTV tok 02/04 4591DC, tok 05 2EFBDA4;
ClearUAVUint tok 94 7B18E4; plus FSR3 (3/frame incl. `FSR3:SaveOpaqueColor`, consumed by GENERATE_REACTIVE) and the
main thread. No ResolveSubresource. vkd3d handles all of these well; depth->colour CopyTextureRegion uses a meta render
pass only without format compatibility (`d3d12_command_list_copy_image`, `use_copy`). A GP2 copy interval also contains
the barrier drain in front of it (vkd3d's `begin_transfer` WAR barrier, the engine's COPY_DEST transitions), so the
1.56 ms overstates copy work. Redundancy cannot be shown from GP2; `vkd3d_cost.py` reports the method mix per frame.

## Measurement (one Ally run, no behaviour change)

`[GpuOpt] Barriers = 2, TraceRecords = 300000, TraceDelaySec = 60` in a combat area, then
`py -I re\scripts_d3d12\vkd3d_cost.py Nioh3EngineFix.gpuopt-trace.csv` (selftest: `--selftest`). It replays the trace
through the vkd3d model above and prints per frame: barrier structs by class (COMMON->X split by the prior state seen in
the trace, R1 supersets, read->read other, write->X, UAV), render-pass ends by cause and begins, OMSetRenderTargets
re-sets with identical args, implicit ClearUAV barriers, copy/clear method mix, R1/COMMON state pairs.
Limitation: gpuopt records the *first* Nioh3 return address, which is 115E9D for every SynTransitionResource barrier.
Integration note for gpuopt: also record the second Nioh3 frame (`CallerRva` loop, keep the next match) so
R1/R2 counts split by the 45 callers.

## Designs (all default off, ASI, same hook as gpuopt: ResourceBarrier vtable slot 26)

**Design 1 - R1 superset drop.** In the ResourceBarrier hook, for calls from Nioh3.exe on the dispatcher thread, under
Wine only: drop a TRANSITION struct with `Flags == 0`, `before != 0`, `(before & after) == before`, `before != after`,
`after & ~0xC3 == 0` (VCB, IB, NPSR, PSR only: INDIRECT_ARGUMENT changes vkd3d's DGC hoisting, COPY_SOURCE its copy
tracking, DEPTH_READ its depth layouts). Why the result stays the same: the engine tracker stores `after`, so every
later engine barrier on that resource names a StateBefore that includes the dropped `after` (or COMMON, which covers
everything): all reads stay covered before the next write. Data hazards between the two read stages cannot exist. Left
over: Vulkan wants a visibility operation per destination stage, which the dropped barrier provided. *(RADV)* cache
invalidation is global and waits are CP-wide, so the earlier `write -> before` barrier already covers the new stage:
identical on RADV, not guaranteed by the spec, so it must stay a gated experiment with an image compare. Engine-side
alternative (no vtable): a cave at 115D94 (`cur` = r9d, tracker pointer r8 = `[r14+10h]`) that, when `cur` is non-zero
and inside 0xC3, stores the OR'd state into `[r8]` and skips the call at 115DA9. 115B9C passes stamp 0 (115D9B), so bind
trackers are never reset per list and these supersets persist across lists. Saves one vkCmdPipelineBarrier2 and one
render-pass restart per hit.

**Design 2 - R2 COMMON narrowing.** Source: the SynTransitionResource callers that pass a stamp (5th arg), e.g. the
ExecuteIndirect argument buffer (6EB908, stamp from 524C04); texture binds (115B9C) pass none.
Track per (resource, subresource) the last explicit StateAfter in execution
order (dispatcher lists are recorded and submitted in order on the GFX queue). For a `COMMON -> X` struct whose
resource is a non-simultaneous-access texture (`GetDesc` cached), whose prior state P is known, with no Copy/Clear/
Discard on it since: replace COMMON by P (or drop if P and X are read-only). Why it is not safe yet: 115B9C skips the
transition when the same SRV is still bound, so a resource can be used in a list before its first tracked barrier there
(binding carried over); a write through such a carried-over binding would make the COMMON barrier a real in-list
dependency. Needed proof: that bindings are reset at list start (`ClearState(NULL)` before Close, 490D5C, resets the
D3D12 bindings, but the engine's staging slots at `SynCtx+0x1118` must be shown to be invalidated too), and where
`SynCtx+0x6D0` advances (expected only at list close). Buffers are excluded (D3D12 decay is real for them; vkd3d
uses global barriers for buffers anyway). **Addendum B settles this:** the bind caches are not reset per list, so `P -> X` is not safe; the stamp path is buffers only; use R2a (B4).

**Design 3 - ECL merge.** Hook `ID3D12CommandQueue::ExecuteCommandLists` on the GFX queue (the ECL thread replays tok 6
ECL / 7 Signal / 8 Wait, table 3BD4DF0): hold a list while the engine's ECL ring (EclRingPush 86F760) already has the
next item and submit `ECL(n)` up to the next Wait. Signals keep their values but fire later. Blocked by semantics:
D3D12 decay/flush happens per ECL; inside one vkd3d `ECL(n)` the lists share one `vkQueueSubmit2` with no semaphore
between them, so engine code that relies on the per-ECL flush (promoted buffer writes, the per-list COMMON reset) would
lose it. Do not build before design 2's binding proof; payoff unknown on the Ally.

## Files

- `re\scripts_d3d12\vkd3d_cost.py` (new): trace -> vkd3d/RADV cost model, `--selftest` OK.
- `re\symbols\incoming\vkd3d-audit.tsv`: 115B9C rename (SynBindTextureSlot), the R1 state computation, the command
  signature creation, SynCtx +0x6E8/+0x6F0.

## Addendum B (2026-10-07): all SynTransitionResource callers, the COMMON mechanics, patch-ready R1/R2

Static only (dump v2.02, `re\scripts_d3d12\barrier_callers.fsx` new). The Ally trace (ASI 1.7.9, `caller2_rva`) supplies the
counts. `re\scripts_d3d12\barrier_sites.tsv` (new, built by `barrier_sites.py`) lists all **74 call sites in 45 functions**
with the return RVA the trace stores as `caller2_rva` (= site + 5), token, role, resource class and stamp source, for the join.

### B1. Mechanics (from the dump)

- `SynTransitionResource(c, res, u32* state, X, u64* stamp, sub)`: if `stamp && *stamp != c+0x6D0` then `*state = 0;
  *stamp = c+0x6D0` (115E55..115E67; the COMMON store is **115E61**). Then `if (*state != X) ResourceBarrier(1, {*state -> X})`
  and `*state = X`. Flags are always 0.
- **`SynCtx+0x6D0` is a per-command-list id, not a frame fence.** The ring's on-close callback 490E50 (called by
  CmdListRingClose at 490E0F with the new fence) stores `fence + 1`. Init (2EFF7C1) stores ring fence + 1. It advances once per
  list (21-28 per frame), so the first stamped use of a resource in every list is `COMMON -> X`.
- List begin: the ring's on-reset callback 492220 calls `list->Reset` and then **SynBeginList 49349C** (list, heaps, root
  signatures, then **494E00**). 494E00 re-applies the 32 bound VB slots (`c+0x260`) and the IB (`c+0x30`) **with their own
  stamped transitions**: inline resets at 494FE8 and 495001, and `ResourceBarrier` calls at **495036 / 495074**. Those two
  emitter sites are missing from gpu-barriers.md §3. It also marks the tables dirty (`c+0x1D20 = 3`, `+0x10F0`/`+0x1E38 = 0xFF`).
- There are six places that store COMMON: 115E61 (115E24), 11469A (114614 IB), 114ABE (114A20 tok 3D IB), 7F5333 (7F52AC tok 9B),
  and 494FE8 (VB) and 495001 (IB) at list begin. 1180D0 (RT/DSV) and 7F9BF8 (copy) have no stamp.
- Stamp pointers come either from the resource object (114C04 walks view -> parent and returns `obj+0x28`, which may be NULL)
  or from the token stream. Every bind token has a stamped and an unstamped twin that share one bind cache (4E/4F-53 vs 56-5B
  on `c+0x1118`, 54 vs 5C on `+0x1718`, 55 vs 5D on `+0x1B98`). Every stamped site whose resource kind is visible handles a
  **buffer** (VB, IB, EI arguments, predication, query resolve, CopyBufferRegion, SO). Every texture path passes NULL (115B9C,
  1180D0, clears, CopyTextureRegion, resolve). So the stamp is the engine's emulation of buffer decay. This is inferred and
  holds at every site, but it is not proven from the resource-creation code.
- This matches D3D12: "at the start of any given ExecuteCommandLists scope, all buffer resources and simultaneous-access
  texture subresources are implicitly in COMMON", and "Buffers ... can be promoted to ANY state" (DirectX-Specs
  `d3d\D3D12EnhancedBarriers.md` §Resource State Promotion and Decay and the legacy-interop paragraph; the legacy rules are in
  `d3d\CPUEfficiency.md` §Common state promotion / §State decay to common). Here 1 list = 1 ECL, so **COMMON is the true D3D12
  state** at these barriers. The engine is right. The cost comes from how vkd3d maps COMMON.
- **Bind caches persist across lists.** `c+0x1118/0x1718/0x1B98` (SRV) and `c+0x1D30/0x1DB0` (UAV) are written only by the
  bind handlers (and 1155B0). List begin (49349C, 49366C, 494E00) touches none of them, and nothing else writes those
  displacements. Re-binding the same descriptor in a later list skips both the descriptor copy and the transition. A buffer
  can therefore be read or written in list N+1 through a carried-over descriptor before any tracked barrier, or with none at
  all. That is legal D3D12 (decay plus promotion). Consequence: the tracker's last state P is **not** the buffer's full history.
  The untracked accesses are shader-stage only (descriptor tables): SRV reads or UAV writes.

### B2. The 45 callers (X = StateAfter; S = stamped, can start from COMMON; N = never stamped)

| function | token | role | res | X | S/N | freq/frame (static estimate) |
|---|---|---|---|---|---|---|
| 113A60 | 5F | UAV bind (cache +1DB0) | buf/tex (stream) | UAV | S (stream) | per buffer per list |
| 1140C4 | 5E | UAV bind (cache +1D30) | buf/tex | UAV | S (arg) | per buffer per list |
| 11422C | - | UAV bind helper (113A60, 1140C4, 7F7C84) | buf/tex | UAV | S (arg) | idem |
| 113BA4 | 4E | buffer SRV bind (cache +1118) | buf | (cur&A83)\|NPSR | S (stream) | per buffer per list |
| 114E24 | 4F-53 | buffer SRV bind per stage | buf | (cur&AC3)\|PSR/NPSR | S (arg) | per buffer per list |
| 114F68, 115664 | indirect | buffer SRV bind variants | buf | popped / arg | S (arg) | idem |
| 115084 | 54 | buffer SRV bind (cache +1718) | buf | computed | S (stream) | idem |
| 815DB0 | 55 | buffer SRV bind (cache +1B98) | buf | computed | S (stream) | idem |
| 116260 | 3C | **VB bind slot** + IASetVertexBuffers (SynTokSetVertexBufferTracked) | buf | (cur&AC3)\|vb+48 | S (obj+28) | per VB per list |
| 1163AC | 71 / 45DDC8 | VB bind variant | buf | idem | S | idem |
| 2EFD430 | 79 | draw with VB + IB | buf | (cur&AC3)\|+50 | S | idem |
| 58BC74, 58BD2C | 81, indirect | ExecuteIndirect dispatch args (after SynFlushComputeState) | buf | obj+30 (IA) | S (114C04) | per arg buffer per list |
| 6EB908, 6EB9CC, 6ECF48, 6ED00C, 2EFD138 | 6F, -, 6E, (89), 80 | ExecuteIndirect draw args | buf | obj+30 (IA) | S (114C04) | per arg buffer per list; PC r5: 31 chains end in EI |
| 2EFD8DC | 89 | GPU-driven batch: buffer SRV + arg buffer, UAV barrier, EI 6ED00C | buf | (cur&A83)\|NPSR, obj+48 | S | per batch |
| 2F00468 | B8 | predication buffer, SetPredication | buf | PREDICATION (200) | S (inline +18) | few |
| 2F0ABC4 | via 2EFDC24 | query resolve target, ResolveQueryData | buf | COPY_DEST | S (may be NULL) | few |
| 2F0075C | 68 | CopyBufferRegion src/dst, then STREAM_OUT + SOSetTargets | buf | CS, CD, SO | S | few |
| 7A5FB8 | 99 / 8B | buffer copy dst/src | buf | CD, CS | S | few |
| 7F6574, 7F67EC | 95 | COPY_DEST, CopyBufferRegion, restore saved state | buf | CD, saved | S | few |
| 7B18E4 | 94 | ClearUAVUint | buf/tex | UAV | S (arg) | few |
| 3D62E4 | main thread | back buffer -> PRESENT | tex | 0 | N | 1 |
| 115B9C | 3E-46, 48-4D, 86 | texture SRV bind (R1 source) | tex | (cur&AC3)\|PSR/NPSR | N | per state change of bound textures |
| 11518C, 1152A4, 115398 | 57-5B, -, 56 | texture SRV binds | tex | obj+24 | N | idem |
| 8507B0, 823534 | 5C, 5D | texture SRV binds (caches +1718, +1B98) | tex | obj+24 | N | idem |
| 698A44, 698C54 | 87, - | texture bind (cur&AC3)\|bit | tex | read set | N | idem |
| 4591DC, 2EFBDA4 | 04 (02), 05 | ClearRTV / ClearDSV | tex | RT, DEPTH_WRITE | N | ~30 |
| 45A020, 45A190, 9E2BA4 | 03, 01/7A/7B, 91 | CopyTextureRegion + restore (depth bits stripped) | tex | CS, CD, prior | N | ~10-35 |
| 45C880, 45DDC8 | 7A, 7B | generic / depth copy (stack trackers) | tex | CD, CS, RT, DEPTH_READ | N | idem |
| 2EFFF54 | 93 | ResolveSubresourceRegion + restore | tex | RESOLVE_DEST/SOURCE | N | rare |
| 2EFE094 | via 2EED2E0 | X -> COMMON helper | ? | 0 | N | rare |

Other emitters: 1180D0 (RT 4, DSV 10/20\|..; N), 114614 / 114A20 (IB; S), 7F52AC (tok 9B; S from the stream), 7F9BF8 (N),
494E00 (VB/IB at list begin; S), and the UAV barriers 1139D4/113A0C. "indirect" means no static reference: these twins are
reached through tables that are not in the dump image.

**COMMON-origin count per frame (formula).** The list-begin rebinds give about (bound VB slots + IB) × lists ≈ 2-4 × 21-28 =
40-110. These sit at the start of a command buffer, so on RADV they drain the previous list's tail rather than in-list work.
On top of that, the bind/EI/copy paths add one per distinct stamped buffer per list. That number is unknown; the trace counts
it per `caller2_rva`. Texture paths start from COMMON only when the tracker really is 0 (back buffer after PRESENT, 2EFE094,
creation), so there is nothing to replace there.

**Is the true previous state knowable at the site?** For S callers the engine still holds P, the last tracked StateAfter.
114E24, 113BA4, 116260, 494E00 and 114614 even read it to compute X. But P is not the full history (B1, carried-over
descriptors), and the real D3D12 state is COMMON. So replacing the before-state with P (`P -> X`) is **not** identical in
general. Example: list N writes EI arguments as UAV and then issues `UAV -> IA`, so P = IA. List N+1 re-runs the culling
dispatch through the cached UAV slot (no bind, no barrier), then the EI. The engine's `COMMON -> IA` (ALL_COMMANDS) orders
that write. `IA -> IA` would be skipped, and the EI would read stale arguments. Under vkd3d on RADV this barrier is also the
only thing that orders work across ECLs: the serializing semaphore is used only after fallback-queue submissions (command.c
25608), and Vulkan's first sync scope includes earlier submissions on the queue.

### B3. What vkd3d does with the before-state of a buffer

In `d3d12_command_list_ResourceBarrier` (command.c 14104-14140), a buffer's before/after states only set the src/dst
stage+access of one global `VkMemoryBarrier2`, ORed into the batch (`barrier_batch_add_global_transition`, 13596). No state is
tracked or validated beyond `is_valid_resource_state` (utils.c 1122-1156: known bits only; a write state cannot be combined
with anything, read states can). The mappings:
- COMMON = ALL_COMMANDS + MEMORY_READ|WRITE (6249).
- UAV = all queue shader stages + CLEAR, SHADER_READ|WRITE + TRANSFER_WRITE (6285).
- NPSR/PSR = shader stages + SHADER_READ.
- IA = DRAW_INDIRECT + COMPUTE (6349).

*(RADV)* ALL_COMMANDS + MEMORY_WRITE means CS/VS/PS partial flush plus CB/DB flush and invalidate (an end-of-pipe wait). A
read-only src scope costs only the partial flushes of those stages and a cache invalidation. RADV might also flush CB/DB for a
global TRANSFER_WRITE src, since meta copies and clears can go through CB; that cannot be checked here because the RADV source
is not on this PC. If it does, any replacement that contains UAV saves nothing on the Ally.

A replacement is identical for buffer B when its src scope covers every access B can have had since its last barrier: scope(P),
plus shader reads (carried-over SRVs), plus shader writes if B can be UAV-bound at all. A buffer created without
`D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS` can never be UAV-bound or ClearUAV'd. Its copies and resolves are tracked (they
change P), and buffers are never attachments. So for such a buffer COMMON can be replaced exactly. For the read-only case: the
earlier `W -> P` barrier already made W available. The replacement `P|NPSR|PSR -> X` (read-only src) still performs the
visibility operation for X's stages, so it is not a drop.

### B4. R2 patch design (gpuopt ResourceBarrier hook; default off)

Switch: `[GpuOpt] R2 = 0|1|2` (1 = R2a, 2 = R2a + R2b). Wine only, dispatcher thread only, live switch. Counters
`r2a/r2b/kept_texture/kept_uav/kept_unknown/kept_foreign` go in the 30 s line. A per-resource cache maps ptr -> {isBuffer,
allowUav, last, lastTid}. `GetDesc` is called again for every candidate (in vkd3d it is a memcpy), so a recycled pointer never
uses a stale type.

```
for each struct s (copied; at most 2n out):
  if s.Type == TRANSITION && s.Flags == 0 && s.Subresource == ALL:
    R = cache(s.pResource); X = s.StateAfter
    if s.StateBefore == 0 && X != 0 && R.isBuffer && R.last != 0 && R.lastTid == tid:
      P = R.last; READ = 0x2AE3                         // VCB|IB|DEPTH_READ|NPSR|PSR|IA|COPY_SOURCE|RESOLVE_SOURCE
      if !R.allowUav:                                   // R2a: identical for a valid D3D12 app
        if (P & ~READ) == 0: s.StateBefore = P | 0xC0                      // one read-only struct
        else:                s.StateBefore = P; append {B, 0xC0 -> X}      // write P (COPY_DEST, SO, ...): two structs
      else if R2 >= 2:                                  // R2b: experiment; may save nothing on RADV (TRANSFER_WRITE)
        if P == X == UAV: s = UAV barrier on B
        else: s.StateBefore = UAV; if (P & ~0xC0) && P != UAV: append {B, P -> X}
    R.last = X; R.lastTid = tid
```

Notes:
- The appended struct must stay in the same emitted batch. Mark the pair so gpuopt's same-resource conflict split does not
  separate it.
- A stale `last` for a newly created resource is harmless: it has no prior GPU access, and the engine uses no aliasing barriers.
- Resources last seen on another thread (async compute, main thread) keep COMMON.
- R2b's `UAV ->` sets vkd3d's `clear_uav_pending = false` (14107). That only makes a difference for an app that leaves out the
  barrier after ClearUAV.

Where it applies (the rule is class-based and ignores the caller):
- R2a hits the stamped VB/IB paths (494E00, 116260, 1163AC, 2EFD430, 114614, 114A20), predication/query (2F00468, 2F0ABC4),
  buffer copies (2F0075C, 7A5FB8, 7F6574, 7F67EC), and the buffer SRV binds (113BA4, 114E24, 114F68, 115664, 115084, 815DB0)
  when the buffer has no UAV flag.
- EI argument buffers and UAV binds (113A60, 1140C4, 11422C, 7B18E4) are almost certainly UAV-capable, so only R2b covers them.
- **Rejected:** the engine-side variant (stop resetting `*state` at the six COMMON stores). It produces the unsafe `P -> X`
  from B2, and it cannot test the UAV flag cheaply.

Verification:
1. Trace before and after: in `vkd3d_cost.py` the COMMON class must move into the read-only classes.
2. Screenshot compare at a paused scene with R2 toggled live.
3. 2 min of play, watching for flicker.

### B5. R1 patch design (read -> read supersets)

Producers (all compute X = (cur & mask) | bit): 115B9C, 698A44/698C54, 114E24, 113BA4, 2EFD8DC (texture and buffer SRVs), and
116260, 1163AC, 2EFD430, 494E00, 114614, 114A20 (VB/IB). This is not spec-exact, because the dropped barrier is the
visibility operation for the new read stages. It is identical on RADV *(global cache invalidation, CP-wide waits)*. It needs a
gate and an image compare.

- **Hook (preferred, covers every producer):** `[GpuOpt] R1 = 1`, Wine only. Drop a TRANSITION when `Flags == 0`,
  `before != 0`, `before != after`, `(before & after) == before` and `(after & ~0xC3) == 0` (VCB, IB, NPSR, PSR only). If every
  struct of a call is dropped, skip the vkd3d call entirely: its ResourceBarrier ends the render pass even with 0 structs. The
  engine's tracker already holds `after`, so later barriers name a before-state that covers the dropped one. R1 never drops
  R2a output (`P|0xC0 -> X`), because there `before` is not a subset of `after`.
- **Engine cave alternative (115B9C only):** the site is 115D94; original bytes `41 81 E1 C3 0A 00 00 48 89 54 24 20 44 0B C8`
  (115D94..115DA2; pattern context 115D8F `83 4C 24 28 FF` ... 115DA9 `E8 76 00 00 00 EB 0A`). Patch: `E9 <cave>` + 10 x `CC`.
  At entry r9d = cur, eax = 0/NPSR/PSR, r8 = &state, rdx = 0 (stamp). rcx, r10 and r11 are free: rcx is reloaded at 115DA6,
  and the call clobbers r10/r11.
  ```
  cave: mov ecx,r9d ; and r9d,0AC3h ; or r9d,eax ; mov [rsp+20h],rdx
        cmp byte [rel R1On],0 ; je .call
        test ecx,ecx ; jz .call                    ; COMMON: real transition
        test ecx,0FFFFFF3Ch ; jnz .call            ; cur within VCB|IB|NPSR|PSR
        test r9d,0FFFFFF3Ch ; jnz .call
        cmp r9d,ecx ; je .call                     ; equal: 115E24 skips anyway
        mov [r8],r9d ; inc dword [rel R1Count]     ; tracker := superset (what 115E9D would store)
        jmp 115DAEh                                ; skip the call (115DAE: jmp 115DBA)
  .call: jmp 115DA3h                               ; mov rdx,[r14]; mov rcx,rdi; call 115E24
  ```

### B6. What the trace has to show before building

Take `vkd3d_cost.py` grouped by `caller2_rva` and join it with `barrier_sites.tsv`. It should show COMMON -> X per caller class
and R1 supersets per producer. The list-begin and inline emitters have their own return RVAs: 49503C/49507A (494E00), 1146D6
(114614), 114AFA (114A20), 7F536B (7F52AC).
- Build R2a if COMMON -> X on buffers mid-list is >= ~50/frame.
- Build R1 if supersets are >= ~50/frame.

Integration note for gpuopt: add the resource's `GetDesc` Dimension and Flags (first sight, cached) to the trace, so
the same run splits R2a from R2b.

### B7. Files

- `re\scripts_d3d12\barrier_callers.fsx` (new): per call site of a target (default 115E24), the instruction that set each argument and one level of its source; `--writes <disp>` lists every write to `[reg+disp]`.
- `re\scripts_d3d12\barrier_sites.py` + `barrier_sites.tsv` (new): 74 sites, `caller2_rva`, token, role, class, stamp source.
- `re\symbols\incoming\barrier-callers.tsv` (60 rows) and `barrier-callers-renames.tsv` (3 in-place renames: 116260 SynTokSetVertexBufferTracked, 815DB0, SynCtx+0x6D0 ListStamp).
