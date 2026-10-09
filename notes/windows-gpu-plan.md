# Windows PC GPU plan: same-look GPU work that could add real fps on the RX 9070 (2026-10-09)

## In plain words

- **At your settings the PC waits on the graphics card, not the processor.** In the 2026-10-08 play session the GPU was busy
  90-92 % of the time and rendered 100-118 real fps (~106 median) before frame generation. CPU fixes cannot raise that.
  Only less GPU work per frame can (`session-2026-10-08-perf.md`).
- **The one GPU fix that is already in your game (the GHDM shadow shader) was the big one.** It measured +3 % without FG
  and about +11 % with FG on this PC.
- **The rest of the built, same-look GPU items are small on the PC.** All together they come to about 0.3-0.7 ms per
  rendered frame out of ~9.4 ms. A realistic outcome is **+2-4 % real fps** (≈ +2-4 fps rendered, and about the same share
  with FG on). The best case is about +7 %. Three items carry most of it: the WDO#28 shadow-mask blur, the ESM blur and the
  GI #414 rewrite. None of them has a valid PC measurement yet.
- **Most other items are worth ≤ 0.05 ms each.** A plain fps test cannot see them. The session below reads them per shader
  (GP2), so each one gets a real number.
- **One item does nothing on your PC: the FSR3 fp16 rebuild.** You run XeSS input -> OptiScaler FSR4, so the game's own
  FSR3 upscaler never runs. The FSR4 pass costs 0.81 ms and belongs to OptiScaler.
- **Not a mod item, but measured:** VSync OFF + FreeSync turns the composed flip into an independent flip. Today 10-20 % of
  the presented frames never reach the screen (session §0.4; `mod-vs-stock-2026-10-09.md` §3 has the fix). That is a
  settings choice, not GPU work, so it is not ranked here. The same note measures OptiScaler's FSR4 at +0.24 ms GPU per
  presented frame over stock.

Tags: **M** = measured, **E** = estimate (the basis is given). PC numbers come from the r5 RGP capture
(`gpu-profile.md` / `gpu-savings-round2.md`). That capture was taken at the user's own settings: 2560x1440, REN_RAT 75
(1080p render), XeSS -> FSR4, HIGH shadows / AO / GI / SSR / volumetric, MODEL_DRAW NORMAL. It ran FG off with the clocks
forced to peak, and the GPU frame was 10.64 ms. Ally ms are **not** scaled to the PC. The only measured PC/Ally ratio is
GHDM's (0.57 ms PC vs 5.3 ms Ally at MAX, ×9.3), and it does not carry over to other passes (the GI #121 rewrite was
+0.9 ms slower on RADV).

## 1. Which staged items are valid on Windows / D3D12 as they are

All shader items are **loose `shader_dx12\*.file` replacements**. The DXIL is compiled and signed with the Microsoft
`dxc` / `dxil.dll` (validator 1.5, the shipped container layout). That is vendor- and OS-neutral: the same file runs on
Windows D3D12 and on Proton / vkd3d.
- **File route (Windows):** proven on this PC by GHDM (installed, measured, `release\profiles.json` status kept).
- **psoswap route** (`[ShaderSwap]`, D3D12 device-vtable hooks keyed by the DXBC container digest): the harness passes
  on this PC's real D3D12 device (`run_psoswap_harness.py`). Only fogpar has a ready `.dxil` for it. The session uses the
  file route.
- SHA-256 table: `gpu-shaders\ally_shaders.sh` (the session parses it, so there is a single source).
- Current PC state (read 2026-10-09): GHDM = replacement; every other item = Steam original.

