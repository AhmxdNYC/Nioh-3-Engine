# Game logic: MissionThread, GameUpdateTick, per-tick lookups (2026-10-08)

Static analysis and the existing profiles only: the 12 PC cpuprof captures, the Ally thread capture (v6, W6 on, stock
environment) and the Ally SP log v71 (reports 7-145). Nothing was attached, applied or patched. The symbols named here
are in symbols/symbols.tsv. Out of scope here: collision, loading, D3D12 call sites, streaming/anim, and TG itself
(cpu-hotspots-ally.md §4).

## TL;DR
- **[App]MissionThread is ~14 % of one core on both machines** (PC 14-18 % CPU, 86 % of its samples are waits; Ally
  13.6 % CPU, 13.3 % user, 0.2 % sys, 143 context switches/s = ~3.2 per tick at 44.6 ticks/s).
  - Per tick that is ~1.3 ms on the PC (at ~110 ticks/s) and ~3.0 ms on the Ally.
  - The Ally is 2.3x slower per tick. That fits the main finding: the work is **memory-latency bound**.
- **It is a fork/join helper of the game tick.**
  - `GameUpdateTick` 2338B0 kicks it as its last call (`MissionThreadKick` 233BB4 sets the start event 47098F8 of
    g_MissionThreadObj 47098F0). The next logic frame joins it first: `MissionThreadJoin` 5AAED4, the first call of
    FrameEarlyUpdate 5AC9EC, waits on the done event 4709998.
  - So the mission work overlaps only the rest of the frame graph and FrameEnd. The sampling profiles here show no
    join wait, but a later join-timing run found the logic frame waiting on it in a logic-bound PC scene: there it is
    on the logic critical path. When the GPU is the limit it does not matter.
  - The kick/join pair inside GameUpdateTick, `GameModeDrawThreadKick` 233810 (start event 4709B68) and
    `GameModeDrawThreadJoin` 233888 (done event 4709C08), belongs to **[App]GameModeDrawThread** (object
    g_GameModeDrawThreadObj 4709B60), not to the MissionThread.
  - Making the mission work faster saves CPU and power, and shortens ticks only where the logic thread limits fps.
