# Nioh 3: effect and character/animation update - what runs, where, and what is waste

Static analysis of the unpacked exe dump (Steam v2.02, raw offset == RVA, base 0x7FF74F410000), Ghidra decompiles and the
existing sampling profiles (r5, ingame2, r6, r7). Nothing was attached, read live, written or applied. Date: 2026-10-06.
Raw decompiles and scratch stay in the private dev repo. Live script: `fx_live.fsx` (its `offline` run printed
**ALL PASSED**). The symbols named here are in symbols/symbols.tsv.

## TL;DR

1. **Neither effects nor character/animation update run on, or are joined by, the render main thread ([App]Game).** In all
   four captures [App]Game has 0 samples in any job-wait or condvar function (3D7CC8, 3E7CC, 403A0, B616A4). Its only
   wait is the Sleep(0) poll on the submit thread (W6, 7D9E90). Both systems live in the **game update on
   KIDSTaskScheduler0** (GameModeUpdateMain 39E4E4, 35-37 % of that thread), and Scheduler0 is what joins them.
   So today they cannot cost fps directly. They would only matter if Scheduler0 became the limiter. It is 79-83 %
   busy and spends ~13 % waiting on the main thread, i.e. about 2 ms of slack per frame at 60 fps. A heavy fight could
   use that up. There is no combat capture to check this.
2. **Invisible effects are simulated every frame.** EffectManagerUpdateInstances (15FC80) walks every live effect
   instance every frame. It sends the displayed ones to 9 "Disp" worker threads. Every **non-displayed** one (beyond its
   display distance, owner hidden or faded, ...) goes to the "NoDisp" worker, which runs the full KTGL effect update
   (timers, emitters, particle simulation) on it. "NoDisp" literally means not displayed. The per-frame work is
   proportional to **all loaded effect instances**, not to the visible ones.
3. **The pool sizes in settings.md (and the thread-pool dump it was based on) are off by one.** The live counts are **Disp = 9 threads,
   NoDisp = 1 thread**, not 4 and 9 (see §1.2). All invisible effects are therefore updated **serially on one thread**.
   That thread is kicked at the end of the game update and joined at the start of the next one. At the shrine it is at
   most ~7.6 % of a core (~1.3 ms/frame), so the join should not block there. It is the one effect-side path that could
   stall the update in a big, effect-dense area.
4. **MOTION_QUALITY does nothing in v2.02.** Its only reader (3971BC) feeds GetLodLevelCount 8/6/4. That value lands in
   a parameter block field (+0x3C) that nothing reads: only the first 0x20 bytes are copied on. The Nioh3Fix "Motion
   Quality" hook (sig #19 at 39722B) patches the same dead value. This matches the settings sweep (no difference) and
   predicts no difference in combat either. Keep it at HIGH; it costs nothing.
5. **VISUAL_EFFECT only gates spawning.** When it is LIMITED, EffectCreate (4CE90C) refuses effects whose definition has
   flag 0x800000 ("optional"). It does not size the worker pools (settings.md is wrong there). That explains "no
   difference at an idle shrine" (nothing is spawning). In combat LIMITED is the real effect-CPU and effect-GPU knob.

| rank | id | candidate | where | cost now | est. saving | patch | risk |
|---|---|---|---|---|---|---|---|
| 1 | **VE** | VISUAL_EFFECT = LIMITED in fights (menu setting, no patch) | EffectCreate 4CE969 | combat only (unmeasured) | fewer optional effects: Disp/NoDisp update + particle GPU | config | visual: optional effects missing |
| 2 | **E1** | Update invisible (NoDisp) effects every N-th pass with dt*N | 160B83 (call 73A6F8) + 7E82B4 (pass counter) | NoDisp thread ≤ 0.076 core at r5 | ≤ (1-1/N) of that, so **~0.02-0.04 core** at the shrine. More in effect-dense areas, and it halves the serial work that the next update joins | `fx_live.fsx` (cave, verified offline) | low: only invisible effects; ≤ (N-1)·dt clock lag once per invisible-to-visible transition |
| 3 | E2 | Give the NoDisp pool more threads (pool 57 count 1 -> 2-4) | record 57 +0xC = 0x4B72834, read once by EffectThreadManagerInit 20A5BD0 | as E1 | no CPU saving; parallelises the serial invisible-effect pass (stall insurance) | boot-time byte, pool_patch-style; **not built** | low-medium: boot timing |
| - | - | MOTION_QUALITY | 3971BC -> 396F54 +0x3C | 0 | 0 | none (dead) | - |
| - | - | Disp workers (visible effects, 9 threads, ~0.45 core) | 3D83BC / 3D9F64 | real work | 0 | none | - |

