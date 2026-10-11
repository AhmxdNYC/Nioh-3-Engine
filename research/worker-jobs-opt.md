# Worker / job-thread CPU: same-result speedups (collision, stage, object update, job system)

Static analysis plus offline verification. Nothing was attached to the game, read live, written or applied, and
the mod source was not touched. Date: 2026-10-07.
Scope: the engine's worker-thread job code (KIDSTS[s9:CollisionUpdate], KIDSTS[s2:StageUpdate], KIDSTaskScheduler
object update, the job system), real work only. The main thread's SP profile and spin loops belong to
`cpu-hotspots-ally.md`.

Live scripts (private dev repo; all default off, `dry / apply / on / off / status / revert`, FT_ALLOW_WRITE guard,
`Mem.safeWrite`): `collprefetch_live.fsx` (CP), `stageprefetch_live.fsx` (SPF), `lockload_live.fsx` (LK).
Helpers (census, AOB patterns, SP aggregation) and saved offline outputs stay there too.
The 38 symbols from this note are in symbols/symbols.tsv.

## TL;DR

The worker job code is **latency-bound pointer chasing, not redundant arithmetic**. Every hot worker function's hot
RIP is the instruction right after the first touch of a per-object line (collider, part, placeable) or after a
`lock cmpxchg` used as a load. So the same-result levers are (1) software prefetch of the next objects of the same
loop, issued from a cave at the loop head, and (2) plain loads instead of locked "atomic loads". Both change nothing
architecturally: same registers, same memory, same branch decisions; only cache state and timing differ.

