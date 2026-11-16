# Mod menu reorganisation plan (v1.8.20 candidate, read-only survey)

Source: C:/Users/Ahmad Hamza/wt-menuplan (menu.cpp / menu_pages.h in Nioh3EngineFix, rematch.cpp + *_menu.h in Nioh3BossRematch).
Default layout is `[Menu] Layout = 0` (Nioh3EngineFix.ini:65); everything below describes that layout.
Nioh3Training has NO menu of its own: every Training row is surfaced either by the Rematch DLL (practice_stats_menu.h,
boss_vitals_menu.h via `Nioh3Training_*` exports) or by EngineFix (frame meter page via `g_pstGet`, menu.cpp:1050-1053).

## 1. CURRENT MAP (as the player sees it)

Legend: [EF] = EngineFix host, [BR] = BossRematch hosted page, [TR] = row backed by a Nioh3Training export. `>` opens a page.

Every [EF] page starts with 3 fixed rows: `Mod menu`, `Close`, `Report` (menu.cpp:1443-1446). Every [BR] hosted page starts
with `Mod menu`, `Close` only (rematch.cpp:5545-5546). A tab bar (Boss / Training / Player / Saves / page names) sits in
the title band (menu.cpp:2428-2451).

```
Mod menu (root) [EF] menu.cpp:1395-1400 + 1458-1460
  Boss Matches            > [BR] uRematch        (only while the Rematch DLL is attached: g_rematchSession)
  Training                > [BR] uPracticeHub
  Player                  > [BR] uPlayer
  Display Overlays        > [EF] pDisplay
  Camera settings         > [EF] pCamera
  Optimizations           > [EF] pSettings
  Save files              > [EF] pSaveFiles  (slots..., Back up current save, Refresh saved slots, Open saves folder; slot > Switch after Quit / Import now)
  Update + Page             [EF] action (opens update web page)                       menu.cpp:3696
  Reverse Mods              [EF] action, A twice parks the mod files                  menu.cpp:3704
  Close

Optimizations [EF] menu.cpp:1331-1337
  Preset (XPRE)             Off / Optimized with effect visuals / Optimized / Extreme / Extreme HQ / Custom
  CPU                     > every kOrder row with SetGroup()==pSCpu, in kOrder order with its section headers:
                            "Performance fixes (safe)": Message thread sleeps (W3), Message wait style (W3 mode, Proton), Render wait timeout (R1),
                              Frame limiter sleeps (L1), Fewer worker wake-ups (WK), Batch GPU barriers (GPU)
                            "Experimental (CPU, A/B live)": ~31 rows W6, GF, IK, ED, WL, B, PP, FPC, AABB, M1, X1, E1, M2, J1, S1, WG, TG, DS, LK, CP,
                              SPF, MQ, PT1, DL, DQ, P1E7, MA, BPO, W1B, RPA, TR
                            "Experimental (CPU scheduling)": RTP, PCM
                            "Next launch": Sleeping limiter on Proton (L1Wine)
  GPU                     > "Extra performance (changes the look)": World shading (VRS), Cloud quality, Volumetric fog detail, GI resolution,
                              Shadow distance, Shadow filtering, Geometry detail, Terrain detail, GPU particles, Ambient occlusion off,
                              Volumetric fog grid scale, Reflections at half size, Render distance, Upscaler sharpening
                            "Experimental (GPU, A/B live)": Async compute slots (AS), Pipelines ready (PJH), Fewer shadow tile redraws (VSMA),
                              LPVR, Sky ambient update rate (SKYR), Fog temporal factor (FOGT), Cloud reprojection (CLDR)
                            "Experimental (GPU, Proton)": R2a, R2b, R1 gpu
  Game                    > Smoother frame pacing (VRR), Frames queued ahead, Job-thread limit (WorkerCap), Keep 3 more pool threads (P12),
                            Freeze game while menu is open
  Frame Gen               > Frame-gen presenter sleeps (F1), Sleeping FG presenter on Proton, Frame generation (status), Frame gen: (status), Upscaler (status)
  Save to ini
  Back

Display Overlays [EF] menu.cpp:1347-1354
  Controller overlay settings > Controller button style, Controller display, X, Y, size, opacity, pressed colour   (settings_catalog.h:30-36)
  Frame meter settings        > Frame meter [TR], Frame meter style, Ring size, All frame rows, Ki window, Key: Startup..(16 toggles) with
                                'All key indicators' before Key: Startup                                                (menu.cpp:1370-1382)
  Input display               > Input display (history), side, rows, Pad debug                                         (settings_catalog.h:37-40)
  FPS                         > Show FPS counter, Show frame time, Show game speed, Move FPS counter, Nudge right, Nudge down, FPS counter text size,
                                Mod menu reminder (top-right), Game's own performance display (does nothing), Input-to-screen timing (TS),
                                Count worker wakes (WakeStats)                                                          (menu.cpp:1362-1369, OverlayRow 1041)
  Time of day [TR]              (menu.cpp:1349)
  Weather [TR]
  Game HUD                    > Hide game HUD + one row per HUD element
  Skip load tip screen          (kAutoCont, menu.cpp:1351)
  Back

Camera settings [EF] menu.cpp:1388-1393 (needs Nioh3CamKiProbe.asi)
  Camera left / right, Camera down / up, Distance (closer / farther), Lens (narrower / wider), Return to default camera, Camera effects off (XPOST), Back

Boss Matches [BR] uRematch rematch.cpp:4474-4490, labels 5673-5675
  Boss death animation / Require final blow / Boss loot / No grapple in Ki break / Final-blow timeout / Zero-HP combo timeout   (death rules FIRST)
  Leave Boss Match | Leave Picture Scroll        (only while one runs)
  List: All / Multi / era / Dojos / Scrolls / Favorites
  <boss rows A-Z, favorites first>  > Boss actions [BR] uActions (4543-4553):
        Staged start: Full climb / Final wave (staged missions only)
        Fight now (boss arena) | Fight now (lesson) | Fight now (this scroll's waves)
        Train in this arena (no boss, no waves)
        Train here
        Shrine before the boss
        Back
  Yamagata mission replay > / Open Battle Scroll replays / Play first owned Picture Scroll   (dev, only with [Rematch] ShowUnverified=1)
  Cancel pending replay (while pending)
  Reset to defaults, Back

Training [BR] uPracticeHub rematch.cpp:4349-4362 (title "Training")
  End stuck arena session                        (only when LeaveArenaOffered)
  Train here              > List filter + boss rows (uTrainHere)
  Train in a boss arena   > List filter + "Train in arena: X" rows (uTraining, title "Training - Choose an arena")
  Active bosses (N bosses) > [BR] uTools (4404-4411):
        Boss A: <name>  <stance>      > Boss A page [BR] uBoss (4412-4427):
              Behavior: Fight (CPU) / Stand still        Follow and target me        Repeat last move        Repeat delay
              Reset boss position
              Health and Ki > [BR] uBossVitals: Current health and Ki, Infinite boss health, Infinite boss Ki, Set HP %, Set Ki %, Refill,
                              + [TR] Boss damage, Boss animation speed, One-hit Ki break, Hold Ki break, Negative Ki rows (PstAddBossRows)
              <move group> (N moves) > [BR] uMoveCat: one row per move, Repeat ON/OFF           (groups: attacks by hits, Ki break, Sequences...)
              Remove this boss, Reset to defaults, Back
        Add a boss (slot B)           > uSummon (A-Z list)
        Switch every boss to Stand still / Fight (CPU) after: N s
        Remove all summoned bosses (A, B)
        Return to where I was
        Back
  Freeze [TR] / Step 1 frame [TR] / Reset fight [TR]   (PstAddHubRows, practice_stats_menu.h:45-48)
  Summon a boss here      > uSummon (same list as "Add a boss")
  Boss Matches            > uRematch   (second door, duplicate of the root row)
  Return to where I was   (scroll training only)
  Reset to defaults, Back

Player [BR] uPlayer rematch.cpp:4364-4373 (title "Player settings")
  Infinite health [TR], Soul core transformations, <one row per equipped core>, Soul core tuning > (10 sliders, soul_core_tuning_menu.h:30),
  Infinite Ki, Infinite ninjutsu, Infinite soul cores, Player damage, Player Ki damage, Player animation speed [TR]  (PstAddPlayerRows)
  Reset to defaults, Back

Return gate [BR] uGate (opened by the pause-menu Return object, rematch.cpp:4554-4578), title "Return - Training" / "Return - Boss Match"
  Fight again (...)      Active bosses > / Summon a boss here >   (Training mode)   OR   death rules (Boss Match mode)
  End stuck arena session, Return to where I was, Back, Close

Startup settings [BR] uSettings (kItSettings "Startup settings", groups General / Training (startup) / Boss Matches / Mod menu controls /
  Remote tools / Debug and logging, settings_catalog.h:6). Listed ONLY on uRoot (rematch.cpp:4343), the standalone-window root.
```