**Honest bottom line.** No candidate here moves fps today: the main thread never waits on this work. The safe,
verified patch (E1) saves a few percent of one worker at the shrine. Its real value is as insurance against the
one-thread invisible-effect pass ever gating Scheduler0. The meaningful effect lever in combat is the existing
**VISUAL_EFFECT** menu setting.

---

## 1. Effects

### 1.1 Per-frame flow (all on KIDSTaskScheduler0 unless noted)
```
GameModeUpdateMain 39E4E4                       (5651D0 <- frame step 5ACF80 ... ; 35-37 % of Scheduler0 incl)
  EffectNoDispJoinIfPending 91142C              join LAST frame's NoDisp worker (mgr+0x49)
  ... ~60 subsystem updates (CharacterUpdateDispatchIfActive 396E38, physics, stage, ...)
  EffectManagerFrameUpdate 39E910
    EffectThreadListsReset 7E82B4               end = begin for all Disp/NoDisp lists
    pass params 8BBF7C                          dt (= frame step, in 60ths), parallel flag (+0x10)
    EffectManagerUpdateInstances 15FC80         walk EVERY instance (std::map), see below
    EffectDispKick 3D6F14                       wake the 9 Disp workers; mgr+0x48 = 1
  ...
GameModePostUpdate 565F64
  EffectDispJoinIfPending 56614C                join the Disp workers (they overlapped the rest of the update)
  EffectNoDispKick 5660F0                       wake the NoDisp worker; mgr+0x49 = 1 -> joined at the next 39E4E4
```
EffectManagerUpdateInstances (5 KB), per instance `e` (dt = step/60):
- **Removal:** if `+0xAB` is set, or `+0xA9 == 0` with an owner handle, or the `+0xA6` lifetime runs out -> erase. This
  is the "finished" path.
- **Visibility ("disp", local_378):** false if the owner is dead or hidden (several owner types: character `[+0x1E0]`
  fade `[+0xB3B0] <= 0.1`, state `+0x8A8`, ...), if the distance to the camera ≥ the instance's display distance
  `e+0xC0`, if the height delta ≥ `e+0x194`, and so on. For character-owned instances, definition flag 0x4000000
  forces it visible.
- **Parallel mode** (pass param +0x10):
  - not displayed, and `sys+0xCC` bit 8 clear -> **EffectNoDispEnqueue 73A6F8** onto the NoDisp list
    (round robin % [mgr+0x7004]). This is the only call, at 160B83.
  - displayed -> push onto the Disp list `[mgr+0x3500 + i*0x18]` (round robin % [mgr+0x7000]) at 160FC0.
    Definition flag 0x2000000 adds an attach transform.
- **Not parallel:** inline EffectSystemUpdate(sys, dt).

### 1.2 Worker groups and joins

**Pool table.** Each record of `g_WorkerPoolTable` (0x4B722D0) is `{+0 name*, +8 prio, +0xC count u8,
+0x10 weight}`. The earlier thread-pool dump printed each count with the next record's name. Read straight from the
exe dump, the pairs are:

| pool | name | count |
|---|---|---|
| 3 | KIDSTaskScheduler | 7 |
| 4 | KIDS_RESOURCE | 1 |
| 5 | ResourceLoad | 4 |
| 6 | StageUpdate | 8 |
| 7 | CharacterUpdate | 8 |
| 10 | CollisionUpdate | 9 |
| 11 | StgGdtUpd | 4 |
| 12 | PhysUpdate | 2 |
| 56 | [App]EffectUpdateDispThread | **9** |
| 57 | [App]EffectUpdateNoDispThread | **1** |

The r5 thread list agrees on every row:
- KIDSTaskScheduler0-6 = 7 threads;
- s2:StageUpdate0-7 = 8;
- s10:StgGdtUpd0-3 = 4;
- PhysUpdate0-1 = 2;
- `[App]EffectUpdateThread` × 10 = 9 + 1.

Consequences for other notes:
- **settings.md** §2 / §4.3 ("EffectUpdateDisp (4) and EffectUpdateNoDisp (9)", "CollisionUpdate 3", "StgGdtUpd 9",
  "StageUpdate 4") is wrong.
