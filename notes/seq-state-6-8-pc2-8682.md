# Mission sequencer states 5/6/8 after a cross-world Fight now: what they wait on, how to pass them (pc2, task #8682)

Read-only RE. No game launch, no game-folder write. Disassembly: capstone on the memory dump `C:/nioh3-re/Nioh3.mem.bin`
(flat image, RVA = file offset), driven by `re/scripts_boss/spawn4943/n3dis.py`. The patch-point bytes below were read from that dump.
Tags: **[S]** proven statically (asm read here). **[L]** seen in a training.log. **[T]** an earlier pc2 test, as recorded in training.c.
**[G]** guessed. All addresses are RVAs (B+RVA).

## 1. Correction to the premise (measured, [L])
The long black wait is mostly **state 5**. State 6 is ~0.5 s. State 8 is the second-largest hold. All values are ms after the request.
| probe (Shingen R15FF) | config | 4->5 | 5 held | 6 held | 8 held | 15->16 (cover clear) |
|---|---|---|---|---|---|---|
| 1015-230009 | AOSkip 500 | 5922 | 28328 | 484 | 10250 | 45562 |
| 1016-090923 (R2EE9) | AOSkip 500 | 2687 | 35234 | 782 | 3610 | 42875 |
| 1016-100404 | PlaceableSkip 1 | 2781 | 37344 | 563 | 6110 | 47391 |
| 1016-105144 | PlaceableSkip 1, 45 s | 2594 | 38172 | 547 | 10296 | 52172 |
| 1016-120211 | NearArenaOnly 1 | 2890 | 29578 | 516 | 16390 | 49953 |
| 1016-120843 | NearArenaOnly 0 | 6485 | 33031 | 516 | 18406 | 59016 |
| 1016-114244 | **LoaderGateSkip 1 (G9)** | 3344 | 24484 | **20578** | (log ends) | - |
| 1016-115434 baseline | all off | 8781 | 42906 | 547 | (log ends at 60 s) | - |
After 8, the sequence is 9 -> 0xB -> 0xC -> 0xF in ~50 ms, then 0xF holds a fixed **500 ms**. GAMESTATE[2] clears when 0xF exits (15->16).
So **the cover drops 0.55 s after state 8 passes** [S+L]. The "~31 s drain" is state 5. State 6 only becomes long when state 5 is forced (114244).

## 2. Dispatcher 5A29F0 (switch on controller+4; every handler mirrors the state to 45BA21C) [S]
1 223BDB0, 2 223B3A8, 3 223B3F8, 4 223B8F0, 5 223AF50, 6 223C53C, 7 223C630, 8 223C4A8, 9 223C684, 0xA 223C5B8, 0xB 223BE08,
0xC-0xE 223BEA8, 0xF 223C308, 0x10 223C2D4, 0x11 223AE58, 0x12 223AC50, 0x13 223AE90, 0x14 5A2A90, 0x15 223AE1C, 0x16 223C6B8.