Alternate trees that also exist for the same rows: `Layout = 1` grouped tree (pRoot > Optimizations > Display Overlays / Frame Gen /
Load-Auto / Performance fixes / Extra performance / Experimental settings > CPU-GPU-Scheduling-Proton / Diagnostics / Next launch /
"Original controls (flat order)" / "Configuration by ini section") (menu.cpp:1402-1436, menu_pages.h:6-9), plus the two-page flat
switcher `kPage` "Optimizations page" (menu.cpp:963, 1045-1047).

## 2. PROBLEMS (concrete, with file:line)

Duplicates / same thing in two places
- D1 `Boss Matches` door on the root (menu.cpp:1396) AND inside Training (rematch.cpp:4358 kItRootRematch). Training > Boss Matches > boss > Train here loops back into Training.
- D2 `Summon a boss here` (hub, rematch.cpp:4357) and `Add a boss (slot X)` (Active bosses, 4406) open the same uSummon list under two names.
- D3 `Close` twice on the root: fixed top row kCloseTopRow (menu.cpp:1444) and bottom kClose (1399). `Mod menu` top row on the root points at itself.
- D4 Death/loot rules rendered on Boss Matches (rematch.cpp:4475), again on the Return gate (4571) and again on the never-listed uFightOpts page (4374-4378): the page exists, the door row `kItFightOpts` ("Death and loot rules") is never added by ItemsBase.
- D5 `Return to where I was` on Active bosses (4409), Boss Matches (as "Leave Boss Match", 4479), the hub (4359) and the gate (4559/4575).
- D6 Three parallel trees of the same EF rows (Layout 0, Layout 1, flat `pAll`, `pConfig` by ini section). The `kPage` row text still says "Boss Matches, Training and Display Overlays are on the first screen. Left/Right or A switches" (menu.cpp:963) but the row is never listed in layout 0.

