# Door-row follow-up #72188

Base: cxl-pc/multiboss-door ec8cb3b4. Offline only; no install, game, saves or lock touched.

## R1DEA
Entry completion checked only the catalogue's selected placement 4092708 (Okita), while the native script can create roster member Yoshinobu, placement 4097963 / spawn key 268880. That kept route=1 and allowed the 120-second door timeout to request fallback despite a named native boss. Entry now searches the existing generated same-row candidates and validates wrapper record placement + spawn key, live handle identity, and exclusion of player/owned summons before accepting it. The selected catalogue placement is not rewritten. Logging prints the actually observed placement. A dormant named actor also retains native ownership.

The catalogue generator needs zero corrected row IDs for this problem: a single placement cannot represent a multi-boss row. The existing identity generator supplies 25 multi-boss rows (23 resolved fallback sequences, 2 ambiguous sequences still observed through all candidates). Entry consumes that data without changing sequence/defeat logic. This does not change KILLID: a harness must use the actual live placement, not the first catalogue placement. R1DEA's 120-second duplicate prevention remains a live validation item.

## RB3FE
New reproducible generator scans every catalogue mission's original script and PlacementBox records. It resolves serialized symbol IDs (not symbol-array indices), requires a unique same-function literal Wait_InvadeBox_Online -> matching GatherWarp -> selected Create/CreateAndWait path, and rejects functions containing WaitWarpStart. Only ONE row qualifies: RB3FE. No manually maintained row, boss, box or coordinate overrides exist in the generator.

Recovered path: archive_09/BE/45C15A1FFB1A..., sub_event_kumagai_battle_start: temp1 read 0x12328 precedes box 8060204 wait 0x123AA, native GatherWarp 0x123D8, CreateAndWait(8066787) 0x1243E. Geometry comes from archive_03/90/8B8339CD8E3A..., box record 8060204. Target is the box centre in X/Z with GatherWarp floor Y + 50, validated inside its vertical extent: 242678,14006,114790. Full provenance/offsets are in native-entry-boxes-72188.json.

Training alone performs the initial landing at that entry box and suppresses its later near-boss nudge. The script still owns GatherWarp, temp1, intro and creation. Door-right code is byte-for-byte unchanged. If the script's temp1 early return wins, the existing timeout fallback remains; no temp/story writes were added. Offline inspection cannot establish the live temp1 value, collision/floor, or successful native creation. Enenra and R0C12 sequence code is unchanged.

## RC1D9
The boss creator is boss_battle_start, not the adjacent function reported by the old array-index disassembler. Corrected script-ID resolution shows boss_clear_check waits on Character::IsScenarioActionFlag(boss_pl_enemy_id(),43) at 0x1E1EC and a counter threshold (float 30 at 0x1E212), or player death. It does NOT simply wait on Character::IsDead or zero boss HP. The success branch later requests/plays RTM 18803, then calls SetMissionClearFlag at 0x1E40A and BossBattleModeEnd at 0x1E41C. Existing clear protection remains unchanged.

Ally log: native active actor at 22:41:09, killid at 22:41:34, DEATHBLOW skip explicitly says force kill, then quit at 22:42:35. It does not log scenario flag 43 or post-kill HP. Therefore this proves the force-kill harness did not complete the scripted condition, not that ordinary player damage cannot win. The skip is requested by killid's temporary g_kill_act; ordinary native damage is not given that force-kill override. No RC1D9 gameplay patch is justified from these logs. Real-hit/finisher completion is UNPROVEN, and flag 43 should be observed before blaming an HP gate or forcing script flags. The available evidence does not prove that DEATHBLOW skip caused the missing flag, only that bypassing the natural action sequence is an invalid completion test here.

## Offline validation
Training build passes (existing unused g_noForceHide/AbFree warnings). Native-entry mock, exact actor identity (8 cases), native phase grace, all 23 resolved native roster sequences, fallback sequences and 6 roster-data tests pass. The entry-box generator reproduces checked-in output from original extracted records. Separate generator tests cover serialized-ID mapping, wrong actor, mismatched box, different function and door exclusion. No live success claim.

The branch changes Training and its offline generators/tests only. Keep the base BR ec8cb3b4; no BR gameplay changes required. Installed PC build remains supplied EF 1de96af5 / BR 65d31658 / TR 99600c91 / Cam 7482c03d; public remains 1.8.6.

Final artifact: Training SHA256 4964090e69d226e7c89c23dc29e23e194388f01fef154c4b862e0c14ef1862da (bridge #5449). Return/ownership/dedupe regression also passes under Zig C++ (MSVC cl was absent from PATH). Five box-path selection tests pass. Source 8f6581c6 before this evidence/checkpoint append.