| rank | id | what | where | cost now | measured / expected saving | status |
|---|---|---|---|---|---|---|
| 1 | **CP** | Prefetch pipeline in the collision worker loop: colliders 3 strides ahead, their part/count/radius arrays 2 ahead, their first 4 part objects 1 ahead | cave at D8180 (loop head of CollisionObjectUpdate D80B0) | PC ~0.9 core (D80B0 0.37-0.42, D8B70 0.18-0.20, D8740 0.08, D8D90 0.07, 43CEC 0.05, 168E80 0.03); **Ally: 45 % of all KIDSTS job samples** (≈0.5 core) | **real D80B0 code, synthetic worlds: -16 to -27 % of the loop's time** (cold and L3-warm). In game: ~10-20 % of CollisionUpdate CPU ⇒ **PC ≈0.1-0.18 core, Ally ≈0.05-0.1 core** | built, **0 mismatches** over 300 worlds / 175k change records, ALL PASSED |
| 2 | **SPF** | Prefetch of the placeable 4 entries ahead (+10h/+70h/+98h/+C8h/+109h) and of its +B0h object 2 ahead | cave at DB8BF (placeable-map loop of PlaceableVisibilityUpdateWorker DA830) | PC ~0.15 core (DBA70 0.06, DA830 0.035, DBB90 0.03, DA6E0 0.02); **Ally: 25.5 % of KIDSTS job samples** (≈0.28 core) | same miss pattern as CP (hot RIP DB928 = 88-95 % of DA830 self) ⇒ est. 15-25 % of the loop ⇒ **PC ≈0.02-0.04, Ally ≈0.04-0.07 core** | built, transparency test 30,000 runs 0 differences, ALL PASSED |
| 3 | **LK** | 18 `lock cmpxchg [m],zero` loads -> plain `mov` (+`test` where a ZF jcc follows), same length | object update 1A188C / 425B0, meshlet job 4A2D0, 130014, job system 40800 / 411C0 (not in J1/M2) | hot RIPs right after these sites: 1A18CD, 4A563, 13009E … ≈0.045 core (PC, 13 captures) | **≈0.03-0.04 core** PC, similar on the Ally; fewer lines taken exclusive | built, 18 rows proven + native test 0 differences, ALL PASSED |
| – | PL | Param-table lookup memo: per-object linear scans of the [g+3D30h] param table (32-byte entries, hash compare) | 121E90, 1201F8, 120160, inline 1234A0 in 121FC0 | PC ≈0.13-0.15 core; Ally: part of the 32 % "object update" share | up to ~0.1 core | **not built**: equivalence needs a live read (table immutability, duplicate hashes); design + probe in §5 |
| – | SH | Hinted search for CollisionPartMapFind (part k>0 starts at part k-1's slot) | D8740 call D83F4 | PC ≈0.08 core | ≤0.04 core, depends on part-pointer locality | not built (§6) |
| – | OP | Prefetch the owner record (D8D90 result, hot RIP D8291 = 21 % of D80B0) | in the CP cave | ≈0.08 core of misses | needs a second D8D90 search per collider; net unclear | not built (§6) |

**Estimated total if CP + SPF + LK are integrated (not measured in game): PC ≈0.15-0.25 core, Ally ≈0.12-0.2 core of worker CPU.**
None of it touches [App]Game or the frame pacing, so **no fps change is expected on the PC** (the workers are not the
limiter there; KIDSTaskScheduler0 joins CollisionUpdate jobs in 3E7CC while helping, so its frame step can get a little
shorter). On the Ally the value is power: ≈0.15 core of busy Zen 4 at Ally clocks is roughly 0.2-0.4 W that the
25 W budget can give back to the GPU. That has to be measured (package power at a fixed fps cap), not assumed.

**Integrate first:** LK (pure byte rows, same machinery as J1/M2, zero runtime cost) and CP (largest share on both
machines, verified on the real code). SPF next (same technique, smaller loop).

---

## 1. Where the worker CPU goes (inputs)

**Ally, ASI SP profiler** (the v7.1 MAX log, 146 reports of 30 s; aggregated with `sp_families.py`):

| group | samples | idle waits (JobWorkerThreadMain 1A6BB84, condvars) | job work | collision | stage placeables | object update | job system | meshlet | AABB |
|---|---|---|---|---|---|---|---|---|---|
| KIDSTS (77 thr) | 1,488,956 | 1,467,160 | 21,796 | **9,830 (45.1 %)** | **5,550 (25.5 %)** | – | 2,286 (10.5 %) | 1,221 (5.6 %) | – |
| KIDSTaskScheduler (8 thr sampled) | 129,240 | 113,487 | 15,753 | – | – | **5,101 (32.4 %)** | 1,340 (8.5 %) | – | 1,745 (11.1 %) |

- Collision = `fn 0xD8152` (the main body of D80B0: the .pdata chain starts there), D8B70, D8740 (`0xD8770`),
  D8D90 (`0xD8DC0`), 43CEC. Stage = DA830 (`0xDA879`), DBA70, DBB90 (`0xDBBE1`), DD7AC/DDC70, D8E00.
- The Ally thread capture (v6) gives KIDSTS[s2:Stage] 75 %, [s9:Collision] 72 % CPU, almost
  all **user** time, while SP puts only ~2.5 % of KIDSTS samples outside the idle waits. On Proton the wait/wake path
  (Wine ntdll + ntsync ioctl) is itself user-mode CPU, and SP files it under the wait (`[ntdll] <- fn 1A6BB84`). So a
  sizeable part of the Ally's "KIDSTS CPU" is wake/sleep overhead, which is the WC / WK / ED territory
  (`research/job-system.md`, `worker-power.md`), not job code. The job code itself is ≈1.1 core, of which
  collision ≈0.5 and stage placeables ≈0.28.

**PC, cpuprof r5/r6/r7/r9/ingame2** (`hotlist.py`, self time averaged over the captures, cores): 48E90 0.61 (M1/M2 done), **D80B0 0.37, D8B70 0.18**, 411C0 0.18 (J1 done), 2515B0 0.13,
1A7BF0 0.10, **D8740 0.08** (as "D8700(leaf?)"), 121FC0 0.08, **121E90 0.08** (as "121E70(leaf?)"), **D8D90 0.07**
(as "D8C40(leaf?)"), 6EFD0/6EA50 (AABB done), 469C0 0.065, **DBA70 0.064**, 425B0 0.058, **43CEC 0.054** …

Hot RIPs (PC cpuprof captures): D80B0: D8291 21 % (first touch of the D8D90 result +10Fh), D81BD 12 %
(collider line 0), D83E0 12 % (part array); D8B70: D8B7E (part +62h); DA830: DB928 88-95 % (placeable +10Ah);
DBA70: DBA89 / DBABD (placeable +70h..+98h); DA6E0: DA71F (placeable +B8h); 1A188C: 1A18CD 46-73 % (after a locked
load). Everything points at memory latency.

## 2. CP: collision worker prefetch pipeline (`collprefetch_live.fsx`)

**The loop.** CollisionUpdateDispatchJobs 319E00 runs CollisionObjectUpdate **D80B0(mgr, out, start)** as 5 strided
jobs over the work vector `mgr+150h` that CollisionGatherInRange **43CEC** fills each frame with every collider in the
grid cells around the player (`+A5h` = 1). For i = start, start+5, … (r13; r10 = mgr) it reads the collider
(+08h/+0Ch owner keys, +88h part array, +90h radii, +98h, +9Ch, +A0h, +A5h, +A6h, +A8h/+B0h grid list, +C0h,
+C8h count), walks the owner std::map, binary-searches the owner array (D8D90), and per part: CollisionPartMapFind
D8740, the distance test D89E0 -> D8B70 (part +62h type, +68h parent, +80h or +B0h position) and grid tests D6E9C;
it emits a 0x30-byte record only when a part's active state flips. Nothing is redundant per se (the inputs - player
position, owner state - change every frame); the cost is the first touch of 4-8 cold lines per collider. One
iteration is hundreds of instructions, so the out-of-order window cannot overlap the misses of two iterations.