Wrong grouping
- G1 `Time of day` / `Weather` (world state, Training feature) live under Display Overlays (menu.cpp:1349).
- G2 `Skip load tip screen` (a loading-screen auto-click) under Display Overlays (menu.cpp:1351).
- G3 `Freeze game while menu is open` (menu behaviour) under Optimizations > Game (SetGroup menu.cpp:1298).
- G4 `Input-to-screen timing` and `Count worker wakes` (log diagnostics) under Display Overlays > FPS (OverlayRow menu.cpp:1041-1042).
- G5 Death/loot rules come BEFORE the boss list on Boss Matches (rematch.cpp:4475 vs 4482): the player scrolls six rule rows to reach a boss.
- G6 Optimizations > CPU mixes 6 safe fixes with ~33 experimental A/B codename rows in one ~40-row page (menu.cpp:1339-1346 iterates kOrder 988-997). GPU does the same (look rows + experiments + Proton).
- G7 Frame Gen page is 3 status read-outs and 2 toggles (menu.cpp:1295); not worth a top-level door inside Optimizations.

Debug / dev rows visible to players
- V1 `Pad debug` on Input display (settings_catalog.h:40, IsInputDisplayKey menu.cpp:1287).
- V2 `Game's own performance display` row that does nothing ("Nothing to switch", menu.cpp:961).
- V3 `Reverse Mods` on the first page (menu.cpp:1460, 3704): a destructive two-press action next to Boss Matches.
- V4 All "Experimental (...)" rows and "Diagnostics / logging" rows (kOrder menu.cpp:990-997) shown unconditionally; only Rematch gates its dev rows behind `[Rematch] ShowUnverified` (rematch.cpp:4484-4488).
- V5 Startup settings groups `Remote tools` and `Debug and logging` (settings_catalog.h:6) are player-visible when reachable.
- V6 Dead rows still rendered with labels but never listed: kItMoves, kItMovesA/B, kItRootSummon, kItSumPassive, kItSumSlot, kItRepNow, kItRepSel, kItAggro, kItResetPos, kItShowTw, kItBrowse, kItFightOpts (rematch.cpp:5760-5812 region).