- **Where its time goes (PC, % of one core):**
  - task-list walks: 5.0 (proximity pass 3.3, state pass 1.7)
  - script VM: 3.2
  - mission object walk: 3.0
  - other managers: ~2
  - The hottest instructions are all **cache-miss loads of the next task/object** (the task's +D9h / +97h fields, the
    object header, the object's +158h flag).
- **Same-behaviour waste is small.**
  - The one clean design is **GL1: software prefetch in the three list walks**. It is semantically a no-op and is estimated to save
    ~2.5-3 % of a core (~20 % of MissionThread). On the Ally that is ~0.03 core, about 0.05-0.1 W.
  - Everything else is ≤1 % of a thread, or needs a writer census before a cache could be proven identical.
- **AI, behaviour, nav and spawn work is not a cost centre in any captured scene.** The engine already does the
  far-object work:
  - far objects (≥10000 units, ~100 m) are despawned (`MissionFarObjectDespawnPass` 4AB64C) or flagged (2866F0);
  - proximity checks are round-robin staggered per group (E266B0);
  - characters' update is ≤0.9 % of a StgGdtUpd thread, and the Ally shows `NavOpenListPopExpand` at 0.02 % of the
    [App] group.
- **The real lever for this code is the tick rate (TG): every item below is per tick.** Reducing ticks or sub-rating
  updates changes behaviour, so it is out of scope here.

## 1. Thread map (per game tick)
```
logic frame (KIDSTaskScheduler0)
  FrameEarlyUpdate 5AC9EC -> MissionThreadJoin 5AAED4: waits for the previous tick's MissionThread (done event 4709998)
  ... frame graph: GameModeUpdateMain 39E4E4 -> MissionThreadArm 39E104 (dt params, g_MissionArmed 45C6E83, gate 4709AB0)
KIDS frame graph -> GameUpdateTaskExecute 907610 (vtable 4035DD0) -> GameUpdateTickRun 458A4 -> GameUpdateTick 2338B0
  TickDeferredCallsFlush 231EF8, TickFirstFrameInit 23203C (once), TickDelayedNotify 233348, TickSetNotEventSceneFlag 233440,
  TickListenersNotify 233460/233480 (vfunc 48h/50h), 5AB780, 2334F4, TickSlotsFlush 23351C/23353C, TickInputBlockCountdown 5AB868,
  233758, 23377C (G+8ECh countdown), TickEventSceneResetCheck 2337A0,
  GameModeDrawThreadKick 233810 (start event 4709B68 of [App]GameModeDrawThread),
  RenderFrameSettingsApply 39B570 (-> ApplyGraphicsSettings 39A39C), 23383C,
  GameModeDrawThreadJoin 233888 (done event 4709C08),
  GiLightestFlagApply 39B808, ApplyMeshletLodScales 2353C0, 5972C4,
  23419C, IsEventScene/G+91Ch, 28AC720, TickResetFrameGlobals 233E54,
  26F36EC (first tick), 204D2B0, TickWaitOptionalEvent 233BE4,
  MissionThreadKick 233BB4 (last call: start event 47098F8), G+610 += 1 ---.
                                                                          |
[App]MissionThread (g_MissionThreadObj 47098F0), MissionThreadEntry E5E060 <'
  waits 47098F8 -> vfunc+10h = MissionThreadRunFrame E63140:
    MissionThreadPreUpdate E63350   (4.6 %)
      MissionTaskListProximityPass 89A48C -> E266B0 (3.3 %)
      MissionFarObjectDespawnPass 4AB64C (0.1 %)
    wait gate event 4709AB0 (armed by MissionThreadArm 39E104; other refs 2E33A4/27F20/18F9830)
    MissionThreadUpdate E64700      (9.2 %)
      MissionManagerUpdate 379620   (5.8 %)
        MissionEntryScriptTick 37A434 (3.2) -> ScriptThreadSetTick 282320
          -> ScriptRunOneSlice 2824EC -> ScriptVmRunOpcodes 13A830 (2.1)
          SpawnTableUpdate 37AD78 -> SpawnTableUpdateByConditions 37A8D4 (0.2)
        MissionTaskListUpdate 89A328 (2.3): teleport test (>1000), E26890,
          E26AD0, MissionTaskListStateUpdate E26680 -> 2866F0 (1.7), E26260
      MissionThreadStep 20B900      (3.0): object set g_4759950, 2 passes
  sets done event 4709998, loops (joined by MissionThreadJoin 5AAED4 at the start of the next logic frame)
```
- The shares in brackets are % of one PC core: the mean of 12 captures, inclusive of the [App]MissionThread samples.
- `GameUpdateTick` itself and all its own callees are below every profile's top-30 (<0.3 % of any thread).
  - `RenderFrameSettingsApply` / `ApplyGraphicsSettings` re-apply the settings every tick. That is cheap, so there is
    nothing to win.
- `GameModePostUpdateEntities` 20B878 runs on **[App]GameModeDrawThread**, not on the MissionThread.

## 2. Ranking (same-behaviour candidates first)
Notes on the columns:
- PC % = % of one core (mean of 12 captures).
- Ally core % is scaled from the PC split onto the Ally's 13.6 % MissionThread CPU (the SP log agrees within its 0.01 %
  rounding); Scheduler0 items use the Ally SP kidsTS group.
- Watts assume ~2 W per busy core-equivalent: ~15 W CPU for ~7 busy cores (worker-power.md). This is rough.

