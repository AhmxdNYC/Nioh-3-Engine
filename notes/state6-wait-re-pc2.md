# Sequencer state 6 (and 8) wait after a cross-world Fight now: what it is, how to pass it (pc2, static RE)

Read-only work: no game launch and no game-folder writes. Disassembly was done with capstone on `C:/nioh3-re/Nioh3.mem.bin` (flat image, RVA = file offset)
through `re/scripts_boss/spawn4943/n3dis.py`, with a byte scan for writers of `+0x138`. All addresses are RVAs. The bytes below were read from that dump.
Tags: **[S]** proven by reading the asm. **[L]** seen in a training.log. **[I]** inferred, not proven.
Mod source read: `C:/Users/User/projects/nioh3-merged/mod/Nioh3Training/src/training.c`. That worktree is checked out on **pc2/merged-near**, not merged-visible.
Builds on notes/seq-state-6-8-pc2-8682.md (sections 2-3: dispatcher, state 8, LoadIsLoadEnd gates). This note does not repeat those sections.

## 1. Control flow

**Sequencer state 6, 223C53C [S]**
- 223C546: `r8 = [45B9E40]` (ActorMgr).
- 223C553 `E8 C4 00 9E FF`: `call 1C1C61C`. 223C558 `test al,al`. 223C55A `74 4F`: `je 223C5AB` (stay in state 6).
- On pass, one of two placement paths runs:
  - if `[[ActorMgr]+0x48]+0x110 != 0`: `call 21A0E0C`;
  - else: `call A5F930([[47514D0]+8]+0x14)` (WarpPointFind / start-position placement).
- Then `call 3454EC(same id)`, and `ctl+4 = 7` / `[45BA21C] = 7` at 223C5A5.
- training.c line 1435 calls 223C55A a "jne". It is a **je**.

**1C1C61C(ActorMgr) [S].** This is a std::list walk:
- `head = [[ActorMgr]+0x10]`; for each node, `obj = [node+0x10]`;
- returns 0 at the first `obj+0x138 != 8` (1C1C62F `83 B9 38 01 00 00 08`, 1C1C636 `75 05`);
- returns 1 when the list ends.

**What the list holds: one entry per mission, each with a script VM.**
- [S] The same list is walked by 1C1C5C4 (state 0x10) and 1C204BC (state 0xF exit).
  - Both use `obj+8` as a script VM.
  - They call `23BD710(vm, "game_start_event")` (is that script function still running) or `94B04(vm, "game_start_event", obj)` (schedule that script function).
  - String 3A8A478 = "game_start_event", 3A8A490 = "mission_setup".
- [I] The objects are mission instances: `+0x120` is the mission id (compared with 0x1AB3 and 0xC12, passed to 1C08D00).

**Who sets +0x138: only A31A50, a per-mission setup state machine [S].**
- Its only caller is **A319B0**, at **A319EF** `E8 5C 00 00 00`.
- A319B0 is the **script binding `Mission::UpdateSetup`**: name/function pair at 3980F40/3980F48.
- So **the native sequencer never advances these nodes. Only the mission's own script does, by polling `Mission::UpdateSetup`.**
- UpdateSetup returns `A31A50() && [ActorMgr]+0x2678` to the script.
- The byte scan found no other write of 1..8 to `+0x138` on this class. 1C7611B is a constructor of another class that zeroes it.

| node state | does (A31A50) | leaves when |
|---|---|---|
| 0 | | `[stage+0x504] == -1` or `D3958(stage+0x540,[stage+0x504])` found -> 1 |
| 1 | 1C1CD50; requests the mission resource (`+0x128 -> +0x3C`) into `+0x18` | -> 2 at once |
| **2** | **WAIT:** `13335C([45B9E60])` (G5, any object with +0x2C8 == 0) **or** `28F307C()` **or** (`[ActorMgr]+0x84` and no player 0). The two flags are OR'd in r8b; the `jne A31CD8` (stay) is at **A31B76**. | on pass, once (when `+0x38 == 0`): 1C1E254, 724E84, 1C1E820, **PlacementBuild_EmakiFilter 1C1E470**, 1C1E3C8, 1C1E9FC, 1C08D00(id); `94B04(vm,"mission_setup",obj)` **starts the mission_setup script**; -> 3 (or 4 if not started and `[obj]+0x460 != 0`) |
| 3 | waits until `23BD710(vm,"mission_setup")` is false (the script function has finished) | -> 4 |
| 4 | | -> 5 |
| 5 | `7D2BCC != 2` -> 345608; 1C1E118 (mission 0xC12 only); `+0x38 = 1`; `1C19FA8(3)` | -> **8** |
| 7 | `call LoadIsLoadEnd` at A31AB0 | -> 8 |
| 8 | done (returns 1 if `+0x141 == 0`) | |