Too deep / reachability
- R1 Changing one boss move: root > Training > Active bosses > Boss A > move group > move = 5 levels (rematch.cpp:4404-4427, 4495-4512). Boss HP: 4 levels.
- R2 `Startup settings` (Rematch ini groups) is only on uRoot (rematch.cpp:4343); the hosted bridge opens pages 1/2/3 = uRematch/uPracticeHub/uPlayer (menu_bridge_impl.h:143), so in the normal in-game menu it is probably unreachable. Verify in a live run before fixing.
- R3 Every page spends 3 rows on `Mod menu / Close / Report` (menu.cpp:1443-1446) while a tab bar already shows the top level (menu.cpp:2428).

Unclear labels
- L1 Four verbs for "fight this boss somewhere": `Train here`, `Train in a boss arena`, `Train in this arena (no boss, no waves)`, `Summon a boss here`, `Fight now (boss arena)`, `Shrine before the boss` (rematch.cpp:5783-5785, 5856-5886).
- L2 `End stuck arena session` (rematch.cpp:5782) vs `Leave Boss Match` (5721) vs `Return to where I was` (5728).
- L3 `Optimizations` (page) vs "Performance fixes (safe)" (section) vs ini section `[Performance]`; `Display Overlays` holds non-overlays; `Active bosses (0 bosses)`; `Update + Page`; `Report`; `Startup settings`; `Behavior: Fight (CPU)`; `Switch every boss to Stand still after: N s`; `Staged start: Full climb`.
- L4 Codenames in brackets after every performance row (W3, R1, PJH ...) drawn dim but still on screen for players (ItemDef.tech, menu.cpp:846-850).

Inconsistent Back / Close
- B1 Return gate lists `Back` AND `Close` (rematch.cpp:4559-4560, 4576-4577); every other page has Back only (+ the fixed top Close).
- B2 Rematch Back label embeds its keys `Back   B / Esc / Backspace` (rematch.cpp:5810); host Back is just `Back` with the hint in the hint column (menu.cpp:3852-3854).
- B3 Hosted [BR] pages have 2 fixed top rows, [EF] pages have 3 (Report missing on hosted pages).

Controller vs keyboard prompts
- P1 Rematch row labels hardcode pad glyph names in the label text: `A: open`, `press A`, `A / Left / Right`, `Left: fight / Right: wait` (rematch.cpp:5584-5812 throughout). Keyboard users see "A". The host has a `Controller button style` setting (settings_catalog.h:30, btnstyle) that these strings bypass.
- P2 Host hint column also hardcodes `A: open` / `B` (menu.cpp:3854, 3713) instead of the device-aware prompt used elsewhere (menu_prompts_test.cpp covers the footer, not these).
- P3 Row footers still mention `A confirm / B back` (rematch.cpp:5096, 5142, 5182, 5416).

