# Summoned boss death toggles (#6185) - 2026-10-15

Branch `pc/summon-death-toggles` (based on `origin/ally/gauge-5437`). Static RE + build only; game NOT launched.

## Source base
- `work-src529` (the 13:50 fade build) is NOT in git and is missing two fixes: the boss-gauge HUD reset (`GaugeStateReset`, ally da8f98a = build 530, KEEP 3/3) and the x533 death fixes (HB heartbeat, HP 0 stable 4.5 s, "gone at <=3 % HP = killed").
- This branch = `D:/nioh3-wt/x533` training.c (= gauge-5437 + x533 fixes) + the src529 fade code (FadeOutActor, BodyFade, BodyFadeDelayMs default 25000) replacing x533's 8 s instant corpse delete + the new toggles.

## RE (Nioh3.dump.exe v2.02)
- VM `Character::EnemyForceKill` 0x238D504 and `EnemyForceKillWithItemDrop` 0x238DAA0 are identical except arg 5: both look up the actor and, if `actor+0xAB84 == -1` and `actor+0x3B0` (comp) != 0, call `786C84(comp, 0, 0, -1, drop, 1, 0, 0, 1)` (drop = 0 / 1).
- 786C84: returns at once if `[actor+0x3A0]+0x48 == 1` (already dead). With arg 6 (force) = 1 it takes the current HP as damage and clears the HP-floor guard (`hp+0x188` bit 1 -> `EC4C5C(hp+0x178,1,-1)`), so it kills a boss that sits at HP 1 waiting for its death blow. arg 5 goes to the death/drop handler `1FFFEDC(comp, type, drop)`; drop = 0 also sets bit 0x8000 ("no drop") in the death message sent by `20028D0`.
- Death sensor 0x4892A8 = 1 when `hp+0x48 == 1` or the actor is in state 0x35 / 0xB0 / 0x2C (finisher/down states). User log 12:30 (captures/pc/user_play_1227.log): boss HP 1 / 51712, sensor 0->1 for ~2.3 s (the finisher), 1->0 (boss back up), 0->1 again = the "two finishers".
- Loot: Ally bridge 458 ran `kill last 1` (same call, drop=1) on a summoned Yamagata: loot pillars + Amrita +151,029. Drop source is the enemy's own drop table; script/mission rewards are not involved (notes/boss-rewards.md, boss-drops.md).

## Implemented ([Summon] in Nioh3Training.ini; defaults = old behaviour)
| key | default | effect |
|---|---|---|
| BodyFade / BodyFadeDelayMs | 1 / 25000 | (from src529) body stays on the floor, then native FadeOut 30 f, delete 6 s later. 0 = body stays. |
| DeathBlowRequired | 1 | 0 = when the summoned boss HP reaches <= 1 the ASI calls the native force-kill at once (no finisher needed). |
| CutsceneOnKill | 0 | 1 = keep the death blow, but when the FIRST finisher ends (sensor 1->0 at HP <= 1) or FinisherKillTimeoutMs (6000) after it started, native force-kill: death animation plays, no second finisher. |
| BossLoot | 0 | 1 = the native kill is EnemyForceKillWithItemDrop (drop table + Amrita). Also enables the one-finisher kill (needed: a natural death cannot be re-killed with drops). |
| FinisherKillTimeoutMs | 6000 | fallback for CutsceneOnKill/BossLoot. |

Only ASI-spawned bosses (actor in g_sp_actor) are touched; one kill per boss-mode session; log tags `DEATHBLOW` / `KILL`. Run-time override over the pipe: `deathcfg <BossLoot> <DeathBlowRequired> <CutsceneOnKill>`.

## Needs in-game test (lead)
1. Defaults: identical to the 13:50 fade build plus the HP-bar reset and x533 death fixes (bar hides, exit, body fades after 25 s).
2. `DeathBlowRequired=0`: boss dies at HP floor without finisher; BBM "dead" follows; bar hides.
3. `CutsceneOnKill=1`: one finisher, then the boss dies (check the kill does not cut the finisher; if it fires too early raise nothing - it waits for sensor 1->0; if the sensor stays 1 the timeout fires).
4. `BossLoot=1`: loot pillars + Amrita after the kill; check both with DeathBlowRequired 0 and 1.
5. Check `KILL ... guard refused (+ab84=...)` never appears (would mean the finisher state sets +AB84; then the kill must wait for the sensor 1->0 edge, which is what CutsceneOnKill does).

## Infeasible / not done
- The mission's boss-defeat cutscene (RTM, `PlayBossDefeatRTM` / `BossDefeatEventMain`) is mission-script bytecode per boss; there is no generic native to play it for a summoned boss outside its mission. CutsceneOnKill therefore means the native death animation after one finisher, not the RTM movie.
- Script/mission rewards (first-clear gear, guardian spirits, story items, mission reward screen) are granted by the mission script / mission tables, which are not located (boss-rewards.md section 3). BossLoot gives only the enemy drop table (Soul Core chance, materials, Amrita). Bosses whose drop table is empty (boss-drops.md: 131/186 have no reward-set link) may drop only Amrita.

## Death-blow revive (#6643, commit 5346c74)
- User logs 18:57 / 19:02 (captures/pc/user_play_1856/1900.log): native kill at HP 1 -> sensor 0->1->0, HP back to 1; a raw HP 0 write was undone the same way.
- Cause (786C84, after damage apply 787268 sets hp+0x48 = 1): `786FFC mov edx,27h; mov rcx,rdi; call 4893B8` = CharacterConditionEvaluate(0x27) (chr byte +0x20D bit 4). True -> `66C188(hp+0x38, 1)` (HP 1, dead flag cleared) + death-blow-pending (+0xEA0 / 0x400000000000); false -> death handler 1FFFEDC(comp, type, BL = drop). Second revive path when actor+0x894 != 0 (786F7E) not touched.
- Fix: E8 at 787004 retargeted to a near stub; cond 0x27 reported false for the boss-mode/summoned boss when DeathBlowRequired=0 and during our own force kills; stub clears BL (no drop, also the 0x8000 bit at 787219) for natural kills with BossLoot=0. Raw HP 0 write removed. Untested in game.
