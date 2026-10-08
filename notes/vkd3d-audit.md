# Nioh 3 under vkd3d-proton: which D3D12 patterns cost more on the Ally, and game-side fixes

Status 2026-10-07. Static work only: vkd3d-proton `D:\refs\vkd3d-proton` (commit 2230755, `libs/vkd3d/command.c`,
`resource.c`), DirectX-Specs, the dump/decomp, and the existing Ally logs (`captures\ally\*.log`, GP2 digests) plus the
PC RGP r5 barrier digest (`D:\nioh3-re\d3d12\bcost\summary_r5.txt`). The game was not run and nothing in `src` changed.
RADV statements are from vkd3d's code paths plus knowledge of RADV (RADV source is not on this PC): marked *(RADV)*.

## TL;DR

The CPU side of the translation is not the Ally's problem: the dispatcher thread is 81 % parked (69 % in the ECL-ring
backpressure wait 4930C8 -> EclRingPush), and vkd3d (`d3d12core`) + winevulkan are 9.3 % of its samples. All
fps-relevant costs are GPU waits that vkd3d emits for the engine's barrier pattern.

| rank | id | pattern (engine) | what vkd3d does (command.c) | per frame | Ally GPU (model) | fix | status |
|---|---|---|---|---|---|---|---|
| 1 | **R2** | Per-list state reset: `SynTransitionResource` 115E24 sets the tracked state to COMMON when the resource's stamp != `SynCtx+0x6D0` (fence value), so the first use per list is a `COMMON -> X` barrier | `vk_access_and_stage_flags_from_d3d12_resource_state`: COMMON = `ALL_COMMANDS` + `MEMORY_READ/WRITE` -> *(RADV)* full wait-for-idle (CS+VS+PS partial flush) + CB/DB flush, mid-list | unknown (which of the 45 callers pass a stamp is not decoded; EI argument buffers do); PC proxy: 485 EOP waits of 1360 barriers | up to ~0.2-2 ms if many are textures with false dependencies; buffers are excluded | rewrite/drop when the real prior state is known (design 2) | **measure first**; not provably safe |
| 2 | **R1** | Texture bind 115B9C ORs read states: `new = (cur & 0xAC3) | PSR/NPSR` (115D94..115DA9), e.g. `NPSR -> NPSR|PSR` | read->read transitions are not elided (`barrier_batch_add_layout_transition`; no layout change, both use `common_layout`): one `vkCmdPipelineBarrier2` *(RADV: partial flush of the src stage + VCACHE invalidate)* and `ResourceBarrier()` ends the render pass | unknown | 0.05-0.4 ms if 50-150/frame | drop strict-superset read->read transitions (design 1) | design; same result on RADV, not spec-exact |
| 3 | RP | Every barrier call, OMSetRenderTargets (always ends, no compare), Dispatch, Copy*, partial clear, ClearUAV ends the dynamic render pass | `d3d12_command_list_end_current_render_pass` at the top of `ResourceBarrier`, `OMSetRenderTargets`, ... | ~1270 barrier calls (Ally, gpuopt off) -> ~520 with gpuopt on | small *(RADV: no tiler; with unified layouts End/BeginRendering emits no layout barrier)*: <= 0.1-0.3 ms | gpuopt `Barriers = 1` already removes ~60 % of the splits | exists (ASI 1.7.x, default off) |
| 4 | E1 | 21-28 GFX lists per frame, one `ExecuteCommandLists(1)` each (ECL thread tok 6, 4F9214) | one `vkQueueSubmit2` per ECL; on RADV no barrier command buffer but a serializing binary semaphore between submissions (`vkd3d_driver_implicitly_syncs_host_readback`, `serializing_semaphore`) -> *(RADV/amdgpu)* same-ring pipeline sync + cache flush per submission | 21-28 | PC: 1.07 ms idle at CB boundaries (~38 us each, mostly gated stalls, gpu-scheduling B2); Ally unknown | merge consecutive ECLs (design 3) | design; D3D12 ECL-boundary semantics make it risky |
| - | - | ExecuteIndirect (tok 6E/6F/81, MaxCount 1) | signatures are plain: created at 2EFF231 (`CreateCommandSignature`, NULL root sig): `SynCtx+0x6E8` DRAW stride 0x10, `+0x6F0` DRAW_INDEXED stride 0x14 -> no state template, no DGC, direct `vkCmdDraw*Indirect` | - | none | - | not a cost |
| - | - | Queries | no BeginQuery/EndQuery/ResolveQueryData in the dispatcher handler table (`D:\nioh3-re\d3d12\vcalls.tsv`); only the frame-end ResolveQueryData on the main thread. The ~1400 GP2 timestamps were the drain (ASI 1.7.8) | 1 | none | - | not a cost |
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
Integration note for the gpuopt owner: also record the second Nioh3 frame (`CallerRva` loop, keep the next match) so
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
uses global barriers for buffers anyway).

**Design 3 - ECL merge.** Hook `ID3D12CommandQueue::ExecuteCommandLists` on the GFX queue (the ECL thread replays tok 6
ECL / 7 Signal / 8 Wait, table 3BD4DF0): hold a list while the engine's ECL ring (EclRingPush 86F760) already has the
next item and submit `ECL(n)` up to the next Wait. Signals keep their values but fire later. Blocked by semantics:
D3D12 decay/flush happens per ECL; inside one vkd3d `ECL(n)` the lists share one `vkQueueSubmit2` with no semaphore
between them, so engine code that relies on the per-ECL flush (promoted buffer writes, the per-list COMMON reset) would
lose it. Do not build before design 2's binding proof; payoff unknown on the Ally.

## Files

- `re\scripts_d3d12\vkd3d_cost.py` (new): trace -> vkd3d/RADV cost model, `--selftest` OK.
- `re\symbols\incoming\vkd3d-audit.tsv`: 115B9C rename (SynBindTextureSrv), the R1 state computation, the command
  signature creation, SynCtx +0x6E8/+0x6F0.