- `28F307C` = `[[45C3760]+0x260]` -> 2912C18, a **mutex-protected read of byte +0xD0** [S]. Its setter was not found: the only static writer found is the zeroing at 2910537 (constructor).
- [I] 45C3760 is the placement/field manager (PlacementSetGlobalDirty uses `+0x268`), so +0xD0 is likely a "placement work busy" flag set by a worker thread.

**Which wait held state 6 for 20.6 s in the G9-skip run (xworldprobe-20261016-114244) [L + I]**
- The mod's gate mask is logged on every change. From +27.9 s to +48.3 s it stayed **0x110** (G8 + G9).
  - G5 (bit 4) was passing the whole time.
  - G8 was **still failing when state 6 exited**: 0x110 at the 6 -> 7 line, 7 cells blocking.
  - So the holder was neither G5 nor G8, and therefore not a LoadIsLoadEnd call (node state 7 or a script `Load::IsLoadEnd`, binding 3B33B0).
- State 6 exited exactly when "placeable loads in flight" fell from 2817 to 37.
- The remaining candidates:
  - **28F307C in node state 2** (best fit: a cross-thread busy flag on the placement manager) [I];
  - or a wait inside the `mission_setup` script in node state 3 [I].
- The brief 0x114 just before the exit fits node state 2 passing: PlacementBuild creates new objects, so G5 is briefly not ready.
- Note: the existing h_aor hook (CrossWorldAreaObjSkipMs) passes G8 only in sequencer states 5 and 8 (training.c line 1403), never in 6. That is why bit 16 shows in state 6.

**State 8, 223C4A8 [S, see 8682 s.3].** It waits on `call LoadIsLoadEnd` at 223C4B5 (`E8 6A 6F 17 FE`). In state 8 the logged holders are G5 + G7 + G9 (0x10C) [L].
- G7 (2BE284) is armed in state 7 (`ActorMgr+0x2679 = 1`). It waits for character spawn slots, which only exist after the node-state-2 placement build.
- So state 8 is the second half of the same chain: missions build their placements -> characters spawn -> G5/G7 hold.

## 2. Why the 223C55A skip hung, and how to avoid it
**Facts:**
- [L] Run captures/rematch/flG-lgs3-20261015-183440: G9 skip plus `74 4F` -> NOP at 223C55A. Sequence 6 -> 7 -> 8 -> 9 -> B -> C -> F in ~140 ms; 0xF was entered at 18:36:29.779.
- [L] Every periodic log line stops between 30.217 and 30.449. These lines are written from the **game-thread pump** (`h_pump`, training.c ~3132), so the game thread stopped.
- [L] The harness then had to kill the process ("game did not exit in 45 s").
- [S] 0xF waits only on a timer: 30 dt60 units = 0.5 s, plus extras for the tip page / region 0xC0. Then it runs its exit code:
  - StopFlagSet(2) (unless 28A8E44);
  - 1C2998C or 1C29A88;
  - 59FF7C(0); 2704064(1);
  - **1C204BC(ActorMgr)**, which schedules `game_start_event` in every mission script.

**Conclusion:**
- The game thread **froze at the moment 0xF expired** (+0.44..0.67 s). It did not wait in 0xF.
- This **contradicts note 8682's guess** that dt stays 0. The exact site was NOT found statically [I].
- At that moment the forced skip had left every mission node short of 8 (probably in state 2). That means:
  - no placement build;
  - `mission_setup` never started or never finished;
  - no 1C19FA8(3);
  - yet `game_start_event` was started on top of it.
- [I] The most likely cause is a script or native routine that assumes mission_setup has run and loops forever. The other 0xF-exit callees were not examined.

**How to avoid it:** never skip a node's progression. Let A31A50 run its native steps 2 -> 3 -> 4 -> 5 -> 8 and only shorten the *wait* in node state 2. Then mission_setup, the placement build and 1C19FA8(3) all happen before 0xF.

## 3. Candidate hooks (both opt-in, cross-world window only, restored at XsEnd)
**Step 0, a read-only probe (do this first; it decides between H1 and H2).**
- Retarget the rel32 of `call A31A50` at **A319EF** (verify `E8 5C 00 00 00`) to a stub. The stub calls A31A50 and returns its value unchanged.
- While SeqState() == 6, log once per change, per node:
  - obj, `+0x120` (mission id), `+0x138`, `+0x38`;
  - `13335C([45B9E60])`, `28F307C()`, `23BD710([obj+8],"mission_setup")`.