Other
- O1 `Save to ini` row on Optimizations (menu.cpp:1335) while closing the menu already auto-saves (comment menu.cpp menu_pages.h:71) and every Rematch edit is saved at once: two save models in one menu.

## 3. NEW LAYOUT

Seven top sections + Close, most-used first. Fixed top rows removed (tab bar stays); each sub-page ends with `Back`, root ends with `Close`.
Hidden-by-default rows sit behind one toggle `Show advanced rows` (General page) backed by a NEW additive key `[Menu] ShowAdvanced = 0`
(EngineFix) that the bridge also exposes to Rematch; Rematch keeps honouring its existing `[Rematch] ShowUnverified` as an OR. No existing
ini key changes.

```
Mod menu
  Boss Matches            > [BR] uRematch
      List: All / Multi / era / Dojos / Scrolls / Favorites
      <boss rows>         > Boss actions: Fight now | Start at the shrine before the boss | Practice this boss here (was Train here)
                                          | Practice in this arena, no boss (was Train in this arena) | Staged start (when staged)
      Leave this Boss Match                      (only while one runs; was Leave Boss Match / Leave Picture Scroll / Return to where I was)
      Match rules: death and loot   > uFightOpts (existing page): Boss death animation, Require final blow, Boss loot, No grapple in Ki break,
                                      Final-blow timeout, Zero-HP combo timeout, Reset to defaults, Back
      [advanced] Yamagata mission replay >, Open Battle Scroll replays, Play first owned Picture Scroll, Cancel pending replay
      Back
  Training                > [BR] uPracticeHub
      Practice a boss here           > uTrainHere      (was Train here)
      Practice in a boss arena       > uTraining       (was Train in a boss arena)
      Summon another boss            > uSummon         (one door; "Add a boss" row removed from Bosses in this fight)
      Bosses in this fight (N)       > uTools: Boss A: name (stance) > Boss A page, All bosses stop fighting after: N s,
                                      Remove all bosses (A, B), Back          (was Active bosses; Return row removed here)
            Boss A page: Behavior: Fights / Stands still, Follows and targets me, Repeat last move, Repeat delay, Reset boss position,
                         Health and Ki >, <move groups> >, Remove this boss, Reset to defaults, Back   (unchanged except wording)
      Freeze fight / Step one frame / Reset fight                      (unchanged, [TR])
      Time of day / Weather                                            (moved here from Display Overlays; same PS_TIME_OF_DAY / PS_WEATHER whats)
      Leave arena                                                      (was End stuck arena session; only when offered)
      Reset to defaults, Back
  Player                  > [BR] uPlayer, unchanged rows, with 3 section headers: Health and resources / Damage and speed / Soul cores
  Performance             > [EF] pSettings (was Optimizations)
      Preset
      Picture quality               > pSGpu look rows only: World shading, Cloud quality, Volumetric fog detail, GI resolution, Shadow distance,
                                      Shadow filtering, Geometry detail, Terrain detail, GPU particles, Ambient occlusion off, Fog grid scale,
                                      Reflections at half size, Render distance, Upscaler sharpening
      CPU fixes                     > pSCpu safe rows only: Message thread sleeps, Message wait style (Proton), Render wait timeout,
                                      Frame limiter sleeps, Frame-gen presenter sleeps, Fewer worker wake-ups, Batch GPU barriers,
                                      Sleeping limiter on Proton, Sleeping FG presenter on Proton
      Frame pacing and input delay  > pSGame: Smoother frame pacing (VRR), Frames queued ahead, Job-thread limit (handhelds), Keep 3 more pool threads,
                                      Frame generation (status), Frame gen: (status), Upscaler (status)     (Frame Gen page folded in)
      [advanced] Experiments        > pExperimental: CPU experiments > (pCpu) / GPU experiments > (pGpu) / CPU scheduling > (pScheduling) /
                                      GPU Proton experiments > (pProton) / Diagnostics and logging > (pDiagnostics: Input-to-screen timing, Count worker wakes)
      Back                                                            (Save to ini row removed; auto-save on close already exists; row id kept)
  Display                 > [EF] pDisplay (was Display Overlays)
      FPS counter                   > Show FPS counter, Show frame time, Show game speed, Move FPS counter, Nudge right, Nudge down, Text size
      Frame meter                   > unchanged
      Controller overlay            > Controller display, X, Y, size, opacity, pressed colour   (button style moves to General)
      Input history                 > Input display (history), side, rows; [advanced] Pad debug
      Game HUD                      > unchanged
      Back
  Camera                  > [EF] pCamera (was Camera settings), unchanged rows: Camera left / right, Camera down / up, Distance, Lens,
                            Camera effects off, Return to default camera, Back
  General                 > [EF] NEW page pGeneral
      Controller button style       (ButtonStyle, from Controller overlay)
      Mod menu reminder (top-right) (kHint, from FPS)
      Freeze game while menu is open (kFreeze, from Game)
      Skip load tip screen          (kAutoCont, from Display Overlays)
      Show advanced rows            (new [Menu] ShowAdvanced)
      Save files                    > (unchanged)
      Boss Match startup settings   > [BR] uSettings via new bridge action OpenSettings (fixes R2); groups Remote tools / Debug and logging [advanced]
      Check for updates             (was Update + Page)
      Report a problem              (was Report)
      [advanced] Turn mods off at next start   (was Reverse Mods)
      Back
  Close
```

