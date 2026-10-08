# Collision bookkeeping: same-result algorithmic wins (2026-10-08)

Scope: KIDSTS[s9:CollisionUpdate] (16.5 % of profiled game CPU, the 2nd largest area). Prefetch work (CP, OP) is in
`worker-jobs-opt.md`; this pass looked for algorithmic waste in the broadphase and per-object bookkeeping.
Built: `patcher\collbook_live.fsx` (CB = DL + CS), default OFF. Proof output: `re\scripts_wjobs\cb_offline_out.txt`.

## Verdict
The cost is not algorithmic waste. It is one per-frame activation sweep: for every collider in the grid cells around
the player, it asks "is each part within max(16000, +98h) + radius of the player?" and checks the owner state. Every
collider is touched exactly once per frame. There are no pairs, no duplicates, no AABB rebuilds, no virtual chains.
The time goes on first-touch misses on scattered objects. The only real same-result waste found is small (DL, about
0.014 core on the PC). The levers that remain are memory-level (CP is built; GP and OP are below). Ally fps is
unaffected (GPU-bound). Ally CPU power: at most about 0.05 W even with everything below.

## Weights (PC cpuprof averages in cores; hot RIPs from `captures\cpuprof-*.txt`)
| fn | cores | where the time goes |
|---|---|---|
| D80B0 CollisionObjectUpdate | 0.374 | D8291 25 % (owner record +10Fh, first touch), D83E0 13 % (part array), D81BD 11 % (collider line 0) |
| D8B70 CollisionShapeGetWorldPos | 0.185 | D8B7E 96 % (part +62h first touch); 94 % called from D89E0, 5 % from the grid-point test D6E9C |
| D8740 CollisionPartMapFind | 0.080 | binary search; callers @D83F9 82 %, **@D8424 17 % (second, global-owner call)** |
| D8D90 (renamed CollisionOwnerArrayFind) | 0.072 | binary-search loop D8DC0 52 % |
| 43CEC CollisionGatherInRange | 0.054 | 43F48 95 % (collider +A5h first touch) |
| D8E00/D6FF8/D6CD8 (StageUpdate side) | 0.068 | placeable list rebuild (SPF covers the sibling loop) |
| 168E80, D89E0, 2D9EE0 | 0.064 | part position / distance leaves |
| 165088/166F20/1675F0/199180/D1908 (queries) | 0.040 | octree, AABB slab lists, closest point: each one is small |
Ally (v71 SP, `sp_families_out.txt`): collision = 45 % of KIDSTS job samples, about 0.5 core.

## Answers to the brief's questions
- **Broadphase:** a uniform grid per manager, 16000-unit cells. The cell lists (cell +90h, and the collider's own
  cell list at +A8h) persist. The per-frame work vector `mgr+150h` is rebuilt every frame (43CEC clears it, then
  pushes every collider in the window of up to 3x3 cells where +A5h == 0). An incremental version can't give the same
  result: the +A5h flag protocol (the gather sets it, D80B0 clears it) has to touch every collider anyway, and that
  touch is the cost (43F48 = 95 %).
- **Pair generation:** none. Each test is collider against the player point (plus the world +1B20h extra points per
  cell, through D6E9C). Duplicates are filtered by +A5h. Each collider is handled by exactly one of the 5 strided jobs.
- **Static or sleeping objects re-tested:** yes, by design. The result depends on the player position, the part
  position (parent transform +68h), the owner state (+10Fh/+106h/+278h/+26Dh), the world +1A68h table and the part
  record. No dirty flag exists. Proving that a collider can be skipped would read the same cold lines that the
  evaluation reads, so a temporal-coherence or distance-margin skip can't be validated more cheaply than it is
  computed. Tightening the gather window would change the side effects (+A5h/+A6h/+C0h, records on owner-state
  changes). Rejected.
- **Virtual-call chains:** none in the loop. The calls are direct (D8D90, D8740, D89E0). There is one table call per
  part (the position function by type +62h, with two types inlined). The cost is the miss on the part before that
  call.
- **Cache-hostile layout:** yes, and this is the root cause. Colliders (3-4 lines touched each), parts, owner
  records and map nodes are separate heap objects. CP covers colliders and parts; GP and OP below cover the rest.
- **Redundant AABB recomputation:** there are no AABBs here; the test is a sphere distance. The one recomputation is
  the part world position, which is computed again for every grid point in D6E9C (5 % of D8B70, about 0.009 core).
- **Repeated queries:** D8D90 and the owner std::map find run once per collider, keyed by owner. They repeat only
  when colliders share an owner, which needs a live census. D8740 runs twice per part on the global-owner path, and
  the first call is dead (DL). The 5 phase jobs each walk the manager list again and call 19CED0 per manager, which is
  5x redundant but shows about 0 in the profile. D8B24 (a TLS getter whose result is dropped) also shows about 0.

