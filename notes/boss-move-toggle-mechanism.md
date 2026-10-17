# Boss move toggles: how a boss move is chosen and where to veto/force it (pc2 #9928)

Static RE of a live memory dump (Opus subagent a95c24b9, 2026-10-17); hook sites checked live by the MoveToggle probe (see bottom). RVAs relative to image base. V = verified in disassembly, I = inferred.

## Correction to earlier notes
state+0x2E4 (RequestActionTrans / 9A732C) is NOT the move path: it holds a mode value 1/2 (-1 reset), not an action id. (V)

## Pipeline
AI tick (27CC04) presses virtual pad buttons (27D1C1 -> 73FFCC) - it never picks an action id. Each frame the state update runs 9021E4, which walks the current action's transition list;
on a match it calls 35CF28 (state vtable +0x1A8) -> **35EF04(state, transition) -> id** (weighted random pick among up to 6 target words at transition+0..+0xA, weights bytes +0x54..+0x59,
0xFFFF = no target) -> state vtable +0xF8 (4F54C4) -> **35BC68(state rcx, id edx, table r8, layer r9d, slot [rsp+0x28])**, the only writer of the current action (state+0xE8); al=1 commit, al=0 refuse
(`cmp edx,0xFFFE; ja` -> refuse before any write). Same id space as 358528 / ActionLog (0x4E21..). Player uses the same classes (ids 0x286x): filter by state.

## Veto (implemented, Nioh3Training branch pc2/move-toggle)
- 35EF04 (prologue `48895c2408 4889742410 57 4883ec30`, 15 B): re-roll up to 8x away from blocked ids; if all blocked return idle `0x2` (the earlier `0xFFFF` experiment stalled Shingen; see live result below).
- 35BC68 backstop (prologue `488bc4 48895808 48897018 48897820`, 15 B): blocked id -> 0xFFFF -> refused. Catches forced/script paths.
- Never veto inside 66C4A8 (already cleared state fields); skip the whole call instead if forced moves must be blocked. Do not hook 35CF28 (relative je in prologue).
## Force
Deferred (safest): state+0x15C4 = id, +0x15C8 = 0, +0x15CC = 0.0f when +0x15C4 == 0xFFFF and +0x788 == -1; the state's own update applies it on the next neutral frame (2E9337 -> 66C4A8).
Immediate: 66C4A8(state, id, 0, 0.0f) (what SetActionID does), invasive.

## Test commands / ini
Training pipe: `moveblock <id> ...` (up to 6, none = clear), `moveforce <id>`, `movestat`, `dumpimg <file>`; ini [Summon] MoveBlock=0x4E4E,0x4E45. Logs `MOVETOG ...`, `MOVEFORCE ...`; ActionLog tsv shows resulting ids.

## Live result (pc2, Shingen 0x15FF, vs idle invulnerable player, ~3 min; xworldprobe-20261017-010114)
- Blocked 0x4E4E 0x4E45 0x4E44 0x4E64 0x4E7E (Shingen's longest/most used): **0 occurrences** in the ACTLOG; boss kept using the other moves (0x4E2D/2E/2F, 4E41, 4E21-23, 4E56, 4E8F ...). 5283 select vetoes, 0 re-rolls (the single-target transitions don't re-roll to another id).
- Veto returning 0xFFFF ('no target') made the boss re-try every frame and never act (37379 vetoes, 3 min, #10001 run). Fix: return idle id 0x2 (ini MoveBlockFallback, default 2) - boss keeps fighting, blocked move is skipped.
- Force `moveforce 0x4E2F` (state+0x15C4): action became 0x4E2F within ~15 ms, 2 of 3 tries took effect at idle (the 3rd was written while the boss was mid-action and ran after).
- Per-boss/per-id UI now implemented and tested offline in #10183; see [menu and live-test instructions](boss-move-menu-10183.md). Hit-count/damage per id and live veto of Ki-break moves remain untested.