Return gate (uGate): keep, but rows = Fight again, Bosses in this fight >, Summon another boss > (Training) OR Match rules > (Boss Match),
Leave arena, Return to where I was, Back. Drop the extra `Close` row (host Close/B at root already closes).

Row mapping of everything not named above: every kOrder row keeps its Id, ini key and ItemDef; only SetGroup()/OverlayRow()/PageRowsBody()
decide the page. `Game's own performance display` (kPerfNote) is no longer listed (Id kept). Layout=1 and pAll/pConfig trees untouched.

Label renames (plain words): Optimizations -> Performance; Display Overlays -> Display; Camera settings -> Camera; Controller overlay settings ->
Controller overlay; Input display -> Input history; FPS -> FPS counter; Update + Page -> Check for updates; Report -> Report a problem;
Reverse Mods -> Turn mods off at next start; Active bosses (N bosses) -> Bosses in this fight (N); Add a boss (slot X) -> removed (use Summon
another boss); Switch every boss to Stand still after -> All bosses stop fighting after; Behavior: Fight (CPU) -> Behavior: Fights /
Stands still; Follow and target me -> Follows and targets me; End stuck arena session -> Leave arena; Leave Boss Match / Leave Picture Scroll ->
Leave this Boss Match; Train here -> Practice this boss here; Train in a boss arena -> Practice in a boss arena; Train in this arena (no boss,
no waves) -> Practice in this arena, no boss; Shrine before the boss -> Start at the shrine before the boss; Death and loot rules -> Match rules:
death and loot; Startup settings -> Boss Match startup settings; Freeze -> Freeze fight; Step -> Step one frame; Save to ini -> (removed).

## 4. IMPLEMENTATION STEPS (for Opus; small, ordered, each buildable and testable)

Baseline first: build the candidate untouched, run `mod/Nioh3EngineFix/tests/run_menu_harness.py`, `run_menu_tabbar_test.py`,
`run_menu_prompts_test.py`, `run_menu_defaults_test.py`, `menu_preview.py` (screenshots of every page), and
`mod/Nioh3BossRematch/tests/ui_harness.cpp`, `page_defaults_offline.py`, `arena_train_rows_offline.py`, `unbeaten_menu_test.py`,
`menu_bridge_harness.cpp`. Keep the screenshots as the before set.

