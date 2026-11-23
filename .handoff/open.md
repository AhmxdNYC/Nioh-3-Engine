## LEFT TO DO (human #93660, 2026-11-12 6:10 pm EST, pc/0). Every item has an owner and is being checked; tick when done.
- [ ] Release the Boss Match fix (Dojos 100%, Boss Match 100% except RB3FE, Training in those arenas; human #93578). Built and proven on Main PC, not public yet | owner pc lead -> regression-pc check, installer-pc packaging, then release with the human's OK
- [x] KL_Div=4 CPU setting | PC2 +7-8% fps (#93495) recheck running on performance-pc2; Ally NO GAIN (#93667: 47.36 vs 47.32 fps, CPU flat, GPU-bound) | decide ship-on-PC-only after PC2 recheck | CLOSED 11-12 no gain all devices (#93770)
- [x] FPS-cap timing margin L1_MarginUs 300: FAIL #93701 (CPU -1.7%, misses 60 cap); keep 1000
- [x] Name lookup caching | performance-pc, queued after L1 margin (#93615 order) | DROPPED 11-12 #93764 (<=0.6% main thread)
- [x] UI copy narrowing | performance-pc, queued | DROPPED 11-12 #93766 (0.19% main thread)
- [x] Page-cache wait (DeepSeek draft 3, 30faa80d, 1.7.61 base): check for existing patch, timing check, port only if worth it | performance-pc, TASKED 11-14 3:59 pm EST after PP FAIL #102259 | DROPPED 11-12 #93728 (0 calls/s in gameplay); re-confirmed 11-14 #102274
- [ ] 4-core player low-visibility option (human #102540/#102575/#102580/#102601, 11-14 5:34 pm EST): opt-in short view distance so only the nearby area / boss arena is drawn; also shorter grass/prop distance, fewer particles. Gameplay and speed must stay identical (no logic-tick change, #102575); look may drop. Not built; first find the draw-distance control (render RE). LEAD: human #102608 sent third-party Nioh3Fix 0.0.6 (bridge file #7654, unpacked D:/nioh3-re/ext-mod-102608/x) whose ini has ModelDrawDistance/PartsDrawDistance/HLODDrawDistance/BackgroundMeshQuality/TerrainQuality/CharacterDrawDistance/MotionQuality multipliers 0.1-10; RE its Nioh3Fix.asi for the patched addresses instead of searching blind. Keep CharacterDrawDistance 1.0. Human #102635: ours must beat Nioh3Fix = in-game SLIDERS per draw distance, not presets (human #102646), plus one "Arena size" button that snaps all sliders to cover a typical arena (human #102651; arena size not measured yet, derive from Boss Match logs player/boss positions per arena), live without restart (no ini editing), each setting measured for CPU/fps and useless ones dropped, stacked with our CPU fixes | owner pc lead -> performance-pc
- [ ] Shadow cache (static-light shadows kept, redraw only changed) | not built; LAST per human #93613
- Dropped (no re-test): light-copy shrink (#93515, 0 copies/s), CapSleep (#93605, already shipped as L1). Parked: world bosses (#92460/#93578), Ally crash 131CA9 (old Nov 7 build, #93576).

## BOSS COVERAGE (human #90129 via ops #90131; standing goal 100%). Pass = enters, spawns, finishes cleanly (win + exit home). TR = Training works there. Updated 2026-11-12 2:40 am EST by pc/0.
Dojos are Main PC only (human #89887); open world runs on all 3 devices. Cells: PASS / FAIL / START (fight started, win unproven) / - (untested).
Open world (catalogue #6815, 19 bosses; dev build broken on Ally #90035, waiting on cx-pc #90042):
| boss | id | Main PC | PC2 | Ally | TR |
|---|---|---|---|---|---|
| Yasha | 560912 | - | - | FAIL #90035 | - |
| One-eyed Oni | 482715 | - | - | FAIL #90035 | - |
| Gaki | XGaki | - | - | - | - |
| Waira | XWaira | - | - | - | - |
| Tsurara-Onna | XTsuraraOnna | - | - | - | - |
| All-Seeing Mage | XAllSeeingMage | - | - | - | - |
| Usui Sadamitsu | XUsuiSadamitsu | - | - | - | - |
| Enki | XEnki | PASS #89249 (BR #6814) | - | - | - |
| Kyokotsu | XKyokotsu | - | - | - | - |
| Karasu Tengu | XKarasuTengu | - | - | - | - |
| Yamainu | XYamainu | PASS #89249 (BR #6814) | - | - | - |
| Tesso | XTesso | - | - | - | - |
| Ninja Yoki | XNinjaYoki | - | - | - | - |
| Blood-Spurting Ogin | XBloodSpurtingO | - | - | - | - |
| Gaki Chief | XGakiChief | - | - | - | - |
| Yoki | XYoki | - | - | - | - |
| Nuppeppo | XNuppeppo | - | - | - | - |
| Aggrieved Warrior | XAggrievedWarri | - | - | - | - |
| Green Shadow's Blade | XGreenShadowsBl | - | - | - | - |
Dojos (Main PC only; BR #6835/TR #6836/TSV #6837; notes/dojo-rematch-89218.md):
| row | name | Main PC | TR |
|---|---|---|---|
| RF5E5 | Yagyu Munenori (Novice, lesson) | START (selftest load #87276) | - |
| R6E65 | Yagyu Munenori (Adept) | PASS (win 11-06, older build) | - |
| R1A5E | Yagyu Munenori (Veteran) | - | - |
| RD759 | Hattori Hanzo (Novice, lesson) | PASS #89417 | - |
| RE18C | Hattori Hanzo (Adept) | PASS #89417 | - |
| R9BE4 | Hattori Hanzo (Veteran) | START #74804 | - |
| R26F3 | Saito Fuku (Novice, lesson) | START (selftest load #87276) | - |
| RA0A1 | Saito Fuku (Adept, lesson) | START #89417; win run #90078 | - |
| R2826 | Saito Fuku (Veteran) | - | - |
Coverage now: open world 2/57 device cells, Dojos 3/9 (PASS on current or older build).

- [x] Fair speed test on this PC - Play the same fight twice, once with the CPU fixes off and once on, to check whether the ~3 % CPU saving is real. Earlier runs compared different fights, so they don't prove it.
- [x] Boss rematch after dying - Check that dying in a boss rematch no longer leaves an empty arena. The fix is installed, but nobody has died in a test since.
- [x] Fix PC2's test recorder - recovered: 441-frame smoke and six comparison captures delivered (#16098/#16104, report1253, f322f34). Matched-combat validity remains separate.
- [x] Re-test on the Ally - Steam updated the Ally's Linux runtime mid-test. Set up the test for the new runtime (don't undo the update), then compare fixes off vs on.
- [x] (Training-mode boss summon DONE: none exists, ally #4313) Later work - skipping the whole mission load screen (not built yet), and graphics/shader speedups.
- [ ] (user #4114/#4115) Cut FIRST load of the boss rematch (37 s on PC; retry loads already 3-6 s). Next: re-run tools/rematch/fightnow_test.ps1 retry mode, split the 37 s by phase (menu->request, native mission load, request->control); compare a warm first load vs cold; then test the cheapest cut. Note: user says the game has no rematch and our mod added it - first load is the point.
- [ ] Rematch in-place restart via scriptcreate (pc2 TASK sent 2026-10-14)
- [ ] Forced Yamagata move live test + move list (pc, after unsummon)
- NOTE: pc2 OFF testing (user #4375); rematch task moved to ally + pc

- [ ] (user #4398) Fight-now menu: also skip the loading screen (enter boss fight directly / instant retry), not only after-death retry. Reuse RetryInPlace + summon work; test in game on all 3 devices.
- [ ] (user #4430, 2026-10-14) Live arena swap: change training/current map into a boss arena in place and spawn boss, no load screen. ally TASK (opus) sent for test; pc does static RE in parallel. Builds on RetryInPlace #459 + ally spawn work.
- [ ] (user #4747) Spawn a boss anywhere in the open world (no arena swap): the open-world-with-boss bug from #4732 as a feature. Reuse the swap/spawn work; ally to test a 'summon here' command on the current map after the HP bar and arena fixes.
- [ ] (parked behind boss swap, 2026-10-15) Re-post AABB bench on pc (#54 failed) and ally (#58 released); AABB pc2 done +6.3%. TG: pc2 CPU -27%, fps flat, 1%low worse -> keep off. Re-post after the swap works.
- [ ] (user #5024) Look at optimizing game logic while keeping it at the full 60 Hz tick rate (cheaper ticks, same behavior). Start from JT join-wait probe (logic thread waits ~1.44 ms/frame on job joins), AABB, BPO; notes/whats-left.md rows 4-6. After boss swap works.
- [ ] (user #5332) Optional polish: after no-load summon, fight inside the boss's own arena cell (unload/hide the open world except arena) instead of the open-world terrain. The older arena-swap code (Nioh3Training ARENA path) already warps to the arena cell in 4 s; consider combining it with the no-load summon + SetAwareFlg.
- [ ] (user #5335) SEPARATE feature from boss rematch: 'summon boss anytime' (spawn in current map, 4942/510 summon-here code: train <R-id> [dist]). Keep as its own command/menu; rematch Fight-now = full arena.
- [ ] (user #6092) Soul cores: find how the game summons/uses soul cores (summon animation, script/event ids, call path) so a mod can trigger it. Static RE first (animation/effect ids, the soul-core use function), then a live trigger test. Owner: pc2 or ally (whoever resets first).
- [ ] (user #6466) Training mode: live arena switching in real time (background preload of the target arena + swap) with a transition effect (fade/whiteout). Start after the load-skip is confirmed on 3 devices; reuse EarlySwap + prewarm (#544) and oworld white-out param.
- [ ] (user #6618) Boss Rematch hosted menu: add top-of-menu toggles for CutsceneOnKill / DeathBlowRequired / BossLoot (Training 'deathcfg <loot> <blow> <cut>' pipe cmd exists). Start after the user's toggle play-test ends; branch pc/toggles-merge + pc/hosted-merge.
- [ ] (user #7465) Toggle KiBreakNoFinisher: no grapple finisher while boss Ki-broken + gold Ki bar; at HP 0 gold Ki drains, boss dies only if not hit for ~2 s (combo extender). Build on pc/ki-break-detect. Opus agent started 2026-10-15.
- [ ] (pc, 2026-10-16) In-place swap hides a live fight: screen black 26-40 s while boss active (ally #8076; cross-world 26 s, same-world 39 s to arena fade-in). Find why arena settle takes 20-39 s and gate boss/player control until visible (or fade in earlier). Test on pc.
- [ ] (user #9140) Cross-world load NOT solved: find what keeps the screen black 20-40 s after GAMESTATE[2] cleared (notes/cross-world-load-2026-10-16.md). After the 4 boss fixes (#9133/#9135).
- [x] (closed human #60212 10-31, old) (user #9296) Tie screen recording to the boss mod: auto-start ~3 fps capture (see captures/pc/rec9291/rec.ps1) when the Boss Rematch menu/Fight now is used, stop on arena ready +N s; include in next build.
- [x] (user #9330) Return/Rematch physical object spawns in the air: make it spawn on the ground at the same x/y spot. [done 2026-10-17, see done.md]
- [x] (user #9329/#9331) Move lists for all other bosses -> pc2 board task posted 22:3x (notes/boss-movesets.md). [done 2026-10-17, see done.md]
- [x] (user #9595/#9596) Debug hotkey: keyboard shortcut to freeze the whole fight + player mid-animation (time scale 0 / actor pause), toggle; to help debug stagger/death-pose. After the current build. Check existing pause/timescale code in Nioh3Training first. [done 2026-10-17, see done.md]
- [x] (#9596) debug hotkey 2: 'BUG HERE' marker (log line with boss state + screenshot jpg) alongside the freeze toggle [done 2026-10-17, see done.md]
- [ ] (user #9608) Cut Return time (now ~51-61 s: WarpOtherWorld back to home world 0xE0, XWORLD return +51 s). Try same tricks: lift cover early / near-area-only load / in-place swap back instead of world warp. Measure first (RETURN log lines). | 11-14 5:25 am: PC2 split #99603: 31-76 s, 80-86% is an unlogged gap after reaching 0xE0 (22-65 s, grows per session); probe build tasked to pc2
- [ ] (user #9585-#9596, queued 2026-10-17 pc) NEXT BUILD list on pc/hp0-stagger (after hp0full 6fc9200b installed): (1) slider in hosted Boss Rematch menu for CutsceneOnKill finisher window (FinisherKillTimeoutMs, live, ini-backed) + KiBreakHp0WindowMs slider; DO NOT change DeathBlowRequired/CutsceneOnKill/BossLoot behaviour. (2) gate object: ALWAYS our Return/Fight-again menu, never native 'end mission' (mask for whole gate radius, open on enter/interact). (3) measure native mission kill->fade-start->gone timing; match KillFadeDelayMs total. (4) hotkey freeze fight+player (log boss action/motion/Ki/HP) + hotkey 'BUG HERE' (log line + screenshot jpg next to recorder output). (5) cut AreaWaitCrossMs/settle on lsend path so control follows black lifting (pc2: black 8.5 s, control 15.5 s). (6) ki-break death-pose-in-normal-break (KiBreakKneelBlockAll built, unproven).
- [x] (user #9658) Move the F9 freeze helper into the mod (Training/EngineFix hotkey), keep boss-state log + screenshot on freeze [done 2026-10-17, see done.md]
- [x] (user #9664/#9670) Return object: sometimes regular native menu instead of ours, or both at once. Make ours always, suppress native. Use user log 01:25-01:37 (captures/pc/freeze9655/Nioh3Training.log). | 11-14 4:51 am: already fixed, PC2 live 6/6 on v1.8.17 (#99492, file #7481); UX gap Leave row on Boss tab -> pc2 #99492 task
- [x] (user #9685) Boss HP bar shows earlier than boss spawns/is visible after Fight now. Hide/delay the boss bar until boss is live+visible (check log timing HUD bar vs spawn in captures/pc/freeze9655 log). | 11-14 4:19 am: closed again, base RC already OK on PC2 (#99413); fix 43c092d7 parked, not merged
- [ ] (user #9715) Boss move data near the action code: static RE of the boss action table entries (358528 / state+0xE8 id -> record) for hit count, damage, anim/name hash, duration fields. Board task posted.
- [ ] (user #9946) Ki-break death-pose fix must not be hardcoded ids: learn per-boss good/bad break action from the boss's own breaks (bits/state), keep ini ids only as override. After testing ffc68ea.
- [ ] (user #10092/#10108, via pc2) Training menu features after move toggles + menu crash #10426: idle stance (boss summons bosses by default, CPU stance normal), spawn several bosses at once, despawn all, rewind game time, play a chosen boss move at a chosen time in X s. Goal: SF6-style training mode. cx-pc multi-boss study: notes/training-multi-boss-despawn-study-10541.md.
- [ ] (user via pc2 #10667) Benchmark: SF6-level Training mode: record/replay, repeatable drills, clear feedback - folded into the Training backlog item above; order stays: move toggles -> menu crash #10426 -> rest.
- [ ] (user #12951) Training: 'Repeat move' - choose [+ adjustable delay between repeats, #12958] a boss move in the menu and the boss performs it on loop (force action every N s, toggle off to stop); then re-shoot 1 min demo inside a boss arena (teleport/fight now), no open-world. Existing: forced-move code (see Forced Yamagata item), move toggles.
- [ ] (user #13262/#13265) object menu mode-aware (Training items in Training, boss-fight items in Rematch); Rematch toggles also in Training. Queue after always-there object + menu restyle workers land (same files).
- [ ] (user #13274) custom menus (Rematch/Practice/object menu) pause the game mid-fight; queue with #13262/#13265 after current workers land.
- [ ] (user #13279) menu clarity pass (plain labels, grouping) + final: install and launch the combined next build for the user
- [ ] (user #13293) Menu clarity pass for the combined build: Practice menu rows plainly labeled: "Train here (where I am)", "Training Mods", "Summon a boss where I am", "Boss Matches" (rename user-visible 'Rematch' -> 'Boss Matches'). Training: obvious way to set boss to do move X + adjustable gap between repeats. Boss Rematch toggles must also exist for summon-anywhere and (pretty much) everywhere. Fold into pc/menu-design integration.
- [ ] (user #13296) Settings menu clarity pass too (plain labels/grouping), same as #13293 for all custom menus.
- [ ] (ally #89 issues, build c3ab21b) (1) Training arena entry: boss attacks ~1.5 s after cover (BOSSHOLD released) though stance=IDLE -> #13167 unmet. (2) Boss A/B move rows 'empty' + Summon page 'into Boss A (now: empty)' while arena boss exists. (3) Long arena names truncated in list. Untested: Summon a boss, repeat-move drill, ReturnGate object, Despawn.
- [ ] (pc2 #13431 on c3ab21b) Boss Matches Yamagata R1CBC boss stays passive (kneels, never attacks; AI on +2.6s, aware x3); summoned bosses (Summon a boss here) log 'SPAWN: no new actor with handle 1000F22E in 3 s' and never register in slot A/B; Anayama R1C7C unknown boss; root Mod menu page still old wide box. Menu-fixes-1 a3a7768 may fix slot issue (verify).
- [ ] (user #13531) Running build c3ab21b has Boss A/B moves empty in Training arena; fix is menu-fixes-1 a3a7768 (arena boss=Boss A). Ensure next build includes it + Return-object float fix; install when game closed.
- [ ] (user #13537) Polish footer control icons in custom menus (A/Back/Change/Page glyphs look rough; Back icon missing). See captures/shot-13537.png. Include in next build.
- [ ] (user) final build pc/menu-final-2 b849cb3 (Engine 7e2527f5 / Rematch 047ceb0d / Training 861cdc52) auto-installs when game closes (captures/install_menu_final2.ps1 running); then relaunch + ping user. Ally errored on #89-like tasks; pc took over.
- [ ] repeat-move UX fix 026aef4 (pc/menu-final-2; Training 3aa4db0e Rematch 9fd2f209 Engine 9fe82515) auto-installs on game close (install_menu_final3.ps1); then relaunch+ping user; verify drill plays after menu closes.
- [ ] (user #13799) Return object native-menu root cause + fix: Opus agent a9bf6722 on branch pc/gate-native-suppress (2026-10-18 23:40). Also mode-page-on-shortcut. Then integrate with pc/stale-boss-mode, pc/menu-final-2 (+ back-nav work), pc/move-filter; install on game close, relaunch, tag user.
- [ ] (user #14153, after token reset) SF6-style frame meter overlay: phase 1 boss-only bar from action records, phase 2 player side + active windows

- [ ] #14655 Remove death/self-kill sequences and tiny twitch moves/sequences from boss selection; identify latest killing sequence from logs, validate useful chains remain. Assigned pc via #14657; main-PC install/test.
- [ ] #14742 Actual native Yoritomo combo logic: pc2 #14744 decode source-backed links, conditions and recovery/end; pc #14707 validate locally. Do not label inferred family chains verified.
- [ ] #14776 Identify previously built 30 Hz CPU-saving feature (~26-27% remembered); ally audit #14777 checks historical OFF/ON evidence, current build inclusion/enabled state and gameplay coverage. Await audit before scheduling any new benchmark.
- [ ] #16376/#16282/#16296/#16332 Current training bugs: repeat Aggro me, multi-hit move/sequence classification, dead boss hanging. Local cx-pc assigned #16389; capture live logs first, preserve active player session, then smallest fix and runtime validation.
- [ ] #22389 Practice features: boss speed multiplier, infinite boss Ki, boss Ki drain, infinite player Ki, infinite player ninjutsu, damage multipliers boss/player | build + test when game free | pc
- [ ] Frame-data fps-independence check (#22575) | rerun the frame probe on 0x4E6E / 0x4E4E with the game at 30 and 120 fps target; numbers must match the 60 fps run (95/157/177/333 and 49/146) | pc
- [ ] (user #22796) SF6-style frame meter for the PLAYER side (startup/active/recovery from player action records), alongside boss meter | pc
- [ ] (user #22826) Controller overlay setting in mod menu: pick PS-style or Xbox-style button glyphs (also for menu footer icons / input display) | pc
- [ ] Integrate + install + in-game test: pc/btn-style-22826 ebd8453f (PS/Xbox button style, EngineFix asi 73B7FAFB) and pc/player-meter-22796 b96fa07a (player frame meter; Training+Rematch asi) onto pc/practice-stats-22389 (e3fff864 already has Ki damage x + one-hit Ki break, untested?). Install when game closed. | pc
- [ ] Hold Ki break toggle (#22918) | Practice Boss page: once a boss is Ki-broken, keep it broken (stunned) while ON; build on NfBreakNow/NfPrimeHold in training.c | pc
- [ ] (user #22920) Training completeness: Stage picker (18 arenas), boss phase/HP preset, position save/load, damage+combo counter, consumable restore; see notes/training-completeness-20261022.md | pc
- [ ] Training time-of-day slider (#22942) | find the game's time-of-day/sky state, add Practice slider | pc
- [ ] (user #22942/#22943) Training: Time of day slider + Sky picker. First research: find game time-of-day/sky/weather params (grep notes/ and decomp for sky, TimeOfDay, weather, atmosphere); report feasible or not | pc
- [ ] Follow-ups from integ-22826 test: boss frame meter fake run on next idle action 0x2 (hitbox still open at action change); PS style still shows LB/RB + 'press A' text; startup counts from 1 vs active from 0 display; one-hit Ki break stays on in memory after ini=0 until restart. Then re-arm captures/pj_bench_105.ps1 (PJ bench) | pc
- [ ] (user #22952) Live controller INPUT DISPLAY overlay (top-left pad graphic showing pressed buttons, PS/Xbox style per ButtonStyle) - NOT built; current ButtonStyle only changes menu glyphs | pc
- [ ] (user #22984) Verify ALL catalogue bosses are summonable / startable in Training and Boss Matches; document which need a cleared mission (Boss Matches replays cleared missions; 100% save likely unlocks them) vs summon-anywhere; produce per-boss table | pc
- [ ] (user #22978) After input-display overlay is built+tested: record ~1 min video of new features + controller display, send via tools/notify/notify.py | pc
- [ ] (user #23013/#23017) Ki model (examples Kagetoki, Yagyu Jubei): verify boss Ki can go negative (stun-extend pool) - find the field, log it during breaks, consider making Hold Ki break use real negative-Ki mechanic and add Ki-floor slider | pc
- [ ] From padviz test (2026-10-22): Hold Ki break doesn't hold (break recovers in 15-400 ms, no hold log lines); Infinite health didn't keep player alive vs Yamagata; boss meter stuck on 'waiting' once + always labelled 'Boss A'; Yamagata moves 'Unlabelled action' + right column cut off; ToD only while Training boss alive. padviz pc/padviz-22952 e3cdeeb1 installed (EngineFix 10EDC415), needs merge into integ branch. Parallel session nioh3-engine-42 also swaps builds: coordinate via test-active marker. | pc
- [ ] (user #23132) Rebuild controller display like Gamepad Viewer DualSense skin (Cubio300/CrazyGrape gist, CC BY 3.0): sprite layers, 807x651 layout in notes/gamepadviewer-research-20261022.md; also an Xbox skin; credit authors | pc
- [ ] (user #24182) AFTER all else: audit every optimization setting: how many on by default (installed EngineFix.ini on pc: 193 keys, 59 =1, 75 =0), and measure which non-default ones help even slightly (all 3 devices) so they can become default. Start from notes/optimization-shortlist-20261021.md, settings-cost.md, untried-optimizations-2026-10-15.md.
- [ ] (user #24195) AFTER #24182 settings audit: final benchmark default Nioh 3 (mods off) vs optimized mod build, same boss fight, 3 runs/arm, on pc + pc2 + ally; report worst frame ms, frames>33ms, 1% low, avg fps per device.
- [ ] Menu 2 pages | F10 > top row 'Settings page' Left/Right: page 1 = same picture, page 2 = look-changing | pc (test by agent once game free)
- [ ] TRACK settings audit stage 1 (human #24532): pc 9/69 valid at 23:40 (running, ~5 min/run). pc2 + ally: no csv on origin as of 23:42, asked Q. Each hour: git fetch, check captures/audit-screen/results-{pc,pc2,ally}.csv on origin; chase silent devices; after all 3 done -> stage 2 confirm, defaults, final stock-vs-mod bench, then 10x ping.
- [ ] integrate+verify automatic hit/damage (cx-pc/action-record-24680) in game after the pc audit ends
- [ ] (user #24936) Add CPU temp + package power logging to the final 3-arm bench (fps-capped run, stock vs mod), all 3 devices.
- [ ] (user #24939) Audit/bench rows only have avg fps/1% low/frametime. Add CPU util, GPU util, CPU temp, power per run to final bench (all 3 devices). Mod has [Profiler] MemStats etc but no per-run util in results.
- [ ] publish final-bench graph (default vs optimized vs optimized+visuals, FG on/off, 3 devices) to GitHub README after all results | user #26593
- [ ] collect per-device in-game settings (pc2, ally asked #user 26922) and list them on the GitHub chart | user #26922
- [ ] NEXT RELEASE GATE #87687 (installer-pc #94203): in-game out-of-date popup self-installs + relaunches; Dev lead live check + Discord video before shipping the final build (BR f1adb650 + TR bcc4783e). Restore Main PC Steam folder to the release files before any release install test.
- [x] BR follow-up (regression #94992, after 1.8.13): g_msg is 160 bytes so the RB24D confirm text + warning is cut ('...Death Veil may not appe'); make it 256 or shorten the warning. Also call WarnFooter on the Train-here row footer (rematch.cpp ~:5260). Owner: pc dev (non-author reviews). | 11-14 4:10 am: DONE on branch pc2/gmsg-256-99125 fc039b7b (BR c5520b17): PC2 live PASS #99184, regression PASS #99384. Merged to main 2a90f6e4 after v1.8.17 (for 1.8.18).
- [x] Frame-wait park (FWP) EF 125f90e9: DROPPED - PC2 GPU-bound null #101741, Main PC no CPU win #101975
- [x] 30 fps load-screen-only cap: FAIL #102012 (loads 44% slower). Follow-up: 90/120 load-window cap, performance-pc
- [ ] SAVED (human #107956, 11-16 12:05 am EST): mod-menu reorg + mockup style, after v1.8.20. Plan notes/menu-reorg-107840.md (layout s3, steps s4, style s5), mockups notes/menu-reorg-107840/. Opus builds when the human says go. | Main PC | pc
- [ ] BACKLOG (RE, waits deepseek credits / #92352) GH #132 crash Nioh3.exe+0x591f4d read -1 (fn 0x591ea4 <- 0x590cc4 <- 0x58fc4c <- 0x58fab8 <- 0x58f614 <- 0x483884), EF 1.8.23, install 22cdafe8. Game-only stack, no mod patch in range. 0.26 s after BossRematch 'FormPrimary: keep; secondary spawn desc=3E333' at dist 0.0 (SoulCoreForms=1, take=0). Map what nulls rax/rbx/rsi there and whether a 2nd spawn during a held form reaches it. Issue kept open; reports-pc owns.
- [ ] Updates-count badge (human #121461, pc/2 NOTE #121464) for the next update after 1.8.24: site endpoint counting updater fetches per anonymous install id (Blob, like site api/crash.py) + shields endpoint badge in the public README next to downloads (71167d38) + Updater (e20d4632) calls it. Owner pc dev; gate regression + installer-pc. BUILT offline 78c66240 (pc0/updates-badge-121461): site/api/updates.py + Updater count_update (version-only, opt-out). Regression GO #121505 (7cb796b5), merged to main; still to do at next release: site deploy (needs Blob token on Vercel) + README badge + ship in next release with human OK. Installer live checks must use --no-count.
- [ ] (pc, from human #121835 / pc/2 #121839) Menu action log names no row/page: 'MENU: closed (rematch action)' then TR FIGHTNOW started a real Hanzo Boss Match where the human expected a Training dummy. Add page + row label to the TR/EF action log line; then fix the dummy-vs-real route once the human says which row he pressed (TO-USER #121838). Giant Toad: hit count known 0 (ban #85979 stands).
- [ ] AFTER usage reset (human #130308 yes to #130311): fold notes/ findings since 11-12 (hold-re-120730, arena-fightstart, playerlog RE) into re/symbols TSVs, rebuild exports, sync to public Nioh-3-Engine via export_public.py. Owner pc (any slot). No game needed.
- [ ] AFTER usage reset (human #130321): write docs/engineer/LIVE-TO-RE.md (live value / Player log line -> find writer -> x64dbg/Ghidra -> symbols row -> pattern in mod; worked example from notes/hold-re-120730.md) and publish performance-features.md + performance-measurement.md to public Nioh-3-Engine. Do with the symbol DB catch-up.
