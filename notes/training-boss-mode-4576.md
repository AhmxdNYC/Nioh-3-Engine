# Boss mode on a swapped-in boss (bridge 4576)

2026-10-14, Ally (Proton, game v2.02, 1920x1080, focus asserted per input/shot, 44 checks; saves/wrapper/mod files restored, result.json restored=true). Evidence /tmp/boss-4576 (bridge file).
Build: mod/Nioh3Training (training.c): new commands `bbm id [notify]`, `bbmend`, `bossmode 0|1 [notify]`, `flag id [val]`; ini `[ArenaSwap] BossMode=1 BossModeNotify=1`.

## RE
- `Mission::BossBattleMode` (VM 23A2DCC) body = `charMgr[+0x26AA]=1`; wrapper=ActorFindById(135B78); if !1341F8(charMgr): 1C19E14(charMgr), clear bytes +3036/+3037 (unless wrapper+0x28==0x4ACDF and 374184(cm,0xC12)), 1C20600(0), if 85948() && +305A==0: 1CA885C(1); then a refcounted listener message {wrapper+0x20} via E33410 + 66BBF4(wrapper,&msg). Replicated natively (no VM context needed).
- `...End` (23A2F70): 1C1AE20(cm)->1C0CFB4(cm); 1C0CEEC, 1C0D068, 1C0E83C, 9DECF8 (all with charMgr). Replicated.
- `SetMissionFlag` = 70275C([47514D0], flag, value, 0). Death sensor = 4892A8(live actor) (0 alive, 1 dead; tail of 7E11F0).
- CreateDynamic returns a handle (0x1000EB3F), not a placement id: the dynamic boss has **no registered wrapper**, so FindById fails -> HUD/mode steps run, the listener message is skipped (notify=0 automatically). Actor is found in the actor table by handle (+4).

## Runtime (Ally)
Arena swap + native boss mode, no mission load: Ibaraki Doji (same world 0xB1, Rajomon): command -> control 0.51-0.52 s, boss bar, attacks (player 2658 -> 674-738). Yamagata Masakage (world 0x4B Hitokoto, from the field, after `flag 456 1`): 0.54 s, boss bar + "Burst Break" tutorial hint shown, boss attacked (player 1659/2658 at +16 s). BBM log: applied at spawn, death sensor baseline 0 -> 1 after `kill`, BossBattleModeEnd steps run 1.5 s later (BBM END), no exception.
OFF (native Fight now, MissionRequestById 0x1CBC/67064 from Hitokoto): registry reload finished at +19.9 s (same world; Continue prompt waits for A), lands at the shrine, boss still a walk away. Earlier OFF from another world: 40-46 s.

## Not proven / limits
- BGM change and tutorial/HUD differences vs BossMode=0 were not A/B'd; only the gauge + hint were seen. Boss-end HUD/BGM restore not eyeballed (boss was killed while the player was dying in one run; the End steps ran without error).
- Listener notify (66BBF4) is implemented but never exercised (no wrapper for dynamic bosses).
- Arena script (fog gate, door, intro camera, clear flags/rewards) is not running; `flag 456 1` only sets the mission flag (no observable effect without the script).
- Environment (time of day/music) follows the running mission.