1. Labels only (no row moves). menu_pages.h:65-68 kPageNames; menu.cpp:2426-2434 (tab names), 2951-2954, 3696 (`Update + Page`),
   3704 (`Reverse Mods`); rematch.cpp:5782-5786, 5721-5728, 5856-5886, 5771 (`Active bosses`), 5751-5753 (stance timer), practice_stats_menu.h
   labels. Update asserts: menu_tabbar_test.cpp:259 (`Display Overlays`, `Frame Gen`), menu_harness.cpp:191 (`Mod Menu > Update + Page`),
   menu_mode_host_test.cpp:439 (`Camera settings`), rematch_harness.cpp / ui_harness.cpp string checks. Risk: inputlog page names change
   (menu.cpp:1427 NoteMenu) -> check inputlog_test.cpp.
2. Add `pGeneral` to `MenuPage` (menu_pages.h:6; insert before `pCfgFirst`, add name to kPageNames in the same slot). All kPageRow/kBackRow/...
   constants are derived and presentation-only, but grep tests for literal page indices (menu_home_test.cpp, menu_tabbar_test.cpp). Build
   pGeneral in PageRowsBody (menu.cpp:1328 branch): kSettingRow(ButtonStyle), kHint, kFreeze, kAutoCont, new ShowAdvanced row, kSaveFilesRow,
   kUpdatePageRow, kReportRow, kReverseModsRow, kBackRow. Remove those rows from pDisplay/pOvFps/pSGame/pOvController/pRoot (PageRows 1458-1460).
3. Remove the 3 fixed top rows: PageRows (menu.cpp:1441-1462) and FirstOwnRow (1463, `n > 3`), ResettableRow loop start `i = 3`, hosted
   pages rematch.cpp:5545-5546 and their sel offsets (g_sel == -2 / -1), UiBack. Keep kModMenuRow/kCloseTopRow/kReportRow ids (used by
   menu_home_test.cpp, menu_tabbar_test.cpp); make them unlisted. Risk: controller navigation and `Home` of the tab bar assume row[0..2];
   run nav_repeat_axis_offline.py and menu_pad_input_offline.py. Root bottom `Close` stays; B on root still closes.
4. New `[Menu] ShowAdvanced` (settings_catalog.h add one Def in group "Mod menu controls"; default 0; additive key). `Visible()` (menu.cpp:1023):
   IsExp rows, kGR*, kXTS/kXWS, PadDebug and kReverseModsRow require it. Wire SetGroup (1294-1303): experimental -> pExperimental sub-pages
   (reuse pCpu/pGpu/pScheduling/pProton/pDiagnostics and their existing names); pSCpu keeps kW3..kWk, kGpu, kL1Wine, kFgWine; pSGame also
   gets kFg/kFgState/kUpscaler (fold Frame Gen); kF1 -> pSCpu. Remove `kSave` from pSettings body (1335) - verify the close-time auto-save
   path really writes everything Save wrote (menu_defaults_test.cpp, run_settings_preservation.py), otherwise keep the row under General.
   Drop kPerfNote from pOvFps. Tests: extraperf_menu_test.cpp (preset still first row of Performance), menu_mode_test.cpp.
5. Pass ShowAdvanced to Rematch through the bridge snapshot (rematch_menu_bridge.h / menu_bridge_impl.h; append a field, bump the size
   check per notes/rematch-handheld-menu-host.md "versioned, size-checked"). Rematch: `g_cfg.showUnverified || hostAdvanced`.
6. Boss Matches page (rematch.cpp:4474-4490): replace `AddBossFightItems(add)` with `add(kItFightOpts)` placed after the boss list and
   Leave row; kItFightOpts -> EnterPage(uFightOpts) already exists in UiAct (grep `kItFightOpts`). Update PageHasDefaults
   (page_defaults_menu.h:8: uRematch no longer has defaults) and page_defaults_offline.py. Gate page (4571): same swap for mode 1.
   unbeaten_menu_test.py:95 asserts the literal source `add(kItFight + g_uiBoss);\n            add(kItBossTrainHere + g_uiBoss);` -
   do not reorder uActions rows, or update that assert with the test.
