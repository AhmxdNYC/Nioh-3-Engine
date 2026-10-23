
## 2026-10-22 19:19:11 session a7924822-795a-45db-83ca-1242c89313af
Nothing is failing on pc2 or ally, and there are no new messages or replies from you since my last note.

I'm waiting on you for one thing: close the game or say "go", so I can restart it with the combined build. The game on this PC is still open (PID 1612, running since 2:20 pm EST). The Jubei Ki-break check is still unplayed.

I sent no messages and changed nothing.
2026-10-22 19:34 pc: relaunched game PID 15936 with pc/combined-1022b (release-1022 b2850017 + ally autoarena-r1022), Training asi ac3dd853, backup captures/pc/play-20261022-pre-combined. test-active marker set. UNPROVEN: Jubei reel (KIAUTO log), arena fight for roster bosses, autobind in-game on pc. NEXT: after user plays, read Nioh3Training.log for KIAUTO/AUTOBIND; delete marker.

## 2026-10-22 19:54:13 session bbf66a59-3890-4a3e-b1cd-ee97e88295d0
I told the human (#23940) that I'll ping them about 10 times on Discord once everything requested is done, and not before. I logged this in `.handoff/user-directives.md`.

Still open:
- **pc2:** the automatic move lists, via the static-derivation task.
- **Ally:** the benchmarks (page pool, KL_Div, RD/M2S/J1).
- **This PC:** the clipped move rows and the damage-multiplier fix for spirits and damage-over-time. The worktree `../nioh3-polish` (branch `pc/polish-23924`) exists but has nothing committed. Two earlier attempts to start this work in a background agent were cut off when my session ended. I'll do it directly or hand it to a separate helper instead.

The plan is in `.handoff/next.md`. I didn't touch the game or change any code.
2026-10-22 20:03 pc: pc/polish-23924 (unclipped boss-move rows + Ki/spirit scaled by damage x; DoT not reachable) built (Training 03fdbfaa, Rematch asi built via tools/msvc) and pushed; captures/install_polish_23924.ps1 waits for game close then installs (no relaunch). UNTESTED in game. Game 15936 in use (log active 19:54). NEXT: after close/relaunch test row layout + damage x on Ki.
2026-10-22 20:05 pc CHECKPOINT | task: automatic boss setup (move lists/arenas/Ki-break) + pc polish | DONE: pc/combined-1022b (release-1022 b2850017 + ally autoarena) pushed, installed, game PID 15936 running since 7:30 pm EST (Training asi ac3dd853); pc/polish-23924 pushed (unclipped move rows + Ki scaled by damage x; DoT unreachable; Training 03fdbfaa + Rematch built), hidden script captures/install_polish_23924.ps1 waits for game close then installs (no relaunch) | UNPROVEN in game: Jubei Ki-break reel (look for KIAUTO in Nioh3Training.log), arena fight for roster bosses, polish row layout + Ki damage x; test-active marker is set (delete when test ends) | pc2: #23907 pc2/autoruntime static move lists from game data (no live fight), near-twin bind rule, roster rows for Mikoshi Nyudo/Himiko | ally: items page pool/KL_Div/RD-M2S-J1 benches (session holds lock, asi #1441) | NEXT: when game closes confirm polish installed, relaunch, test Jubei + polish; merge pc/polish-23924 into combined; ping user 10x on Discord only when ALL requested work done | files: captures/install_polish_23924.ps1, .handoff/needs-player.md

## 2026-10-22 20:49:43 session 5b28d499-3b66-4ca8-8837-3f4bccd992fa
The merged build is not installed yet, so nothing has been tested in the game. Nioh 3 (PID 15936) has been open since 7:30 pm EST. The installer is queued and runs when the game closes.

pc2 and ally have no unanswered errors or BLOCKED messages. I'm not starting anything new while the game is open: the only remaining step is the in-game test (Yoritomo's 88 attacks, Red Kappa bound, summoning Mikoshi Nyudo and Himiko). It can only run once the game closes.

## 2026-10-22 21:13:50 session 5b28d499-3b66-4ca8-8837-3f4bccd992fa
The merged build is installed on this PC. Yoritomo checks out in the game, but Red Kappa, Ippon-Datara, Mikoshi Nyudo and Himiko are untested.

**What passed (Yoritomo):**
- The Training log shows 173 moves from game data, 88 of them attack moves, and 11 sequences.
- The runtime list matches the old hand list on all 173 ids (0 disagreements), and the live frame bar read Yoritomo's attacks.
- The roster grew from 83 to 101 summon rows.

**What I couldn't test:**
- Red Kappa and Ippon-Datara: "Fight now" refused both because those missions aren't cleared on this save.
- Mikoshi Nyudo and Himiko spawn: I can't open the mod menu with synthetic keys, so I couldn't summon them.

I closed the game by its exact PID and removed the test marker. I didn't back up or restore saves for this test, so I haven't checked whether the in-place fight changed the save.

I sent the result to pc2, told the user, and added a needs-player item: summon those two from the F10 menu, which should take about a minute. `next.md` and `done.md` are updated. Nothing was committed or pushed this run beyond the earlier `pc/merge-autoruntime-polish` branch.

