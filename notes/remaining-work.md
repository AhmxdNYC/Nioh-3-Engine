# Remaining work to call the project done (2026-10-08, ASI 1.7.30 in src)

**Done =** (1) every built fix has a per-device KEEP/DROP verdict from the re-test matrix, (2) the KEEP set is the default
ini of a per-device release zip (`release\`), (3) the final STOCK vs ALL-KEPT benchmark on PC and Ally is in the README,
(4) no open crash/leak risk caused by the mod, (5) user docs match the shipped zips. Not required for done: the standing
decompile goal (keep 1 RE agent while budget allows) and the boss-rematch feature (separate track, needs the user).

Columns: owner/status | effort (agent-hours, game-hours) | needs game. "free" = nobody owns it.

## 1. In flight (owned - don't touch)
| # | item | owner / status | effort | game |
|---|---|---|---|---|
| 1.1 | PC ABBA re-test B / PP / GF / MQ (+ OFF sentinels) and PC load EP A/B (`pc-keep-candidates.md` s4) | PC test agent; Block A aborted 10:29 (user active) | 2 h game | y |
| 1.2 | ~~Ally session R2: SR (1.7.25) ABBA~~ DONE: SR=1 47.25 -> 38.45 s, 0 failures; SR = -1 auto (on under Proton) is the default since 1.7.28. Still open: SR+RC vs SR (low value: 740392 is 94 % < 8 us now), LoadOnlyCpuFixes pair | Ally agent | 1 h game | y |
| 1.3 | Next Ally load fix: ReadFileEx 631FD5 (9-10 s ResourceLoad) -> ASI >= 1.7.26 (src has PF prefetch in progress) | read-fix agent (owns src) | 4-6 h | n (build), y (test) |
| 1.4 | PC-vs-Ally load analysis (`pc-vs-ally-load.md`) | load-analysis agent | - | n |

## 2. CPU / PC frame time - built or designed, not decided
| # | item | owner / status | effort | game |
|---|---|---|---|---|
| 2.1 | Barriers=0 in the PC default (measured -7 %) | DONE in `release\profiles.json` (PC) | - | n |
| 2.2 | gpuopt trace B=0 vs B=1 (`submit_redundancy.py`) + screenshot pair: B draw-loss check before B can be KEEP | free; after 1.1 | 1 h game | y |
| 2.3 | SF1+PM PSO skip/key memo, SF3 OM/RS skip - build only if 2.2 shows >= ~200 redundant calls/frame | free (queue 3, opus) | 4-8 h | y (trace first) |
| 2.4 | FPC material FindParam cache (1.1-1.6 % main), PT1, DL, AABB, M1/M2, J1, E1, S1, X1 - built default off, need PC A/B | matrix (final re-test) | in matrix | y |
| 2.5 | K1 frame-end drain (CmdFlushLimit), D retry quantum 1 -> 0.2 ms, P1E7 texture-loop prefetch, mainthread-drawprep C (glyph atlas, needs live read) | free; designed / live script only | 3-6 h each | y |
| 2.6 | PSO first-use stutter: SPR count (built) -> PL (create outside lock) / PJ (journal + prewarm) | PJ BUILT 1.7.31 (`[Patches] PJ = 0`, offline ALL PJ CHECKS OK); PL dropped (no frame gain, `stutter.md` 3.1). STILL NEEDED: PC + Ally A/B per `stutter.md` 6.1 (PJ off vs on, two launches each) | 1 h game/device | y |
| 2.7 | Stutter classes not measured on PC: traversal, new-effect combat, post-load (`stutter.md`) | free; add as windows in the final run | 1 h game | y |
| 2.8 | AS0 / AS-N async slots (AMD), TG tick governor (behaviour change, opt-in only) | matrix (dropped-but-kept) | in matrix | y |

## 3. GPU side (Ally is GPU-bound: only these move Ally fps)
Measured Ally budget: `ally-gpu-budget.md` (2026-10-08). USER preset (LIGHTEST, REN_RAT 50) = ~31 ms GPU, FSR3 3.7 ms; MAX native = ~61 ms, FSR3 7.0 ms.
Old Ally ms from `gp2_L*_full` were menu-diluted (26 of 38 windows = title menu) and the 175 logs are REN_RAT 100, not 540p. M = Ally-measured, E = estimate.
Top 3 measured targets: FSR3 accumulate 2.1 ms USER (fp16 rebuild 0.2-0.5 E, user decision), culling chain 3.5-4.5 ms USER (0.1-0.5 E), VSM shadows 2.6-3.8 ms USER (3.6 census). ~22 ms of the USER frame has no per-PSO data yet -> GP2 Top=100 session (`ally-gpu-budget.md` s7) first.

| # | item | owner / status | effort | game |
|---|---|---|---|---|
| 3.1 | Per-shader Ally A/B of the staged pack. M: gi #121 +1.5 ms, fog +0.5, toonedge2 >= +0.13 SLOWER (drop confirmed); WDO#28 unresolved (orig still ran). ESM_fast (0-0.9) and MPC#23/mshlod (0.11-0.24) values are PC-scaled E, every target PSO < 0.42 ms at MAX; post-v1 closed by 3.3. WDO#14 (`wdo14`, S_wdo14) staged 2026-10-08, not yet run. RULE: none ships without an Ally win | matrix / Ally sessions; GP2 Top=100 run first | 2-3 h game | y |
| 3.2 | GI A0/A1 light-group merge, ~0.25-0.5 ms Ally at MAX, ~0.03 ms at USER (E). **BUILT 1.7.29** (`gimerge.cpp`): A0 probe `[Debug] GIProbe`, A1 merge `[Patches] GIMerge` (2 = observe, 1 = merge after qualification), both default 0; offline harness green; not run in game. Next: Ally A/B protocol `gi-highest-audit.md` s6 (A0 session first) | Ally session | 10 min A0 + 1 h A/B | y |
| 3.3 | ~~Exe-embedded post chain via psoswap (MB/DOF/bloom/TAA, 8-10 ms Ally)~~ CLOSED 2026-10-08: the 8-10 ms was a PC x 6 estimate. Gameplay-only recheck: compose 6a305be4 0.46 ms/frame (not 0.17; menu-diluted), DOF/MB/CA rare; exe blits >= 3.2 ms (not 1.44), straight-line. Verdict unchanged; post-v1 not shipped (`gpu-post-runtime.md` s7-8) | DONE | - | n |
| 3.4 | RADV prescreen tool for VCLOUD hoist + SFB dedupe + FSR3 rebuilds (R1 fp16 / R2 wave size, `ally-gpu-budget.md` s5). M targets: VCLOUD CS 1.50 ms async, PB2 opaque 2.91 ms at MAX (4x the old figures); both rewrites fail the register rule | free; blocked on an Ally-side tool | 4 h | y |
| 3.5 | ~~WDO#14 VSM page-flag dedupe~~ DONE 2026-10-08: built at DXIL level (no HLSL port needed), bit-exact (psim), staged OFF as `wdo14` (`gpu-wdo14-flag-dedupe.md`); A/B in 3.1. M: shader 1.08 ms at MAX; win 0.2-0.4 ms MAX, ~0.05-0.1 ms USER (E); RGA gfx1103 VGPR 40 -> 48 | done | - | n (build), y (A/B) |
| 3.6 | TileStats=2 / TileBatch=2 census (tilestats.cpp built, not wired). Targets a measured block: VirtualShadowmap + Batch Tiles 8 = 2.6-3.8 ms USER, 4.5 ms MAX | free; integration after 1.3 | 2 h + 1 h game | y |
| 3.7 | ESM 2048->1024 - changes the look: only as an opt-in with user visual sign-off. "1.5 ms Ally" was PC 0.2 ms x 7.5 (E); no ESM PSO reaches 0.42 ms at MAX, likely over-stated | free; user decision; measure first | 2 h | y |
| 3.8 | FSR3 upscaler (user setting): 3.7 ms USER / 7.0 ms MAX (M); output-res part ~2.6 ms is fixed. Upscaler-mode arms (XeSS, OFF, sharpness 0, FG, REN_RAT 67/100) not measured -> protocol `ally-gpu-budget.md` s7. Only lever > 0.2 ms: R1 fp16 rebuild (near-look, ask the user) | free; Ally session + user decision | 1 h game | y |

## 4. Loading
| # | item | owner / status | effort | game |
|---|---|---|---|---|
| 4.1 | Ally load after SR (38.5 s; `loading-ally-fixed.md` 8.9): G8 ~15.5 s = manager A draining ~1000 queued requests at in-flight cap 10 while pools / disk / half the CPU idle; G5+G9 ~7 s = its second batch; pre-gate ~14 s (CPU-saturated at +8..+13). Fix built: `LC = N` (cap 11..64, 1.7.30, default off), expected 3-8 s (up to -14 s at 32), 0 if TrySync-bound (then + `TSY = 6`). Next: Ally R4 ABBA LC=32 vs base (protocol 8.9); after it, the pre-gate CPU block (SP cannot see the unnamed TrySync threads: add them before profiling it) | Ally agent (R4); free after | 1-2 h game | y |
| 4.2 | Add SR/RC/AQ/TSY/SH/EP/PF + LoadOnlyCpuFixes to `tools\testmatrix\matrix.json` (queue 1b) | free (sonnet); after the harness owner releases it | 1 h | n |
| 4.3 | AutoContinueLoad on death-respawn and fast-travel routes (only title->Continue verified) | free; one check in any session | 15 min game | y |
| 4.4 | LM load-memory sweeper (3-6 s when a swap burst occurs) - built default off | matrix | in matrix | y |

## 5. Input latency
| # | item | owner / status | effort | game |
|---|---|---|---|---|
| 5.1 | Latency in 3 configs (STOCK / OPTIMIZED / +FG): PC PresentMon 2.6 v2 metrics (`final_pc.ps1`), Ally 240 fps camera | final phase; tooling done | in final run | y |
| 5.2 | IK on a frame-capped PC (only tested GPU-bound on the Ally), MaxFrameLatency 1 vs 2 on the Ally | matrix | in matrix | y |
| 5.3 | LM/LS logic-frame pacing (3-7 ms) - design only, needs TS data | free; after 5.1 | 6 h | y |

## 6. Stability / crash / leak risks
| # | item | owner / status | effort | game |
|---|---|---|---|---|
| 6.1 | Crash Nioh3+2526D1 (1 in ~60 launches, panel applying fixes during load, SSS off) | OFFLINE DONE 2026-10-08 (`crash-2526D1.md`): first cmd-stream token of a frame (dev+3C40 write ptr bad, i.e. stream base unallocated/freed = device lifecycle); no mod code/data on the path, ASI writes code only at ~1 s before any frame -> no guard. Only link = legacy panel whole-process freezes during load (timing). Repro: launch with `--enable_output_dump`, RTSS closed; 30 relaunches each, SSS ON/OFF alternating + 20 s title-menu scroll: A stock, B ASI release profile, C legacy panel apply, (D stock+RTSS). Hit in A/D = game bug; only in C = retire panel live apply. Per dump: rax, thread name, dev+3C30/3C38 | 2 h game (PC) | y |
| 6.2 | Long-session stability: 2 h+ per device with the release profile (mod log + exit code, MemStats=1: handles, VRAM growth across fast travels) | final phase (protocol item 6) | 2 h game/device | y |
| 6.3 | Hook register safety: BossLog thunk xmm0-5 fix and readfix/prefetch hooks never run in game | covered by 6.2 + Phase 0 | - | y |
| 6.4 | FG on Proton crash: default stays status-only (FrameGenToggle=1) - keep; re-verify the fgmarker restore once | Ally, 10 min | 10 min | y |
| 6.5 | VRR online default: DONE in 1.7.27 (`[Patches] VRROnline = -1` auto-suspends VRR while online-session-detect fires, restores 5 s after, logged "VRR:"). STILL NEEDED in game: co-op session test - confirm log shows "online session detected" on joining/hosting and "back offline" after, game speed stock while online, no false positive in solo hub/missions; if no detection, set VRROnline = 0 for co-op | free; detection exists (`online-session-detect.md`) | 3 h + co-op test | y |
| 6.6 | Base ini defects: 2 control chars (lines 395, 527: `\v`, `\b` from unescaped paths), duplicated [Memory] block, 6 keys undocumented (InstallDelaySec, PF*) | DONE 1.7.26 (control chars fixed, second [Memory] block removed, InstallDelaySec + PF* documented; check_release 0 ini warnings) | - | n |

## 7. Final defaults per device
| # | item | owner / status | effort | game |
|---|---|---|---|---|
| 7.1 | Per-device ini profiles PC / ALLY with status per key (kept / provisional / off) | DONE: `release\profiles.json` -> `release\profiles\*.ini` (`make_profiles.py`) | - | n |
| 7.2 | Flip provisional keys from the matrix verdicts (PC: W6 GF B PP MQ; Ally: MQ, W3_TimeoutMs; + SR/EP/LO/PF after 1.2/1.3) | after 1.1 / 1.2 / final matrix | 30 min | n (edit) |
| 7.3 | Recommended graphics preset per device (bang-for-buck sweep, `tm.py table`) | final phase | in final run | y |

## 8. Final re-test matrix + benchmark
| # | item | owner / status | effort | game |
|---|---|---|---|---|
| 8.1 | Re-test every flag + shader one at a time vs all-off, both devices (DROPPED-BUT-KEPT list in STATUS) | final phase; harness `tools\testmatrix` ready (PC); Ally `ally_final\` | 4-6 h game/device | y |
| 8.2 | Final benchmark STOCK vs ALL-KEPT, MAX + user preset, 3 runs interleaved, LOAD1/LOAD2, hitches, temps/W | protocol `final-benchmark-protocol.md`; ASK the user to schedule | 3 h game/device | y |
| 8.3 | Results -> `notes\final-benchmark-results.md`, README tables, Discord (webhook pasted in-session) | after 8.2 | 2 h | n |

## 9. Release packaging
| # | item | owner / status | effort | game |
|---|---|---|---|---|
| 9.1 | Packaging script: per-device zips (loader, asi, profile ini, guide, option reference, optional GHDM shader with hash check, PROFILE.txt, SHA256SUMS) | DONE: `release\package.py` (`-rc` while provisional; `--final` refuses) | - | n |
| 9.2 | Regression test before any build/release: ini lint, src<->ini keys, profiles fresh, bin version = src, `--full` = test_patterns + 9 harnesses | DONE: `release\check_release.py` | - | n |
| 9.3 | Final build from the latest main after 1.3 merges, `package.py --final --full`, smoke launch per device | after 7.2 | 1 h + 15 min game/device | y (smoke) |
| 9.4 | Retire `mod\ally-package` (hand-built, stale README) in favour of the zips | after 9.3 | 15 min | n |

## 10. Docs
| # | item | owner / status | effort | game |
|---|---|---|---|---|
| 10.1 | End-user guide (install PC/Proton, settings, menu, shader, problems, uninstall) | DONE: `release\USER_GUIDE.md` (ships as README.md in the zips) | - | n |
| 10.2 | README: device sections from verdicts, final tables, charts refresh; trim WIP wording | after 8.3 (queue 5) | 2 h | n |
| 10.3 | `bin\README.md` (option reference) - sync with 1.7.2x keys (EP, SR, RC, AQ, TSY, SH, PF, LoadOnlyCpuFixes) | free (sonnet), after 1.3 | 1 h | n |
| 10.4 | settings-cost.md measured columns (queue 5) | after 8.1 | 1 h | n |

## Order to finish
1.1-1.4 (running) -> 4.2, 6.6, 10.3 (no game, small) -> 2.2 + 6.1 + 4.3 on the next PC/Ally turns -> 3.1 Ally shader A/Bs ->
8.1 matrix -> 7.2 flip -> 9.3 final build -> 8.2 benchmark with the user -> 6.2 long sessions -> 8.3 / 10.2.
Larger optional levers if budget remains: first the Ally GP2 Top=100 session at the USER preset (`ally-gpu-budget.md` s7, fills the ~22 ms unattributed), then 3.6 census, 3.8 R1 (user decision), 2.3 submit filters; 3.2 GI merge is now low value (~0.25-0.5 ms at MAX only, E).
