
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

## 2026-10-23 18:32:29 session 24f4977c-0ded-46f9-8e4d-cfd55e02dc1a
Not done yet, and I told the user so in #26849.

- **Main PC:** the frame-generation-ON benchmark is running now. It started around 6:10 pm EST and should end around 6:50 pm EST. Its results file has 5 data rows so far.
- **pc2:** its frame-generation-ON result is in.
- **Ally:** its frame-generation-ON result is still pending.

Once all three are in, I'll build the GitHub chart and message the user.

I haven't touched the game this turn.
2026-10-23 6:52 PM EST pc CHECKPOINT | pc FG-ON bench DONE 6/6 (146.8/152.6/156.0 fps), config.xml restored OFF, game closed. Waiting: ally FG-ON (#121 ~7:30 pm), pc2 boss-fight 12 runs (#26758). NEXT: when both in, build README chart (dataviz skill) from results-pc-final*.csv, pc2 final-pc2-118/120, ally ally/final-bench-119 + FG-ON; notify human | captures/final-bench/results-pc-final-fgon.csv

## 2026-10-23 ~7:03 pm EST pc (context full)
- TASK: final 3-arm bench (stock / arm2 pure-perf / arm3 full mod) x FG off/on, all 3 devices, then GitHub chart (user #26593).
- DONE: pc FG-off (stock 76.2 / arm2 98.1 / arm3 97.3 fps; lows 50.7/72.5/66.6) and pc FG-on (146.8 / 152.6 / 156.0; lows 84.8/90.9/94.9), files captures/final-bench/results-pc-final.csv + results-pc-final-fgon.csv. pc2 FG-on: 87.1/88.3/93.6 (Kyoto scene, game speed 0.69x in stock FG-on => not comparable). 
- BROKEN/REDO: ally runs INVALID: player died ~30s in (god-mode off in bench INI?) AND platform limit stuck ~5W not 30W. Ally redoing FG off+on with god-mode on + power fix. Open Qs to ally: #26947 #26953 #26961 #26967 #26992 (god-mode key diff vs human's INI; game speed per arm). Human says god mode was ON by default and played Ally 30-45 fps fine.
- NOT tracked on pc: temps/power/latency (no sensor tool; offered to install, awaiting yes/no). In-game settings per device asked of pc2/ally (#26923); pc = CUSTOM preset, XeSS, landscape/meshlet HIGH.
- NEXT: digest ally/pc2 RESULTs; re-check pc2 game speed; then build chart + table + per-device settings on GitHub README (open.md item), ping human. Directives logged in user-directives.md (#26524..#26964).
2026-10-23 07:45 pc | meter build 25589 INSTALLED+tested (Yoritomo, shots bridge #1575/#1576); KI DEBT on duelist Ito (D084F) NOT drawing: frame meter/KI row/negKI overlay invisible although log says following+overlay ready (HoldKiBreak=1 also holds his AI off; with 0 he fights). | cx-pc TASK #25653 pending (patch for overlay HWND selector 1ae6042f on cx-pc/ito-overlay-review-25639 + draw-gate logging) | NEXT: install cx-pc bundle, retest D084F with HoldKiBreak=0 (Training.ini orig saved captures/meter-final-0723/Training.ini.orig; current INI is the original), then audit: pc stage-1 done (67 valid, captures/audit-screen/results-pc.csv, NOT pushed/reported: git pull --rebase blocked by unstaged changes; run `task report 110`), pc2/ally results pending, then confirm stage, final bench, 10x ping | files: tools/rematch/audit_screen_run.ps1 (local commit only), captures/meter-final-0723

## 2026-10-23 19:36:39 session b0fc9051-736a-4c8e-a18e-14ee016e3f16
I told the user the Ally should now benchmark much faster, since its CPU hits 4.1 GHz under load again instead of about 0.9 GHz. That is a guess: I haven't seen any new Ally numbers yet.

I said the first valid runs are due soon and that I'd send them when they land. I haven't touched the game, and I'm still waiting on the Ally's results.

## 2026-10-23 19:58:01 session b0fc9051-736a-4c8e-a18e-14ee016e3f16
The answer to the user is no: this PC hasn't started the default + fps-fix-only run. I was told not to touch the game in this run, so I haven't launched it.

I told them it launches in my next work run: same boss fight, god mode on, game speed logged. I logged the next step in `.handoff/next.md`: build and launch the main-PC arm B after taking the game lock and backing up saves. The earlier final-bench runner scripts in `captures/audit-final-20261023/` are the pattern to follow. pc2 and the Ally are already on their sets.
2026-10-23 20:27 | pc arm B done | fps-fix vs stock-like ~equal fps on pc; speed unlogged | wait ally/pc2, build chart | captures/final-bench/*B*
2026-10-23 8:37 PM Eastern | cx-pc #27402: DRAFT bench chart/table pushed 78bd3c8a (cx-pc/bench-chart), local ../nioh3-bench-chart/docs/bench-draft.{png,md}. 14 measured aggregates, 16 PC capture counts verified, separate fix-only baseline; PC2 VRR-off caveat and Ally pending. Next pc review/integrate then update pending results; generator docs/build_bench_draft.py.

2026-10-23 8:43 PM Eastern | cx-pc #27408 | three README refreshes committed/pushed e6c3d65b on cx-pc/readme-refresh; no main merge.
Verified 55 INI defaults, local links, unchanged renderer section; docs cover source/integration differences. Next pc: review/cherry-pick documentation; benchmark chart remains separate.
2026-10-23 8:46 PM Eastern | cx-pc #27430: benchmark redesign pushed f58a3a24 cx-pc/bench-chart; Discord file sends confirmed for PC, PC2, combined PNG. Sorted avg/1%/derived0.1 bars, deltas, PC frametime traces + CPU/display panels; unavailable sensors hatched, PC2 invalid-stock deltas withheld, Ally pending. Next pc review/integrate docs/bench-draft*.png/md. No game touched.
2026-10-23 8:52 PM Eastern | cx-pc #27449: 9317dcaa pushed cx-pc/bench-chart; fixed REAL DEFAULT/REAL FPS FIX/SAFE MODE/EVERYTHING colours/order, +FG labels, proxy caveats. Three Discord PNG sends confirmed. Next pc review docs/bench-draft*.png; no game or measurement changes.

2026-10-23 8:59 PM Eastern | cx-pc #27479: hitch panels built and pushed cb1505c4 on cx-pc/bench-chart; 16 raw PC captures hash/count verified, strict thresholds/percentile checks PASS.
PC paired FPS-fix worst 111.09 ms, >50 ms 1.00/min vs default 21.60 ms, 0/min; descriptive only, game untouched. Next: send PNGs to Discord and pc review; docs/bench-draft-hitches.md + pc-hitches.json.

- 2026-10-23 9:02 PM EDT cx-pc #27507: README draft pushed 4b2c8b37 on cx-pc/readme-final-27507 (based on e6c3d65b); report/Vercel workflow, one EngineFix INI OS auto-detection, optimization groups, pad/frame/KI features, benchmark table and chart evidence. 27 links and 19 imported evidence files verified; no code/game changes. Next pc: review branch only, do not merge to main until approved; final fleet figures still await valid PC2 baseline/Ally.

2026-10-23 9:02 PM Eastern | cx-pc #27491 | PC2 five-arm chart update pushed f3381603 on cx-pc/bench-chart; 10 files, retained bridge 1712/1713 inputs.
Exact FPS/lows, FG flags, B-relative deltas and PNG integrity/size PASS; offline only. Next: send PC2 + combined PNGs to Discord, pc review. Hitch panel retained from cb1505c4; PC2 raw intervals unavailable.

2026-10-23 9:02 PM Eastern | cx-pc #27491 | PC2 and combined PNGs successfully sent via Discord notify; source f3381603 pushed.
Next pc: consume charts; PC2 hitch counts remain n/a until raw intervals are supplied.

2026-10-23 9:07 PM Eastern | cx-pc #27555: MPC20 guarded scalarization built/signed; branch cx-pc/mpc-scalar-27555 pushed. 149 original/298 candidate blocks checked, 3 mutation tests caught, 38 other records unchanged.
Native/perf and full interpreter untested; no game touched. Next: finish ranked CPU/preset plan and deliver branch + artifact; files gpu-shaders/meshcull/{mpc_uniform,verify_mpc_uniform,build_mpc_uniform}.py + mpc20-uniform-manifest.json.

2026-10-23 9:09 PM Eastern | cx-pc #27555: ranked plan complete/pushed 971e232b (implementation 270191a6), cx-pc/mpc-scalar-27555. Candidate bridge file 1729, sha256 2652a13f0fb7; 38 unchanged records, 3 mutation tests, compiler/roundtrip PASS.
Native ISA/perf/full interpreter untested; MPC33 planned, CPU wait tuning and opt-in 60%/50% presets ranked with explicit arms. Next pc: review notes/remaining-perf-plan-27555.md, test isolated MPC20 OFF/ON 2 repeats each. No game touched.

2026-10-23 9:10 PM Eastern | cx-pc #27562 | ranked historical Ally GPU/CPU evidence and 5 large-win hypotheses; built opt-in XLOD4 x6, pushed ce4a0f7e cx-pc/big-wins-27562.
Native ExtraPerf + 257-site pattern validation PASS; ASI B4600562ABBC8F6B02A701DB3FB5D02E35266D8AA70FBAB63425D92C513E98D6. No measured gain/game access. Next pc/ally integrate source into current matched bundle, test XLOD0 vs4.
2026-10-23 9:11 PM EST | gx-pc #27623 big-risky priority | human wants shadow/GI/cloud half-rate, VRS, dynamic res, culling rewrite first | each opt-in default off, tests on 3 devices later | next code step is meshlet culling milestone 1 | STATUS #27633 | game not opened
2026-10-23 9:14 PM EST | gx-pc #27660 exclusivity | cx-pc static preset sets REN_RAT 60/50 and DYNAMIC_RESOLUTION OFF, restores originals when disabled, no renderer hooks | gx-pc dynamic-res hook must stay off while that preset is on | next build is still meshlet culling opt-in | STATUS #27668 | game not opened
2026-10-23 9:14 PM EST | gx-pc #27603 big-wins rank | four families all under 4.3 ms on Ally USER 28.7 ms | best picture cut 1.5-2.7 ms E | commit 4199075e pushed | RESULT #27671 | game not opened | next measured step remains fogpar on Ally, not a new patch

2026-10-23 9:15 PM Eastern | cx-pc #27600 complete: 58ffdeea pushed cx-pc/temporal-drs-27600. Offline policy tests PASS; reviewer stale-generation/old-ack defects fixed with monotonic generations and proposal tokens.
New native DRS boundary: 5AFA40/5AFAB0 use 3-sample timing path before separate 60-sample avg. Ally 50->35% model saves 4.39 ms (18%, unmeasured); temporal alone not shown >=15%. Next pc: fixed-scale characterization + native producer/resize audit; notes/temporal-drs-bigwins-27600.md.

2026-10-23 9:15 pm EST | Big-wins design review, no game | Proven: none of the current designs has a measured 4.3 ms Ally saving. XLOD x6 can clamp to the engine cap of 10. MPC20 record grew to 57724 bytes. VRS is unwired. DRS policy cannot drop Ally below 50%. | Next: run arms A1 then A2 on Ally before stacking. Do not serve compute-PSO cache hits. | branch gxl-pc/big-wins-review-27604 commit 9fcc558e notes/big-wins-review-27604.md

2026-10-23 9:25 pm EDT | gx-pc #27607 reuse / dynamic res / VRS | none clears 4.3 ms of the Ally 28.7 ms frame as a same picture | REN_RAT 35 is an unmeasured ~4.4 ms E dip; the live policy floor of 50 saves 0 | commits 4411dc11 and 077576b5 pushed on pc/fightnow-swap-wire | next: do not build; static 35% arm is review A4; culling stays with cx-pc

- 2026-10-23 9:17 PM EDT cx-pc #27635: matched XLOD integration pushed1243206c (code090f5bcb) on cx-pc/xlod-matched-27635, base1cdccfb1. 1.7.73-xlod4-27635 release build + ExtraPerf/padviz/260 patterns PASS; SHA473345313EF9A4B7C60A6134AA2F6807D229A38E365B006F1AA1E68DD7D47637. No game/Ally access. Authenticode unavailable (no signing cert); clarification Q27693 pending. Temporal condition not supported by gx note4199075e; Q27708 asks for any separate hook proof. Next pc: review/install hash-verified artifact after signing clarification; preserve matched addons and queue XLOD0 vs4 later.

2026-10-23 9:17 pm EDT | cx-pc #27608: built opt-in 60%/50% fixed-resolution and CPU wait presets; code 844b2039 on cx-pc/reswait-27608 pushed.
176 native offline checks PASS; ASI SHA256 a230a462368254f042916aec66f8617f4a492f6182cbb07eeb5c95a5fc990a03; runtime/gain unproven.
Next pc: integrate/rebuild current stack, verify first-launch resolution, then pc2/Ally OFF vs ON 2 repeats/arm per notes/reswait-presets-27608.md; no game touched.

2026-10-23 9:17 pm EDT | gx-pc #27607 RESULT #27733 to pc. Shared #27734 cx-pc and #27735 cxl-pc. Note 077576b5. Game not opened.
2026-10-23 9:17 PM EST | gx-pc #27701 map radius | seven distance knobs, none clears 4.3 ms | XLOD2 and XSHD3 measured about 0 on Ally | best unmeasured 0.2-0.8 ms E foliage | commit 53e519e1 pushed | RESULT #27737 | game not opened | do not shrink the cell-load radius for fps
2026-10-23 9:18 PM EST | gx-pc #27717 mined named notes | no measured item clears 4.3 ms Ally except untested REN_RAT 35 | failed: lodu, mpc shaders, fog-to-direct, VSMA, LPVR/SKYR/CLDR/FOGT, gi #121, VRS has no call | file 1744 RESULT #27746 | game not opened
2026-10-23 9:18 PM EST | gx-pc #27724 bar correction | +15% fps at 28.7 ms is 3.74 ms; 4.3 ms is 15% less frame time | rankings still miss both | commit c1648fe9 | A #27763 | game not opened | next is cxl-pc reuse map, not a new gx-pc patch
2026-10-23 9:18 pm EDT | gx-pc #27744 | cx-pc 27608 is static 60/50 presets only, not scale 35 | no culling build assigned | asked pc in #27766 | A #27765 | next: wait for pc to name the culling owner

2026-10-23 9:21 pm EDT | cx-pc #27635 / approval #27696: SHA256-only artifact requirement resolved; existing matched XLOD build rehashed to 473345313EF9A4B7C60A6134AA2F6807D229A38E365B006F1AA1E68DD7D47637, base 1cdccfb1 ancestry confirmed.
Build/test evidence remains 090f5bcb/1243206c; no code change or runtime claim. Unsupported temporal reuse explicitly skipped by pc.
Next pc: retain matched addons and queue XLOD OFF0 vs ON4, 2 repeats/arm FG OFF on Ally; notes/xlod-matched-27635.md.

2026-10-23 9:19 PM EDT | cxl-pc #27716 prior-research map DONE/pushed 3e3918a6 on cxl-pc/vrs-27602.
Reviewed requested CPU/GPU/streaming/settings/rewrite notes with independent GPU read; historical hooks and failed/neutral attempts consolidated, no new measurements.
Next big-wins team: reuse RCO/GIS/EPL/EPF/native EffReduction; read notes/big-wins-prior-research-27716.md before integration. VRS remains unwired; no game touched.

- 2026-10-23 9:19 PM EDT cx-pc #27700: fixed characterization specification on cx-pc/drs-characterization-27700; policy integration belongs to separate cx-pc policy session58ffdeea (ownership confirmation Q27748). No adaptive ASI produced here: producer units/native resize acknowledgment unresolved. Next pc/policy owner: integrate authorized5AFA40/5AFAB0 only, floor35/40/50; no60-frame-average patch. Signing requirement resolved: SHA256 is sufficient for XLOD27723. No game access.

- 2026-10-23 9:19 PM EDT cx-pc #27720 confirms conditional #27635 temporal step is not supported: gx-pc has no verified G-buffer/shadow history or invalidation hook; GI1872C4/187A50 change size only. No blind skip; gx-pc retains far-shadow/GI ownership. Existing matched-XLOD report6285c951 already records pc skip instruction/signing resolution. Next pc: use delivered XLOD artifact for queued testing; no temporal build queued by this worker.

2026-10-23 9:22 pm EST | Mined prior research for the big risky wins | Proven misses: fog queue move 0.06 ms, lodu 0 fps, MSV stretched triangles, LPVR/CLDR/FOGT dropped, XLOD x2 -0.4% on Ally. Only unmeasured 4.3 ms idea is REN_RAT 35. Potato already measured +9.6% Ally | gx-pc half-rate build must skip RefProbe 4b4c3cbe, not the old knobs, and must not rebuild clouds or lodu | notes/big-wins-mined-27706.md
2026-10-23 9:22 pm EDT | gx-pc #27778 | applied 3.74 ms fps bar and VRS tier 1 vs tier 2 | reuse and VRS still miss both bars | commit 6a567260 | next: no new build; culling owner still waiting on pc #27766
2026-10-23 9:20 PM EST | gx-pc #27770 culling handoff | family 2.88 ms, under the 3.74 ms fps bar | addresses in notes/culling-hooks-for-cx-27770.md | commit 8ee53b88 | A #27805 to cx-pc | dynamic res stays opt-in default off, no patch | game not opened

- 2026-10-23 9:21 PM EDT cx-pc #27778: prior-research map reconciled into ranking bd261f7d, cx-pc/big-wins-magic-27582. Corrected cloud-reuse bounds for existing partial refresh; FPS/time and VRS tier distinctions already correct. No ASI/game change. Next implementation owners reuse validated seams and avoid listed rejected experiments.

2026-10-23 9:22 PM EDT | cx-pc #27715: prior-research ledger complete (15 requested notes, both gpu-rewrite rounds); notes/research-reuse-ledger-27715.md.
Offline evidence only: existing hooks cross-checked in patches.json; terminal MSV failure, async neutrality and stale build-status claims reconciled; no new performance claim.
Next pc: forward ledger to current scale/DRS/culling owners; keep scale35 and XLOD test arms separate, 2 repeats/arm; no game touched.

2026-10-23 9:23 PM EST | gx-pc #27831 DRS pointer | no patch, no menu row, no ini key | owner gx-pc, notes only on pc/fightnow-swap-wire 8ee53b88 | A #27838 | fixed preset cannot fight a hook that is not built | game not opened | do not edit cx-pc menu worktree
2026-10-23 9:23 PM EST | gx-pc #27840 | cx-pc 60/50 presets and CPU waits stay separate from DRS and the 35% note | no DRS patch, no culling build | A #27845 | their 176 offline checks are not a measured frame gain | game not opened | wait for pc before a dip below 50
2026-10-23 9:23 pm EST | gxl-pc #27711 load-distance | already covered, no new note | cell load about 0 ms; foliage/entity distance under 1 ms E; Ally XLOD2 -0.4% | note notes/map-radius-rank-27701.md commits 53e519e1 and c1648fe9 | next: leave Ally A1/A2 to pc; do not shrink the 1000 m load radius
2026-10-23 9:55 pm EST | gxl-pc answered cx-pc load-distance review | split agreed, no code | residency cut unproven; render-only later must not touch collision or the page pool | cx-pc note 3b9d502a, our rank c1648fe9 | next: no hook until a moving-route test is assigned by pc
2026-10-23 9:56 pm EST | gxl-pc accepted cxl-pc #27778 research map | no new code | map already matches our don't-rebuild list; 3.74 ms bar stands; VRS still 0 draws | commit 3e3918a6 notes/big-wins-prior-research-27716.md | next: no second VRS or fog build
2026-10-23 9:57 pm EST | cxl-pc #27883 closed the research agreement | no reply sent | 3.74 ms bar already shared; no build assigned | none | next: wait for a real TASK, do not ping this thread
2026-10-23 9:58 pm EST | cx-pc #27918 closed the load-distance split | no reply sent | both sides agree about 0 ms and no hook | notes stay separately owned | next: pc owns any later moving-route test, not stacked on A1/A2

2026-10-23 9:28 PM EDT | cx-pc #27740: combined 1.7.73-reswait-drs-27740 built on1cdccfb1; reswait + DRS source merged; fixed presets suppress adaptive override.
Release build, 176 reswait checks, 20 DRS cases, pad four-suite and261 patterns PASS; catalog added rows/base defaults checked (legacy whole-suite stale). Runtime unproven.
ASI SHA256 E02AA77793E6A79A7FDE521BD7EA19B9E63F3E683539AB7DC250BC866BC5F076; next pc verify/install matched artifact and run separate OFF/ON tests (2 repeats/arm); notes/reswait-drs-matched-27740.md; no game touched.

2026-10-23 9:29 PM EST | gx-pc #27895 | DRS prototype is cx-pc only: cx-pc/drs-matched-27690 0312d96b tip 828ac860, hook 5ABA87 to 5ABF64, default off, floor stays 50 | no second hook | culling 2.88 ms still under 3.74 | note d5873208 on pc/fightnow-swap-wire | A #27919 | temporal reuse stays gx-pc | 9046AA untouched | game not opened
2026-10-23 9:30 pm EDT | gx-pc #27921 | cx-pc DRS prototype is default off, hook 5ABA87->5ABF64, not static 35% | 17 wrapper and 261 patch checks pass, live ms unmeasured | pc owns the test | next: keep Enabled=0 during fixed-scale tests

- 2026-10-23 9:31 PM Eastern | cx-pc #27751 Ultra35 built/pushed815223a6, report ab502aa7 on cx-pc/ultra35-27751 | 110 native checks, EngineFix menu,260 patterns PASS; baseline addon assertion remains | next pc/ally verify actual35 scale vs50 baseline; file1772.
- Artifact SHA2565709E939724950ECAA607841BF59281A8F05355211A68B653482B44045281123 | defaultOFF/restart, restore viaOFF before removal; no game access.

2026-10-23 9:36 PM EDT | cx-pc #27936: 3 current guides updated, old README archived | local links/whitespace PASS; gains unproven, no game touched | next pc: integrate pushed 604608ca on cx-pc/docs-current-27936.
README.md, mod/Nioh3EngineFix/README.md, release/USER_GUIDE.md, notes/archive/readme-before-current-27936.md; base pc/report-specs.
2026-10-23 9:39 pm EST | gx-pc half-rate menu built | offline harness passed, game not opened | Ally estimate 0.7-1.35 ms, misses 3.74 ms, not measured | branch gx-pc/half-rate-reuse eccb0650 | next: ASI sha on the bridge (TASK #28002), then pc benches after current Ally arms
2026-10-23 9:43 PM EST | gx-pc #28002 half-rate ASI | built eccb0650, HRS HRG HRC left at 0 | harness skip/reuse/cut ok, not a frame time | file 1780 sha256 BE2FFBF0848AF00311CB7DC19D55C986D34065F0E875336C04CFAC05F5230922 | RESULT #28020 | game not launched | next is a device test only if pc assigns one
2026-10-23 9:44 pm EST | gxl-pc verified half-rate ASI | file 1780 sha matches BE2FFBF0848AF00311CB7DC19D55C986D34065F0E875336C04CFAC05F5230922 | frame time still unmeasured | eccb0650 | next: pc benches after current arms, one key at a time
2026-10-23 9:44 PM EST | gx-pc #28031 | gxl-pc accepted file 1780 | HRS HRG HRC stay 0 | not a measured gain | no further build | A #28037 | game stays closed
2026-10-23 9:45 pm EST | gx-pc #28037 closed the half-rate build | no reply sent | file 1780 stays off, frame time still unmeasured | eccb0650 | next: pc benches after current arms, one key at a time
2026-10-23 10:04 PM EST | gx-pc #27798 | gxl-pc mined list saves 0 ms | do not rebuild fog-queue, cloud skips, LPVR, CLDR, FOGT, lodu, meshlet VS rewrite | 35% dip still unmeasured | half-rate stays file 1780 keys 0 | DRS stays cx-pc | A #28096 | game stays closed
2026-10-23 10:04 pm EST | gx-pc #28096 closed the don't-rebuild list | no reply sent | nothing measured clears 3.74 ms; 35% dip still unmeasured | file 1780 stays off | next: no new gx-pc build until a bench is assigned

- 2026-10-23 10:18 PM Eastern | cx-pc #28160: half-rate counters built; cx-pc/halfrate-counters-28160 12788058, artifact1792 SHA25683a37dea5c99242a4616bc280e2807932efdf539b04be95f55eb197da17d7afc.
  Production gate/counter/log harness + existing policy harness PASS; full ASI build PASS. Default OFF unchanged; cumulative game-Present windows and calls, not GPU completion. Next pc/pc2: use notes/halfrate-counters-28160.md activation deltas; pc owns game tests.

- 2026-10-23 10:44 PM Eastern | cx-pc #28259 compute-culling gate: STOP, no justified sub-stage build from verified hooks | reviewed8ee53b88 culling-hooks/gpu-culling-rewrite/meshlet-redundant-lod and gx27805; historical same-result gain0.055-0.06ms PC, Ally0.11-0.24ms estimate; skip cnt/cbuffer equality unproven | next owner obtain MinRun1 per-stage Ally timing before reconsidering; no code/ASI.

- 2026-10-23 10:44 PM Eastern | cxl-pc #28264: checkpoint audit complete; cxl-pc/vrs-27602 at 3e3918a68a7cf5e4b924140ba203b54239baf29d matches live origin; tracked worktree clean.
  Prior-research map saved in notes/big-wins-prior-research-27716.md; VRS runtime coverage remains 0 draws, no measured gain. Next: pc assigns implementation/test; no cxl-pc work in flight.
2026-10-23 9:15 pm EST | pc final-bench coordination kept from the previous checkpoint | pc arm B 82.8 vs 81.9 avg | pc2 14 cells #27552 | Ally redo and XLOD OFF/ON were still queued then | game was closed
2026-10-23 10:50 pm EDT | gx-pc #28264 checkpoint | half-rate reuse is already pushed, three menu keys default off, no in-game milliseconds | commit eccb0650 on gx-pc/half-rate-reuse, ASI file 1780 | DRS stays cx-pc and off | next: pc benches one key at a time after the current arms
2026-10-23 10:45 PM EST | leftover levers ranked for #28243 | none of async, jobs, skinning, or particles clears 3.74 ms | particles 1.34 ms E, jobs about 0 frame ms, skinning has no stamp | commit 8927133e notes/new-levers-28243.md | next: do not build these; the 35% dip stays with cx-pc
2026-10-23 10:46 PM EST | standing order #28258 checked | gxl-pc code already pushed | lever note 8927133e matches origin; half-rate eccb0650 matches origin | no uncommitted code of mine | next: keep committing each step on its own branch

- 2026-10-23 10:46 PM Eastern | cx-pc #28234: open-world research +15-area renderer coverage pushed b40ff17a on cx-pc/openworld-research-28234; notes/openworld-research-28221.md.
  Primary external sources + local notes/symbols checked;23 links and3.7435 ms budget arithmetic checked. All new savings are estimates, no game work. Next pc: reuse existing50/35 and half-rate tests, then combat/route G-buffer/VFX/skinning attribution; no new implementation ownership.

2026-10-23 10:46 PM EDT | cx-pc #28242: CPU animation/physics rank complete and pushed c20573da (cx-pc/cpu-anim-28242), notes/cpu-anim-physics-rank.md | reproduced 139 Ally SP windows and 2 PC JT windows; no new runtime result | next pc: reuse existing controls/timing probes before a new optimization build.
Verdict: no demonstrated >=1 ms animation/physics frame saving; PC joins character+stage 0.424 ms/tick, historical AABB+BPO work estimate 0.62-0.85 ms/tick; pc2 render drain 5.09-6.1 ms is stronger structural target, already studied. No game touched.

2026-10-23 10:49 PM EST | gx-pc #28269 | VRS treats 0 draws: helper unwired and the game never loads the List5 interface | hook exists on draw slots 12 and 13, not built | next: do not wire VRS until a device tier query and one PSO allow-list exist | notes/vrs-followup-28258.md on gx-pc/vrs-followup-28258 76c35f1b | game not opened

- 2026-10-23 10:50 PM Eastern | cxl-pc #28298: offline web research committed/pushed 29722c8c on cxl-pc/openworld-research-28298; notes/openworld-research-28279.md maps 14 technique families to existing hooks and dispositions.
  Proven: source-backed survey, duplicate check, git diff --check. Unproven: spatial upscaler runtime/quality/gain; complete FSR family ceiling 3.59-3.65 ms <3.74 ms. Next: pc considers only this new hypothesis; existing owners retain tests. No game touched.

- 2026-10-23 10:53 PM Eastern | cx-pc #28343: half-rate cave ABI fix351ef60d on cx-pc/halfrate-abi-28343, artifact1812 SHA2566edadcd4ccfe6514a09af1344ec90a0e7e2c909972603f419ebcd2130ad5beca.
  Camera RCX + volatile SIMD/flags preserved;3000 executable cave calls + hostile helper + policy/telemetry harnesses PASS, full build PASS. Exact PC2 crash causality unproved. Next pc2: controlled startup test of1812, all OFF first; no game touched by cx-pc.
2026-10-23 10:56 pm EST | gx-pc #28247 screen-space half-rate | pushed db572882 on gx-pc/post-reuse. HRR 0x295C44 about 0.04 ms E, HRA 0x3688234 about 0.02-0.08 ms E, HRB/HRP not hooked. All keys 0. Harness passed. Game not launched.
Proven: offline skip/reuse/camera-cut policy. Unproven: frame time, a later SSR merge overwriting the reused target, and whether the 2135-byte AO apply holds the HBAO draws. Caves are the pre-351ef60d entry ABI.
Next: merge onto cx-pc/halfrate-abi-28343 before any install. Do not turn keys on. Do not replace artifact 1812.
Files: notes/post-reuse-28247.md, mod/Nioh3EngineFix/src/halfrate.cpp, commit db572882.

- 2026-10-23 10:59 PM Eastern | cx-pc #28358: matched1.7.73-drs-hrabi artifact1813, SHA256729ac5fc763a2fac2c397f69323e90622e038f62fdc757b43dde1b7257c48f77; cx-pc/halfrate-matched-28358 source96433c40.
  Full build/261 patterns/17 DRS/3000 cave calls+hostile helper PASS; existing menu IDs and6 interface/core files preserved. Next pc2: allOFF startup then existing12-window test with DRS off; native crash fix unproved. No game touched.
2026-10-23 11:02 pm EST | gx-pc #28247 ABI merge | tip b5e0eb88 on gx-pc/post-reuse includes the register-save caves. HRR 0x295C44 0.04 ms E, HRA 0x3688234 0.02-0.08 ms E. Both harnesses passed. Keys 0. Game not launched.
Proven: offline policy plus 3000 executable cave calls. Unproven: frame time and whether the SSR merge overwrites the reused target.
Next: no install until pc assigns a device test. Do not turn keys on.
Files: mod/Nioh3EngineFix/src/halfrate.cpp, commit b5e0eb88.

## 2026-10-24 00:06:40 session 086e967c-e09e-4942-a9f2-3ab9caf58ce4
Board task #127 is done on the main PC. Ultra Performance 35% gave no fps gain, and the half-rate build hangs the game at startup, so it has no numbers.

**Ultra Performance 35%** (RX 9070, 1440p XeSS, frame gen off, same Yoritomo fight, 2 repeats per arm):

| | OFF (50% scale) | ON (35%) | Change |
|---|---|---|---|
| Avg fps | 90.3 | 89.3 | −1.2% (within noise) |
| 1% low | about 59.1 | about 56.1 | −5.2% (within noise) |
| GPU active time | 7.77 and 8.16 ms | 6.94 and 7.33 ms | −0.83 ms |

- **Why no fps gain:** this PC is CPU/pacing-bound, with 7–8 ms of GPU work in an 11 ms frame.
- **Caveat:** the 35% render size was never sampled, and `config.xml` still read 50 after the ON runs. The 0.8 ms GPU drop fits it being applied, but that is unproven.
- **Power:** not recorded.

**Half-rate shadow/GI/cloud** (bridge file 1780, the HRS, HRG and HRC keys): the game hangs at startup on all 6 launches, even with every half-rate key off. The same source without half-rate boots at 88–93 fps. That points at the half-rate hooks. I sent the blocker to gx-pc (#28648) to fix and resend.

I filed the result on task #127 and the bridge (#28647), messaged you, and added a line to `.handoff/done.md`. The helper restored the install, config and saves to their pre-test hashes, and no game process or lock marker is left. Its results note (`notes/ultra35-halfrate-pc-127.md`) went to `gx-pc/post-reuse`, not the current branch.

Still pending: Ally's XLOD, fixed-scale and DRS tests. I haven't seen

2026-10-24 12:10 am EST | pc lead: big-wins tests | DONE: pc DRS (#123) no gain 83.8 vs 79.0 fps; pc2 DRS no gain (47.79 vs 47.53); pc Ultra35 (#127) no fps gain 90.3 vs 89.3 (CPU-bound, GPU -0.83ms); half-rate build file1780 hangs at boot (BLOCKED sent to gx-pc #28648). Culling owner = cx-pc (after 35% build). | UNPROVEN: Ally A1 XLOD 0 vs 4 (file 1758), A3 fixed scale 50 vs 35 (file 1762), A4 DRS (art 1765) queued behind Ally 12-run bench (last ETA 9:35 pm, result not yet seen). | NEXT: digest Ally bench RESULT + A1/A3/A4, update charts/README, await gx-pc half-rate fix then re-test on pc/pc2/ally; check pc2 #122/ally #124 board tasks. | files: notes/drs-pc-test-123.md, notes/ultra35-halfrate-pc-127.md (on gx-pc branches), captures/drs-123, captures/test127
2026-10-24 12:11 am EST | gx-pc #28648 half-rate boot hang | file 1816 is 1.7.61-hrabi from 940e1d62. Camera cave restores RCX. Keys 0. sha256 8422FC6527CBD40F809CA432171C4A836854A1FAFEEF1D862BD9130DDA0E20F8.
Proven: source of the 6/6 hang is the old camera cave resuming 0x244264 with RCX destroyed, and the new ASI contains version 1.7.61-hrabi. Unproven: a real boot. pc holds nioh3-pc and test128.
Next: when that lock is free, install file 1816 over a backup, keys at 0, and confirm a window within 60s, then restore.
Files: mod/Nioh3EngineFix/src/halfrate.cpp, notes/halfrate-abi-28343.md, bridge file 1816.

2026-10-24 00:20 | pc lead: digesting big-win results | done: DRS (pc2 -0.55%, pc no gain), Ultra35 on pc no gain (89 vs 90, CPU-bound), culling/anim/post-reuse/far-LOD all <0.3ms parked, docs refreshed (cx-pc/docs-current-27936 604608ca, not merged to main), chunk rule #28530 recorded+sent to ally | half-done: half-rate (HRG only reuses, pc2 +3.2% inconclusive); pc test128 running (marker fresh, Nioh pid 39792, not mine); Ally FG-on 0/6 valid, rerun in chunks pending, Ally Ultra35 #124 queued; cx-pc CPU submit task #28702 + cxl-pc open-world research #28298 pending RESULTs | next: wait for test128/Ally/cx-pc RESULTs, update charts when Ally done, merge docs branch to main after human OK | ids: board #124 #127 #128, files notes/halfrate-*.md