2026-10-22 20:58 pc CHECKPOINT | task: automatic boss setup (#23436/#23905/#23909) | DONE: merged build (pc2/autoruntime static move lists + polish rows/Ki-dmg + auto arenas + summon-all) INSTALLED, Training asi sha 1781C86C, game relaunched PID 19452 | UNPROVEN: everything in game; poison/burn dmg scaling not done | NEXT: test Yoritomo menu 88 attacks, Red Kappa bound, summon Mikoshi Nyudo/Himiko; collect ally bench RESULTs (#23929) and pc2 results; when ALL requested done ping human 10x on Discord (#23933); stale marker uo-agentd/pc/test-active (19:30) should be deleted | files: captures/install_autoruntime_1025.ps1, worktrees ../nioh3-merge1025 ../nioh3-polish

## 2026-10-22 21:39:46 session 5b28d499-3b66-4ca8-8837-3f4bccd992fa
I sent ally the GO for its benchmark job (#24116), with a smaller scope than it proposed. No benchmark numbers have come back yet.

Ally's job needed 27 arms, about 2 hours of game time. I cut it to:
- **Item 1 first:** PP off versus on, three interleaved fights per arm, one fight per launch.
- **Then item 2, then item 3** if time allows.
- **Abort rule:** drop any arm that is under 30 fps in the open world.
- **Control run:** ally records the fps drop on the first summon separately and runs one fight with `Nioh3Training.asi` removed. That drop (36 to 3–10 fps, CPU at 98%) might be the Training mod's own load on the Ally's CPU rather than low RAM.
- **Steam:** it leaves Steam and its web helper alone, per the correction I sent earlier.
- **Input fix:** if the Continue prompt ignores the virtual pad, it should use the input path that worked on its first launch.

Results should come back per arm with average fps and 1% low.

I haven't sent the 10 Discord pings. Ally's benchmarks and pc2's Himiko fix are both still open.
2026-10-22 21:55 pc: built combined pc/all-1022 (pc2/autoruntime-all f0237741 + pc/autostagger), pushed; worktree ../nioh3-autoall, Training.asi sha cd089a4a (mod/Nioh3Training/Nioh3Training.asi there). NOT installed: another job's test marker is fresh (game PID 24224 started 9:50pm). NEXT: when marker clears, back up + copy asi to game dir, relaunch, check ARENASCAN + autobind lines for Himiko/Kappa/Ibaraki, send result.
2026-10-22 21:58 pc: pc/all-1022 (Training cd089a4a) INSTALLED, game PID 20848 loads, ARENASCAN 131 rows. Earlier test same day showed Himiko FINAL COMPOSED 376/376 rows in game on pc. Backup captures/pc/play-20261022-pre-all1022.

- 2026-10-22 10:27 PM EDT | cx-pc #24269: two-stage planner c9cc05aa and revised settings-audit-plan.md DONE; Stage 1 pc/pc2/ally 32/31/31 keys, 69/67/67 captures including 5 baseline anchors/device.
  Proven: 9 offline tests PASS; fixed Yoritomo R7B35 fight-now/60s/PresentMon; Stage 2 only drift-qualified keys, 3+3, KEEP mean gain >2x baseline spread and no 1% low regression. No game touched.
  Next: pc integrates planner, game owners run split screens and return results-template.csv to generate confirmation.
2026-10-22 22:35 | pc: auto move lists / duelists / report button / settings audit | DONE+verified in game on pc: Yoritomo 173 moves, Red Kappa 107/107, Mikoshi, Ippon-Datara, Himiko (composed 376/376); merged branch pc/merge-kappa (worktree ../nioh3-merge1025, pushed) = pc2/autoruntime-all + polish-23924 + cx-pc report button; Report-button build installed 22:18 (another pc session testing it, marker 22:18). UNPROVEN: Report button UI in game; Ogin/Denrinbo/Kawara AUTOBIND in game (pc2 #24262 33/33 duelists, needs actionsets.tsv, installer captures/install_actionsets_1022.ps1 queued hidden); ally bench #23929 (GO sent, reduced scope, no result yet). NEXT: when game closed install actionsets.tsv, spawn Ogin + check Report button; then cx-pc 2-stage settings audit (#24269) -> post board tasks bench,on:pc/pc2/ally; then stock vs mod boss benchmark 3 devices x3 runs (#24195); then ping human ~10x on Discord (#23933). | files: captures/install_*_1022.ps1, branch pc/merge-kappa, cx-pc/settings-audit
2026-10-22 22:30 | pc: pc2 autoruntime-all 10ef2467 confirmed merged in pc/merge-kappa (ebdade8a) + installed (asi d3cf3ccac50d); actionsets.tsv installed in game dir | UNPROVEN in game: Ogin/Denrinbo/Kawara autobind | NEXT: launch, summon Ogin, check AUTOBIND log; then settings audit, final bench, 10x ping | branch pc/merge-kappa
2026-10-22 22:35 pc CHECKPOINT | task: finish all requested work then 10x Discord ping (#23933) | DONE+tested in game: auto move lists (Yoritomo/Mikoshi/Datara/Kappa 100%, Himiko composed 376/376), auto arenas, Report-a-problem button (Practice page; zip clean; site URL empty -> need grok's URL for [Report] Url in Nioh3BossRematch.ini), duelists 33 resolve (pc2). Installed build = merge1025 + report button. Game closed | HALF-DONE: settings audit stage 1 (board task #110 claimed by pc, plan notes/settings-audit-plan.md, captures/audit-screen/<device>/; pc2+ally have own tasks; one setting at a time); ally #23929 bench was RAM-blocked, reduced scope; poison/burn dmg scaling not done | NEXT: finish #110 on pc, collect pc2/ally audit RESULTs, run stage 2 (3 OFF+3 ON) for advancing keys, then final stock-vs-mod boss bench on all 3 devices (#24195), then 10x Discord ping; in-game menu driving: py -I tools/testmatrix/vpad.py key F10/DOWN/RETURN | files: .handoff/next.md open.md done.md
2026-10-22 22:35 | menu 2 pages (same-picture vs may-change-look) built, compiles | NOT yet tested in game | next: install after audit slot frees the game, F10 -> 'Settings page' row, check both pages | menu.cpp
- 2026-10-22 10:51 PM EST | gx-pc checked #23272 PJ bench | capture matches 0 hitches, worst about 13 ms | board task 105 still failed with the old not-run text | no rerun | NOTE #24370
2026-10-22 22:52 pc | settings audit stage 1 (board task #110) RUNNING on pc | runner tools/rematch/audit_screen_run.ps1 (wrapper captures/audit-screen/run-pc-wrapper.ps1, log run-pc.log, results captures/audit-screen/results-pc.csv, resumable: valid rows skipped); first baseline 80.99 avg / 45.81 1%low; ~4 min/run, 69 runs ~4.6h, started 22:46 | finish watcher run-pc-finish.ps1 commits csv, clears test-active marker, sends RESULT | NEXT when done: py -m uo_bridge task report 110, run settings_audit.py --stage confirm with all 3 devices' results | files: tools/rematch/audit_screen_run.ps1
2026-10-22 10:53 PM EST | gx-pc board 71 action record | 173/136/141 rows for Yoritomo/Shingen/Otoroshi | hit count, damage, name, duration are not fields on the 336-byte row | commit 3669b4d4 pushed on pc/fightnow-swap-wire | next is a live hit-count probe, not run
2026-10-22 10:58 PM EST | website progress #24420 | 0 files saved | new build started 10:57 pm EST | phone #24422 | next: other gxl-pc slot sends the link | game not touched
- 2026-10-22 10:58 PM EST | gx-pc #24392 hitbox rows | 182 / 194 / 162 rows | motion does not index the row, the action id does | commit ad80ca1d | RESULT #24423 | game not opened
2026-10-22 11:00 PM EDT | cxl-pc #24405: offline audit pruning DONE/pushed 29fe08d3 on cxl-pc/audit-short-24405; 92->91 keys, 199->197 captures, only Ally SR provably redundant.
Proven: 197 preserved run rows, 15 anchors, all 185 copied and committed INI hashes pass. 30s/15s UNPROVEN (only one baseline).
Next pc: read notes/settings-audit-short-24405.md; retain active 60s run, adopt safe pruning at owner-controlled transition; conditional 30s budget needs fresh stability validation. Separate worktree ../nioh3-audit-short-24405; no game/running files changed.
2026-10-22 11:05 PM EST | gx-pc #24426 hit count | no stored field; distinct +0xA5 ids | Yoritomo 0x4E6E = 2, Shingen 0x4E4E = 3, Otoroshi 0x4E4E = 1 | commit 50a3c0d8 | RESULT #24449 | game not opened | next: live frame is still required for hits-so-far

2026-10-22 11:08 pm EST | Nioh 3 feedback site serves three forms and files GitHub issues | Proven: page 200, issue #2, feature #3, unnamed file #4 (all closed after the check). Unproven: phone open from another device, GitHub file attach (API 422, files stay on this PC) | Leave the server on port 8791; do not start a second site | branch gxl-pc/mod-site commit 622d3fa1, site/server.py site/index.html
2026-10-22 23:12 | pc audit runner: retry stalled launch added (every 2nd launch stalled 'no player after Continue'); runs 0001,0003 valid | rerun runner after pass to fill failures (skips valid ones) | tools/rematch/audit_screen_run.ps1
2026-10-22 11:12 PM EST | #24466 public host | Vercel can, GitHub Pages cannot | PC not signed in to Vercel | phone #24470 #24474 | next: wait for a Vercel sign-in | site stays on port 8791 | game not touched
2026-10-22 11:22 PM EST | #24486 Vercel setup ready | script site\vercel-signin.ps1 | not published | private site still port 8791 | next: human signs in | game not touched

## 2026-10-22 23:43:08 session ff0e64e6-687d-4329-a5d8-ecbf60f9cf63
Ally confirmed its settings audit is running on Linux. It has 3 valid runs done: the baseline plus 2 settings, about 8 minutes each. Its results are in `/tmp/t23929/audit/results.csv` on the ally, and it will push each chunk. It gave no ETA for the full list.

pc is at about 9 of 69 runs. pc2 hasn't confirmed anything yet, so I'll chase it. I sent the human a short status.
2026-10-22 23:48 | pc2 audit failed (no boss HB, 8 s wait too short); runner now polls 60 s (21a3cc19) | unproven on pc2 | next: pc2 reruns 2 runs, check results; pc audit continues | tools/rematch/audit_screen_run.ps1
2026-10-23 00:04 | audit: pc 16/69 valid, pc2 2 valid running, ally running | all 3 on stage 1 | next hourly check of results csvs, chase silent | open.md TRACK

## 2026-10-23 00:25:41 session b8158c6b-d846-4c6a-b760-e6cf37db9f82
I've sent you the 12:25 am EST update. pc has 21 of 69 audit runs valid, at about 5 minutes each, and should finish around 5 am EST.

pc2 and ally are running their shares and report after each chunk of 16. Neither has pushed results to the repo yet, so I have no fresh counts for them. No errors showed up anywhere, and nothing needs you. The next real checkpoint is the first pc2 and ally results, in about an hour.

I didn't touch the game this run.

## 2026-10-23 00:46:42 session ff0e64e6-687d-4329-a5d8-ecbf60f9cf63
I haven't found anything that needs action, so I'm not changing anything.

- **pc:** 25 of 28 run rows are valid. That is only about 1 more row in the last 20 minutes or so, well below the earlier 5 min per run. The runner may be stalling, so I'll look at it on the next run if it doesn't pick up.
- **pc2:** its branch still has no results CSV, because its first chunk of 16 isn't finished.
- **ally:** my read of its results is still garbled, so I have no confirmed count.

The inbox is empty and no device has reported an error.

2026-10-23 12:52 AM EST | gx-pc #24647 hit-id table | 175 attack actions, 110 with a hit | Yoritomo 41/96 sum 61 max 3; Shingen 50/57 sum 86 max 5; Otoroshi 19/22 sum 19 max 1 | commit da331ce2 | RESULT #24651 | game not opened | next: task 60 still needs a live soul-core test

- 2026-10-23 1:09 AM EDT | cx-pc #24680: plan f771b89c + compiling feature a8a57f9b pushed; archive 8a20056a dependency (8232bf80 equivalent here). 11476-row parser/catalogue tests and BOTH ASI builds PASS.
  Proven offline: triple identity, zero-power handling and no hand hit metadata; live menu untested. Next pc: integrate both ASIs + generated runtime TSV, test under existing game ownership. notes/action-record-integration-24680.md.

## 2026-10-23 01:34:44 session b8158c6b-d846-4c6a-b760-e6cf37db9f82
pc has 35 of 69 audit runs valid, with about 3 hours to go. I sent you that at 1:35 am EST.

pc2 and ally haven't reported any new errors. I got no fresh counts from them this time, and neither has pushed a results file to the repo. I'll check their counts at the top of the hour and chase either one if it goes quiet.

I didn't touch the game this run.
2026-10-23 01:50 | Settings audit stage 1 running on all 3 devices (pc 38/43 rows valid of 69, pc2 23/23 of 67 on branch pc2/audit-run-24483, ally 1/3 of 64 on ally/audit-results, ally had duplicate drivers, now one) | runner fix 21a3cc19 (60 s boss HB poll) pushed; boss action table (gx-pc 8a20056a) and auto hit/damage integration (cx-pc/action-record-24680) built offline, NOT tested in game | NEXT: hourly check pc2/ally counts, chase ally; when pc audit ends (~5 am EST) merge+install cx-pc/action-record-24680 and verify in game, test 2-page menu, then stage 2 (py tools/settings_audit.py --stage confirm), stock-vs-mod bench on 3 devices, then 10x Discord ping | files: tools/rematch/audit_screen_run.ps1, captures/audit-screen/, notes/settings-audit-plan.md
2026-10-23 02:07 | TASK: settings audit stage 1 tracking (human #24532/#24749) + 2-page menu. DONE: menu.cpp 2 pages built+pushed (untested in game); audit runner retry+-MaxRuns pushed; pc audit running detached (40/69 valid at 01:56, ~5 min/run, ends ~05:00; results captures/audit-screen/results-pc.csv); pc2 running (12/67 at 01:00, own csv push per 16); ally restarted after fix (collision w/ old relaunch script killed), no valid run seen yet. HALF: pc2/ally exact counts requested (Q #24752), no csv on origin yet. NEXT: git fetch, check results-{pc,pc2,ally}.csv hourly, chase silent; rerun runner on pc to fill failed runs (skips valid); then test 2-page menu in game (F10 > Settings page) when game free, stage 2 confirm, defaults, final stock-vs-mod bench x3 devices, last 7 duelists + poison/burn, report site URL from grok, then 10x ping. Tracked in .handoff/open.md TRACK.

## 2026-10-23 02:31:47 session 62b5729a-9088-474f-aa68-fcd76c5a56c5
I sent you the progress update on the phone. As of 2:32 am EST:

- **Main pc:** the results file has 55 audit rows of about 69, and the game is mid-fight on the next run. It should finish around 5 am EST.
- **pc2:** the last number I have is 23 of 67 valid, from about 2 am.
- **Ally:** it was retrying run 5 of 64 after the black-screen hang. I have no newer count.

I just asked pc2 and ally for exact counts and will pass them on when they answer. I haven't touched the game on any machine.

## 2026-10-23 02:52:26 session 62b5729a-9088-474f-aa68-fcd76c5a56c5
I sent you the progress update on the phone. As of 2:52 am EST:

- **Main pc:** it's on audit run 61 of 69, with about 1.5 hours left. It should finish around 4:30 to 5 am EST.
- **pc2:** 35 of 67 valid at 2:40 am, with no failures. That is the last count I have from it.
- **Ally:** 5 valid, and it's the slow one at about 7 hours left. Its 2 invalid runs aren't rerun automatically, so I'll queue a manual rerun after its main pass.

Nothing is stuck. I'm only watching and haven't touched the game on any machine.

## 2026-10-23 03:13:23 session 62b5729a-9088-474f-aa68-fcd76c5a56c5
I sent you the answer on the phone: mostly yes, and I wouldn't do a rewrite.

Patching the shipped game can't double or triple fps on its own. The limits are how the engine schedules CPU work and draws, and we can only trim those. There are two routes to a 2-3x jump:

- **Visual-lowering settings:** clouds, GI, fog and terrain can give big gains on the GPU-bound machines (pc2 and Ally), at a cost in image quality.
- **Rewriting the renderer or engine:** this isn't realistic for us.

My rough expectation is +10-30% for free, and more only by trading visuals. That range is a guess, not something I measured.
- 2026-10-23 3:29 AM Eastern cx-pc #24886 | Six controller appearance/position rows plus zero fill implemented; d3a2f4e6 on cx-pc/pad-style-24886. Both ASIs build, three offline suites and catalog check pass. Next pc: integrate and verify live menu persistence/visuals; no game touched.

## 2026-10-23 03:51:18 session ff9738e9-70c2-4819-b0f6-947e35eba710
pc's rerun of its 5 invalid audit runs hasn't started. It was blocked first by a quoting mistake in my launch command, which I fixed. It is now blocked by the shared game lock, which pc2 holds on purpose until its last run, about 4:35 am EST.

A retry loop (`run-pc-retry-loop.ps1`) checks every 5 minutes and starts the rerun once the lock frees. I told the human this in #24966. I also deleted the `test-active` marker so it doesn't look like a test is running. Nothing was lost.

pc2's runner is alive at 55 of 67 valid with 0 failures, and I told the human that too (#24964).
- 2026-10-23 4:02 AM Eastern cx-pc #24980 | Original default restored in b35dea02 (cx-pc/pad-style-24886); optional Transparent + white border preset retained. Both ASIs build; 393216 default command comparisons and 4 offline suites pass. Next pc: integrate correction and verify live menu; no game touched.
2026-10-23 04:17 pc CHECKPOINT | task: settings audit stage 1 + user Q&A | DONE: pc audit finished 62 valid/67 rows (5 invalid: 4 'no player after Continue', 1 no boss; need rerun once pc2 frees game lock); pc2 ~59/67 valid done ~4:40 am, then TASKed #25003 (ideas #9 memcpy probe, #3 PSO prewarm); ally ~16/65 runs, ~noon EST, slow. Pad customization built by cx-pc (default restored to old look, Transparent+white border preset), previews sent to Discord via universal-optimizer/tools/notify/notify.py file <path> <text>. | UNPROVEN: cx-pc/action-record-24680 + pad build not merged/installed/tested in game | NEXT: rerun 5 invalid pc rows; analyse results (settings_audit.py); merge+install cx-pc branches, verify in game (Yoritomo hits, pad preset), send in-game pad screenshot via notify.py file; build bench logging CPU/GPU util, temps, power, latency; 3-arm final bench (stock / no-visual-change mod / full mod) on all 3 devices, no second full audit; keep rule: keep setting if it helps ANY metric; then 10x Discord ping | files: captures/audit-screen/results-pc.csv, settings-list.md, .handoff/user-directives.md (#24846..#24979), open.md

## 2026-10-23 04:54:30 session ff9738e9-70c2-4819-b0f6-947e35eba710
Nothing is failing, but pc's 5 audit reruns are still waiting. pc2 took the game lock back to finish its 4 missing audit runs, which should take about 30 minutes. The retry loop on pc checks every 5 minutes and starts the reruns once the lock frees. I told the human this (#25088). I did not touch the game.

Ally is still running. Its last count was 18 of 65 valid at 4:26 am EST.

## 2026-10-23 05:20:24 session ff9738e9-70c2-4819-b0f6-947e35eba710
pc2's audit is finished: 67 of 67 runs are valid, with the game closed and the lock released. I told the human (#25133).

pc2 says run `s1-pc2-0066` (Memory.LM ON) had already measured fine at 43.11 fps before my message, so it was not marked NOT_MEASURABLE. The baseline run `s1-pc2-0067` came in at 51.82 fps. The results are on the branch `pc2/audit-run-24483`.

I tasked pc2 (#25132) with its next job. It builds and measures two small optimizations, OFF versus ON, on its RX 6650 XT: the memory-copy hot spot probe and shader precompile for boss-fight hitches. That uses the same Yoritomo fight, 60-second PresentMon, 3 repetitions per arm.

pc's 5 reruns should start on the retry loop's next check. The last check I saw, at 5:16 am EST, had not started them. I haven't confirmed that they have started.

Ally is still at about 18 of 65.

## 2026-10-23 05:42:58 session c72f5b92-5e9f-4b93-ac59-4666484b9cc2
I built the MCP probe `Nioh3EngineFix.asi` from `cx-pc2/mc-probe-25132` (d18e0ab1) and uploaded it to the bridge as file #1527 (sha256 prefix b4be7d2180be). I haven't run it in the game, and I told pc2 and cx-pc2 in #25177; pc2 does the test.

The branch's `build.bat` had two bugs that stopped it linking:
- `mcprobe.obj` was missing from the link line, which caused the unresolved-symbol errors.
- A stray `build\mcprobe.obj` sat on the `psomemo` compile line.

I fixed both and pushed the fix to branch `pc/mc-probe-build`. It is not merged into cx-pc2's branch.

To use the probe, set `[Patches] MCP=1`; it defaults to 0.

I didn't launch the game or touch the running audits. I also didn't write a checkpoint or update `.handoff/next.md` this run.
2026-10-23 06:12 pc | pc audit rows complete (results-pc.csv 73 lines); evaluate stage waits for ally rows | unproven: cx-pc builds not yet installed/verified | NEXT: merge cx-pc/action-record-24680 + cx-pc/pad-style-24886, build, install, verify in game; then settings_audit.py --stage evaluate | branches above
- 2026-10-23 6:13 AM Eastern cx-pc #25224 | Combined action records + Original-default pad at 1baaca34, cx-pc/merged-pc-install. Three ASIs and five offline suites PASS; artifact hashes in notes/merged-pc-install-25224.md. Next pc: install matching Training/Rematch + TSV and EngineFix, verify live; no game touched.

2026-10-23 06:25 | pc: pc audit reruns done (all 67 rows); pc2 67/67 valid; ally ~20/65 running. Merged build (cx-pc/merged-pc-install 78139235: pad customization w/ Original default + auto hit/damage action records) INSTALLED on pc and run in game: Yoritomo fight OK ~90 fps, pad display original look + cyan pressed OK, ACTIONRECORD 11476 rows loaded. | UNPROVEN: new menu page (preset/colour rows, TransparentWhite preset visually in game), per-hit damage readouts in menu (F10 via keybd_event did not open menu; use pad View+DpadUp 1s). | NEXT: open menu via pad, screenshot new rows + preset; analyse pc/pc2 audit csvs; final 3-arm bench w/ temps/CPU/GPU/latency. Backup of old files: captures/pc/play-20261023-pre-merged. | files: captures/merged_verify.ps1, captures/merged-verify-20261023/
- 2026-10-23 6:20 AM Eastern cx-pc #25250 | Pre-colour-picker default audit PASS: 393216 exact draws and identical atlases; default White already correct, Cyan optional. Both ASIs rebuilt, 5 suites + catalog pass. Next pc: use Original/White for any saved Cyan config; no game touched. See notes/pad-pressed-default-25250.md.
2026-10-23 06:30 | pc loose ends: MCP probe ASI built (bridge #1525/#1526, branch pc/mc-probe-build-25148, build.bat link fix) for pc2 to measure; pc2 also doing PJ shader precompile; ally audit ~10:45 am EST, its 2 AS0 OFF runs need manual rerun; user wants final 3-arm bench with temps/CPU/GPU/latency, no second full audit | NEXT: open mod menu via pad (View+DpadUp 1s), screenshot new pad rows + TransparentWhite preset; analyse results-pc.csv + results-pc2.csv; answer pc2/ally RESULTs
2026-10-23 06:40 | pc: installed cx-pc 25250 EngineFix cb453309 + Rematch 24292782 on pc (game closed); INI pad = Original, opacity 0.8, press colour White (old look). Unverified in game; backups *.pre25250.bak | NEXT: launch, screenshot, menu rows
- 2026-10-23 6:27 AM Eastern cx-pc #25267 | SF6-style frame-meter drawing implemented on cx-pc/frame-meter-sf6; Training build and frame_meter_test PASS; sampler unchanged. Mock captures/frame-meter-sf6/mock.png. Next pc: review mock, integrate and check live visuals.
- 2026-10-23 6:33 AM Eastern cx-pc #25280 | Colour key added (10 swatches, responsive wrap), saved Practice/FrameMeterLegend toggle default ON. Training/Rematch build, stats/frame-meter and targeted menu tests PASS. Mock updated; next pc integrate branch cx-pc/frame-meter-sf6 and check live.
- 2026-10-23 6:35 AM Eastern cx-pc #25288 | All features merged on cx-pc/merged-pc-install (85d61923); 3 ASIs + runtime TSV bundled captures/pc-install-25288. Eight targeted suites + catalog PASS. Hashes notes/merged-pc-install-25288.md. Next pc install 4 bundle files and validate live.
- 2026-10-23 6:37 AM Eastern cx-pc #25301 | Removed number axis; run lengths now inside each final coloured cell. Training rebuild + meter/stats tests PASS; mock captures/frame-meter-sf6/mock.png. Next pc: replace Training only, other #25299 files stay compatible. Branch cx-pc/merged-pc-install.

- 2026-10-23 6:41 AM EDT cx-pc #25310 | Three real SF6 screenshots downloaded/viewed; cited design notes pushed a4d284bd on cx-pc/sf6-reference-25310 | paired strips and outside summaries proven visually; concurrent drawing owner unresolved | next: integrate drawing in isolation then coordinate merged bundle; no game touched.
- 2026-10-23 6:41 AM Eastern cx-pc #25323 | v4 973ba295 merged into cx-pc/merged-pc-install; exact renderer/mock, pad + records preserved. Three ASIs rebuilt; 8 targeted suites + catalog PASS. Four-file bundle captures/pc-install-25323; hashes notes/merged-pc-install-25323.md. Next pc approve/install/live check.

- 2026-10-23 6:49 AM Eastern | cx-pc #25350 combined thin+centred bundle verified, pushed review 65ec81e1 on cx-pc/merged-pc-install (bundle build f23bcafb).
  9 independent suites PASS, all 4 bundle bytes verified, EngineFix/action data retained. Next pc install captures/pc-install-25345 from ../nioh3-merged-pc-install and validate live; notes/merged-pc-install-25350.md.

- 2026-10-23 6:50 AM EDT cx-pc #25310 | Real SF6 reference study plus thin/centred combined source validated;3 ASIs rebuilt,9 targeted suites pass,12960 count cases | full catalog coverage fails existing keys; later25379 visuals remain separately owned | next: integrate documentation-only delta and deliver snapshot hashes | cx-pc/sf6-reference-25310, notes/merged-pc-install-25310.md

- 2026-10-23 6:51 AM EDT cx-pc #25310 COMPLETE | reference-study branch fast-forwarded/pushed onto cx-pc/merged-pc-install de8b73e9; final merge changes documentation/images only, accepted thin code unchanged |9 targeted suites pass; full catalog coverage fails baseline keys | next: existing25379 owner ships newer inline labels/readability | bundle ../nioh3-sf6-reference-25310/captures/pc-install-25310, notes/merged-pc-install-25310.md

- 2026-10-23 06:52 AM Eastern cx-pc #25376 | 52aa2139 pushed: PLAYER/BOSS, single default-ON top readout, bold white outlined centered counts, near-black idle cells; 52px total at 1080p. Training + Rematch builds, 12960 GDI cases, sampler/stats PASS. Next pc integrate/install/live check; set saved FrameMeterReadout=1. Mock captures/frame-meter-sf6/mock.png. No game touched.

- 2026-10-23 6:53 AM EDT cx-pc #25400 review only | immutable52aa2139:12960 GDI cases PASS, actual geometry harness gives18+18+16=52px FAIL vs40px; separate readout, purple unblockable/no grab key remain | no final bundle approved | next existing implementation owner finishes inline/colours then resubmits; no source/game/build-output edits

- 2026-10-23 6:55 AM Eastern | cx-pc #25379 isolated inline meter built/pushed 99fc1d4c on cx-pc/frame-meter-25379-isolated; 40px two-row layout, inline key/readout, near-black idle, readable counts.
  Training build PASS; remaining: authorized experimental grab-bit classifier, suites and bundle. Isolated worktree ../nioh3-frame-meter-25379-isolated avoids duplicate-session writes.

- 2026-10-23 6:56 AM EDT cx-pc review #25422 | isolated99fc1d4c geometry compiled:20px x2=40 PASS;12960 count placement cases PASS; inline readout/key, labels, red UB verified | grab changes still uncommitted by existing owner, no final bundle approval | next review owner's final immutable commit/bundle; no edits/builds in owner worktree

- 2026-10-23 06:56 AM Eastern cx-pc #25411 | 2dea3caa implements silver native grabs, red unblockable, retained key + readability; three ASIs built and bundled captures/pc-install-25411 with TSV + manifest. Offline tests PASS; no game touched. Next pc install bundle, set FrameMeterLegend=1/FrameMeterReadout=1, validate live. Hashes notes/frame-meter-bundle-25411.md.

- 2026-10-23 6:56 AM Eastern | cx-pc #25379 isolated grab colour + production GDI checks built/pushed; branch cx-pc/frame-meter-25379-isolated.
  40px total height and grab/recovery/unblockable states PASS offline. Next rebuild Rematch/EngineFix, suites, package; grab predicate live-unverified per pc #25434.

- 2026-10-23 06:57 AM Eastern cx-pc #25430 | Audited 11476-row TSV: no grab field; documented native H->R->flags pointer proof in notes/grab-classifier-25430.md. Existing 25411 bundle unchanged. Next pc live-check silver grab windows; cx-pc no game lease.

- 2026-10-23 6:59 AM Eastern | cx-pc #25379 authoritative isolated bundle complete, pushed a4bdf751 on cx-pc/frame-meter-25379-isolated.
  Three release builds + 10 suites + 40px production-renderer/catalog checks PASS. Bundle ../nioh3-frame-meter-25379-isolated/captures/pc-install-25379; next pc install all four files, set readout/key=1, verify experimental grab classification live.

- 2026-10-23 7:08 AM Eastern | cx-pc #25490 decision relayed to active #25479 implementation session: negative-pool signal alone gates KI DEBT, no human classifier.
  Supersedes #25486 and human-gate portion of review #25499. Single label constant KI DEBT, violet, key Ki debt; next implementation owner update mock/code and pc validate. No code/game changes.

- 2026-10-23 07:15 AM Eastern cx-pc #25479/#25500 | fab5ad11 pushed: contrast + optional KI DEBT row; 6 suites and Training/Rematch builds PASS. Bundle captures/pc-install-25479 (3 ASIs, records + actionsets TSVs, two mocks, manifest); hashes notes/frame-meter-bundle-25479.md. Next pc install, set FrameMeterKi/Legend/Readout=1, validate Yoritomo/Himiko composed-human gate and per-frame colours. No game touched.

- 2026-10-23 07:20 AM Eastern cx-pc #25541/#25550 | Final combined placement branch cx-pc/frame-placement-25541 at 7ffe005c includes KI counts d2608bf4, boss records, original pad default. 3 builds + 11 offline suites PASS. Bundle captures/pc-install-25541; hashes notes/frame-meter-placement-25541.md. Next pc install and set FrameMeterY=-1 (Ki/Legend/Readout=1), verify player-HUD clearance and human KI gate. No game lease.

- 2026-10-23 7:21 AM EDT | cx-pc #25541: merged approved fab5ad11 Ki debt with combined pad/hit-data build; default meter anchors bottom at 82% minus 8px/1080p, left -3%; explicit Y preserved. Code pushed 82c92098.
  Proven: 3 release builds, 8 Training suites (12,960 number cases), 4 pad suites (393,216 comparisons), targeted menu and catalog freshness. No game accessed. Next pc: install captures/pc-install-25541 and set FrameMeterY=-1; validate HUD clearance in game.


2026-10-23 7:21 AM Eastern | cx-pc #25547 | player-health auto placement + saved menu Y built/pushed; X unchanged; KI DEBT retained.
Training/Rematch builds + four offline suites PASS. Next: bundle matched binaries and report to pc; notes/frame-meter-position-25547.md.

- 2026-10-23 7:21 AM EDT cx-pc #25541 FINAL correction: use ONLY captures/pc-install-25541-82c92098 (5 payload files + hashes/INSTALL), branch cx-pc/meter-position-25541 at45d6a52a, code82c92098. Unqualified captures/pc-install-25541 already contained another session's mocks/manifest; our first copy replaced its ASIs, so that mixed directory is not authoritative. Next pc: install unique bundle and set FrameMeterY=-1; game remains untested.

2026-10-23 7:24 AM Eastern | cx-pc #25547 | bundle captures/pc-install-25547 ready; source 23190b8c, hashes notes/frame-meter-bundle-25547.txt pushed.
Four suites + Training/Rematch build PASS. Next pc install both ASIs together with FrameMeterY=0; verify above-HP position live.

- 2026-10-23 7:27 AM Eastern cx-pc #25589 | Saved Frame meter Y menu complete at dc95f215 (Auto/-1 or 0..90%, live + persisted); X preserved. Training/Rematch builds and 3 targeted suites PASS; bundle captures/pc-install-25589, hashes notes/frame-meter-y-menu-25589.md. Next pc install and verify estimated 83% bottom anchor in fight. No game touched.

- 2026-10-23 7:38 AM Eastern cx-pc #25639 | Offline Ito diagnosis: AGGRO ran; HoldKiBreak=1 then explicitly disabled AI. Idle does not gate either overlay; identified wrong-window caching path, actual draw gate unlogged. Proposal notes/ito-overlay-review-25639.md; next pc release hold and instrument/validate HWND + capture. No game access.

- 2026-10-23 7:47 AM Eastern cx-pc #25653 | a5292b89 pushed: shared overlay host repair and draw/KI gate diagnostics; Training build +3 offline suites PASS. Bundle captures/pc-install-25653 hashes notes/overlay-gates-25653.md; other binaries hash-verified unchanged from25589. Next pc install/retest Ito, inspect DRAW mask and detail; no game access.

- 2026-10-23 8:16 AM Eastern cx-pc #25713 | Centered meter from live-tested82c92098, zero X offset, total panel capped60%, text clipped, Y anchor unchanged. Build +position suite PASS at6 resolutions. Bundle captures/pc-install-25713-centered60-20261023-0817; next pc install Training only and check HUD clearance. No game access.

## 2026-10-23 08:43:02 session 5c502ad0-b830-4eb0-a0ca-b91f531f5e93
Ally's audit is still at about 49 of 69 rows, so there's nothing new to start. The final 3-arm benchmark still waits on ally, expected around 10:50 am EST. I changed nothing this run, and nothing is blocked.

## 2026-10-23 09:04:43 session 5c502ad0-b830-4eb0-a0ca-b91f531f5e93
Ally's settings audit is moving again. `results-ally.csv` is now at 57 lines (about 56 of 69 rows), up from 50 at 8:54 am EST, so it's still running. The final 3-arm benchmark waits on it. Nothing is blocked, and I changed nothing this run.

## 2026-10-23 09:33:47 session 5c502ad0-b830-4eb0-a0ca-b91f531f5e93
Ally's settings audit is at about 56 of 69 rows, and nothing has been added since 9:04 am EST. `results-ally.csv` on its branch still shows 57 lines. I haven't checked whether ally is stalled or just slow on a run. pc2 hasn't replied yet to my reboot and pagefile suggestion for the stalled PJH A/B test. Nothing is blocked on you, and I changed nothing this run.

## 2026-10-23 10:05:03 session 5c502ad0-b830-4eb0-a0ca-b91f531f5e93
Ally's csv is at 65 lines (about 64 rows), so its main pass is nearly done. It was at 57 lines at 9:04 am. The queued AS0 OFF rerun still has to run before the final 3-arm benchmark can start. Nothing is blocked, and I changed nothing this run.

## 2026-10-23 10:35:55 session 5c502ad0-b830-4eb0-a0ca-b91f531f5e93
Ally's audit csv hasn't changed since about 10:00 am EST, so it's stuck at 64 rows (65 lines). The final 3-arm benchmark still waits on it, because I need ally's final results to settle which settings go in arms 2 and 3.

I asked ally (#25846) whether the AS0 OFF rerun is running or done, and to push the final csv. Nothing else is waiting in my inbox, and I changed nothing this run.

- 2026-10-23 11:19 AM Eastern cx-pc #25896 | Offline audit runner fix: elevated preflight, unique ETW session/PID/v1, stderr+exit checks, batch fail-fast, fresh boss heartbeats, owned foreground enforcement. Helper regressions/parser PASS; no live capture. Next pc elevated MaxRuns1 stage2 retry; notes/audit-capture-25896.md. No game/ETW touched.

## 2026-10-23 11:52:43 session 19bfb199-51ef-4603-8232-a697697998f5
I'm not starting any work. The only open step is the main PC's stage-2 settings check, which is waiting on you. I already said so once in #25939, so I'm not repeating it.

The task board shows no new failures from pc2 or ally since then. The failed entries it lists are old stage-1 and earlier tasks. I'm ending this run.
2026-10-23 12:28 pm EDT | cx-pc #25976: MCProbe 955ad56c and JTCP 48edcc1a pushed; both OFF by default, full builds + offline harnesses PASS. Runtime/benefit unproven. Next pc diagnostic captures after audits; artifacts captures/newopt-25976-20261023-1228/{mc,jtcp}.
2026-10-23 12:46 PM Eastern | cx-pc #26019 complete: 9b29fa44 pushed on cx-pc/newopt-design-26019. Identified constant-buffer copy paths and meshlet request coordinator; 1344 model cases + native CBU harness PASS. Next pc: review design notes/newopt-design-26019.md; establish shadow ownership before MCS, use narrow phase trace before scheduling changes.
2026-10-23 12:50 PM Eastern | cx-pc #26036: 54b9f33f pushed (cx-pc/newopt-census-26036). Installed and MC-original INIs omit all seven CBU controls; code defaults zero, so CBN OFF in MC capture. Next pc census existing CBN before MCS; meshlet phase plan narrowed to 8 call sites, 100000 aggregate cases PASS; native phase probe not built. notes/newopt-census-26036.md

## 2026-10-23 13:16:46 session 92d972c4-c27e-4aef-9d84-93cf4123d632
I checked on pc2 and ally and started no new test. Everything left on the main PC needs an admin PowerShell, and only you can open that.

- **Ally:** its memory-copy test (MCE) shows "failed" on the board, but it's only queued behind the settings check. It should start about 2:25 pm EST.
- **pc2:** its two queued tests (memory-copy MCE, constant-data skip CBN+CBT) have sat unclaimed since about 12:45 pm EST. I messaged pc2 asking why and to claim the constant-data one first.
- **Main PC:** the stage-2 settings check and the constant-data test both need the admin step, so I'm leaving them.

I added a line to `next.md`. Next time pc2 reports, I'll go through the memory-copy and constant-data results. The final three-way benchmark (stock, pure-performance mod, full mod) follows once those are in.

## 2026-10-23 13:39:37 session 9f4ef70b-e35e-494c-9fc6-2ac67339cdf4
Ally has finished its whole settings check. Run 28 of 28 completed at 1:32 pm EST, earlier than the 2:25 pm estimate because it dropped the third repeats.

- **Ally:** I asked it to push its results with a verdict per setting, then start the memory-copy (MCE) test (task #112). I haven't confirmed it started.
- **pc2:** the MCE test and the constant-data skip test (CBN+CBT) have sat unclaimed for about an hour. I asked pc2 to take the constant-data one first. If it's blocked, I asked it to say why.
- **Main PC:** I haven't started anything. You said to forget PowerShell for now, and I didn't launch the game.

I haven't done any work on the constant-data skip since the last census, so there's no new result on it. Once Ally's numbers arrive I'll send them to you.
2026-10-23 1:55 PM EST pc: CBN=1,CBT=1 ON in Yoritomo fight (pc, no admin): PASS no crash, skipped ~50k uploads/s = ~225 MB/s of ~900 MB/s (21%), table reuse hits 33k/s. fps effect not measured (needs PresentMon admin); pc2 earlier measured +15.9% with CBN+CBT. Shipped repo INI already has CBN=CBT=1; installed INI lacks keys (code default 0) -> add keys to installed INI. NEXT: pc2 #113 A/B; ally MCE ~2:25pm; final 3-arm bench.

## 2026-10-23 14:22:40 session 9f4ef70b-e35e-494c-9fc6-2ac67339cdf4
Two things are in motion:

- **Report button:** the specs file is built and committed as `96611268` on `pc/report-specs`. It isn't installed yet. I'll install it once the constant-data test is done.
- **Constant-data test:** two of the four fights have started, with the skip off and then on. When all four finish, I'll compare the average frames per second between off and on and tell you whether the main PC is free.
- 2026-10-23 pc: answered 'ally done?' with last known 21/44 stage2, ETA 2:25pm EST (#26181)

## 2026-10-23 15:13:15 session 9651b621-53f7-49f6-9b41-8469f66b3f7a
At 3:13 pm EST, ally is idle and its game is closed, but its memory-copy test result hasn't arrived. I've asked ally for the memory-copy fps numbers (off vs on), or the reason it failed.

pc2 hasn't picked up its two tests, memory-copy and constant-data skip, in over an hour, so it looks offline. I won't wait on it. Next I'll build the default-settings list from the data I already have.

I told you the same in the phone chat. I didn't start or touch the game.
2026-10-23 pc: ally MCE neutral (12.11 vs 12.00 fps). Classified keys: notes/default-on-candidates-20261023.md (fc3645ac): group A 32 no-visual keys (default-on candidates), group B 12 visual keys (full-mod arm), 51 leave off. NEXT: build arm INIs (stock / A / A+B), final 3-arm bench on pc, pc2, ally (PresentMon needs admin on pc); pc2 offline? #111/#113 unclaimed.

## 2026-10-23 15:39:13 session 9651b621-53f7-49f6-9b41-8469f66b3f7a
Progress on the final benchmark and the settings choice, as I told you in the phone chat:

- **Settings classified:** from all the audit data, 32 are safe to turn on by default, 12 lower the picture (full-mod arm only), and 51 stay off. The list is in `notes/default-on-candidates-20261023.md`.
- **Arm settings files:** built for the pure-performance arm and the full-mod arm.
- **Ally:** it has the final 3-way benchmark running.
- **pc2:** its try failed 35 seconds after it claimed the job, with "worker ended without reporting". This is probably the Windows update. I asked pc2 which step failed and re-posted the task.
- **Main PC:** still waiting on the admin run. Paste the one-line command I sent earlier into the admin PowerShell. It takes about 25 minutes.

There are no final numbers yet. I didn't touch the game.

## 2026-10-23 ~3:50 pm EST pc (context full)
- DONE: feedback site live+redesigned (https://nioh3-mod-feedback.vercel.app, Vercel project nioh3-mod-feedback, site dir C:/Users/Ahmad Hamza/nioh3-mod-site/site; node at C:\Program Files\nodejs must be added to PATH). Report button default URL = that site (pc/report-specs 1cdccfb1, worktree C:/Users/Ahmad Hamza/nioh3-report-specs; NOT built/installed in game yet). 32/12/51 settings classification: notes/default-on-candidates-20261023.md.
- HALF-DONE: final 3-arm bench: ally task #115 running; pc2 retry #116 open (#114 failed after 35s, asked pc2 why, Windows update); pc leg needs admin PresentMon: created tools/rematch/make_elevated_task.cmd + elevated_entry.ps1 (gitignored dir 'tools', so NOT committed; main worktree has an unfinished cherry-pick, left untouched). Human must right-click make_elevated_task.cmd > Run as administrator once (needs-player.md line added); then I can `schtasks /run /tn Nioh3ElevatedRun` with script path in tools/rematch/elevated_next.txt.
- NEXT: (1) when human confirms, write elevated_next.txt -> pc final bench (stock / pure-perf arm2 / full arm3, 2 reps each interleaved, all metrics: fps, lows, latency, temps, util, power, in-game+external); (2) digest ally #115/pc2 #116 RESULTs, tell human; (3) human #26397: ONE universal mod -> make platform-only keys (Proton/handheld/non-ReBAR) auto-detect (-1), re-check borderline MW,P12,M1,DL,R1; (4) after bench: install pc/report-specs build on pc (rematch asi) and test report button; (5) answer human question pending: switch Proton-only keys on for Ally? (they said one mod, so use auto).

## 2026-10-23 16:38:56 session 92d972c4-c27e-4aef-9d84-93cf4123d632
I'm waiting for the last baseline run of the main PC's 8-run check. Once it ends I launch the final three-way benchmark through the same admin task.
2026-10-23 4:52 PM EST pc CHECKPOINT | task: final 3-arm bench + default-on settings. DONE: key classification (notes/default-on-candidates-20261023.md: 32 safe / 12 visual / 51 off), arm INIs + pc plan (captures/final-bench/{arm2-pureperf,arm3-fullmod}.ini, pc-plan/), runner fixes (per-device lock nioh3-$Device; accepts 'fight now: in place'), stage2 EP on pc (OFF 93.3 vs ON 91.0, neutral; EP maybe veto), feedback site live https://nioh3-mod-feedback.vercel.app, CBN+CBT run OK on pc (225MB/s skipped). HALF-DONE: pc final bench RUNNING elevated since 4:49 pm (captures/final-bench/run-pc-final.ps1 -> results-pc-final.csv, pc-sensors.csv, newest captures/audit-screen/run-pc-*; marker test-active set, runner removes it at end). ally #115 running (another ally session), pc2 #113 CBN then #117 final bench then #111 MCE (pc2 has dup-session issues). NEXT: when pc run ends verify saves restored + lock nioh3-pc released + test-active gone; collect results of all 3 devices, compute 3 arm means, tell human; reconsider EP in group A; make installed INI use default-on (CBN/CBT missing keys); merge pc/newopt-20261023 MCE; ONE-mod auto device detection for platform-only keys.
2026-10-23 5:23 PM EST pc CHECKPOINT | pc final 3-arm bench DONE (6/6 valid): stock 77.3/75.2 avg (low1 51/50), pure-perf 100.5/95.7 (77/68), full 95.5/99.0 (58/75). Sensors in captures/final-bench. pc2 #118, ally #119 reposted/running. NEXT: digest pc2/ally RESULTs, then FG ON/OFF set, then README chart | captures/final-bench/results-pc-final.csv

## 2026-10-23 17:49:24 session c4e7c846-c79b-4ffe-b1de-f03286909067
I logged pc2's note and took no other action, so it needs no reply.

The note says the FSR frame-generation overlay can wrongly read OFF. Its "rendered" counter also just copies the displayed fps when it has no game-thread samples. In frame-gen runs I'll use PresentMon or the externally displayed fps instead. pc2's PresentMon figure of 87.72 fps matched the on-screen counter at about 85–88.

I added this as a line in `TECH_DEBT.md`.

pc2 also said its earlier final117 run is an incomplete frame-gen-off capture set, not a finished six-run benchmark. Its resumed session found a newer game launch with a fresh test marker, so it is leaving the game, ASIs, INIs and saves alone.

Still pending:
- **ally:** its 3-arm task (#119) is still running.
- **Frame-gen runs:** frame-gen-on runs are still needed on all three devices.
- **GitHub chart:** this comes last.
2026-10-23 5:56 pm EST pc | pc final 3-arm bench done (results-pc-final.csv) | FG-on runs, pc2 final, ally #119, GitHub chart pending | next: queue FG-on arms on all 3 devices | captures/final-bench/results-pc-final.csv
2026-10-23 6:15 PM EST pc | pc FG-on final bench launched elevated (run-pc-final-fgon.ps1, results-pc-final-fgon.csv), ~35 min; config.xml restored by finally | pc2 FG-on done (stock 87.1/safe 88.3/full 93.6); ally FG-on sent | next: collect, then GitHub chart | captures/final-bench
2026-10-23 6:11 PM EST pc CHECKPOINT | pc FG-ON final bench launched elevated (run-pc-final-fgon.ps1, config.xml FRAME_GENERATION=FSR, auto-restored from captures/final-bench/config.xml.prefg after). pc2 FG-ON done (stock 87.1/pure 88.3/full 93.6), pc2/pc FG-off done, ally FG-OFF done (+4-5%), ally FG-ON task posted. NEXT: when results-pc-final-fgon.csv has 6 rows + test-active gone: verify config.xml FRAME_GENERATION=OFF restored, digest, then ally FG-ON RESULT, then README chart (dataviz skill) + notify human.
2026-10-23 15:23 | cx-pc2 tasks #26479/#26541/#26682/#26768/#26779/#26785/#26793 all DONE+reported (offline, game untouched) | final3arm fixes f94357c1, run_summary v2 cols a65352a5 (branch cx-pc2/final3arm-26458); PC2 FG x 3-arm plan+runner on cx-pc2/pc2-fg3arm-26768 (worktree ../nioh3-pc2fg-26768) head 98be78ff: STOCKLIKE=pc arm1-stocklike.ini, v1 primary + v2 companion PM session + sensor_collector | UNPROVEN live: two PM sessions on one pid (rerun -NoV2Companion if v1 breaks) | next: wait for pc2 TASKs; no open work | files: tools/rematch/audit_screen_run.ps1, audit_capture_helpers.ps1, make_final_pc2fg_plan.py, captures/audit-screen/final-pc2fg
