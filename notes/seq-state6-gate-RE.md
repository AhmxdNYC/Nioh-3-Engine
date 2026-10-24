# Cross-world Fight now: what the mission sequencer waits on (static RE, re/Nioh3.dump.exe, RVAs)
Sources: bridge #8311/#8630/#8636/#8681, origin/pc2/merged-visible training.c (SeqState/LoadGates/AreaObjPending), capstone on the dump.
(notes/ally-xworld-phases-8244.md, notes/cross-world-arena-5243.md, captures/ally-coverrel-8647 are NOT in this repo.)

## State numbering (mirror u32 B+0x45BA21C, handler writes the next value)
- 5 = MissionLoadStep 223AF50: `1FC6674(stage+0x500) && LoadIsLoadEnd 3B3424` (je 223AF92 / 223AF9F), then mission setup
  (character/actor spawns), writes 6 at 223B372/223B37A. **This is the ~31 s hold** (ally #8630: "seq 5 +5 s .. seq 6 +36 s,
  placeable loads 3080 -> 1"). "state 6 holds 31 s" in the task = time until 6 is reached.
- 6 = 223C53C: `1C1C61C([B+0x45B9E40])` = every node of list [[mgr]+0x10] has node->obj+0x138 == 8 (actors ready); je 223C55A; writes 7.
  Normally passes in <0.5 s. Becomes the long wait if state 5 is forced early (pc2 #6801), and skipping it (223C55A) hangs at 0xF.
- 7 -> 8 at 223C668 (no wait). 8 = 223C4A8: `LoadIsLoadEnd 3B3424` again (je 223C4BC), then 613754(cell,1) per stage cell, writes 9 at 223C529.
  **This is the ~9 s gate**: ally #8630 shows only G9 (0x100) blocking.  9..0xF quick; 0xF 223C308 timer then GAMESTATE[2]=0 -> 17 -> Load::Character 974821.

## LoadIsLoadEnd 3B3424 predicates (all must pass)
G1 3B35B4(stage+0x500) / G2 8B3868(stage+0x540) / G3 3B3974(stage+0x450) / G4 UI 3B3934 / G5 13335C([45B9E60]) char res +0x2C8 /
G6 D3958 / G7 2BE284 / **G8 3B3A28(stage,0,-1)** = every stage cell o=[A+0x30]: o+0x1F0!=0, **o+0x1EC==0** (placeables in flight), A+0x1C0==0 /
**G9 at 3B3513..3B3523**: `46518()!=0 && 3B2970()!=0 -> fail` (jne 3B3523 = 75 6E).
- 3B2970: loader L=[B+0x475A420] (static obj, RVA 0x475A1C0): busy = L+0x248 || L+0x249.
- L+0x248: set 1 at 1FF384 each loader step; cleared at 1FF365 only when count(L+0x1E0) + count(L+0x1F0) == 0
  (+0x1D8 = incoming list, spliced into +0x1E8 pending list at 1FF443, then dispatched into active vector +0x218).
- L+0x249: 1FEE5F/1FEE79, 1 while [L+0x200] or [L+0x210] (second queue) non-empty.
- o+0x1EC++: D97E8 / D9EC7 (placeable queued, +0x10D=3, L+0x24A=1); -- in 2007E4 on completion.
- Writers of the queue: placeables (D8E00 state-0 block via D90C0), characters, mission setup of state 5. Pending list is re-sorted
  (133FEF0/1340850) by request+0x50 ascending whenever L+0x24A is set; placeables use [r14] or -3 (D9E62..D9E6C).

## Why it is slow (and why the manual GAMESTATE[2] clear made it worse)
- State 5 = pure drain of ~3000 arena-cell placeable loads (G8 + G9). Dispatch cap per loader step: **1FF3D6 `mov r9d,1` / 1FF3DF
  `cmove r9d,10`** -> 10 parallel if byte [[B+0x4759910]+0x4D] (static obj RVA 0x4759090, byte RVA 0x47590DD) == 0, else **1**.
  Same flag in the second queue: 1FEE80 `call 7F50D4` / 1FEE85 `neg al` -> r15 = 1 or 10. The dump has it 0; value during
  Fight now is UNKNOWN (no static writer found; likely gameplay vs loading-screen mode). If it is 1 during the swap, that alone
  explains ~100 loads/s, and clearing GAMESTATE[2] (gameplay resumes: more streaming queued, possibly throttled) explains +28 s.
- State 8 = G9 only: L still has queued non-placeable work (from state 5's mission setup / resumed streaming) for ~9 s.

## Patch proposal (arena-swap window only: Training XW claimed + target world current; restore on seq>=9 or 60 s)
P0 (read-only, do first): log byte [[B+0x4759910]+0x4D], counts [L+0x1E0],[L+0x1F0],[L+0x200],[L+0x210] and active
   ([L+0x220]-[L+0x218])/16 once per second from click to seq 17. Decides P2.
P1 (state 8, est. -3..9 s, smallest): only while SeqState()==8, single-byte write **3B3524: 6E -> 00** (jne +0 = G9 ignored),
   restore 6E when SeqState()!=8 or window ends. Byte-atomic, no hook. G1..G8 (incl. G5 character resources) still enforced, state 6
   (actors ready) has already passed, so this does not repeat pc2's state-5 skip failure. Reuse LoaderGatePatch/k_lgs (training.c
   ~1433) but scope it to seq==8 instead of world request..XsEnd.
   Risks: 3B3424 has other callers (UI/load-end checks) during the 1-2 s window -> they also see "loader idle"; queued loads
   finish after the cover drops (pop-in, nothing cancelled); seq 9..0xF run while L still busy (untested).
P2 (state 5, est. up to -25 s if P0 shows flag==1): force dispatch cap 10 during the window: **1FF3D8: 01 -> 0A** (mov r9d,10;
   cmove then yields 10 either way) and **1FEE85: F6 D8 -> 30 C0** (xor al,al -> CF=0 -> r15=10). Both single-instruction, byte
   writes, restore after seq 17. Risks: up to 10 concurrent I/O + TrySync work (3 threads) -> CPU/IO spike, frame hitches
   under the black cover (harmless) but also memory peak; if the flag is already 0 there is no gain and the 31 s is real I/O
   throughput -> only lever left is fewer counted loads (D90C0 skip with NO arena whitelist from the click, + P1), cost:
   arena props and their collision stream in after the cover drops (~100/s).
Not recommended: forcing G8/G9 in state 5 (moves wait into state 6, pc2 #6801), NOP 223C55A (hang), manual GAMESTATE[2]=0 (+28 s).
Test order: P0 log run (1 Shingen cold), then P1 alone n=2, then P1+P2 n=2; measure seq 5->6, 8->9, cover drop, pop-in shots.