## Candidates
| id | what | RVAs | same-result argument | est. PC | est. Ally | status |
|---|---|---|---|---|---|---|
| **DL** | skip the dead first CollisionPartMapFind when the global-owner path re-searches | D83F4 (call) -> cave; skip resumes D8400 | D8740 is pure (0 stores; writes rax/rcx/r8/r9/r10 only; doesn't read incoming r8-r10). On sil != 0 and r11 != 0, stock either overwrites rdi/[rbp-51h] at D8424/D8427 or reaches D8662, where everything the call set is dead (walker from D840F: ok). [rbp-51h] is read only at D84E7, behind a store on every path | ≈0.014 core (17 % of D8740) | ≈0.008 core, ≈0.01 W | **built** |
| **CS** | conditional stores `owner+110h = +10Fh` (D867B) and `mapobj+126h = 0` (D86A4) | D867B, D86A4 | Any run of `cmp; je; mov` is a run that stock also allows (a skipped store would have rewritten the value already in memory). Flags are dead at D8682/D86AB (walker). Avoids dirtying owner lines that the other 4 jobs read | 0 to ≈0.03 core; depends on how many colliders share an owner across jobs (unknown) | ≈0 to 0.02 core | **built**, gain unproven |
| GP | gather prefetch: in the 43F3A loop, prefetcht0 [[rbx+8*8]+A5h] (guarded by rbx+64 < r12) | 43F3A | prefetch only, so the same loads happen later | 43CEC 0.054 × 20-40 % ≈ 0.01-0.02 | small | not built (CP-style, about 40 lines) |
| OP | owner-record prefetch (from worker-jobs-opt §6): in the CP cave, collider i+10: key -> D8D90 -> prefetcht0 rec+F8h/+10Fh | CP cave | prefetch only; D8D90 is pure | hides part of D8291 (0.09 core) ≈0.03-0.05, minus an extra search | ≈0.02 | test in the CP harness first |
| SI | block-indexed lower_bound for D8D90/D8740: a cached sample of every 16th key, then a block search validated by the two boundary keys read from the live array (correct for any sorted array; falls back to full search) | D8D90, D8740 | the result equals lower_bound whenever the boundaries hold, so stale samples only cost speed | ≈0.04-0.07 (dependent chain becomes 2-3 parallel misses) | ≈0.02-0.03 | research: needs per-array side tables, thread-safe publish, more than a cave |
| OM | per-job memo of the last owner key -> (std::map value, D8D90 record, global owner) | D81F0..D8353 | the maps don't change during the phase | 0..0.1 | 0..0.05 | needs census: do consecutive colliders in a job share keys? |
| HP | hoist the part position out of the D6E9C point loop | D84B0 loop | D8B70 is a function of the part only | ≈0.009 | ≈0 | not worth it |

Realistic total beyond CP: DL + GP + OP ≈ **0.05-0.08 core on the PC, 0.03-0.05 core on the Ally** (worker CPU and
watts only, no fps).

## CB proof (`dotnet fsi --optimize- patcher\collbook_live.fsx offline`)
- The sites match the dump. Each site is one instruction (`call` -> `call`, 7-byte `mov` -> `jmp` + nop2), and no
  direct branch in the whole image lands inside a site. There is no overlap with OTHER_SITES or CP (D8180).
- Static checks: D8740 purity; the walker shows rdi/rcx/r8/r9/r10 dead from D840F; flags dead at D8682 and D86AB.
  The cave writes only rsp (DL), and its only stores are the two original ones (CS).
- Real-code harness (the CP harness, `#load`ed: dump image mapped, D80B0 and its real callees, synthetic worlds with
  owners shared about 3 ways): **300 worlds, 0 mismatches**. Each world was compared byte for byte, together with all
  180,024 change records, across these runs: CB on, CB installed with switches off, DL only, CB + CP, and stock vs
  CB + CP with the **5 jobs on 5 threads at once** (134 worlds).
- Timing in the harness is within noise for DL and CS (the swings were ±8 % in both directions between two runs).
  The expected effects are about 2 % of the family, below this harness's resolution. A PC A/B (CP on vs CP + CB on,
  CollisionUpdate thread CPU) is the only meaningful measurement.
- `--optimize-` is needed because `#load`ing collprefetch_live.fsx under the optimizer trips an F# script
  static-init bug (NullReference in the wj_common init). collprefetch_live.fsx now runs its main only when it is the
  script being run.

## Next steps
1. When CP goes into the ASI, add DL as a 2nd site of the same patch group (a 5-byte `call` retarget plus a 30-byte
   cave). Keep CS out unless the census shows that owners are shared across jobs.
2. Live read-only census on the PC (mgr+150h vectors): colliders per frame, distinct owner keys, colliders per owner,
   same-owner neighbours inside one stride class, the share on the global-owner path, parts per collider, and the
   sizes of the part map and owner array. This decides CS, OM and SI.
3. GP and OP: build them in the CP harness (prefetch only, same machinery).