| # | id | what | RVAs | PC % core | Ally % core / W | same-behaviour design | risk |
|---|---|---|---|---|---|---|---|
| 1 | **GL1** | 3 pointer-chasing walks per tick, stalled on misses: E266B0 (hot RIPs E266E6 39 % / E266F3 36 % = loads t+D9h, t+97h), 2866F0 (286710 65 % = t+D9h again), 20B900 (20B9CE 69 % = obj header, 20BB9B 21 % = obj+158h), plus dependent misses in 20A298 (entity+11Eh) and 4AB6E4 (4AB6FF 86 %) | E266B0, E26680/2866F0, 20B900 loops 20B9BC..20BA3A / 20BB7F..20BBA6, 20A298, 4AB64C | self ~6.5, of which ~70 % is miss stall | ~6 / ~0.12 W in total; saving ~2.5-3 / **~0.05-0.1 W** | `prefetcht0` of entry i+2's task (the vectors are 16-byte {?, task*} arrays, so the pointer is available without a miss) at +80h/+C0h, and of the next object header. Bounds-checked against `last`. No architectural effect | low |
| 2 | GL2 | O(n²) pause gate: `ScreenHandlerTableUpdate` F2E68 walks 237 handler slots, and per slot `ScreenHandlerUpdateGated` F317C → `GamePausedGate` F32D8 → `PauseSourcesAnyActive` F33A0 re-scans all 237 slots (vfunc +170h each) | F2E68, F317C, F32D8, F33A0 (hot loop F33C6..F340A) | Scheduler0 0.67 | ~0.6 of Scheduler0 (0.08 % of kidsTS) / <0.02 W | memo F33A0's result within one F2E68 pass; invalidate after any handler vcall that can mutate (+28h, +98h, +E8h, +118h, 3022C0). Identical as long as the query vfuncs (+48h, +F0h, +110h, +170h) have no side effects, which must be audited per handler class | medium |
| 3 | GL3 | `GlobalEntityFindById` 1363D4 (220 callers) does a deque binary search per call | 1363D4 → E32D90 | Sched0 0.96 + Mission 0.29 | ~0.6 of Scheduler0 / <0.02 W | one-entry last-hit cache keyed on id + a container generation. No generation counter is known: needs a census of the writers of [g_9C9E40]+20h/+38h/+40h | medium |
| 4 | GL4 | `GameContentIdIsAvailable` 284ED4 (FNV-1a hash per call) from the per-tick erase pass of `ProximityTriggerListUpdateA/B` 284B78 / 285DF0 | 284ED4 (hot loop 285006..285018) | Sched0 0.51 | <0.01 W | per-tick memo by id is only valid if game state g_4751848 cannot change mid-pass. Micro-fix that is bit-identical: 284B78 computes the same `sqrtf(dx²+dz²)` twice (BE0378 on identical inputs, 2nd for the +E764h test); reuse the first | low (micro) / medium (memo) |
| 5 | GL5 | `SaveItemTableGetCountById` 2FA4C4: linear search of 1500 x E8h records (≈1500 cache lines) per call, 78 callers (scripts, UI) | 2FA4C4 | Mission 0.22 (+UI) | <0.01 W | a memo is not identical without proof that ids are unique (stackable vs equipment records) or a writer census; leave | - |
| 6 | GL6 | `ItemBoxScanFlaggedEntries` F450C (2500 x F0h) polled each tick via `ItemConditionEvaluate` 555F44 kind 0 (caller 70A774) | F450C, 555F44, 605C9C/605EE4 | Sched0 0.28 (2.1 + 0.7 in ingame2) | ≈0 at the Ally's MAX spot | result cache with invalidation on writes to entry +18h (needs writer census) | medium |
| 7 | - | Script VM: compare-tree opcode dispatch (≈6 compares/op), `ScriptVmResolveVarRef` 0.31, natives 1.1 | 13A830, 13B590, 13B18C | 3.2 incl. | ~3 / ~0.06 W | a jump-table dispatch is identical but saves ≤0.1 %; the rest is real script work | - |
| 8 | - | `RollingAvgListUpdate` 12B440 | 12B440 | Sched0 1.09 | | already covered (mainthread-logic.md §4.2 RA; a running sum is not bit-identical) | - |

### GL1 design (built later as mod option GL1, 1.7.14, default off)
- **Measured:** the only listed measurement is the PC load route, where GL1 showed no change (docs/BENCHMARKS.md,
  section (c)). No gameplay A/B is listed; the savings below are estimates.
