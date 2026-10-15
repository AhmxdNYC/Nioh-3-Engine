# Boss BGM on the Fight-now swap (bridge 4640)

2026-10-14, Ally (Proton, game v2.02, 1920x1080, saves/wrapper/mod files restored, result.json restored=true). Evidence /tmp/bgm-4640.
Build: mod/Nioh3Training/src/training.c (+ Nioh3Training_bgm.tsv, generated from notes/boss-script-args.csv). ini `[ArenaSwap] BossBgm=1`.

## RE (live dump of the Sound script wrappers, objdump)
- Sound state object S = `5A16F8()`. `S+0xB4` = boss BGM id (-1 none), `S+0x88` = mission BGM id.
- Sound::PlayBossBGM wrapper 23AD8A4 -> native `29057EC(-, hash)` (writes S+0xB4=hash, S+0xB8=0). StopBossBGM 23AF10C -> `59FFDC` (S+0xB4=-1). StopMissionBGM 23AF16C -> `5A0014`. RequestBossBattleBgmEndState 23ADE5C -> `2906098`. ActivateBossBattleBGMWithID 23AD224 = same PlayBossBGM native. GetPlayingBgmID is a stub (returns -1).
- Per-boss hash = first `Sound::PlayBossBGM` constant in the boss script (boss-script-args.csv): 20 of 43 table bosses have one (Yamagata e254478a, Yoritomo 2ba01e2a, Shingen f9b6ca15, ...). The other 23 scripts never call PlayBossBGM (automatic boss music through BossBattleMode / SetEnableAutoBossBGM) and are left alone.
- Field music is not touched: StopBossBGM just clears S+0xB4, so the field BGM state S+0x88 resumes. Script end sequence = RequestBossBattleBgmEndState, StopBossBGM, StopMissionBGM + PlayMissionBGM (field id unknown), so only the first two are replicated.

## Implementation
Swap: after BossBattleMode -> `BgmStart(hash)`. Boss death (death sensor) -> `RequestBossBattleBgmEndState`, 6 s later `StopBossBGM`. A new swap or `bgm` command stops it first. Commands: `bgm 0xHASH` / `bgm`.

## Runtime (Ally, Rajomon save, 1920x1080 fullscreen)
| Case | Result |
|---|---|
| Yoritomo (world 0xB1) fnow, model preloaded | 0.53 s command->control, `BGM PlayBossBGM(0x2BA01E2A) +B4=0x2BA01E2A`, boss attacks; cold model 13.9 s |
| Yamagata (world 0x4B, after native load of 0x1CBC + `flag 456 1`) fnow | 0.53 s, boss bar + Burst Break hint, `PlayBossBGM(0xE254478A)`, +B4 verified |
| `kill last 1` (force kill with drops) | death sensor 0->1, EndState called, StopBossBGM 6.0 s later; amrita +151,029 (Yoritomo) and +3,672 (Yamagata) from drops |
## Not verified / not done
- Audibility: game output on Ally is near silent (stream RMS ~20/32768 in the field, SE peaks ~300; sink volume 1%), A/B captures (field vs PlayBossBGM) showed only +1..2 RMS and a spectrogram change near the call. State is verified, the sound itself is NOT proven. PC please confirm by ear.
- Arena intro / fog gate: not attempted. They are script-driven (RTM::Request/PlaywithSetFrame cutscene ids, Gadget::SetReplacementStatus fog, invisible_wall_boss_enable) and need registered actor wrappers; the dynamic boss has none.
- Clear flags: not set (Fight now only offers cleared bosses, flags are already set; setting them on a first clear would write progression).
- Natural kill rewards untested (the boss kills the player first); `kill last 1` drops work.
- The boss HP bar stays on screen after the boss dies (HideBossGauge 1C208BC with the dynamic handle does not hide it); not fixed.
- Ally environment note: Steam's UI stole gamescope focus twice during the run (memory pressure); restored with GAMESCOPECTRL_BASELAYER_APPID + xdotool windowfocus.