7. Training hub (4349-4362): new order; delete `add(kItRootRematch)`; add `kItPstBase + PS_TIME_OF_DAY` and `+ PS_WEATHER` (PstLabel already
   has them, practice_stats_menu.h:79; confirm PstSet/Left-Right handling covers those whats, which EngineFix currently drives via
   kTimeOfDayRow menu.cpp:1175). Then drop kTimeOfDayRow/kWeatherRow from pDisplay (menu.cpp:1349; keep ids). Active bosses (4404-4411):
   remove kItSumAnother and kItReturnHome rows (both reachable from the hub / Leave row); keep ids. Gate (4559-4560, 4576-4577): remove kItClose.
8. R2 fix: new bridge action `OpenSettings` (append to the rmb/mb enum only; menu_bridge_impl.h:143 maps 1/2/3 -> add 4 = uSettings),
   host row in General posts it like kRematchRow does (menu.cpp:2225-2231). Hide groups Remote tools / Debug and logging unless advanced
   (rematch.cpp:4525-4528 loop).
9. Prompts: strip embedded `A: open`, `press A`, `A / Left / Right`, `B / Esc / Backspace` from rematch row text (rematch.cpp:5584-5812) and
   footers (5096, 5142, 5182, 5416); let the host hint column / footer show device-aware prompts (host already has btnstyle; make
   menu.cpp:3854 `"A: open"` / 3713 go through the same helper menu_prompts_test.cpp exercises). Rematch harness string asserts will need
   the new texts.
10. Dead-row cleanup (rematch.cpp:5760-5812 and matching UiAct branches): kItMoves, kItMovesA/B, kItRootSummon, kItSumPassive, kItSumSlot,
    kItRepNow, kItRepSel, kItAggro, kItResetPos, kItShowTw, kItBrowse. Grep `EnterPage(uMoves` / `uMoves)` first; keep the constants
    (negative ids are referenced by tests and static_asserts at 3755).
11. Regression (standing order): untouched-parts tests for all three DLLs, no_hardcode_audit copied into the candidate first
    (memory: audit-root pitfall), menu_preview.py after-set compared with the before-set, then the pc/pc2 gameplay gate with a
    controller AND keyboard: open every page, Back from each, B at root closes, Return gate from the pause menu in Training and in a Boss Match.

Rules honoured: no per-boss hardcoding (boss rows stay catalogue/roster driven); every existing row still reachable (dev rows via Show advanced
rows / ShowUnverified); no ini key renamed or removed (one additive key `[Menu] ShowAdvanced`); row Ids and bridge enums append-only.

## 5. STYLE (human #107935, 11-15 11:55 pm EST: "I'd take that sort of style")
Target = the mockups in notes/menu-reorg-107840/menu01-07.png (script mock.py, colors there).
- Background panel (16,16,22) behind a page panel (28,28,38); title in gold (225,185,90), larger scale.
- Tab bar across the top with the 7 section names; current section filled (150,110,40), others panel color.
- Selected row: dark-gold bar (60,50,30) full width; submenu rows end with a gold '>'; values right-aligned in gold as '< value >'.
- New/renamed rows green (110,200,120) only for the first release after the change (optional); advanced rows purple (170,140,210); Back dim.
- Footer: one caption line + button hints from the host's Controller button style (no hardcoded A/B text).
Renderer limits (EF osd.h: DrawRect + DrawText bitmap font osd_font.h): rounded corners and the proportional Segoe font in the mockups are
NOT available as-is. Do square rects first (same colors/layout); rounded corners via corner rects or a font/atlas upgrade only as a later step.
Keep it in EF draw code (menu.cpp ~2170-2380); rematch pages already render through the host, so one style covers all 3 mods.
