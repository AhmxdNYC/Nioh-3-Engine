# Handoff pack (nioh3-engine, 2026-10-23 15:17)
_Trimmed: result Two-stage settings audit; result Boss action record: Yoritomo, Shingen, O; result Stage-2 audit failure diagnosis and runn; truncated Open tasks; truncated Pointers; truncated Goal; truncated Owners & leases; truncated Next step_

## Your directives
- #23305 (2026-10-22): Xbox pad display: pressing a button should light/animate the WHOLE button like the PlayStation skin does (currently it doesn't).
- #23305 (2026-10-22): Xbox pad display: pressing a button should light/animate the WHOLE button like the PlayStation skin (currently doesn't).
- #23309/#23313 (2026-10-22): pad display: PRESSED buttons fully opaque/solid; pad body colours dark + see-through like the mod menu.
- #23351 (2026-10-22): controller overlay position AND size must be adjustable in the mod menu.
- #23367 (2026-10-22): pad display: color picker for the pressed-button colour (menu setting).
- #23400 (2026-10-22): any negative Ki pool should be auto-detected and shown as its own negative Ki meter under the boss's real Ki meter.
- #23436/#23437 (2026-10-22): Ki-break/out-of-Ki animation fix and the whole boss setup (move learning, hit counts, Ki states) must be an automatic algorithm covering EVERY boss and future DLC bosses, not per-boss manual learning (Jubei has no learned stagger move).
- #23447 (2026-10-22): the automatic boss setup must auto-detect/pick up new bosses (incl. DLC) and support the whole feature set without manual per-boss work.
- #23450/#23451 (2026-10-22): frame data and arenas must also be fully automatic (auto-detected for every boss/new DLC boss), part of the same automatic whole-boss-setup system as #23436/#23447.
- #23479 (2026-10-22): The mod should not be locked to one game patch: boss setup/addresses should be found automatically (signature scan), so game updates and DLC do not break it.
- #23905 (2026-10-22): NOTHING by hand anymore: no hand-written move lists/rows for any boss; everything (move lists, frame data, arenas, Ki-break, summon) must be fully automatic, incl. new DLC bosses, even the 19 older hand-listed bosses and the 33 human duelists.
- #23909 (2026-10-22): No live-fight-reading approach for move lists: derive from the boss data/files the game loads (static, deterministic), not by observing moves during a fight.
- #23933 (2026-10-22): When EVERYTHING requested is done, ping the human about 10 times on Discord (notify). Not before.
- #24182 (2026-10-22): when everything else is done, audit ALL optimization settings: how many are on by default, and confirm which non-default ones actually help (even 0.001%) so they can be enabled by default.
- #24195 (2026-10-22): after the optimization-settings audit (#24182), run the final benchmark: stock/default Nioh 3 vs the optimized mod version in boss fights on ALL 3 devices (pc, pc2, ally).
- #24256 (2026-10-22): human is away from the PC and stays away until pc says EVERYTHING is done: use the PC freely (no hands-on asks), game launches/closes are fine.
- #24206 / #24258 (2026-10-22 11:08 pm EST): The Nioh 3 feedback site is up at http://100.120.236.18:8791/ and http://192.168.1.165:8791/. It files issues on AhmxdNYC/nioh3-engine for a mod problem, a feature request, and a file whose type is not named, plus text and images. Do not start a second site. Do not launch the game for this.
- #24317 (2026-10-22): mod menu: settings that keep the game looking the same (pure performance) must be separated from the rest into 2 sub menus.
- #24318 (2026-10-22): human finds the 4.5 h settings audit too long; shorten it.
- #24466 (2026-10-22 11:11 pm EST): The feedback site should be a free public host if possible. Vercel can do the uploads. A GitHub Page cannot, because it cannot save a file or open an issue. This PC is not signed in to Vercel. Do not put the GitHub token in a public page. The working link stays http://100.120.236.18:8791/ until a Vercel sign-in exists. Do not launch the game for this.
- #24486 (2026-10-22 11:20 pm EST): Set the feedback site up for Vercel so the human only signs in later. The script is nioh3-mod-site\site\vercel-signin.ps1. Do not publish until they run it. Do not restart the site on port 8791. Do not launch the game.
- #24846 (2026-10-23): final benchmark = THREE arms on all 3 devices: stock default game vs optimized mod with NO visual change (pure perf) vs optimized mod WITH visual-affecting settings (best fps).
- #24854: confirmed arm2 = all proven no-visual-change settings ON; arm3 = arm2 + visual-lowering settings.
- #24918: when pad customization is done, send the human a screenshot of the pad display (transparent body + white border) like the commenter described.
- #24936 (2026-10-23): a user wants lower heat at native 120fps/4K measured by CPU temp before/after (same fps, less work). Add CPU temp/power logging (before/after) to the final benchmark.
- #24943 (2026-10-23): final bench must also record latency/delay (input/frame latency) plus CPU/GPU util, temps, power. Keep-on rule: a setting that is not an fps win but lowers CPU/GPU utilization/temps/latency, or is clearly better code, stays ON (judge on all metrics, not fps only), as long as it doesn't change visuals or hurt other metrics.
- #24946 (2026-10-23): do NOT repeat the whole ~69-run audit on each device again. Final bench = only the 3 arms per device (stock / pure-perf mod / full mod); stage 2 confirm only a short list of candidates, not all settings.
- Discord images/files: py -I C:/Users/Ahmad Hamza/universal-optimizer/tools/notify/notify.py file <path> <text> (<=8 MB). Use it when the human asks for pictures/videos in Discord.
- #24979 (2026-10-23): pad display default must stay the ORIGINAL look; transparent+white-border is a selectable preset only. cx-pc fixed (b35dea02 on cx-pc/pad-style-24886); pc installs+verifies after audits.
- #25333 (2026-10-23): frame meter is too vertically tall; must be much skinnier (drop extra info rows: readout/legend extras).
- #25337 (2026-10-23): frame meter: run-length numbers must be centered/aligned inside their frame squares.
- #25456 (2026-10-23): frame meter layout: colour KEY goes BELOW the two cell rows; the Startup/Active/... readout text goes ABOVE the top row of cells as its own line, NOT part of the row. Colour key stays ON by default (they want it).
- #25485/#25487/#25488 (2026-10-23): human withdrew the objection to the row name; keep my choice "KI DEBT" for the negative-Ki frame-meter row.
- #25529 (2026-10-23): KI DEBT row must show the run-length number in its last cell like the other rows.
- #25534 (2026-10-23): frame meter design approved -> add it to the game. Default position: move the whole overlay down to just above the player healthbar / skills, shifted a bit LEFT of the healthbar.
- #25534/#25544 (2026-10-23): frame meter default position = right above the player health bar, NOT shifted left; KI DEBT row (human bosses only, number in last cell) goes into the game build; then get in a fight and send mid-fight screenshots with controller display on (#25536/#25538).
- #25536/#25538/#25544 (2026-10-23): frame meter default position = just above player healthbar, NOT shifted left at all (centred over it). pc must get into a fight with controller display ON and send mid-fight screenshots to Discord.
- #25964 (2026-10-23): settings audits use 2 repeats per arm only, never 3 (all devices, stage 2 included).
- #25964 (2026-10-23): stage-2 confirm = 2 repeats per arm only, no third repeat (all devices).
- #26131 (2026-10-23): the automatic report button (feedback) must include the user's specs (OS: Linux or Windows for sure, plus CPU/GPU/RAM) in the report.
- #26198 (2026-10-23): after the MCE test, analyse data across all 3 devices, pick best universal default settings and turn them ON if unlikely to harm; THEN run final 3-device benchmarks.
- #26201 (2026-10-23): a setting that is on paper more optimal (even without measured win) should be ON by default. Only two groups: optimized-no-visual-change vs optimized-affects-visuals.

