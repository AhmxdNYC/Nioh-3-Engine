# Passive spawned boss: perception list check (bridge 5201)

2026-10-15, ally (Proton, 1920x1080 fullscreen, Nioh focused; gamescope focus app 3681010 logged per input/shot). Evidence: /tmp/ai-5201 (runner.log, focus.jsonl, live-training.log, screenshots). Saves (11 files), wrapper and game files restored byte-for-byte (result.json restored=true).

## Main finding: the old "player-entry -1" diagnostic was wrong
- A perception entry ref is not an actor pointer. It is `{qword key = *(u64*)actor, dword serial = *(u32*)(actor+0x18)}` (writer `27B870`), and the empty value is key 0xF.
- `20A6F0(perc,i)` / `20A858(ref)` resolve the ref through the actor table hash (`[4745348]+0xB98`, `20A7E0`) and compare the serial.
- The 4942/5089 `AwareDiag` compared `[perc+0x28+i*0x58]` with the player pointer. So it printed `player-entry -1` and `entries 8` on every boss, whatever the real state was. The pc log line "player not in the perception list" is therefore not evidence.
- Fixed: `PercDiag` resolves every slot with the native `20A6F0`. It logs PLAYER/actor/id/team/aware for each slot, plus perc+2D0 (current target), +2DC, +558, +378, ai+1574, ai+1AA8, actor/player +8A4, actor+ACCA and the main target `A8520`.

## Static RE: who fills the list (verified disassembly)
- The per-tick perception update is `288910(perc, flag)`, which calls `289CB0(perc, flag)`:
  - It iterates the global character list `8C8C00()` (node+0x10 = actor). It skips same-team actors unless special cases apply.
  - Its filter is `20C764(perc, cand)`. This rejects a candidate with +8A4 == 2 (offline, unless perc+2D0 == 7). Its other gates are placement-link wrappers via `[self+0xF8]+0x6C..0x78` and a few flag tests.
  - It inserts a new target at `28A0BD..28A135`: it clears the slot, writes the ref from `27B870(cand)` and sets perc+2DC = 0. For a team-0 target it then calls `2BE2E8(perc, idx)` (perc+2D0 = idx plus notify).
- The update is skipped while offline when self+8A4 != 0. A counter at perc+558 counts up to 15 when this happens.
- Native enemies have the player in slot 0 even 33-39 m away (aware 0). Placeholder actors with +8A4 = 2 have AI off, 0 slots, and perc+558 = 15.

## Live results (fix OFF = native behaviour; Nuribotoke R401E char 392871, Distance 300, player idle)
| arm | world | owner | perception | distance (m) | attack |
|---|---|---|---|---|---|
| summon | 0xB1 Rajomon | live 360684 | slot 0 = PLAYER, aw 1, 2D0 = 0, main target = PLAYER from +2 s | 3.0 -> 5.2 (+3 s) -> 3.3 (+5 s) -> 1.4-3 | yes: HP 2658 -> 2053 at +12 s, player killed ~+22 s |
| train | 0xB1 | forced player (0x50000001) | same | 3.1 -> 5.2 -> 1.5-2.6 | yes: player beaten (HP 2) at +12 s |
| train | 0xE0 Kiyomizu Defiled (pc's world, reached with `oworld 0xE0 4083044 1`) | live 4030801/4030863 | same | 3.1 -> 5.3 (+3 s) -> 2-3 | yes: player beaten rounds 2-4 at 4.6-11 s |
- Pattern on spawn: the boss backs off from 3 to 5 m in the first 3 s, then closes in and attacks at about +5 s.
- **The passive boss did not reproduce on ally.** That held with a live owner, with owner = player, and in both worlds.

## Fix candidate shipped OFF (ini [Summon] PerceptionFix = 0)
- `PercInject` replays the native insert. It writes the player ref into a free slot, sets perc+2DC = 0, calls `2BE2E8(perc, idx)`, then SetAwareFlg `1FF1FA0(comp,1,0)`. With PerceptionFix 2 it calls `66982C` first.
- MonTick runs it 0.6-8 s after spawn, but only while the player is missing.
- It never fired live, because the player was always present. **Untested.** Commands: `perc [id]`, `percnear [m]`, `percfix N`, `percdiag 0|1`, `inject [id]`, `bpoke off size val`, `mon [id]`.

## Next step for pc
1. Run the new build with PerceptionDiag = 1 and read the `PERC mon` lines.
2. If PLAYER is in slot 0 with aw 1 and main target PLAYER while the boss stays at 5-10 m, perception is not the cause. Look at movement/attack selection instead (e.g. navmesh or the boss's initial "back off" phase).
3. If the slot is missing, or actor+8A4 != 0 / +558 = 15, set `percfix 1` (or 2) and compare.
