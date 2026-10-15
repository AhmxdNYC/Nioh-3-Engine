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
