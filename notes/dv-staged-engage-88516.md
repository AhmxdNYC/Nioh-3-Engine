# Death Veil (RB24D) is a staged fight, not a warp-in (pc/0 #88516, Opus offline, 2026-11-11 ~5:55 pm EST)
Source: decompiled mission script archive_09/37/663667288B5D... (re/scripts_re/lfm_disasm.py --calls/--stmts). Offsets are in that script. Static analysis only.
PROVEN (static):
- 13c2a-13c80 tenshu_location_switch: checks the host is inside box 573194 (CH07_MAIN01_EDO_SORANAKI_BOX, 86441 20050 114735), then StartWarpByGadgetRights(573194,60,300) and WaitWarpStart(573194). BR's rights request satisfies this.
- 13d34 EndWarpRights, fade, then Create 573339 and four companions (13dd2-13e02), then Player::SetPos(86328 19910 114735 yaw 180) at 13e60. This matches the live landing exactly, so the door path ran to completion.
- 1414a: the re-entry warp is GatherWarp(573194, 86264 19911 115433).
- 14062/14070: AlgoActiveFlag(573339,F), AlgoMode 42. SetCompassDestination(89226 27700 86522).
- Four wave stations (box -> WaveBattle -> boss HP after):
  - 571326 WB_THUNDER -> 65218 -> 0.75
  - 574736 WB_NEEDLE -> 16889 -> 0.5
  - 575383 WB_ICE -> 24284 -> 0.25
  - 578226 WB_FIRE -> 63339 -> 0.0001
  Mission flags 479-482 record each station. AlgoActiveFlag(573339,T) is at 15768, only after the fourth station. The script never calls BossBattleMode for 573339. The Hiruko fight comes from interacting with 573339 (16a36).
- The decompiler's function labels for the stations are shifted by one; the box IDs and names are the reliable part.
UNPROVEN: the stations work in a replay with flags 479-482 clear (walk into 571326 should start WaveBattle 65218). That flag 482 leads into Hiruko. That WaitPlayerPrepareDuelAnyRange(573194) at 140c2 returns at the landing.
PROPOSED universal change:
- gen_native_entry_gates.py gate_paths(): collect the script landing (SetPos/GatherWarp after WaitWarpStart) and the staged-engage chain (AlgoActiveFlag F, then box/WaveBattle waits, then AlgoActiveFlag T).
- rematch_catalog.py pick_fight_start(): use the script landing as the start; add an engage=staged column.
- training.c: a staged row counts as entered once the player lands at the script position and the boss exists; progress = the first wave starting; never nudge or recover toward the boss stand.
DECISION NEEDED: what counts as a Death Veil rematch. Either the full four-station climb (no flag writes), or skip to the final state (needs flag writes 479-481, which policy forbids).