## 3. Transitions and exact wait conditions [S]
**5 -> 6, MissionLoadStep 223AF50.** The state is written at 223B37A.
- Wait: `1FC6674(stage+0x500)` (the player's position is in a loaded area), then `call LoadIsLoadEnd` at **223AF98** (`E8 87 84 17 FE`).
- On pass it runs the one-frame mission setup S1-S7 (loader-bypass-patchnotes-pc2 s.3.3).

**LoadIsLoadEnd 3B3424.** It has 12 callers, so never hook it globally. The gates are checked in this order:
- G1 `3B35B4(stage+0x500,1)`;
- G2 `8B3868(stage+0x540)` (any area-tree node whose collision entry has no active object yet);
- G3 `3B3974(stage+0x450)` (request queue);
- G4 UI pair 3B3934 (only when [45C6F2C] is set);
- G5 `13335C([45B9E60])` (any character-resource entry with +0x2C8 == 0);
- G6 `D3958(stage+0x540,[stage+0x504])`;
- G7 `2BE284`;
- G8 `3B3A28(stage,0,-1)` (cells: o+0x1F0 / o+0x1EC / A+0x1C0);
- G9 `46518() && 3B2970()`;
- then a per-character loop: `3B39AC != 0` holds for up to 120 dt60 units, kept in [45C84A4] (float [3BD4220] = 120.0).

New facts about G9 and G7:
- **G9 has two parts.** `46518` = `123A930(GameMain+0x588)`, which is nonzero if any of the GAMESTATE bytes +0x588..+0x58D is set. So
  **G9 is only checked while the cover or a stop flag is up.** That is why the CoverReleaseMs run (114900) logged only 0x1/0x5 after the forced clear.
  `3B2970` = `[475A420]+0x248 || +0x249`.
  - +0x248 is written only in `1FF320` (manager B poll): `= (+0x1E0 + +0x1F0) != 0` at 1FF363/1FF384. +0x1D8/+0x1E0 is the placeable
    queue that D8E00 pushes to. **So G9 = "the placeable queue is not empty".**
  - The same poll caps in-flight loads at `mov eax,0xA` at **1FF3BA** (`B8 0A 00 00 00`), or at 1 when GameMain+0x4D is set.
- **G7 `2BE284` only scans while ActorMgr+0x2679 != 0.** State 7 sets that byte (223C66E). So in state 5 G7 always passes, and in state 8 it
  blocks while any entry of the ActorMgr+0x48 list (16-byte entries, obj = [e+8]) has `obj+0x50 == 1 && obj+0xF6 == 0` (`2BE55C`).
  [G] These are character spawn slots still being created.

**6 -> 7, 223C53C.** It waits on `1C1C61C(ActorMgr)` (its only caller is **223C553**, `E8 C4 00 9E FF`; the branch is `je 223C5AB` at **223C55A** `74 4F`).
- The predicate is true when **every node of the std::list at [[ActorMgr]+0x10] has [node+0x10]+0x138 == 8**. [G] Every queued character task is created.
- Then it either calls `21A0E0C` (when CharMgr+48->+110 is set) or `A5F930(id)` (WarpPointFind / start-position placement), then `3454EC(id)`, and writes state 7 (223C5A5).

**7 -> 8, 223C630.**
- Offline (`1C9DBA0` false), it goes immediately to state 8 and sets **ActorMgr+0x2679 = 1** (this arms G7).
- Online, it also waits `1C9CF24 && !E884B4(7)`.

**8 -> 9, 223C4A8.** The wait is `call LoadIsLoadEnd` at **223C4B5** (`E8 6A 6F 17 FE`, return address 223C4BA). That call is the state 8 gate.
- On pass: if [45BC462] (queue head +0x42) is set, it runs `613754(cell,1)` for each loaded cell; then `219FCE4`; host only, `9412CC`; then state 9 (223C529).

**9 -> 0xB.** `282B5C(0)` is false when no event-camera slot is busy. **0xF, 223C308:** a timer of 30 dt60 units (100 with the tip page, +60 in region 0xC0), then
`StopFlagSet(2,0,0)` (cover off) and state 0x10. **0x10, 223C2D4:** waits until `1C1C5C4(ActorMgr)` is false; this is after the cover is off.

**Gate bits logged per state [L]** (training bits: 1 G2, 4 G5, 8 G7, 16 G8, 256 G9):
- state 5, late: 0x101 (G2 + G9) for ~12 s, then 0x100 (G9 only) for ~12-17 s (120843: 0x100 from 27.1 to 39.5 s);
- state 6: 0x110/0x114;
- state 8: **0x10C (G5 + G7 + G9)** for ~17 s, then 0x108, then 0x100 (~1 s), then pass.

So state 5 waits for the cells and the placeable queue to drain, and state 8 waits for the character resources and spawn slots, plus the same queue.

## 4. Why the earlier tries moved the wait instead of removing it
- **G9 skip (3B3523, 75 6E -> 90 90) [L 114244, T]:** state 5 ended ~8-14 s earlier, but state 6 then held 20.6 s.
  - [G] The character tasks in the ActorMgr+0x10 list reach +0x138 == 8 only after their resources load, and those resources sit behind the same placeable FIFO.
- **Skipping the state 6 wait (223C55A) [T, training.c:1435]:** the game hung in state 0xF. The 0xF timer advances by [rdx+0x18].
  - [G] That value is the dispatcher's dt, which may stay 0 while character tasks are not created. Not re-tested. **Do not repeat.**
- **Forcing the cover off [L 114900]:** this turns G9 off (see 46518), but G2 then held state 5 until +38.7 s. Screen black, no gain.

## 5. Ranked skip / advance candidates
| # | what | patch point and bytes | risk | INI opt-in? |
|---|---|---|---|---|
| 1 | **State 8 early pass once the arena is ready.** Retarget the rel32 of `call LoadIsLoadEnd` at 223C4B5 (verify `E8 6A 6F 17 FE`; write one locked 4-byte rel32, the same technique as D90C0) to a stub. The stub calls 3B3424. If that returns 0, return 1 only when: the xworld window is open, SeqState()==8, the arena cells are not blocking (AreaObjPending over kCells), the boss resource entry has +0x2C8 != 0, the boss actor is live, the player node +0x138 == 8, and the condition has held for N ms. | 223C4B5 | **Low-medium [G].** Only G5/G7/G9 are passed early (state 8 runs no setup that depends on them). Far enemies and NPCs keep loading and spawn late, which is the normal streaming path. 0xF/0x10 still run natively, and 0x10's wait happens after the cover is off. Expected gain: the state 8 hold, **3.6-18.4 s [L]**. | **Yes:** `[ArenaSwap] CrossWorldSeq8PassMs = N` (default 0 = no patch) |
| 2 | **Shrink the queue that feeds every gate:** NearArenaOnly / CrossWorldPlaceableSkip (D90C0). These already exist. | D90C0 | Low (already shipped as opt-in). | Measured n=1 each: state 5 29.6 vs 33.0 s, total 50.0 vs 59.0 s [L]. Needs n>=3. |
| 3 | **State 5 early pass, caller-specific.** Same stub on `call` at 223AF98 (`E8 87 84 17 FE`), same arena-ready condition, with 1FC6674 still required. MissionLoadStep's own setup is a single frame, so it runs normally. | 223AF98 | **Medium [G].** The wait moves to state 6, as the G9 skip did (20.6 s), unless the boss and player tasks are already created. Use it only together with #1 and #2, and only after a probe shows which list-node holds state 6 (s.6). | Yes, with a separate key, `CrossWorldSeq5PassMs` (default 0) |
| 4 | **Placeable in-flight cap** 10 -> 16/32: imm32 at 1FF3BB (`B8 0A 00 00 00`). | 1FF3BA | **Low-medium [G].** It is a throughput change, not a skip. The drain is read-bound on 3 threads, so the gain may be zero. The cap is global, so restore it at XsEnd. | Yes: `CrossWorldLoaderInFlight = N` (default 0 = untouched) |
| 5 | Skip the G5/G7 jcc inside 3B3424 (3B34AB `0F 85 E2 00 00 00`, 3B34F7 `0F 84 96 00 00 00`). | - | **High.** It affects all 12 callers of LoadIsLoadEnd, including other load paths. Rejected; use the caller-specific #1. | No |
| 6 | Skip the state 6 wait (223C55A) or force controller+4. | - | **Rejected [T]:** hangs at 0xF, and it omits the 21A0E0C / A5F930 / 3454EC placement calls. | No |

## 6. Probe to run before #1 and #3 (read-only, cheap)
While SeqState is 6 or 8, once per second, log:
- for each node of [[ActorMgr]+0x10]: [node+0x10], +0x138, and the char id;
- for G5: the first [45B9E60] entry with +0x2C8 == 0 (its char id / resource handle);
- for G7: the first ActorMgr+0x48 slot with +0x50 == 1 && +0xF6 == 0 (slot +0x20/+0x28 ids, which 2BE284 already writes out);
- [475A420] +0x1E0 / +0x1F0 counts.

What it decides: if the arena boss and the player are ready long before state 8 ends, and the holders are far-world enemies, #1 is safe to ship opt-in.
If the boss itself is the G5 holder, then fix the loading order instead (put the boss's Load::Character ahead of the placeables), not the gate.
