# Death Veil player reports #59/#60/#61 (pc2/1, #95567/#95585), candidate BR 9b2dede2 TR f759e0fa
Evidence: GitHub issues 59-61 (mod reports, one session 2026-11-13 00:15-00:21 local, Ryzen 7600X / RX 9070). Logs are 40-line rings per mod.

## #59 'turned into a crab' - FIXED (BR fa3f56e9, commit 72be5e6d)
Every chr spawn passes 65A8A8 (HkSeal N=0 -> FormKeySealDesc -> PickLearn; SpiritSpawnNote -> FormKeySealNote, any chr < 15 m).
PressPrimarySeal prefers the pressed core id but falls back to the FIRST spawn after the press. A wave enemy (crab) that spawns
right after the soul-core press (player used the seal throw ~every 10 s during the waves: 'PRACTICE infinite soul core ... seal throw')
is learned as the core, bound as g_fkSealChr / form body (FkAppearApply) -> player takes the crab body.
Fix PressSpawnForeign(): with the equipped core map published, a spawn in a press is the seal only if its chr id == pressed core, or
a helper within [Spirit] FormHelperMs (1500) after it. Live check: 'FormCore foreign spawn chr=... not this press's seal' during waves.

## #60 Death Veil not spawned + bar early - ROOT CAUSE, NOT FIXED (needs a decision)
- Snapshots 00:15/00:20/00:21: slot A 'Death Veil chr 0xAFF10 HP 3879/3879 Ki 1100/1100' unchanged 6 min = never fought.
- Fought boss 000001A5167BB140 (55857 HP, actions 0xFA6/0xFB2/0x4E40) = chr 947280 Jakotsu-baba (moves.tsv 5546-5555).
- 'ROSTER RB24D: confirmed chr 947280 defeated, mask 11/7FF': bit 0 = 720656/573339 (fallback_rosters_gen.h:20) already set
  while alive at full HP -> RosterNoteProp (training.c:1343) dropped it as a 0-attack part (CE_SORANAKI_BG_00).
- 336c4166 made fight_placement() (tools/rematch_catalog.py:735) keep 573339 (has gate box 573194; Hiruko 572110 has none):
  the row now targets Death Veil's BACKGROUND prop, which never fights; NativeRosterTick (training.c:2394) also stops Training
  requesting a roster member while the wave chain exists -> nothing brings the real fighter in. Matches #87610/#94523 notes.
- Early bar: NativeSlotsTick (native_slots_game.h:31) copies any native gauge row with HP>0 into slot A (no AI check), so a gauge
  row for the prop existed from 00:15. Who called ShowBossGauge(573339) is unproven (script vs RosterRequestBar training.c:3656
  / RosterWakeMember :3671, which count 'engaged' on any HP change).
Proposed universal fix: (a) catalogue: a placement whose action file is a 0-attack part is never a fight placement/gate target
(fight_placement/pick_gate) - but RB24D then falls back to literal 572110 (Hiruko), conflicting human #87830 vs #87610: decide first.
(b) bar/wake only for a live fighter (AI on + exchanged hits), refused while StagedChainOwnsGate. (c) NativeSlotsTick skips prop
chrs / AI-off bodies. Live lines: BossLog 'ShowBossGauge actor id 573339', Training 'leave out chr 720656', 'NATIVE SLOT A ... 720656'.

## #61 skip waves without the 25 % HP cost - DONE (BR fa3f56e9)
The 25 % is the native script's SetHpRate before the last station (native_entry_waves_gen.h hp_thou 250). Staged start now cycles
Full climb / Final wave (25 % health) / Final wave (full health); mode 2 skips NativeEntryWaveFinal's health write.

## Rebase onto main (#96641)
Branch pc2/dv-reports-rebased-95567: #59 only (BR c2f71021). #61 needs the Staged start feature, which main lacks; it stays on pc2/dv-reports-95567.
