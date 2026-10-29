# Cross-world cell streaming after World::WarpOtherWorld (bridge 6047)

2026-10-15, static RE only (capstone on /tmp/re-dump/Nioh3.mem.bin, v2.02 runtime image, RVAs). Game NOT run.
Branch ally/xstream-6047, mod/Nioh3Training.

## Pipeline (stage mgr S = [45B9E58], sub P = S+0x500)

- Per-frame stage update `39FBF4` (from 319A57): 39F838(P+0x240), 39F9F4(P+0x2B0), **3A0408** (if [S+0x11F1] and P+0x30),
  39FAC8, 39FED4, **31D28C(P+0x40)** (cell set update), 39FD5C, FB7D30.
- `3A0408`: for every cell of the world grid ([P+0xC] x [P+0x10], 3A18F8) not already present calls
  `3A0534(P, cell, immediate=0, 0, 0)`; drains removal list P+0x18 (9B16D8 / 4B1CE4); then `3A083C(P, &playerPos)`.
- `3A0534`: while state != 5 and !immediate the cell goes to the deferred list P+0x390 (0x18 stride); else, cell record
  (HashIndexFindRecordD8 [db]+0xE0) +0xD5 in {0,1,4} -> 1FB460C / 1FB4738 creates the cell object (1F8FC00) in the set P+0x40.
- `StagePathInitStep 3B35B4` (state P+0x389):
  0 -> 1FCCFF4(P+0x240) -> 1; 1 waits [P+0x260]==3, 1FD1FBC, 1FCCAB4(S+0x450) -> 2; 2 waits !StageRequestQueueBusy(S+0x450)
  and 1FCCEE0(P+0x2B0), 2264FF0 -> 3; 3 waits !1FC692C(P+0x2B0), adds ONLY the main cell immediately (3A0534 imm=1) if
  the set is empty, 1FB4554 -> 4; 4 waits **31D480(P+0x40)** (every cell in the set ready) -> 5; 5 flushes P+0x390.
  So state 5 = path data + main cell; the rest of the world's cells are only admitted after 5.
- `31D480` walks the set and calls **`31DFB8`** per cell (also from 31D28C every frame). 31DFB8 starts the cell's load
  (613284 request, 4AA824, 4A9384) when its state [cell+0x10]==0, but for type-0 cells (record +0xD5==0, FB0B80) and
  cell+0x7D==0 only while:
  - **A** `1FBFA30(S) + n < 4` at 31E0B2 (`83 F8 04`): n = ids in S+0x19C0 list whose area (S+0x540) is in state 2..9
    (1FC6F70 = loading); 1FBFA30 = type-0 ids in S+0x1A90 + entries of the vector S+0x1AA8 (release queue, filled by
    FB3000/FAB3D0 from the sorted map S+0x1B40).
  - **B** `[S+0x1B48] < 4` at 31E0BB (`48 83 BB 48 1B 00 00 04`): size of the sorted map S+0x1B40 (8-byte entries keyed
    by cell id, SRW lock [S+0x1B58]+0x10).
  => at most 4 concurrent cell loads, and pending releases of the old world occupy the same slots. This is the only
  per-frame count limit found on the cell path; no timer/wait in 3A0408/3A0534/31DFB8/3B35B4 (state 0-4 waits are
  resource-completion polls, not timers). The WhiteOut fade is the sequencer's (not decoded here).
- Iteration order of 31D480 is set (cell-id) order, not distance; if the priority set +0x28 is non-empty only its
  cells are processed (31D51C). Not used by the change.

## Change (default off)

`[ArenaSwap] CrossWorldCellLoads = N` (5..64): XsStart (in XwStart and `oworld`) verifies the 15+14 bytes around both
sites and writes imm8 = N at 31E0B4 and 31E0C2; XsTick restores 4 when state 5 was reached after the reset and every
set cell has its active object +0xE8 set, or after 60 s, on native fallback, or on a tick exception. Logging (also
with the flag 0): `XSTREAM <tag> +ms: world, state, cells, loaded, loading areas, release queue, map 1B48` on change /
1 s, `stage state 5 at +X ms, r/n cells loaded`, final line with both times. Command `cellcap [N]`.
Also fixed: Warp() called 3A083C without rdx (the centre pointer it reads as a float3); now passes S+0x10B0 like native.

