# Boss rewards (research 2026-10-08; static + web; game not run)

Result: **the reward/drop data tables were NOT located.** What is established is the grant code surface, where grants are NOT (scripts), and partial web names. Extends `boss-loot.md` (its enemy->drop gap is still open).

## 1. Grant path (code)
- Script natives (names in script-bindings.csv): `Character::AddDropTable/ClearDropTable/ChangeDropType/AddDropFixItem/AddDropKeyItemDirectGet/SetDropAmritaCoefficient/ForceDropItem(+Position)/ForceDropAmurita(+Position)/EnemyForceKillWithItemDrop`, `Mission::SetReward/GetMissionReward/GetGameClearRewardNormal(0x23a444c)/Hard(0x23a43ec)`, `Item::AddItem/AddEquipment/AddFixItem/AddMagicItem`, `Player::AddGuardianSpirit/AddSkillDojo`, `Hell::StatusUpReward/CreateHellRewardDrop`, `System::Trophy::RequestUnlock`, `UI::RequestTitleLogo`.
- The Add/Clear DropTable wrappers (0x238b3ec, 0x238b194) read two args (char/placement id, table id) and push them into an actor record found via charMgr `[0x45B9E40]` -> the table id is DATA, not in script.
- GameClearReward Normal/Hard are fixed id pairs (0x4472,0xcd6f) / (0x7730,0x2641) passed to 0x2280c84, which looks them up through `[[0x45B9E30]+0xD0]` (UI/notice records). Treat as story-end reward only.
- Mission-clear UI strings exist (`MENU_ID_MISSION_REWARD`, `02_clear_reward_list`, `10_07_reward_icon`, "Add clear reward"/"Apply end reward" debug text, no xrefs). Net sync structs carry `reward_object_type_`, `object_id_` (0x2201294), `is_first_` (0x220089c, guardian/skill gain notice), `guardian_spirit_id_`, `esoteric_skill_id_`: so the engine does model a "first time" flag per grant for guardians/skills.

## 2. What the mission scripts contain (decoded 100 scripts, archive_09 + 16)
Boss scripts share one skeleton; the reward is NOT a constant in them. Direct constants found: `ForceDropAmurita(placementId, rate-float)` batches in the kusabi/Hiruko-stage script (archive_09/37/6636...), `ForceDropItemPosition/ForceDropAmuritaPosition(placement)` in `BossDefeatEventMain` (45C15A1FFB1A) and jouka event (6D14B07C9687), `Player::AddGuardianSpirit(349),(380)` (F5C49D3F2BD2), `HasGuardianSpirit` gates (137, 37, 145, 197, 15), `AddFixItemNoNew` switch of item ids (7667713, 87687171, ...). Library helpers (`Function::AddEquipment*`, `RtmContinuousLoad`) take variables, i.e. data-driven.
`Emaki::*` natives in scripts only handle wave-battle setup; no scroll reward constants.

## 3. Still missing (needs a live hook or another data file)
Enemy/placement -> drop table, soul core rate and rarity per difficulty/cycle, per-mission reward table (keyed by mission id), Emaki record reward fields (`[0x45B9E30]+0xA88`, 0x34 B, no reward field seen at +0x0C..+0x31), the already-granted flags. Best next step: hook `Item::AddEquipment`/`AddDropTable`/`SetReward` natives and `MissionFlagSet` (0x70275C) during one boss kill and one replay; first-vs-repeat then reads off directly (is_first_, `Item::IsAlreadyGetItem` checks seen in kitan_45 script show per-item "already obtained" gating for field pickups).

## 4. First-clear vs repeat (evidence level)
- Soul core: guaranteed first defeat, random later (web, single snippet). Rarity: only Common/Uncommon in first cycle, Rare after post-game (web).
- Guardian/skill gains: `is_first_` flag in notice path (static). Field items: `Item::IsAlreadyGetItem` (static).
- Mission replay doesn't clear mission flags (boss-rematch-paths.md), so script-side one-shot grants (AddGuardianSpirit behind HasGuardianSpirit) should NOT repeat; drop-table loot should.

## 5. Web cross-check
PowerPyx boss guide lists no rewards; mapmaster.io blocked (403); Game8 page text yielded only Takasugi Shinsaku (Fujiwara Sadayasu Lv109, Shock Troop Greaves Lv109, Samurai's Locks); search snippet gave Yamagata Masakage (Palisade Breaker, Rosewood Tonfa, Longbow, all Lv2). Per-boss Game8 subpages are the next web source.
