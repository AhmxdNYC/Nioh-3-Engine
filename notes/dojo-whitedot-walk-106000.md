# Dojo white-dot Proceed by walking (#106000)

Offline. No game. Branch cx-pc/whitedot-106000 from f6180c85. Lesson rows (RF5E5 RD759 R26F3 RA0A1) end with a white dot
and the Proceed clear gadget at a script point. Live tests only reached it by warp (#76208 RD759: walk failed, warp
76000 3557 55750) because the harness held a per-Dojo table (`$dots`/`$warps` in Temp/dojo98893/run.ps1) and the old walk
orbited (stick mirror, fixed in dojo98893 WalkTo but never run on a lesson row: that run was R9BE4 only).

## Rule (universal, nothing per Dojo)
- The white dot is the script native `UI::SetCompassDestinationIndex(i, x, y, z)` (lesson end, right after
  `Gadget::CreatePlacementGadgetPosition(gadget, x, y, z)` with the same xyz) or `UI::SetCompassDestination(x, y, z)`
  (clear-gadget flow `Function::SetOnlineEventNumber`). PROVEN by script listing (RA0A1 0AD2EFD76D68: 1022a/10244, 7f9a).
- Native table v2.02: SetCompassDestinationIndex fn 0x23B7FD0, SetCompassDestination fn 0xA3E0C8 (entries 0x3982140 /
  0x3982120). Both are `kNative` sites resolved by name, head `48 89 5C 24 10` (derive_sites.py, sites_gen.h).
- BR hooks both, log-only, only while a Fight now attempt runs. The last three float VM args are x y z. One line per
  point per attempt: `Rematch: white dot X Y Z (form) - player X Y Z, D m flat - Fight now row ID`.
- The harness walks to the newest line for the row: `tools/testmatrix/white_dot_walk.ps1 -Row RA0A1` (parser
  `white_dot.py`). If the walk stalls, the Training test warp goes to the same logged point. Then RB is held 3 x 2.5 s.
- Why not a catalogue column: the runtime call covers every mission that shows a dot, including future ones, and needs
  no static arg resolution (the lesson xyz come through the entry call's params). rematch_catalog.py is unchanged.

## Proof so far (offline)
- tests/test_white_dot.py PASS: production LogWhiteDot body compiled with VM-arg fixtures (silent outside Fight now,
  float xyz only, once per attempt, new attempt logs again), parser reads the exact mod output, walk script carries no
  Dojo coordinates, 4/4 lesson scripts' entry reaches the compass native.
- test_native_gate_retry PASS. test_sites.py has the same 10 probe-simulation failures as base f6180c85 (not new).
- BR builds (no new warnings).

## Not proven (needs one live run, Development seat with the lock)
- That the hook fires with the decoded xyz (the VM arg layout is the IsDeathGroupEnemy one; float type 2 per bosslog).
- That a straight stick walk reaches the dot from the lesson start (RD759 8.6 m, RA0A1 18 m) without walls.
- Live pass lines: `white dot 76494 1270 37765` on RA0A1 (or 76135 3555 55802 on RD759), harness `reached=True via=walk`,
  then the Proceed prompt / gate page on RB and home.
- Lesson waves-win code lives on cx-pc/dojo-89218 974758df, which is NOT in f6180c85. This branch only adds the dot; the
  live run needs the Dojo build or a merge of both.