**Patch.** `D8180  49 8B 8A 50 01 00 00` (`mov rcx,[r10+150h]`, the loop head; only branch into it: D86BB) ->
`E9 <cave> 66 90`. Cave (316 bytes, code at +40h, switch dword at +0):
```
if (!en) goto stock
push rdx; push r8
rcx = vec = [r10+150h]; rax = n = ([r10+158h] - vec) >> 3
A: j = r13+15: if j < n && (p = vec[j]): prefetcht0 [p+08h], [p+88h], [p+C8h]
B: j = r13+10: if j < n && (p = vec[j]) && [p+A5h]: prefetcht0 [[p+88h]], [[p+C8h]], [[p+90h]]
C: j = r13+5 : if j < n && (p = vec[j]) && [p+A5h] && (c = [p+C8h]) && (a = [p+88h]):
       for k < min([c], 4): if (q = a[k]): prefetcht0 [q+62h], [q+88h], [q+B8h]
pop r8; pop rdx
stock: mov rcx,[r10+150h]; jmp D8187
```
Distances are in vector elements of this job's own residue class (stride 5), so a job only ever looks at colliders it
processes itself. The sweep in `offline` (A/B/C ∈ {10,15,20,25}/{10,15}/{5,10}, K ∈ {2,4,8}) put (15,10,5,4) at the
top (-27 %, cold); K and A=20 are within noise of it.

**Why it is the same result.**
- Registers: the cave writes rax, rcx, rdx, r8, rsp and the flags only (decoded census in `offline`); rdx/r8 are
  pushed/popped; rcx gets the original instruction's value; **rax, rcx and all status flags are proven dead at D8180**
  (path walker: D8180 writes rcx, D8187 writes rax, D818E `sub` writes the flags before any read on every path).
- Memory: no store except the two pushes below rsp. Every load is one the stock loop performs later for the same
  collider under the same guards (index < size: D8195; non-null: D81A2; +A5h: D81AB; [+C8h]/[+88h] non-null and part
  index < count: D83B0..D83E7); `prefetcht0` never faults. A collider of this residue class is only touched by this
  job, and nothing in the loop writes the fields the cave reads (+88h/+90h/+C8h/part arrays).
- The site is one instruction; D8180 is the only instruction start shared by both encodings, so `safeWrite` can park
  threads only there; no direct branch lands in D8181..D8186 (whole-image sweep).

**Offline proof (`dotnet fsi collprefetch_live.fsx offline`):**
- Real-code harness: the whole dump image (raw == RVA) is mapped RWX into the script's process, so D80B0 and its real
  callees **D8D90, D8740, D89E0, D8B70, 2D9EE0, 168E80, 3CD840, D6E9C** run unmodified. Stubs, identical in both runs:
  19CED0 (world lookup -> the synthetic object), D8B24 (TLS-guarded getter whose result D89E0 ignores), the type-2
  position function, int3 at 4B807C / 1FB3AD8 (never hit). The position table 3BE3780 is pointed at the copy.
- Synthetic worlds: owner std::maps (real MSVC `_Tree` layout, header = nil node), the flat owner array, the global
  owner map (`[4759FC0]+28h`), part maps, the world +1A68h index table, grid cells with points, colliders with 0-6
  parts, null entries/arrays, A5=0/A6=1 cases, parent transforms (+68h, doubles at +210h). Objects sit in random
  0x300-byte slots, arrays in a second arena.
- **300 worlds (0..3000 colliders), 5 jobs each: stock vs cave on vs cave switched off - 0 mismatches**, compared
  byte for byte over both arenas and all 175,063 emitted change records. (The 16 record bytes D80B0 never writes -
  +12h..13h, +19h..1Fh, +29h..2Fh, stale stack in stock too - are zeroed before the compare.)
- The cave alone, 2,000 executions with random registers: all 13 other GPRs unchanged, rcx = [r10+150h], no memory
  byte changed.
- **Timing on this PC** (all 5 jobs back to back on one thread, median):