- **pool_patch.fsx (P12).** Its comment says the ≤12-CPU penalty hits pool 3 "KIDS_RESOURCE" and pool 5
  "StageUpdate". By record index, pool 3 is **KIDSTaskScheduler** (7, or 10 without the penalty) and pool 5 is
  **ResourceLoad** (4, or 7). The patch bytes are unaffected; only the labels are wrong. This is worth re-reading for
  anyone reasoning about P12's effect.

EffectThreadManagerInit (20A5BD0):
- Builds the Disp group at `mgr+0` (count from pool 0x38 = 56) and the NoDisp group at `mgr+0x3800` (pool 0x39 = 57).
- Workers are 0x1A8 bytes: start event +8, done event +0xA8, list pointer +0x1A0.
- Every worker is a thread named `[App]EffectUpdateThread` (EffectUpdateThreadStart 20AC5D8).
- Thread loop (1A7A774): `KtglEventWait(start); vtable+0x10 (EffectWorkerRunList); KtglEventSet(done)`.
- The joins are KtglEventWait (3D7CC8, a condvar wait with INFINITE timeout) on each done event, on Scheduler0:
  - EffectDispJoin 3D6F60, from 56614C;
  - EffectNoDispJoin 911454, from 91142C.

### 1.3 What a worker does
EffectWorkerRunList (3D83BC). For each 0x30-byte entry `{sys, obj, dt, attachPos}`:
- **Attached, with a non-identity attach position (Disp only):** `setPos(pos)`, `EffectSystemUpdate(sys, dt/2)`,
  `setPos(identity)`, `EffectSystemUpdate(sys, dt/2)`. This is sub-stepped emission for moving emitters, a quality
  feature rather than waste.
- **Otherwise (all NoDisp entries):** one `EffectSystemUpdate(sys, dt)`.

EffectSystemUpdate (3D9F64):
- For every track (count `+0xC8`, stride 0xF0): if active (bit0), advance its time; once it is started (bit1), update
  every child via vtable+0x40. The default child update is EffectEmitterUpdate C7A7C: fade, release when faded,
  bindings, and particle groups via the type switch at C8498.
- Cost scales with tracks × children per instance, whether or not anyone can see it.

### 1.4 Measured cost (r5, idle shrine, 20 s)
- 10 `[App]EffectUpdateThread`s with CPU 7.6, 5.8, 5.5, 5.5, 5.3, 5.2, 5.2, 5.1, 4.1, 1.1 % = **0.50 core**.
  About 96 % of their samples are in the condvar wait; the rest is 3D83BC -> 3D9F64 -> C7A7C -> C8498.
- Nine of them are Disp (visible effects). The NoDisp thread is one of the ten: **≤ 7.6 %** of a core.
  The profile cannot say which one.
- 15FC80 itself does not reach Scheduler0's top-30 self list (< 0.5 %), so the walk over all instances is cheap at
  this spot.
- "Finished but not freed" cannot be settled statically. The removal paths exist (§1.1, and fade-release in C7A7C).
  `fx_live.fsx status` prints the number of invisible instances per pass, so a count that only grows over a session
  would expose a leak.

## 2. VISUAL_EFFECT (setting 29, 0x45BAFA4)
Readers of the setting:
- **EffectCreate 4CE90C** (23 callers, the generic effect spawn), at 4CE969:
  `if (VISUAL_EFFECT == LIMITED && def(id)->flags(+0xC) & 0x800000) return null handle`.
  This is the only functional reader.
- 607F78 (menu), 1FE59F1 (save), 12A4183 / 281F60C (writers).
- 1FE2CC8, 100F0A0 and 281F204: getter thunks with no code or data references.

So LIMITED removes "optional" effects at spawn time. It does not change pool sizes, rates or distances. The visual
impact depends on which definitions carry 0x800000; the effect-definition table (5713D8 lookup) was not dumped.
**A/B test in a fight**: VISUAL_EFFECT NORMAL vs LIMITED, with `cpuprof.fsx` (EffectUpdateThread family + Scheduler0
busy) and PresentMon (CPU busy p95).

## 3. Characters / animation

### 3.1 MOTION_QUALITY (setting 30, 0x45BAFA8) is a no-op
- **Readers of the setting:** GetLodTier 3971BC (HIGH 0, STANDARD 1, LIGHT 2), plus menu 606CF0, save
  1FE5A0D, writers 12A2E58 / 281F5B4, and the unreferenced getters 1FE1348 / 281F20C / 204510C.
- **Users of the tier:**
  - GetLodLevelCount 397218 -> 8/6/4. Nioh3Fix sig #19 = the `mov edx,8` at 39722B.
  - IsMotionQualityLightPlus1 3971DC.
  - Both are called only by MotionLodParamsBuild 396F54, which writes them to `+0x3C` and `+0x50` of a 0xE0-byte
    stack block.