| # | item | Windows-valid? | target (r5 PC, user settings) | expected PC gain per rendered frame | basis | rank |
|---|---|---|---|---|---|---|
| - | **GHDM** fast depth reduction | yes, **installed** | 504 µs CS | **M: +3 % fps no FG, ~+11 % FG** | MODLOG 2026-10-06 19:35 | done |
| 1 | **wdo28** WDO#28 sun-shadow-mask blur, latency fix | yes (file) | 367 µs PS, render res | **0.15-0.25 ms (E)** ≈ +1.5-2.5 % | RGA gfx1201 + r5 segment. The 2026-10-06 bench-r9 pair was invalid (60 cap). Ally: neutral (the original variants still ran) | high |
| 2 | **gi** RefProbe #121 + #414 rewrite (one file) | yes (file) | RefProbe ~0.65-0.7 ms at GI HIGH | **-0.15 … +0.1 ms (E, wide)** | Model: #414 ×1.45-1.8 on gfx1201, #121 ×0.95-1.0 (≤ +0.1 ms slower). Ally RADV: #121 +0.9 ms slower (a different compiler) | high value, high risk |
| 3 | **esm** ESM blur, R+1 gathers | yes (file) | 297 µs (V 239 + H 57) | **0-0.13 ms (E)** | address-rate bound -> −40 %, bandwidth bound -> 0. The PC V pass is bounded by something the trace does not show | medium |
| 4 | **AS0** async slot 0 on AMD (ASI, launch) | yes (`[Patches] AS0`) | effect GPU chains on GFX, 0.32 ms at ~4 % occupancy | **0-0.3 ms (E)** | `gpu-scheduling.md` A4. Measured "neutral" only at CPU-bound settings (`hardware-compatibility.md`). **Risk high** (KT disabled it on AMD on purpose; effect lag / corruption possible) | medium, needs the shot check |
| 5 | **GIM** GIMerge A1 (ASI: skip the empty group-1 gather, widen group 2) | yes (gpuopt D3D12 hooks; the IAT hook covers `sl.interposer.dll`) | #121 / #414 group 1, ~97 % empty waves | **0.04-0.08 ms (E)** at GI HIGH (⅓ of the 0.12-0.24 ms at HIGHEST) | `gi-highest-audit.md` §6. Self-qualifies (60 clean pairs) and logs merges | low-medium |
| 6 | **mpc** + **mshlod** culling chains | yes (file) | MPC#23 51.8 µs, #28 20.4, MshLod#3 42.6 | **≈ 0.055-0.06 ms both (E)** | `gpu-culling-rewrite.md` §4 (r5 segment times, 2.2 µs dispatch floor) | low |
| 7 | **wdo14** WDO#14 VSM page-flag dedupe | yes (file) | 120 µs | **0-0.04 ms (E)** | gfx1201 VGPR 40->44, still 16 waves, +10 % instructions, loads skipped at run time | low |
| 8 | **fogpar** VFSCAT#235 early-exit (bit-exact) | yes (file, and psoswap .dxil) | 293 µs **async**, 100 % overlapped with GFX | **0-0.03 ms (E)** | **Ally M: −0.18 ms** (USER S1 ABBA, shotdiff within noise). On the PC the fog runs async and is hidden under graphics, so the frame-time gain is close to 0 | low (ships anyway: bit-exact) |
| 9 | toonedge2 (optional, not in the default set) | yes (file) | 45 µs | 0.02-0.03 ms (E) | Ally RADV ≥ +0.14 ms slower. The Windows compiler differs | optional |
| 10 | trvs / pbvs (optional) | yes (file) | PB2 VS | ~0 (E) | the driver probably merges the repeated loads already | optional |
| - | **FSR3 R1 fp16** | **not built; no effect on this PC** | the game's FSR3 ACCUMULATE does not run with XeSS -> OptiScaler FSR4 | 0 | `gpu-profile.md` (FSR4 0.81 ms = OptiScaler) | drop for PC |
| - | PB2 M1 meshlet-VS load scalarization | not built (psim needs wave ops) | G-buffer geometry ~1.5 ms | 0.1-0.4 ms (E, speculative) | Ally finding (75 % VS). The PC split is unmeasured | next build if Phase A shows PB2 VS on top |
| - | "VSCAT" (= VFSCAT): F2 wave32 metadata build | not built | async fog | 0 ± (E) | async on the PC | skip on PC |
| - | barrier work R2a/R1 | ASI | 1.16 ms barrier chains | **M: −7 % on PC** | `release\profiles.json` | rejected |

**What is not on this list and why.**
- The ~1 ms/frame device-wide GPU stall with work already queued (`gpu-scheduling.md` B2) is the largest unexplained
  block, but its cause is unproven.
- The 0.99 ms of GFX idle bubbles is mostly not submission timing (B3: ≤ 0.1 ms).
- Copies and exe post passes are necessary work (`gpu-copies-postres.md`).

## 2. The unattended PC GPU session (`tools\testmatrix\pc_gpu_session.ps1`)

