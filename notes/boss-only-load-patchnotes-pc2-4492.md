# Boss-only load: patch notes for pc (task 4492)
2026-10-14 pc2. Static only, from decomps in C:/nioh3-re/bridge-files (task3271-decomp, decomp_batch1_*). RVA = VA - 0x7FF74F410000 (dump identity in notes/boss-only-load-pc2-3271.md).

## 1. MissionLoadStep 0x223AF50: nothing in it can be skipped to save load time
It is the sequencer's post-load step (state 5 -> 6). It runs only after `1FC6674(stage+500)` and `LoadIsLoadEnd 3B3424` are both true, i.e. AFTER all loading finished. Its body is player/Emaki/net setup (MissionSetupEmaki 1C2794C, 1C27898, 1C29768, 1CA4F8C...). Skipping stages of it removes zero loading and breaks setup. Do NOT patch it for speed.
The load is SCHEDULED one step earlier, in **0x223B8F0** (same caller 5A29F0; ends by writing state 5 at the end of the function, `_DAT_7ff7539ca21c = 5`), and the per-object work is done by the placement builders (below).

## 2. Hook point A (area/cell load) - 0x223B8F0
- Streaming centre: `3A1700(areaByte [45BC428], &pos)` -> cell id (`uVar7`); pos comes from the start record (`+0xC`) or the override you already install. Keep the arena start as centre (already done).
- Scheduling variants chosen by bytes: `45BC465 != 0` skips the whole cell-schedule block (also skips 3A1700) - not useful, the arena cells would not load. `45BC463 != 0` -> `1FC4208(ctx,area,cell)`; else `45BC462 != 0` -> `1FCEE1C(ctx,area,cell,1)`; else `74fd281d8(ctx)` + `1FCECE4(ctx,area,cell)` (RVAs 0x1FC4208, 0x1FCEE1C, 0x1FCECE4).
- What each variant loads (one cell vs a ring) is NOT established by the decomp. Cheap test for pc: log-only hook on 0x1FCECE4 / 0x1FCEE1C / 0x1FC4208 args + count of cells scheduled in a Fight Now load, then try flipping 45BC462 (writable game byte) and see whether the ring shrinks. Do not claim a saving before this.

## 3. Hook point B (what gets spawned) - placement builder 0x1C1E470 (single caller 0xA31A50)
Per mission `rec = param_1+0x128`, mission id `param_1+0x120`.
- Pass 1 (resource `rec+0x60`, enemy placement set): loop `i < 750FC8314(set)`; `id = 750FCB8C4(set,i)` (RVA 0x1BBB8C4); `if 750FC8DA0(set,id)` (RVA 0x1BB8DA0) then **`74FBE38FC(id, missionId, 0)` = RVA 0x7D38FC creates the placement** (call is `uVar4 = placement id`, `param_1+0x120 = mission id`).
- Pass 2 (table `rec+0x6C`, start/gadget rows): row eligible via `74F7C2648` (RVA 0x3B2648), then **`74FBE3BDC(&struct)` = RVA 0x7D3BDC** creates it. struct+0x0 row id (`piVar6[0]` -> `local_170`), mission id at `local_70`. Existing native filter to copy: for mission 0xCC96 the row is skipped when `*row != emakiRecord+0x10` (goto 0x1C1E783).
- The other enemy sets of the mission record (+0x54,+0x58,+0x5C,+0x64,+0x68) are NOT created in 1C1E470; they must come from `PlacementSetsBuildFromMissionDb 1C1D148` / `1C1D288` (not decompiled yet).

### Recommended patch (log first, then filter)
Detour **0x7D38FC** (placement create; RCX=placement id, EDX=mission id, R8=0 per decompile, verify with the asm window before trusting registers):
1. Mode LOG: record (mission, id) for the whole Fight Now load and call the original. Gives the full list of what is spawned.
2. Mode FILTER: if mission == current Fight Now mission and id not in keep-set -> return without calling the original (decide return value from asm; caller ignores it in 1C1E470).
   keep-set = catalogue `placement_id` (boss) + phase-2 placement ids of the same boss (45 missions have several boss candidates, notes/boss-duel-flow.md:57) + anything the arena script waits on (gate/door/wave-start rows live in pass 2, so DO NOT filter 0x7D3BDC at first).
Same pattern for pass 2 only after the LOG shows which row ids the mission script's gates need.
Same-hook risk: mission script may wait on kill counts / all-enemies-dead for door open -> hang. The LOG run + one filtered run on a single easy boss (e.g. D22D Honda duel, already near single-enemy) tells that.

## 4. Not recommended
Return/jump out of 223AF50, forcing LoadIsLoadEnd 3B3424, forcing sequencer state (`7ff7539ca21c`), or setting 45BC465: all unsafe/unsupported.

## 5. Gaps (need exports from pc to be bytes-exact)
- asm windows for call sites of 7D38FC/7D3BDC inside 1C1E470 and for 7D38FC entry (to confirm arg registers and relocation span) - ask for `0x7D38FC`, `0x7D3BDC`, `0x1C1D148`, `0x1C1D288`, `0xA31A50`.
- Expected gain is unmeasured; previous notes say 14.6-16.3 s remain after resource-manager drain, which placement filtering may not shorten.