- **What happens to the block:** it is passed to CharacterUpdateBalanceAndRun 399C1C. That function hands it only to
  31A86C / 31A2C0, and **both copy just bytes 0..0x1F** (dt, raw dt, camera position) into g_CharacterUpdateParams
  (0x44BB9F0, readers 84554 / 406D10 / 31A940 / 31A1DC). Nothing else gets the pointer. 0x44BBA2C has no references.
- **Result:** both MOTION_QUALITY-derived fields are dead stores. The setting changes nothing in v2.02, and lowering
  it buys nothing. settings.md §6 "MOTION_QUALITY HIGH -> STANDARD (CPU relief)" should be dropped.
  Nioh3Fix's sig #20 (39A86C) is unrelated: it sits in the render-settings setup 39A39C (Menu Framegen).

### 3.2 How characters are updated
- **Dispatch.** GameModeUpdateMain -> CharacterUpdateDispatchIfActive 396E38 -> CharacterUpdateDispatch 3972C4 ->
  CharacterUpdateBalanceAndRun 399C1C.
  - It takes every actor with `+0x7C4 == 0` and places it in the least-loaded of n buckets, by cost fields
    `+0xB030/34/38`.
  - It dispatches jobs (31A86C, 31A2C0) and **joins them on Scheduler0**: CharacterUpdateJoin 3963A8 ->
    job-counter wait 3E7CC (help-while-waiting).
  - **There is no frustum or visibility test at this level.** Every active actor is updated each frame.
- **Distance-based reductions that do exist:**
  - UpdateCharacterLodDistance 133D18: camera distance `+0xAE18`, draw distance, active flag `+0xAE14`.
  - CharacterActivationUpdate 42F3AC: an inactive character -> `+0x8A4 = 2` and a reduced path.
  - CharacterDistanceLodState 42E90C: `+0xB124 = 2 / 3` (near / far) at 18 m / 20 m with hysteresis. This feeds
    CharacterModelLodApply 42EA40 ("far" flags to the model and the second body).
  - CharacterAttachmentUpdate 5BD8E0: skipped beyond 18 m.
  - CharacterFarCullFlag 876EB8: per-model distance flag.

  All of these are **distance-based, not view-based**: a character behind the camera within ~18 m gets the full
  update. That is reasonable for a melee game, since such enemies can still hit you.
- **Not established:**
  - Whether skinning runs on the CPU at all. No skinning hot spot appears in any capture (likely GPU or compute).
  - The cloth LOD (`MotorCharacterSetting.Model.ClothModulesLODSimulateMode`, not inspected).
- **Cost in the captures:** negligible. The CharacterUpdate family (8 threads) was 0.095 core in r5, a shrine with
  no enemies. There is **no combat capture**, so the combat cost is unknown.

## 4. Data-level knobs (system view DB 0x62550860)

| field | value | what it does | visual impact |
|---|---|---|---|
| EffectDisplaylistMaxPrimNum | 7600 | cap on effect primitives in the effect display list (render side, capacity) | effects/particles beyond the cap are not drawn in heavy scenes; **no effect on the update cost** (§1). The second view uses 3800 |
| EffectDisplaylistMaxGPUEffectObjectNum | 1024 | cap on GPU-simulated effect objects | GPU effects dropped beyond the cap |
| EffectDisplaylistMaxDecalParticleNum | 256 | decal-particle cap | fewer blood/scorch decals |
| EffectDisplaylistDepthResolution | 4096 | effect sort depth buckets | sorting artefacts if lowered |
| OnCreateEffectDlNumPerView / ViewFlag | 10 / 63 | display lists created per view at view creation | capacity only |

All are capacities or caps read at view creation. Lowering them trades dropped effects for GPU and memory; none of
them reduces the CPU update of effects. The CPU-side "data knob" is **pool 57's count** (E2, §5). The definition
flag 0x800000 that LIMITED keys on lives in the effect definition table and was not mapped.

## 5. E1 patch: throttle the invisible-effect update (`fx_live.fsx`)

**Site A** 7E82B4 `33 D2 44 8B CA` -> `jmp caveA`.
- This is the entry of EffectThreadListsReset. It is called once per update pass (39E9B0) and by the clear-all
  20AAA34.
- caveA does `++pass; xor edx,edx; mov r9d,edx; jmp 7E82B9`. Registers and flags at 7E82B9 are identical to stock;
  this is checked natively, including the flags of `xor`.

