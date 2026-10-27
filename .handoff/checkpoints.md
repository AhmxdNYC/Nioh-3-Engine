2026-10-17 4:46 am EDT | cx-pc2 #10870: both ASIs build; 202 moves plus idle/menu/cover suites pass. Startup unproven; next isolated atomic patch reproduction for #10995, then owner test. Integration b583133 plus menu-ID correction; notes/move-toggle-full-build-10870.md.
2026-10-17 4:48 am EDT | #10995: exact BossBattleModeEnd installer and thunk execute offline; 150 patch layouts pass, 10 refusals preserve bytes; 63 hook/core checks pass. No startup fix proven. Next owner exclusive startup with crash context; tests/atomic_patch_harness.cpp.
2026-10-17 4:51 am EDT | #11017 idle-default implemented and both ASIs built; 202 move tests, expanded idle lifecycle and merged-menu dispatch pass. CPU stance survives retry; new Training session defaults Idle after readiness. Live unproven; next owned-cleanup adapter. Files idle_hold.h, training.c, rematch.cpp, idle_hold_test.c.
2026-10-17 4:53 am EDT | #10995 correction: exact generic ASI target reproduces 0xC0000005; companion cx16 fix cherry-picked as 314f9f5. Generic-target 225+150 patch cases, 10 safe refusals, 63 hook/core and menu regression pass. Next owner exclusive startup; hashes in notes/move-toggle-full-build-10870.md.
2026-10-17 4:59 AM EDT | cx-pc2 #10870: requested integ/move-toggle-on-hp0 branch independently rebuilt; all move/menu/idle/hook/core/cover offline suites PASS. Source base 3602481 preserved.
Live unproven; next pc2 owner tests exact fresh pair. Paths/hashes in notes/move-toggle-full-build-10870.md; no game access.
2026-10-17 5:01 am EDT | cx-pc2 #11017: bounded C owned registry passes lifecycle gates + 200 recycling cycles; move/idle 202 entries pass. Native adapter/despawn-all/append unimplemented. Next native owner proves admission, late attribution, release and reader drain, then wire single-boss dispatch. Files owned_registry.h, owned_registry_test.c, notes/owned-cleanup-11017.md.

2026-10-17 5:05 AM EDT | cx-pc2 #11037: existing fix 4f7b7c7 verified on integ/move-toggle-on-hp0; moves=-29, idle=-30, sliders=-27/-28.
PC2 offline generic-x64 harness rebuilt: both timing sliders, idle and 12 editable move dispatches PASS; no game touched. Next: pc2 live validation; publish exact requested branch.
2026-10-27 3:40 pm EDT | cx-pc #40138 | shared GDI stroke helper + Training/Rematch conversion builds; 285 glyph checks,12960 count checks,1000 handle-stability draws pass. Next pc: integrate and live-check; notes/gdi-stroke-40138.md. No game touched.
2026-10-27 3:54 pm EDT | cx-pc #40170 | all3 smooth-text ASIs build; GDI 13245 cases+alpha regression and both actual GPU text/sprite cases PASS. Next pc integrate/live-check. notes/smooth-text-40170.md; no game touched.
2026-10-27 3:58 pm EDT | cx-pc #40219 | approved Xbox/PS dpad in separate history button slot; stick arrows preserved; cached4x AA; input+2GPU previews PASS, EF e4a7d0dc built. Next pc merge camera on top; notes/dpad-40219.md.

2026-10-27 4:08 pm EDT | cx-pc #40254: background HTTPS update checker + three-line hint implemented on pc/final-combined-4 d874f32b; Release EF build PASS, 64 mocked HTTP/version assertions + both menu suites PASS.
Next: final review/preview and report separate branch to pc; site/version.json prepared only, no deployment or game access.

2026-10-27 4:11 pm EDT | cx-pc #40254: update checker completed; Release EF 549f11cd, 70 offline assertions and both menu suites PASS; offline hint preview sent Discord.
Separate branch cx-pc/update-check-40254 from d874f32b; site/version.json prepared NOT deployed. Next pc integrate and publish manifest with a real release; details notes/update-check-40254.md. No game/installed files touched.

2026-10-27 4:42 pm EDT | cx-pc #40452: pushed aceefc52 on cx-pc/update-click-40452 from cxl package4c3bbe1d; version1.7.81, clickable update notice/URL + root Open update page; fixed asynchronous ShellExecute URL, controller shortcut guidance.
Both menu suites +94 checker assertions PASS (mock shell target, throttling, click bounds/focus/action); Release EF bridge2900 SHA b479d8b150f33c0d1d63fcc79c017d043aecb15852e9c4101145764fa511e0b6.
Next pc integrate/install and validate native mouse/controller/browser. No game/deployment changes. Site/version.json only prepared in this branch; cxl-pc owns shared staging.

2026-10-27 4:57 pm EDT | cx-pc #40553: hourly update poll committed/pushed fef827fd on cx-pc/update-click-40452; worker scheduling and 30-minute allocation/thread failure backoff unchanged; MOD_VERSION1.7.81.
Both menu suites +94 update-check assertions and hourly compile-time assertion PASS; Release build running. Next verify build hash and bridge artifact to pc; no game access.

2026-10-27 4:57 pm EDT | cx-pc #40553 COMPLETE: hourly poll fef827fd on cx-pc/update-click-40452, Release EF bridge2910 SHA8ddb270c300e72e6c27dd7338f95adfa2ba41e2141de9b16f2c6106e1c16e6b8, MOD_VERSION1.7.81.
Both menu suites +94 update-check assertions + hourly compile-time assertion PASS. Same background worker and failure backoff; pc installs/accepts. No game or deployment touched.