| world | stock | CP | change |
|---|---|---|---|
| 60,000 colliders, cold (caches flushed) | 60.6 ms (1009 ns/collider) | 50.9 ms (849) | **-15.9 %** |
| 4,000 colliders, warm (L3-resident) | 2.42 ms (606) | 1.91 ms (476) | **-21.3 %** |
| 4,000 colliders, cold | 2.87 ms | 2.35 ms | **-18.0 %** |
| 20,000 colliders, cold, default distances | 18.7 ms | 13.7 ms | **-26.9 %** |

The synthetic world is harsher than the game in one way (every map node and record is a separate random slot, so
misses the cave cannot cover - owner map, D8D90 result, part-map records - are over-represented) and the absolute ns
are not the game's. The relative gain on the part the cave covers is what transfers. Expected in game: ~10-20 % of
the CollisionUpdate family.

**Expected saving.** PC: ~0.9 core × 10-20 % ≈ **0.1-0.18 core**. Ally: ~0.5 core × 10-20 % ≈ **0.05-0.1 core**
(LPDDR5 latency is higher than the desktop's DDR5, so the covered share should not be smaller). fps: none.

## 3. SPF: stage placeable prefetch (`stageprefetch_live.fsx`)

**The loop.** PlaceableVisibilityUpdate 567A48 -> **PlaceableVisibilityUpdateWorker DA830** on KIDSTS[s2:StageUpdate].
After two bit-vector passes it walks **every entry** of the stage placeable map `[r15+180h]` (16-byte
`{key, placeable*}`, count `[r15+188h]`; rbx = entry, r14 = end; DB8B6..DB9BC) and for each placeable runs
10DEB50, PlaceableDistanceLod **DBA70** (+70h..+98h, two sqrt), PlaceableFlag27Check **DA6E0** (+B8h, +109h, +C8h),
and PlaceableUpdateDetail **DBB90** (+B0h object) when `[r15+8]` and `+10Dh == 0`. The first touch of each placeable
is the cost: DB928 = 88-95 % of DA830 self (PC), DBA89/DBABD in DBA70, DA71F in DA6E0; on the Ally DA879 + DBA70 +
DBBE1 are the biggest StageUpdate items.

**Patch.** `DB8BF  48 8B 05 92 E5 4D 04` (`mov rax,[rel g_pSceneDrawCfg 45B9E58]`) -> `E9 <cave> 66 90`:
```
if (!en) goto stock
rsi = rbx + 4*16; if rsi < r14 && (p = [rsi+8]): prefetcht0 [p+10h], [p+70h], [p+98h], [p+C8h], [p+109h]
rsi = rbx + 2*16; if rsi < r14 && (p = [rsi+8]): prefetcht0 [[p+B0h]]
stock: rax = [g_pSceneDrawCfg]; jmp DB8C6
```
**Same result:** the cave writes no memory and only rax (the original value), rsi and the flags; **rsi is written at
DB8C6 and the flags at DB8CA before any read** (walker). Loads: map entries strictly below r14 (the loop's own bound)
and the +B0h field inside a placeable the loop visits two iterations later; prefetches never fault. No branch lands
in DB8C0..DB8C5. Native transparency test (dump image mapped, exit stub at the copy's DB8C6): **30,000 executions,
300 random maps with null placeables / null +B0h, 10 % switched off: 0 differences** in 13 GPRs, rax and 4 MB of
memory. There is no real-code timing for this loop (DA830's callees pull in
too much engine state); the estimate rests on the CP measurement of the same miss pattern: 15-25 % of the loop,
**PC ≈0.02-0.04 core, Ally ≈0.04-0.07 core**.

Note for later: the loop visits every placeable of the stage every frame, including far ones. Its outputs (+109h LOD
byte, +10Bh, the DBB90 updates) are per-frame state, so skipping far ones would change behaviour; not proposed.

## 4. LK: locked "atomic loads" -> plain loads (`lockload_live.fsx`)

`lk_census.fsx` decodes every hot worker function (all functions with ≥0.002 core self in the PC
profiles) and lists each `lock cmpxchg [m],r` whose source register **and** eax/rax are provably zero at the site
(forward dataflow from the function entry): 60 such loads. J1/M2 already cover 13 of them. With
eax = src = 0 the instruction either finds 0 (writes the same 0 back, ZF=1, eax = 0 = [m]) or finds v ≠ 0 (no
store, eax = v): **eax = [m] and memory unchanged in both cases**. A plain `mov` gives the same eax without taking the
line exclusive and without the full fence.

Each row is checked by `offline`:
1. original bytes = dump, decoded `lock cmpxchg [m],r`; new = `mov eax/rax,[same m]` (+ `test` when the next flag
   reader is a ZF-only jcc, all other flags proven dead at both successors) + NOP fill, same length;
2. src and eax/rax zero (dataflow), flags dead or ZF-only;
3. **StoreLoad screen:** forward dataflow from the entry, a plain non-stack store (or any call) since the last locked
   instruction that reaches the site marks the row REVIEW. Only rows with a clean screen are in the default set, so no
   default row can be the second half of a Dekker pattern inside its function. (19 rows, e.g. the hot 1A83D3 in
   1A7BF0 and 4A539, are excluded because a call or a plain store can precede them; they are listed for a later look.)
4. no direct branch into any row; no overlap with any patches.json site or live-script site;
5. native test: original vs new bytes run on 400 random memory values each: rax, the memory word and (for ZF rows) ZF
   identical. **18 default rows, 0 differences.**

| group | rows (default) | evidence (hot RIP right after the row, PC) |
|---|---|---|
| LKO object update | 1A18C9, 1A1935 (1A188C ObjHandleResolve), 42877 (425B0) | 1A18CD: 19.6 %-thread-sum over 72 thread-captures (46-73 % of 1A188C) |
| LKM meshlet job / stream claims | 4A545, 4A55A, 4A708, 4A719 (MeshletRequestJob 4A2D0); 130096, 1301B4, 1301C5, 1301E4, 130225 (130014) | 4A563: 15.4; 13009E: 15.1; 4A54B: 3.4 |
| LKJ job system (beyond J1) | 40921, 40A14, 40CE4, 40D75 (40800); 41325, 41447 (411C0 steal scan) | 41447: 0.8, 40A14: 0.7, 40921: 0.5 |

Rows 41380 / 4149F (64-bit, ZF consumer) do not fit (`mov rax,[r14]` + `test rax,rax` = 6 > 5 bytes) and 40A55 overlaps
WK's site; all three are left alone. **Saving:** the stalls at these RIPs sum to ≈0.045 core on the PC; a plain load
on a shared line removes most of it: **≈0.03-0.04 core**, plus less cache-line ping-pong for the other threads.

## 5. PL: param-table lookups by linear scan (documented, not built)

121FC0 (17 KB, object update on KIDSTaskScheduler workers, from 1202B0) calls small getters that scan a param table
`T = [g+3D30h]` (g = the graphics context; CmdStreamEndFrame 418B94 copies +3CA8h -> +3D30h every frame):
`T = {u32 count; ...; Entry* e @+8}`, `Entry = {u32 flags @0, u32 hash @4, ..., void* value @18h}` (32 bytes), and
they return the first entry with `hash == H && (flags & 0F000000h) == 04000000h`:
ParamTableGetU32_D15EC139 **121E90** (default 7; hot loop 121EA3..121EBA), ParamTableGet_C4984B63 **1201F8**,
ParamTableFind_8B252323 **120160**, the inline scan for ED9DF832 at 1234A0 in 121FC0 and more getters around
121E6C..121FAA / 120120..1201F8. PC cost ≈0.13-0.15 core; on the Ally it is inside the 32 % object-update share
(121EB3/121EB5/1220BD/1218EC).
- A memo keyed by (T, T->count, T->e, H) -> first-match index is only "same result" if the entries of a table never
  change while its pointer and count stay the same, and that cannot be shown statically (the pointer flips per frame).
- **Next step (read-only, 1 minute in game):** sample `[[45BA588]+3D30h]` (or the g that 121FC0 uses) every 100 ms,
  record count, the hash list, whether H appears more than once, and whether any entry changes under an unchanged
  (T, count). If the tables are immutable per pointer, a 1-entry-per-call-site memo (validated by T, count and
  `e[idx].hash == H`) removes nearly all of the scan.

## 6. Smaller candidates (documented, not built)

- **SH, CollisionPartMapFind D8740** (lower_bound on 16-byte `{part*, record}`): for part k > 0 of a collider, start a
  galloping search at part k-1's slot (Patch B style; the result is identical for any sorted array, duplicates
  included). State would live in the 7 free bytes of D80B0's r9 home slot (`[rbp+80h..86h]`; only the byte at
  `[rbp+7Fh]` is used). Pays only if parts of one collider are adjacent in the map, which needs one live read.
  ≤0.04 core.
- **OP, owner-record prefetch:** the D8D90 result's +F8h/+10Fh miss (hot RIP D8291, 21 % of D80B0) could be prefetched
  from the CP cave by running D8D90 (pure, 99 bytes) for collider i+5. It doubles D8D90's own 0.07 core to hide
  ≈0.08 core of misses; try it in the CP harness before building.
- **D89E0 calls CollisionTlsStaticGet D8B24 and drops the result** (per part). Removing the call changes when the
  static is first initialised; not worth the risk for ~5 loads.
- **StageUpdate: PlaceableCollisionUpdate D8E00** (from 567A48, PC 0.044 core, hot loop DA0E8..DA0F5 = first touch of the entry): same prefetch
  technique; smaller than SPF.
- **Job granularity:** 319E00 pushes one 0x40-byte job per collision manager plus 5 phase jobs and joins twice. The
  manager count (`[45B9E58]+560h`) is unknown; nothing suggests the per-job overhead matters next to 0.5-0.9 core of
  object work.
- **EffectUpdate ×8 (Ally 36 %)**: real work spread over CE7E0 / 219E80 / C8958; ED and E1 already cover the
  structural waste. **MissionThread (Ally 14 %)**: 20B900 / 2866F0 / E266B0, spread, mostly condvar-blocked.
  **SynergisticDispatcher (39 %)**: D3D12 translation, real work (see worker-threads.md §2).

## 7. ASI integration (ready to paste)

patches.json rows (AOBs from `aob.fsx`, unique in the image; rel/RIP bytes wildcarded):
```
{ "id": "CP",  "group": "CP",  "rva": "0xD8180", "len": 7, "orig": "49 8B 8A 50 01 00 00", "kind": "jmp", "target": "caveCP",
  "pattern": "49 8B 8A 50 01 00 00 49 8B 82 58 01 00 00", "offset": 0 },
{ "id": "SPF", "group": "SPF", "rva": "0xDB8BF", "len": 7, "orig": "48 8B 05 92 E5 4D 04", "kind": "jmp", "target": "caveSPF",
  "pattern": "48 8B 05 ?? ?? ?? ?? 48 8B 73 08", "offset": 0 },
// deps
{ "group": "CP",  "rva": "0xD8187", "len": 1338, "fnv1a": "0x90CE4274", "what": "CollisionObjectUpdate loop D8187..D86C1 (r10 = mgr, r13 = i, stride 5 at D86AF)" },
{ "group": "SPF", "rva": "0xDB8C6", "len": 251,  "fnv1a": "0x7F023EF8", "what": "placeable map loop DB8C6..DB9C1 (rbx = entry, r14 = end, stride 10h)" },
```
LK rows (kind "bytes"; for the 130014 rows the anchor is 130096's pattern plus an offset, because their own bytes
repeat elsewhere in the image):

| id | rva | orig -> new | pattern (offset) |
|---|---|---|---|
| LKO_1A18C9 | 1A18C9 | F0 0F B1 1F -> 8B 07 66 90 | `F0 0F B1 1F A9 FF FF 7F 00` (0) |
| LKO_1A1935 | 1A1935 | F0 0F B1 1C 91 -> 8B 04 91 66 90 | `F0 0F B1 1C 91 41 85 C0 75 ?? 45 85 C8 74 ?? BA 00 00 00 08 48 8D 8E D8 02 00 00` (0) |
| LKO_42877 | 42877 | F0 41 0F B1 9E CC 09 00 00 -> 41 8B 86 CC 09 00 00 66 90 | `F0 41 0F B1 9E CC 09 00 00` (0) |
| LKM_4A545 | 4A545 | F0 44 0F B1 04 0A -> 8B 04 0A 0F 1F 00 | `F0 44 0F B1 04 0A A8 80` (0) |
| LKM_4A55A | 4A55A | F0 44 0F B1 81 B4 00 00 00 -> 8B 81 B4 00 00 00 0F 1F 00 | `F0 44 0F B1 81 B4 00 00 00 85 C0 0F 8F ?? ?? ?? ??` (0) |
| LKM_4A708 | 4A708 | F0 44 0F B1 04 0B -> 8B 04 0B 0F 1F 00 | `F0 44 0F B1 04 0B A8 80` (0) |
| LKM_4A719 | 4A719 | F0 44 0F B1 81 B4 00 00 00 -> 8B 81 B4 00 00 00 0F 1F 00 | `F0 44 0F B1 81 B4 00 00 00 85 C0 0F 8E ?? ?? ?? ??` (0) |
| LKM_130096 | 130096 | F0 44 0F B1 1C AE -> 8B 04 AE 85 C0 90 | `F0 44 0F B1 1C AE 8B D0 75 ??` (0) |
| LKM_1301B4 | 1301B4 | F0 45 0F B1 59 08 -> 41 8B 41 08 66 90 | 130096's pattern (+286) |
| LKM_1301C5 | 1301C5 | F0 45 0F B1 59 08 -> 41 8B 41 08 85 C0 | 130096's pattern (+303) |
| LKM_1301E4 | 1301E4 | F0 45 0F B1 59 0C -> 41 8B 41 0C 66 90 | 130096's pattern (+334) |
| LKM_130225 | 130225 | F0 43 0F B1 54 99 10 -> 43 8B 44 99 10 66 90 | 130096's pattern (+399) |
| LKJ_40921 | 40921 | F0 4D 0F B1 02 -> 49 8B 02 66 90 | `F0 4D 0F B1 02 48 8B C8` (0) |
| LKJ_40A14 | 40A14 | F0 0F B1 72 30 -> 8B 42 30 66 90 | `F0 0F B1 72 30 8B C8 8B 42 28` (0) |
| LKJ_40CE4 | 40CE4 | F0 41 0F B1 48 10 -> 41 8B 40 10 66 90 | `F0 41 0F B1 48 10 8B D0 0F 1F 40 00` (0) |
| LKJ_40D75 | 40D75 | F0 48 0F B1 77 30 -> 48 8B 47 30 66 90 | `F0 48 0F B1 77 30 48 8D 0C 10` (0) |
| LKJ_41325 | 41325 | F0 49 0F B1 1E -> 49 8B 06 66 90 | `F0 49 0F B1 1E 48 85 C0 74 ?? 48 63 C8 48 8B D0 4D 8D 14 8E` (0) |
| LKJ_41447 | 41447 | F0 49 0F B1 1E -> 49 8B 06 66 90 | `F0 49 0F B1 1E 48 85 C0 74 ?? 48 63 C8 48 8B D0 4D 8D 0C 8E` (0) |

MASM for `caves.asm` (same conventions as caveM1: `xx_resume` filled by dllmain before the site is written; the
trampoline must be a plain `jmp`, no stack use):
```
PUBLIC caveCP, cp_enable, cp_resume, caveSPF, spf_enable, spf_resume, spf_gworld
; data
cp_enable   DD 0
cp_resume   DQ 0                ; Nioh3+D8187
spf_enable  DD 0
spf_resume  DQ 0                ; Nioh3+DB8C6
spf_gworld  DQ 0                ; Nioh3+45B9E58 (address of g_pSceneDrawCfg)

; ---- CP: CollisionObjectUpdate loop-head prefetch. Site D8180 `mov rcx,[r10+150h]` -> jmp; nop2.
; r10 = mgr, r13 = i (stride 5). Writes rax/rcx/flags (dead at D8180); rdx/r8 saved. No stores.
caveCP PROC
    cmp     dword ptr [cp_enable], 0
    je      cpStock
    push    rdx
    push    r8
    mov     rcx, qword ptr [r10+150h]
    mov     rax, qword ptr [r10+158h]
    sub     rax, rcx
    sar     rax, 3
    lea     rdx, [r13+15]
    cmp     rdx, rax
    jae     cpB
    mov     rdx, qword ptr [rcx+rdx*8]
    test    rdx, rdx
    je      cpB
    prefetcht0 byte ptr [rdx+8]
    prefetcht0 byte ptr [rdx+88h]
    prefetcht0 byte ptr [rdx+0C8h]
cpB:
    lea     rdx, [r13+10]
    cmp     rdx, rax
    jae     cpC
    mov     rdx, qword ptr [rcx+rdx*8]
    test    rdx, rdx
    je      cpC
    cmp     byte ptr [rdx+0A5h], 0
    je      cpC
    mov     r8, qword ptr [rdx+88h]
    prefetcht0 byte ptr [r8]
    mov     r8, qword ptr [rdx+0C8h]
    prefetcht0 byte ptr [r8]
    mov     r8, qword ptr [rdx+90h]
    prefetcht0 byte ptr [r8]
cpC:
    lea     rdx, [r13+5]
    cmp     rdx, rax
    jae     cpDone
    mov     rdx, qword ptr [rcx+rdx*8]
    test    rdx, rdx
    je      cpDone
    cmp     byte ptr [rdx+0A5h], 0
    je      cpDone
    mov     r8, qword ptr [rdx+0C8h]
    test    r8, r8
    je      cpDone
    mov     eax, dword ptr [r8]
    mov     r8, qword ptr [rdx+88h]
    test    r8, r8
    je      cpDone
    cmp     eax, 4
    jbe     cpK
    mov     eax, 4
cpK:
    test    eax, eax
    je      cpDone
cpLoop:
    mov     rdx, qword ptr [r8]
    test    rdx, rdx
    je      cpNext
    prefetcht0 byte ptr [rdx+62h]
    prefetcht0 byte ptr [rdx+88h]
    prefetcht0 byte ptr [rdx+0B8h]
cpNext:
    add     r8, 8
    dec     eax
    jne     cpLoop
cpDone:
    pop     r8
    pop     rdx
cpStock:
    mov     rcx, qword ptr [r10+150h]
    jmp     qword ptr [cp_resume]
caveCP ENDP

; ---- SPF: placeable-map loop prefetch. Site DB8BF `mov rax,[rel g_pSceneDrawCfg]` -> jmp; nop2.
; rbx = entry (16 B), r14 = end. Writes rax (original value), rsi, flags (rsi/flags dead at DB8C6). No stores.
caveSPF PROC
    cmp     dword ptr [spf_enable], 0
    je      spfStock
    lea     rsi, [rbx+40h]
    cmp     rsi, r14
    jae     spf2
    mov     rsi, qword ptr [rsi+8]
    test    rsi, rsi
    je      spf2
    prefetcht0 byte ptr [rsi+10h]
    prefetcht0 byte ptr [rsi+70h]
    prefetcht0 byte ptr [rsi+98h]
    prefetcht0 byte ptr [rsi+0C8h]
    prefetcht0 byte ptr [rsi+109h]
spf2:
    lea     rsi, [rbx+20h]
    cmp     rsi, r14
    jae     spfStock
    mov     rsi, qword ptr [rsi+8]
    test    rsi, rsi
    je      spfStock
    mov     rsi, qword ptr [rsi+0B0h]
    prefetcht0 byte ptr [rsi]
spfStock:
    mov     rax, qword ptr [spf_gworld]
    mov     rax, qword ptr [rax]
    jmp     qword ptr [spf_resume]
caveSPF ENDP
```
Suggested ini: `[Experimental] CP = 0|1|2` and `SPF = 0|1|2` (2 = installed, switch off, like ED), `LK = 0|1`
(all three groups; or LKO/LKM/LKJ separately). Live menu switches for CP/SPF are just the enable dwords.

## 8. How to test (nothing here was run against the game)

```
dotnet fsi collprefetch_live.fsx offline        # ALL PASSED (+ timing on this PC); add "quick" for a short run
dotnet fsi stageprefetch_live.fsx offline       # ALL PASSED
dotnet fsi lockload_live.fsx offline            # ALL PASSED (18 default rows)
# --- game running, in a real level (combat or a dense area: collision/stage only cost where there is geometry) ---
dotnet fsi collprefetch_live.fsx dry ; dotnet fsi stageprefetch_live.fsx dry ; dotnet fsi lockload_live.fsx dry
$env:FT_ALLOW_WRITE=1
dotnet fsi collprefetch_live.fsx apply ; dotnet fsi stageprefetch_live.fsx apply ; dotnet fsi lockload_live.fsx apply
dotnet fsi cpuprof.fsx 20 cpuprof-wj-on.txt
dotnet fsi collprefetch_live.fsx off ; dotnet fsi stageprefetch_live.fsx off ; dotnet fsi lockload_live.fsx revert
dotnet fsi cpuprof.fsx 20 cpuprof-wj-off.txt
```
| signal (ON vs OFF) | expectation |
|---|---|
| KIDSTS[s9:CollisionUpdate] family CPU | -10 to -20 % (PC: ~-0.1 to -0.18 core) |
| D80B0 / D8B70 / 168E80 self; hot RIPs D81BD, D83E0, D8B7E | down; D8291 (owner miss, not covered) becomes the top RIP |
| KIDSTS[s2:StageUpdate]; DA830 hot RIP DB928, DBA70 | down |
| 1A188C self, hot RIP 1A18CD | roughly halves |
| PresentMon fps / p99 | unchanged (this is the no-regression check) |
| Ally (ASI SP) | `fn 0xD8152` + `0xD8B70` and `0xDA879` + `0xDBA70` sample counts down; package power at a fixed cap down |

## 9. Files (private dev repo, except symbols)

- `collprefetch_live.fsx` - CP: cave builder, static proofs, real-code harness (image mapping, synthetic
  worlds, equivalence, timing, distance sweep), live modes.
- `stageprefetch_live.fsx` - SPF: cave, static proofs, native transparency test, live modes.
- `lockload_live.fsx` - LK: row generator + proofs (dataflow, flags, StoreLoad screen, branch sweep, native
  row test), ASI rows, live modes.
- `wj_common.fsx` (shared walker / dataflow / sweep / .pdata helpers), `lk_census.fsx`, `hotlist.py`, `riphits.py`,
  `sp_families.py`, `aob.fsx`, `mk_symbols.py`, and the saved offline outputs.
- symbols/symbols.tsv: the 38 rows from this note (20 functions, 2 cave sites, 18 LK labels).