- Also walk `[[ActorMgr]+0x10]` from the pump, to catch nodes whose script does not call UpdateSetup.
- It answers: which node, which state, and which predicate holds.

**H1 (if the probe shows node state 2 held by 28F307C): pass that one predicate in the setup state machine only.**
- Site: **A31B41** `E8 36 15 EC 01` (`call 28F307C`). Retarget the rel32 with one locked 4-byte write, as done for D90C0 and 223C4B5.
- The stub calls 28F307C. If it returns 1, it returns **0** only when all of these hold:
  - the xworld window is open;
  - `[45BA21C] == 6`;
  - stage world == target and stage state 5;
  - the arena cells are not blocking (AreaObjPending over the arena cells);
  - the boss resource is loaded;
  - N ms have passed since state 6 began.
- If G5 is also a holder, do the same at **A31B33** `E8 24 18 70 FF` (`call 13335C`) under the same conditions.
- Both call sites are private to A31A50, so none of the 12 LoadIsLoadEnd callers or other 13335C / 28F307C users change.
- Do **not** touch A31B76 (`45 84 C0 0F 85 5C 01 00 00`), 223C55A or 1C1C636: they skip the node's work, not just its wait.

**H2 (if a node sits in state 3 or 7 and its wait is LoadIsLoadEnd / G8): mod-only change, no new patch site.**
- In h_aor (training.c ~1403), change `(q != 5 && q != 8)` to `(q < 5 || q > 8)`, so the existing 500 ms G8 pass also covers states 6 and 7.
- This only helps if LoadIsLoadEnd is really what holds. Section 1 shows it was *not* the final holder in 114244, so expect a small gain alone.

**State 8:** keep 8682 candidate #1 (stub on 223C4B5 with arena-ready conditions). Once H1 lets the placement build run earlier, G5/G7 in state 8 should also clear earlier, because the spawns start sooner [I].

## 4. Risks and what a test must show
**Risks:**
- **Early placement build (H1).** PlacementBuild runs while the placeable queue is still full.
  - Far enemies and objects then appear later, through normal streaming (same class of risk as CrossWorldAreaObjSkipMs).
  - [I] If an object type's setup dereferences a resource that is not ready, it could crash. Mitigation: the arena-ready conditions plus the N ms floor.
- **28F307C's real meaning is unknown.** If it guards a non-resumable step (e.g. a placement-table rebuild in progress), passing it could corrupt placements. The probe must show it flips to 0 by itself later, as the drain timing suggests.
- **mission_setup may contain its own waits.** If so, the hold just moves to node state 3. The probe shows this directly (state 3 for seconds).
- **Mission ids gated in A31A50:** 0x1AB3 (`+0x3C4`) and 0xC12 (1C1E118 net path). Fight now rows are offline, but log the id.

**A test must show, for each of OFF / H1 at n >= 3 (Shingen R15FF cold fnow, prewarm 0):**
- (a) every node reaches `+0x138 == 8` *before* 6 -> 7, through 3 -> 4 -> 5 (the probe log);
- (b) 0xF exits and the game thread keeps ticking (pump log continues for >= 10 s after 0x10/0x11);
- (c) `game_start_event` finishes (state 0x10 -> 0x11);
- (d) the boss fight starts, the arena is intact, and the player has control;
- (e) click -> control time and the state 6 / 8 hold times against OFF;
- (f) a second Fight now and a return to the field both work (mission state is consistent).

## 5. Not confirmed
- **Which predicate held the 20.6 s state 6.** 28F307C vs a wait inside the mission_setup script: inferred from the gate log, not proven. The probe decides.
- **The setter of `[[45C3760]+0x260]+0xD0`** and what that flag means. The name "placement busy" is a guess.
- **The exact freeze site after 0xF** in the 223C55A run. 1C2998C/1C29A88, 59FF7C, 2704064 and the game_start_event script were not traced.
- **How many nodes the list holds during Fight now**, and whether an old-world mission is among them.
- **That `+0x120` is the mission id and the list holds one node per mission.** This rests on the script names and constant usage, not on a type.
- **Who puts a node into state 6/7.** It was not found: the `+0x138 = 7/6` writers at 3A3A0D, 6FFAA3, 1844631, 1DCD1CD, 1E3A7E7 were not checked against this class. Node state 6 has no handler, so it would stick.
- No timing gain is claimed. Nothing here was run.