2026-10-27 5:02 pm EDT | cx-pc #40571: pushed 3ccd4a82 on cx-pc/update-click-40452; website action also selects module-folder Nioh3Updater.exe via Windows Explorer, folder fallback if missing; same worker/2s throttle/hourly poll/version1.7.81.
Release EF bridge2914 SHA28bb5750e6611df7c00288daa1d778daa58273f48f764529bfd5debf0046bbfc; both menu suites +105 checker assertions PASS (mock shell, module path, quoted spaces, missing/directory updater, truncated/failed module path, throttle).
Next pc install/native acceptance; no game/deployment touched. Offline preview sent using normal-Python notify fallback after -I media import failure.

2026-10-27 5:34 pm EDT | cx-pc #40646: short update instruction pushed 34de61b7 on cx-pc/update-notice-40646, based on later1.7.83 source3129200e. Notice: Update <ver> available: Mod Menu > Open update page; URL removed, click/site+folder behavior preserved.
Both menu suites +107 checker assertions PASS (no URL, exactly2 reminder lines, viewport fit320/800/1280, longest accepted version). Release EF bridge2926 SHAbcc09202ad709294c55f1d1acdc207496c9d536fa5f6d3abea8f0a3741b16267, MOD_VERSION1.7.83.
Next pc install/native acceptance, cxl-pc publishing; no game or deployment touched. Offline preview sent via normal notify fallback (-I missing media module).

2026-10-27 5:57 pm EDT | cx-pc #40696: Row/Ring setting, independent PLAYER/I-FR/BOSS/KI DEBT tracks and frozen-menu repaint implemented on cx-pc/ring-meter-40696.
Training Release compile + both menu suites + 5-resolution production-DIB checker PASS; preliminary draw timings collected during compilation, native appearance/performance unproven.
Next: finish EF Release build, inspect/send preview, isolate offline draw cost and optimize Ring label redraw if needed; no game touched. Files mod/Nioh3Training/src/frame_meter_{draw,ring,game}.h and menu/stat integration.

2026-10-27 6:05 pm EDT | cx-pc #40696: cached Ring stroke labels preserve exact premultiplied glyph pixels; final EF + Training Release compile and renderer/core tests PASS.
PC Ryzen 5 7600X, 1080p all4 lanes+legend/readout, CPU GDI paint/flush/alpha: OFF Row vs ON Ring means frozen65.701/2.139ms, changing67.041/10.872ms (25 samples each); native upload/FPS unproven.
Next pc install both ASIs, switch Display Overlays > Frame meter style, validate native appearance/toggles/menu-open visibility and optional Debug FrameMeterDrawCost logs. No game touched.

2026-10-27 6:06 pm EDT | cx-pc #40696: Ring feature7a442b67 + cached-font/perf proof b8665395 pushed; both menus +7 Training suites pass; EF2948 d46705ebadf5 / TR2949 3ed3f4780cb6 uploaded.
Offline production preview sent to Discord. PC1080p changing OFF Row67.041ms vs ON Ring10.872ms CPU draw; native upload/gameplay unproven. Next pc install both, test toggles/menu/position/pulses and capture in-game pictures; report notes/ring-meter-40696/report.txt.

2026-10-27 6:13 pm EDT | cx-pc #40835: replaced stair-stepped polygon edges with cached analytic pixel coverage for circular arcs/radial dividers; same four lanes and masks.
Five-resolution renderer checks incl partial-alpha coverage, glyph equivalence, Ki checks and all lane toggles PASS; Training Release build PASS; revised production preview sent to Discord.
Next: repeat offline Row/Ring CPU measurements for this AA version, upload replacement Training, report final hashes. Native game visuals remain pc-owned/unproven. Files frame_meter_ring.h, frame_meter_draw_test.c, production-preview-smooth.png.

2026-10-27 6:16 pm EDT | cx-pc #40835: smooth arcs/dividers 4b615014 pushed; Training2955 SHA68a5f928702f, pair EF2948 d46705ebadf5; revised screenshot sent Discord.
Final AA checker PASS; PC1080p changing OFF Row66.872ms vs ON Ring10.745ms CPU draw (25 samples each). Native appearance/upload/FPS remains pc test. Next pc install/check/capture; notes/ring-meter-40696/results.json + draw-cost-smooth.txt.
2026-10-27 6:37 pm EDT | cx-pc #40972: compact160/210/260px ring sizes implemented on combined430624ad; Training8fb6b814 built, all7 suites+16261412 renderer assertions and both menu suites PASS. Next finish EF build, send artifacts/preview; notes/ring-size-40972.md.
2026-10-27 7:24 pm EDT | cx-pc #41161: side legends implemented on release 2cc759b4; all 3 sizes, same 18 entries, no bottom legend. Seven suites and 16,949,251 renderer assertions pass; native preview sent media. Next pc: install/runtime check; branch cx-pc/ring-side-41161.
2026-10-27 7:25 pm EDT | cx-pc #41161 completed: side legends 10px at 1080p, all 3 sizes; 16,949,251 renderer assertions, both menus and 128 update-check assertions pass; sent media. Training bridge3007 SHA316b8776; EF bridge3009 SHA02d348e2. Code0c7a6f76 on release820f3093. Next pc: install/test; no game touched.