**Site B** 160B83 `E8 70 9B 5D 00` (the only call of EffectNoDispEnqueue) -> `call caveB`:
```
if (!en) jmp 73A6F8                                        ; switch off = stock
ph = (u32)((sys>>7) ^ (sys>>13)) + pass                    ; fixed per-instance phase
if (ph & (N-1)) { ++skips; ret }                           ; not queued this pass
xmm3 *= N; ++updates; jmp 73A6F8                           ; queued with N passes of dt; returns to 160B88
```

**Checks.** `offline` verifies:
- Both sites match the dump, and there is no overlap with any other patch set (cpu_caves, fg_cave, ft_vrr,
  mainthread, W1/W2, meshlet, pool_patch, workers_live M1/M2/J1).
- No direct branch in .text targets the patched bytes.
- 73A6F8 has exactly one caller (160B83).
- rax, r9, r10 and the flags are dead at 160B88 (proven by the path walker). xmm3 is volatile across a call (MS x64 ABI).
- The **native test**, which runs the cave in its own process against a recording stub:
  - with the switch off, 2000 calls are forwarded with identical arguments;
  - with N = 1, 2, 4 and 8, over 36,329 instance windows each instance is queued **exactly once per N passes with
    dt·N**, with rcx, edx and r8 intact.
  - 0 mismatches.

**Semantics vs stock** (invisible effects only):
- An invisible effect's state is up to N-1 passes old.
- On an invisible-to-visible transition its clock can lag by up to (N-1)·dt, once (16 ms at N=2 / 60 fps).
- It is simulated with dt·N steps.
- Visible effects, the Disp workers and instances with the engine's own `sys+0xCC` bit 8 ("always update") are
  untouched.
- If an effect flickers between visible and invisible every frame, it can lose up to (N-1)/N of its time while
  flickering. Display-distance flicker would need the camera to sit exactly on the boundary.

**Expected.**
- The NoDisp thread's CPU drops by ~(1-1/N); that is ≤ 0.04 core at the shrine with N=2.
- The serial pass that the next game update joins is about half as long.
- No fps change is expected; PresentMon is only there to confirm nothing regressed.

**Commands:**
```
dotnet fsi fx_live.fsx offline                         # no game access (ALL PASSED)
dotnet fsi fx_live.fsx dry                             # read-only on the game
$env:FT_ALLOW_WRITE=1; dotnet fsi fx_live.fsx apply    # one cave per process, N=2
dotnet fsi fx_live.fsx status                          # passes/s, invisible instances queued/skipped per second
dotnet fsi fx_live.fsx rate 4 ; ... off ; ... on       # live A/B (data writes only)
dotnet fsi fx_live.fsx revert
```
**What to look at.**
- Run `status` in an open area and in a fight. "~N invisible instances per pass" is the first hard number for how
  many invisible effects the stage keeps alive.
- In `cpuprof.fsx`, the `[App]EffectUpdateThread` whose CPU halves is the NoDisp worker.
- Scheduler0 samples in KtglEventWait under 39E4E4 (via 91142C) would show the join actually waiting. The current
  profiler only keeps the top 30 inclusive frames, so that needs a targeted capture.

**E2 (not built).**
- Set pool 57's count byte 0x4B72834 from 1 to 2-4. The arrays at mgr+0x3800 / +0x6D00 are sized for 32 workers.
- The byte must be written after the pool table is sized at boot (SizeWorkerPools 1FE2FD4) and before
  EffectThreadManagerInit 20A5BD0 runs. That is a pool_patch-style boot race, so it was not attempted.

## 6. Main-thread joins (question 3): answer
- **[App]Game.** No samples in KtglEventWait, the job-counter wait (3E7CC / 403A0 / 40800), B616A4 or
  WaitForSingleObject in r5, ingame2, r6-A1 or r7-A1. Its only wait is ZwDelayExecution (W6, submit thread).
  Static paths agree: every effect and character join (3D6F60, 911454, 3963A8) is reached only from
  GameModeUpdateMain / GameModePostUpdate on KIDSTaskScheduler0.
- **KIDSTaskScheduler0** (the game update):
  - It joins the Disp effect workers inside its frame (56614C), the NoDisp worker at the start of the next frame
    (91142C), and the character jobs (3963A8 -> 3E7CC, which runs jobs while waiting).
  - Its 13 % condvar time cannot be split between these joins and the frame handshakes (5AA5DB … 5ACDC2) from the
    existing data.
  - The effect workers are ≤ 8 % busy each, so the effect joins should be short at the shrine.