## Goal
Make Nioh 3 run better on the user's machines (PC RX 9070, PC2 RX 6650 XT on a slow chipset slot, ROG Ally) through game-side mods only (engine-fix AS...

## Current state
(see Owners & leases and Next step)

## Owners & leases
- STATE: ACTIVE - the user resumed Nioh work from the phone on 2026-10-14 and wants the whole open task list worked through until they say pause (pause = .handoff/PAUSED file). History: notes/handoff-codex-2026-10-14-paused.md.
- LEAD: Claude manager = agent `pc` run by uo-agentd on this PC (user decision 2026-10-14). The old Codex lead controller (task CodexNiohContinuation) is DISABLED; Codex works as the PC2/Ally agents and takes TASKs from pc.
- MAIN (this PC): the human's game is open. Nioh3 PID 28240 started 4:28 pm EST on 2026-10-20. Do not install, relaunch, or close it. Installed engine 1.7.73 combined (sha 09cb2e9d...) + addon 1.4.22-retry3 (sha 2c381f3c...); INI RD/M2S/J1/WG/LK/MW=1, M1/M2=0. Never install the old main-branch bin over it; identify builds by hash.
- PC2 (Codex): restored, lease free, PAUSED. Ally (Codex): restored, Steam runtime changed during startup (kept, not rolled back), lease free, PAUSED.
- Old peer ids/owners/threads in the handoff are historical: re-discover on resume, never reuse launch admissions.
- Fleet tooling (uo-agentd, protocol, inbox; universal-optimizer branch fleet-v2) is separate from Nioh work and may be installed while paused.
- RESUMED 2026-10-14: pc2 = task A (INI preservation audit/patch, branch only), ally = task B (frame-gen DXGI), pc = task C (rematch mission-load skip). Game launches on MAIN only by pc.
- 2026-10-14 user: Linux/Proton tests happen on ally (not pc/pc2).
- GAME MACHINES (user, 2026-10-14): pc, pc2 and ally can run Nioh 3 AT THE SAME TIME - parallelize benchmarks/tests across them; each holds its own game lock. Steam 'Play anyway' is auto-clicked (Windows: tools/steam/steam_autoplay.ps1).
- Linux tests run on ally only (user #3492); pc/pc2 do Windows.

- 2026-10-17 4:07 AM Eastern cx-pc: #10160 study complete; no game ownership or settings changes; pc owns test scheduling. Available for next assigned offline task.

- 2026-10-17 4:14 AM EDT cx-pc: #10179 offline task complete; pc owns all game testing; awaiting next assigned offline task.

- 2026-10-17 04:16 AM EDT cx-pc #10221: Return study complete: 61.579 s log includes 25.000 s cell wait + 8.845 s cover tail; no game touched. Plan notes/return-time-plan.md, branch cx-pc/return-time-10221. Next pc: implement Return-only CoverAfterPutBackMs=-1/500 probe, AABB and destination-readiness trace.
2026-10-17 4:19 AM EDT cx-pc #10277: shared collector implementation done/pushed 6bbc8bd; runtime validation belongs to pc/ally/pc2. Offline worker available for next assigned task.

- 2026-10-17 4:22 am EDT cx-pc: offline Return plan and boss action-record study delivered on task branches; pc retains all game ownership. Pending offline follow-up is action-event -> damage/hit/name mapping.
- 2026-10-17 4:22 am EDT | cx-pc #10319: Return-only CoverAfterPutBackMs probe built/pushed 8b092ec (cx-pc/return-cover-10319); ASI sha256 a3903d58c1abef1f5881430a8d7258db3d2aac052e3473431a3e289ee8b40c77 | 128517 offline cases PASS, game untested | next pc: OFF=-1 / ON=500 AABB; artifact ../nioh3-return-cover-10319/mod/Nioh3Training/Nioh3Training.asi. Collector done 6bbc8bd; action-table #9715 already actively owned by concurrent #10252 session in ../nioh3-boss-records-10252. No game touched.
2026-10-17 4:27 am EDT cx-pc: #10486 runbook delivered; pc owns game/testing. Queued #10472 JT attribution tooling remains separate.
- 2026-10-17 4:29 am EDT cx-pc: #10472 offline complete; pc owns runtime/raw JT trace capture. No game access.

2026-10-17 4:31 AM EDT cx-pc #10541: offline multi-boss/despawn-all study delivered; no active implementation or game lease. pc owns next assignment and live validation.

2026-10-17 4:42 AM EDT | cx-pc #10841 offline model delivered (c8692bc); pc owns game/native integration. No game ownership taken; available for next assigned offline work.

2026-10-17 4:47 AM EDT cx-pc #10773 offline exporter task complete, branch cx-pc/jt-export-10773 at 44ef511 pushed; no game/lock/settings touched. pc owns runtime testing and next offline assignment.

2026-10-17 5:44 am EDT cx-pc #11440: offline review complete/pushed 055b0e6; no game/lock/config ownership. pc owns implementation scheduling and native validation.

2026-10-17 5:45 AM Eastern cx-pc #11477 complete: offline only, no game lease or installation; available for next pc-assigned task. Contract commit 2671b6f.
2026-10-17 5:46 AM EDT cx-pc #11488 complete: offline only, no game ownership; pc owns probes and further task assignment.

2026-10-17 5:52 AM EDT cx-pc #11536: assigned offline cleanup study complete/pushed; no game/lock ownership. pc owns native implementation/testing; requesting next assigned offline item.
2026-10-17 5:54 am EDT cx-pc #11561 offline implementation/tests complete and pushed adb2e9b; move-code ownership released to pc for integration. No game/lock/settings touched. Available for next scoped offline task.
- 2026-10-19 11:56 pm EDT: PC2 assigned #14550 fix/test drill aggro (#14546), isolated branch from 2eb32369; pc main-PC install owner. cxl-pc triage done, no game lease or code edits.

- 2026-10-19 11:57 PM EDT cxl-pc #14549: drill-aggro implementation/install owner = pc; PC2 asked to stop competing implementation and coordinate validation (#14555), acknowledgement pending. cxl-pc holds no game lease or code ownership.

- 2026-10-19 11:57 PM EDT cxl-pc #14559: PC2 stand-down confirmed; pc is sole drill-aggro implementation/install owner. PC2 has no competing implementation assignment.

- 2026-10-19 11:59 PM EDT cxl-pc #14563/#14564: main-PC-only drill-aggro fix/relaunch assigned to existing owner pc (#14569); PC2 validation cancelled (#14568). cxl-pc has no game lease or code ownership.
- 2026-10-19 11:59 pm EDT cxl-pc #14564: pc ownership unchanged; progress/completion response requested #14573. No competing installation started.

- 2026-10-20 12:00 AM EDT cxl-pc #14570: PC2 cancellation fully acknowledged; no PC2 drill-aggro lease/work. pc sole local implementation/test/install owner; cxl-pc no game ownership.

- 2026-10-20 12:02 AM EDT cxl-pc #14577: delegated workers must be Codex per user app setting; pc remains main-PC drill-aggro owner, cxl-pc coordination only.

- 2026-10-20 12:03 AM EDT cxl-pc #14580: pc confirms sole drill-aggro implem...

## Open tasks
- [ ] (user #4114/#4115) Cut FIRST load of the boss rematch (37 s on PC; retry loads already 3-6 s). Next: re-run tools/rematch/fightnow_test.ps1 retry mode, split the 37 s by phase (...

## Last results
(none)

## Next step
(older entries in .handoff/next.md)
2026-10-23 1:50 PM EST pc: decided NOT to edit installed INI (CBN/CBT keys missing = off) yet, to keep audit/stage2 baselines comparable; include CBN=1,CBT=1 in final bench arm2 (pure-perf) once pc2 #113 confirms. ally: stage2 21 valid, ETA 2:25pm.
2026-10-23 14:25 pc: report specs.txt built (pc/report-specs 96611268, rematch asi sha 558ee0e6...), offline tests pass, not in-game/Proton tested; install after CBN A/B ends. CBN A/B (captures/cbn_ab_arms.log) running on pc, marker test-active set.
2026-10-23 14:36 pc: CBN A/B pc (mod fps counter, no admin): OFF 83.2 avg (80.5,86.2) vs ON 86.4 (85.8,87.2) = +3.8%, noisy. Marker removed, game closed. NEXT: install pc/report-specs rematch asi (558ee0e6), pc2 #113/#111, ally MCE, final bench.
2026-10-23 pc: ally stage2 DONE 27 valid, all neutral (S1 +2.7 best, P12 -0.7); ally now on MCE #112 3/6. NEXT: digest MCE, pc2 #111/#113, final 3-arm bench.
2026-10-23 pc: human #26198 plan = MCE test -> merge all-3-device data -> set defaults ON for non-harming -> final 3-arm bench. NEXT: wait ally MCE, digest stage2...

## Pointers
- .handoff/open.md
- .handoff/checkpoints.md
- notes/audit-capture-25896.md
- notes/boss-action-reco...