## Unverified / risks
- Whether the cap binds after a world switch (the log's `loading areas`/`release queue`/`map 1B48` columns show it).
  If state 0-3 dominate the 13 s (path data), the change cannot help that part.
- Meaning of S+0x1B40 map (assumed async cell release tracking); raising B may raise the memory peak.
- Code patch of live imm8 while the game thread runs (single-byte store; old or new value is seen).

## Round 2 (bridge 6047b): Ally test, cap 12 = no gain; what holds the switch

Ally data (/tmp/xs6047 OFF1/OFF2/ON1, route oworld E0 then fnow 0x7B35): stage state 5 at +12..15 s, GAMESTATE[2] stays 1
to ~+31 s, map 1B48 = 4 and cell flags constant -> the cell cap is not the limiter (CrossWorldCellLoads stays, default 0).
Key timing: with the boss preload the stage world byte changes only at **+6.5 s / +5.95 s**; in the plain `oworld` leg
(no preload) at **+0.12 s**.

Static chain:
- GAMESTATE = GameMain+0x588 stop-flag block; StopFlagSet 3B28FC -> A461E4 is a counter (+0x598 for idx 2; on=1 adds,
  on=0 subtracts, clamped at 0). XwStart's pre-call (2,0,0) is a decrement.
- Mission load sequencer (state [seq+4], dispatched by 5A29F0, mirrored in u32 **B+0x45BA21C**):
  1 MissionLoadStep 223AF50: waits 1FC6674(stage+0x500) **and LoadIsLoadEnd 3B3424** before consuming the request;
  2 223B8F0 (warp point, 3A1700 main cell, 1FCEE1C/1FCECE4 unload/load world); 3 223B3F8; 4 223B3A8; 5 223BDB0
  (UiLoadingScreenStart); 6 223C53C; 7 223C630 (sets [[45B9E40]]+0x2679 = 1, enables gate G7); **8 223C4A8 waits
  LoadIsLoadEnd**; 9 223C684; 0xA 223C5B8; 0xB 223BE08 (219F3A0 wait, UiLoadingScreenEnd); 0xC-0xE 223BEA8 (5A29B0 /
  1CFCD24 / player gates, actor AI-on 66982C, autosave) -> 0xF; **0xF 223C308: fixed timer 30 or 100 dt60 (+60 for journey
  0xC0) then StopFlagSet(2,0)** = GAMESTATE[2] clear -> 0x10/0x11.
- LoadIsLoadEnd gates (order): G1 StagePathInitStep, G2 8B3868(S+0x540) area with no active object, G3 3B3974(S+0x450),
  G4 UI pair (if byte 45C6F2C), **G5 13335C([45B9E60]) any resource entry +0x2C8 == 0**, G6 D3958, G7 2BE284 blocker
  list, G8 3B3A28, G9 3B2970, then characters (3B39AC) with a 120 dt60 (2 s) timeout (acc float 45C84A4).
- [45B9E60] is the list ResEntry() searches and +0x2C8 is the flag XwStart tests: **the CrossWorldPreload request
  (3F9E58) holds G5 -> state 1 -> the world switch until the boss model is loaded.** The remaining ~17 s after state 5
  are most likely state 8 (field placement resources, registry 44 -> 228 at +17 s); not confirmed which gate.

Change: `[ArenaSwap] CrossWorldPreloadAfterSwitch = 1` (default 0) defers XwPreload until CurWorld() == target.
New log (always on while XSTREAM runs): on every change `GAMESTATE[2] / mission seq state (+ time in previous state) /
LoadIsLoadEnd gate mask (1 G2, 2 G3, 4 G5, 8 G7, 16 G8)` via read-only predicates from the pump (game thread).
Expected: switch at ~+0.1 s instead of ~+6 s, total ~5-6 s less (UNVERIFIED). Risk: if state 8 then waits on the boss
model instead of field entities, the gain shrinks; the gate mask shows which. Early arena swap with GAMESTATE[2] = 1 was
not built: the screen stays covered until state 0xB and control until 0xF, so it saves only the 0.7 s settle.

## Round 3 (bridge 6047c): gate 16 'area objects' holds ~16 s

Ally: CrossWorldPreloadAfterSwitch no gain (switch still at ~+6 s); log shows sequencer state 4 for 5.4 s, state 5 from
+6 s to +29.8 s with only gate 0x10 failing from +14 s. **Correction** of round 2 numbering (cold dispatch 19DBA8C):
1 223BDB0 (UiLoadingScreenStart), 2 223B3A8, 3 223B3F8, **4 223B8F0** (starts with StageAreasObjectsReady(S,0,-1) for the
OLD world = the 5.4 s before the switch, then unload/load world), **5 MissionLoadStep 223AF50** (1FC6674 + LoadIsLoadEnd),
6 223C53C, 7 223C630, 8 223C4A8 (LoadIsLoadEnd), 9 223C684, 0xA 223C5B8, 0xB 223BE08, 0xC-0xE 223BEA8, 0xF 223C308 (timer,
clears GAMESTATE[2]). So the preload refutation is consistent: G5 was not what held state 5.

Gate 16 = StageAreasObjectsReady 3B3A28(S, force, area): per loaded cell A = D3B9C(S+0x540,i) (active object), o=[A+0x30]:
ready iff o+0x1F0 != 0, **o+0x1EC == 0**, A+0x1C0 == 0. o+0x1EC = placeable loads in flight: incremented in
PlaceableCollisionUpdate D8E00 (D97E8 / D9EC7, placeable +0x10D = 3) when a load request (prio at req+0x50) is put on the
loader [475A420] list +0x1D8 (count +0x1E0, mutex +0x98, flag +0x24A); decremented in 2007E4 on completion. The loader
feeds the TrySync step pool (3 threads, ReadFile-bound on the Ally per loading-ally-fixed.md). No per-frame budget or
immediate cap found in D8E00; throughput is I/O/step-thread bound (TSY / RC in the engine fix are the throughput levers).

Change: `[ArenaSwap] CrossWorldAreaObjSkipMs = N` (default 0). Inline hook on 3B3A28 (15-byte prologue verified):
original result first; if 0, area == -1, XSTREAM active with target world current, stage init state 5 and sequencer state
5 or 8, then after N ms the hook returns 1 (first 3 times logged with blocking cells / placeable loads in flight). State 4
(old world before unload) and single-area callers (1FD4388, 28B75xx) untouched. Placeable loads are not cancelled.
Also logged on every gate change with bit 16: `area objects: X cells blocking, Y placeable loads in flight`.

Risks (UNVERIFIED): props/destructibles/doors and their collision appear after control returns; scripts that look up a
placeable right after the load may not find it yet; the autosave in 0xC runs with placeables still streaming.
Expected: control ~+15-16 s instead of ~+31 s on the Ally route.