Run it **after run8** (run8 holds the game lock and the measuring lock). `-WaitRun8` waits until Nioh3, every harness
and any live measuring lock are gone:
```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\pc_gpu_session.ps1 -WaitRun8
```

**Phase A: per-PSO GPU budget (GP2) at the user's real settings, FG on as the user plays.**
- Config: the session's `config.xml` (`captures\playsession\20261008-2059-session\config.xml`).
- Route: `run_tour.ps1 -Input vpad`, list `main` (15 main missions, every area group), 3 points × 12 s.
- Profiler: `GPUProf=2`, Top 100, 1 frame in 8, MinRun 4, SP off.
- Digest: `ally_budget.py gp2 --min-draws 1000` + `gp1` -> `<session>\A_budget.txt`. That is the PC GPU budget table,
  the PC twin of `ally-gpu-budget.md` §2.
- About 22 min.

**Phase B: launch A/B of every Windows-valid item.**
- Config: the same, with **FRAME_GENERATION OFF** (FG pacing flaps made earlier PC A/Bs INVALID) and `[VRR] MaxFps 240`,
  so the 120 cap never binds. `-KeepFG` keeps the user's FG instead.
- Default items, ranked: `wdo28, esm, gi, AS0, fogpar, wdo14, GIM, mpc, mshlod`.
- Launch order: `BASE`, then for each pair `x y BASE y x BASE`. Every item gets 2 launches, each bracketed by BASE, and
  the report interpolates BASE for drift.
- Each launch:
  - VirtualPad title -> Continue to the save's fixed spawn, with no camera input;
  - 45 s settle;
  - 3 × 20 s windows behind the quiet guard (noisy windows are dropped);
  - PresentMon `GPUBusy` per frame (the primary metric) + fps;
  - the GP2 per-PSO row of the target shader (proves the replacement ran: a new PSO hash; ASI items need their log line);
  - one screenshot at the spawn view. It is taken only when the PC has been idle for 120 s; the game takes the
    foreground for ~5 s, then it is handed back.
- `pc_gpu_report.py` -> `<session>\B_report.txt`:
  - **KEEP** = GPU ms down beyond the BASE noise floor in both launches, shotdiff within the BASE-vs-BASE noise, and the
    item ran;
  - **DROP** = slower, or a different image;
  - **NEUTRAL** = inside the noise (the GP2 target-row delta is still printed);
  - **INVALID** = the item did not run.
- 29 launches × ~5.5 min ≈ **160 min**. A quick variant: `-Items wdo28,esm,gi,AS0` = 13 launches ≈ 72 min.

**Safety.**
- `harness_guard` (owner pid, deadline, commit headroom, max 3 crashes, pid files) + the measuring lock + the bridge
  game lock.
- Backups of `config.xml`, the mod ini / ASI, the save folder (SHA-256 manifest) and every shader file the session
  touches. All of them are restored and verified at the end, and also on a guard abort.
- `-DryRun` (temp folders, fake shader table, `run_tour -DryRun`, fake launches through the real report) ends with
  **DryRun OK** (2026-10-09). It also checks read-only that the real replacement files match the table.

**Total: ~3 h** (A ≈ 22 min + B ≈ 160 min). The script prints its own estimate.

## 3. After the session (decision rules)

1. Install every KEEP item for the user (`gpu-shaders\apply_*.ps1`, the file route) and add it to `release\profiles.json`
   with the PC numbers.
   - fogpar ships on the Ally evidence already (bit-exact). A PC NEUTRAL does not block it.
   - AS0 only ships if KEEP **and** a combat check shows no effect lag. Its shot is taken at a calm spawn.
2. Rank the next build from `A_budget.txt`, the per-PSO PC table:
   - If PB2 G-buffer VS rows lead (as on the Ally), the next build is M1 load scalarization.
   - If RefProbe leads and gi is DROP, the next build is a #414-only file. `gi` today bundles #121, which is neutral to
     slower on gfx1201.
3. Re-run Phase B with `-KeepFG` for the final KEEP set only. That confirms the gain with the user's FG pacing.

Caveat: "new PSO hash seen" is a heuristic against the Top-100 cut. A row that falls under the cut in BASE can look new.
The target-row delta and the GPU ms decide; the hash check only catches a replacement that was never picked up.