- **E266B0 loop head** at E266D1:
  - `cmp rbx,[rbp]` / `jz` / `mov rdi,[rbx+8]` (rbx = entry, rbp = &last).
  - The cave adds: `lea rax,[rbx+20h]; cmp rax,[rbp]; jae skip; mov rax,[rax+8]; prefetcht0 [rax+80h]; prefetcht0 [rax+0C0h]`.
  - It then replays the displaced instructions and jumps back.
  - t+97h sits in line +80h and t+D9h / t+DAh / t+E7h / t+F0h..FAh sit in line +C0h. The +48h / +50h fields are in the
    line at +40h; add that line if the cave is profiled with the 3-line version.
- **E26680 loop** (E26690..E266A2, rbx = entry, [rdi] = &last): the same cave around the call to 2866F0 (2866F0 reads +50h,
  +D9h..+EFh, so the same lines).
- **20B900**: the objects come from a segmented set (`MissionObjSetBegin` 725CA4 / `SegmentedRangeInit` 8FE4E8 /
  `SegmentedIteratorAdvance` 54AE40). There the next pointer *is* the miss.
  - Prefetch the node after next: one step of the segment walk ([iter] + 16-byte stride within the segment), and
    `prefetcht0 [next-0E0h]` and `[next+78h]` (flag +158h).
  - The second pass (20BB7F) restarts from the head after every removal, and in the common no-removal case it is a
    second full walk. Folding it into the first pass is **not** identical, because 6F8B80 / 6F9E50 / C76864 / 6F97F0 /
    D1D0C8 run in between and can set the flag. Prefetch only.
- **Proof plan** (the same method as the AABB cave in mainthread-logic.md §3):
  - native harness on the dump with synthetic task vectors;
  - check that the outputs (all task fields written: +F0h, +F8h, +F9h, +FAh) are byte-identical with and without the
    cave;
  - sweep for branches into the displaced bytes;
  - check that the prefetch addresses never leave [first,last) for the pointer load (the prefetch itself cannot fault).
- **Expected:** −30..45 % of the walk self time. The PC estimate is −2.5..3 % of a core. The Ally gain is probably
  larger relative to the thread, because its per-tick time is 2.3x the PC's.
- **Validate** with `count` and `cpuprof`: E266B0 + 2866F0 + 20B900 self, and MissionThread CPU at a fixed spot.

## 3. Out of scope (would change behaviour)
- Fewer logic ticks (TG, cpu-hotspots-ally.md §4). This is the only change here that is worth whole cores.
- Sub-rating the proximity pass (E266B0 already staggers the "my turn" flag +F8h by group, but computes +F0h for every
  eligible task every tick).
- Skipping script slices while a script waits: the VM runs each active script once per tick until a yield/halt
  (+9Ch&2) and restarts it at the stream end (2E2A60). Skipping changes timing.
- Decimating far-enemy AI or spawn-table checks (37A8D4 re-evaluates every spawn condition every tick); the engine
  already despawns at ≥10000 units.
- Merging the two task walks or the two object passes: other work runs between them and can change the inputs.

## 4. Confidence and open items
- High: the thread split, the kick/join events (event layout 0xB8: mutex +0, cv +50h, flag +99h, auto-reset +98h;
  MissionThread object g_MissionThreadObj 47098F0; GameModeDrawThread object g_GameModeDrawThreadObj 4709B60, written
  in 28040 / E6B9A0), the hot RIPs and the loop bodies.
- Medium: the role names of 379620 / E63350 / 37A434 / F2E68 / 555F44 and the tick-callee names.
  - The gate event 4709AB0 is armed by MissionThreadArm 39E104; 2E33A4, 27F20 and 18F9830 also reference it.
- Task count: the proximity vector holds ~4,400 entries in idle PC gameplay (live count below), far more than the
  ~800-1000 a self-time estimate at ~100-160 ns per task suggested.
- The PC tick rate in the 12 captures is assumed ~110/s (mainthread-logic.md). The Ally number (44.6) is from the v71 log.

### GL1 live counts (2026-10-08, PC idle gameplay)
Proximity vector ([[0x45B9E40]]+0x48) holds 4380 entries (not 800-1000), constant over 20 s; second vector (+0x60) 314. Mission-object set count not obtained.
