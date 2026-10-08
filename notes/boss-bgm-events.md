# Boss BGM and cutscene hooks (research only, 2026-10-08)

Static analysis only; game and mod untouched. CSV: `notes\boss-bgm-events.csv` (one row per script of `boss-fights-table.csv`; columns = 1 if the script's embedded C-API name table lists that API).

## Findings
- **BGM is not a table lookup by name in the script.** Scripts only carry the API names (name table is per-script: only used APIs appear). Of 104 boss scripts: `Sound::PlayBossBGM` 52, `StopBossBGM` 37, `StopMissionBGM` 52, `PlayMissionBGMAndState` 29, `PlayMissionBGM` 37... see CSV. `SetEnableAutoBossBGM` 13, `RequestBossBattleBgmEndState` 13 (fight-end music state). `ActivateBossBattleBGMWithID` is in 0 of these scripts (exists in the exe at 0x3bff670; used elsewhere).
- Cue/BGM ids are **bytecode integer constants** passed to those calls, not strings. Decoding the call operands needs the script VM disassembler (not written). No BGM cue-name table (Wwise/CRI/ADX bank names) found in strings.tsv, the extracted LFM, or kidsobjdb dumps. Sound banks are not in an extracted/identified form. Install has `archive\` and `package\` only (no loose .awb/.bnk/.pck/.wem/.usm).
- **Name hints live in stage placement data** (`data\extract\lfm\archive_03`, 1295 files): BGM trigger boxes and preload boxes are named per area, e.g. `CHxx_WORLD_SENGOKU02_BGM_001..`, `CHxx_WORLD_HEI_*_BGM#`, `CHxx_WORLD_EDO_*_BGM_#`, `..BAKUMATSU_*_BGMBOX_#`, `CHxx_MAIN#_SHIMABARA_BGM_BEFORE_BOSS`, `CHxx_MAIN#_HITOKOTO_BGM_BEFORE_BOSS`, `CHxx_MAIN#_MIBUDERA_BGM_MIBUDERA_BOSS_AREA`, `KURAMA_BGM_START/MIDDLE/LAST_#`, `PARALLEL_#_ENEMY_BGMSTOPBOX`, `PARALLEL_#_BGM_MAIN_#`, `HEI_KITAN_#_BGM_#`. BGM boxes are driven by `Sound::SetBgmIdBgmBox / SetBgmBoxBgmTable / IsEnterBgmBox(Group)` (ids again numeric).
- **Cutscenes = RTM (real-time movie / timeline) + TalkEvent + telop**:
  - RTM API: `RTM::Play/StartID/PlaywithSetFrame/Request/WaitEndOfPlay/WaitReadyToPlay/IsEndOfPlay/SetPlayed`, `Function::RtmContinuousPlaySeamless/LongLoad`, `Function::RtmPlaySubFunction`, theater-mode lookups `Mission::GetTheaterModeRTMID/IsTheaterModeExistRTM` (RTM id numeric; theater mode maps id -> replayable cutscene). Present in all 104 scripts via a shared include (RtmContinuousLoad00-04 / RtmContinuousPlay helpers).
  - Boss defeat cutscene: user fns `BossDefeatEventMain`, `BossDefeatEventParallel`, `PlayBossDefeatRTM` (4 scripts each) plus `RequestWarpEvent`.
  - Boss intro scaffolding: `sub_event_boss_battle_start` (34), `sub_event_play_boss_bgm` (4), `sub_event_show_stage_name` (33), `sub_event_check_boss_area`, `BOSS_AREA_ENTER`, `invisible_wall_boss_enable/disable` (21/20), `sub_event_boss_clear_check` (40), `sub_event_check_boss_battle_end`. Cutscene preload boxes in placement data: `*_RTM_PRELOAD_BOX_*`, `CHxx_EDO_RTM_PRELOAD_BOSS`, `CHxx_MAIN#_YAMA_BOSS_RTM_PRELOAD`, `*_GOSHO_PRVLOAD_RTM_BOX_OKITA_/YOSHINOBU_`.
  - In-game events: `Mission::StartIngameEventMode/EndIngameEventMode` (82/31 scripts), `Camera::RequestScriptCamera`, `TalkEvent::*`/`Function::ForceInteractionTalkEvent*`, `UI::RequestMovieTelop/WaitMovieTelopPlay` (telop; 7 scripts request one).
  - Skip hook: `RTM::IsContinuousSkip`, `RTM::SetRtmCameraModeKeepFlag`.

## Next steps
1. Write a bytecode disassembler for the LFM script VM (opcode set not documented); then extract integer args of PlayBossBGM / RTM::StartID per script, giving script -> bgm id / rtm id.
2. Find the id -> cue-name table (BGM id likely indexes a game table in archive_03 or the SoundManager init; xref RVA 0x3bff948 wrapper `Game::MissionScriptFunction::Sound_PlayBossBGM`).
3. Map archive_03 stage files to scripts via stage_names.tsv to fill the empty `stages` column.
