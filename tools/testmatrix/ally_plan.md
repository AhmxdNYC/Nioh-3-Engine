# Ally X test plan (ROG Ally X, Bazzite / Proton, MangoHud + measure.sh)

Same matrix as `tools\testmatrix\matrix.json` (PC runner: `run_pc.ps1`). The Ally has no unattended runner: you (the Ally agent)
drive it step by step with the user present. Read `AGENT_BRIEF.md` first. Game-side only: no TDP / governor / env-var / gamescope changes.
PC and Ally run one after the other (Steam shared account): `py -m uo_bridge.gamelock acquire nioh3` before launching, `release` after.

## Per item (one launch per item; live items may be batched in one launch, see below)
1. Edit `Nioh3EngineFix.ini` in the game folder: start from **ALL-OFF** (every `[Patches]` flag, `[GpuOpt]` Barriers/TileBatch/R2a/R2b/R1/DropUndoPairs,
   `[Latency]` MaxFrameLatency, `[Memory]` LM, all `[Profiler]` items and `[ShaderSwap] Enabled` = 0 / `WorkerCap = 0`), then set only the item's lines
   (the `ini` column; `+needs` = dependency lines that must also be set). `matrix.json` `alloff` is the exact list.
2. Shaders: `./ally_shaders.sh status all`; make the installed set equal the item's `shaders` column (`apply <name>` / `revert <name>`). Post-v1 swaps: copy
   `gpu-shaders/post/out/<digest>.dxbc` to `<game>/Nioh3EngineFix_shaders/` and set `[ShaderSwap] Enabled = 1`; delete the file afterwards.
3. Launch from Steam, load the SAME save and the SAME spot as every other run (final protocol: `notes\final-benchmark-protocol.md`). Wait until the HUD is up.
4. Measure with MangoHud + `measure.sh`: STAND 2 x 20 s, MOVE 1 x 20 s (same camera sweep), plus COMBAT 1 x 25 s where the column says `combat`,
   plus a timed LOAD where it says `load`. Collect GPU W / MHz / busy %, fps avg, 1 % low, p99 frame time.
5. After 35 s in the level copy the mod's `stats` lines from `Nioh3EngineFix.log` (the digest script keeps `stats/s`, `GP`, `SP`, `stats lm`, `stats mem`, `stats latency`).
6. `shot` items: save one screenshot of the same view as BASE (static camera) for an image compare. Any visible difference = DROP.
7. Close the game, restore ini + shaders to ALL-OFF. Run **BASE** three times (first, middle, last): the spread is the noise threshold.

## Verdict rule (identical to the PC)
noise% = max(0.5, spread of the BASE fps runs in %). KEEP: fps > +2 x noise (or W/MHz clearly lower at equal fps for CPU-only items).
DROP: fps < -2 x noise, p99 worse by > 4 x noise, any visual difference, a crash / hang. Else NEUTRAL. Latency items (IK, MFL2): judge by the `stats latency` line.

## Live batching (one launch, mod menu F10 / triple-tap Start)
Items with `mode = live` can share one launch: install them with the `live` value (ED/WG/DS/CP/SPF/Barriers = 2 = installed but off; others = 1), switch ALL
off in the menu, measure BASE-live, then per item: switch on, measure, switch off. Relaunch for `launch` items.

## [TAG] key=value digest (bridge, one line per item, <= 15 lines per message)
```
[RESULT] item=W6 dev=ally fps=47.9 low1=41.2 p99=24.1 gpuW=11.8 gpuMHz=2210 gpubusy=99 move=47.1 combat=44.8 load_s=31.2 d_fps=+0.3 verdict=NEUTRAL note=cpu-only
[RESULT] item=BASE#1 dev=ally fps=47.7 low1=41.0 p99=24.3 gpuW=12.1 gpuMHz=2210
[BLOCKED] item=AS0 dev=ally why=gpu-hang
```
Keys: item, dev, fps, low1, p99 (ms), gpuW, gpuMHz, gpubusy, move, combat, load_s, d_fps (vs BASE median, %), verdict (KEEP|DROP|NEUTRAL|INVALID), note. Only [RESULT]/[BLOCKED]/[Q] wake the PC side.

## Step list (Ally-applicable items; wine = Proton-only, hh = handheld-only)
| # | id | type | mode | ini (+needs) / shaders | risk | extra | what to watch |
|---|---|---|---|---|---|---|---|
| 1 | BASE | baseline | launch | ALL-OFF x3 | low |  | reference; run 3x (start/middle/end) for the noise threshold |
| 2 | W1 | asi | launch | W1=1 | low |  | main-thread CPU, ~0 when GPU-bound |
| 3 | W2 | asi | launch | W2=1 | low |  | main-thread CPU |
| 4 | VRR | asi | launch | VRR=1 | low |  | game speed stays real below the cap; OFFLINE ONLY |
| 5 | W3 | asi | launch | W3=1 | low |  | one core of spin less |
| 6 | W3WineSleep | asi | launch | W3=1 W3WineSleep=1 | low | wine | fewer message-thread wakes, power |
| 7 | R1p | asi | launch | R1=1 | low |  | CPU only |
| 8 | L1W | asi | launch | L1=1 L1OnWine=1 | med | wine | pacing risk if Wine ignores the flag: compare frame-time p99 |
| 9 | WK | asi | launch | WK=1 | low |  | less CPU power; fps-neutral when GPU-bound |
| 10 | WorkerCap10 | asi | launch | WorkerCap=10 | med | hh | less wake power; check streaming hitches |
| 11 | WorkerCap8 | asi | launch | WorkerCap=8 | med | hh | as above, stronger |
| 12 | P12 | asi | launch | P12=1 | low |  | ~0 (no gain on a 7600X) |
| 13 | AS0 | asi | launch | AS0=1 | high | combat shot | effects GPU recording off the main thread on AMD; possible corruption or GPU hang. PC round L4. COMBAT window |
| 14 | W6 | asi | live | W6=1 | low |  | biggest Ally CPU-power saving; fps-neutral |
| 15 | B | asi | live | B=1 | low |  | +5 % main thread on PC |
| 16 | PP | asi | live | PP=1 | med |  | main thread; residency timing |
| 17 | FPC | asi | live | FPC=1 | med |  | render thread; check visuals |
| 18 | AABB | asi | live | AABB=1 | low |  | game-update thread |
| 19 | M1 | asi | live | M1=1 | low |  | worker CPU |
| 20 | E1 | asi | live | E1=1 | med | combat | game-update CPU; effect timing: COMBAT window |
| 21 | ED | asi | live | ED=1 | low |  | wake count down |
| 22 | WG | asi | live | WG=1 | med | load | fewer wake storms after load: measure the LOAD window |
| 23 | TG_W | asi | live | +VRR=1 +L1=1 +L1OnWine=1 TG=1 | high | wine combat | as TG |
| 24 | CP | asi | live | CP=1 | low |  | -16..27 % of the collision loop |
| 25 | SPF | asi | live | SPF=1 | low |  | worker CPU |
| 26 | IK | asi | live | IK=1 TS=1 | low |  | input latency -4..9 ms: judge by the TS latency line, not fps |
| 27 | GF | asi | live | GF=1 | low |  | no spinning core; same wake moment |
| 28 | M2 | asi | launch | M2=1 | low |  | worker CPU |
| 29 | J1 | asi | launch | J1=1 | low |  | worker CPU |
| 30 | S1 | asi | launch | S1=1 | low |  | main-thread power |
| 31 | LK | asi | launch | LK=1 | low |  | worker CPU |
| 32 | RWb | asi | launch | RWb=1 | low | load | fewer futile wakes after loads: LOAD window |
| 33 | LM | asi | launch | LM=1 MemStats=1 | med | load | fewer level-load hitches / swap on the Ally: LOAD window + MemStats |
| 34 | Barriers | asi | live | Barriers=1 | med |  | 2-3 % on a GPU-bound PC; ~0 on the Ally |
| 35 | R2a | asi | live | +Barriers=1 R2a=1 | med | wine | avoids the ALL_COMMANDS idle on RADV; screenshot compare |
| 36 | R2b | asi | live | +Barriers=1 +R2a=1 R2b=1 | high | wine shot | stale data / flicker possible: screenshot compare |
| 37 | R1g | asi | live | +Barriers=1 R1=1 | high | wine shot | not spec-exact: screenshot compare |
| 38 | DropUndo | asi | launch | +Barriers=1 DropUndoPairs=1 | med |  | small |
| 39 | TileBatch | asi | launch | TileBatch=1 TileStats=1 | low | load | CPU, streaming |
| 40 | MFL2 | asi | launch | MaxFrameLatency=2 TS=1 | low |  | maybe more fps when GPU-limited, up to +1 frame input delay (TS line) |
| 41 | RR1 | asi | launch |  | low | NOT IN ASI YET | low value (workers only) |
| 42 | S_ghdm | shader | launch | shaders: ghdm | low |  | +8.7 % at MAX on the Ally |
| 43 | S_esm | shader | launch | shaders: esm | low |  | shadow pass |
| 44 | S_wdo28 | shader | launch | shaders: wdo28 | low |  | - |
| 45 | S_mpc | shader | launch | shaders: mpc | low |  | culling |
| 46 | S_mshlod | shader | launch | shaders: mshlod | low |  | culling |
| 47 | S_toonedge2 | shader | launch | shaders: toonedge2 | low |  | 2 files |
| 48 | S_gi | shader | launch | shaders: gi | med | shot | probe GI; image compare |
| 49 | S_fog | shader | launch | shaders: fog | med | shot | volumetric fog; image compare |
| 50 | S_trvs | shader | launch | shaders: trvs | low |  | ~0 if the driver already merges the loads |
| 51 | S_pbvs | shader | launch | shaders: pbvs | low |  | ~0 |
| 52 | X_04c1 | swap | launch | swap: 04c1052d | skip |  | GP2 only: <= 0.06 ms/frame, fps cannot resolve it (gpu-post-runtime.md s7) |
| 53 | X_4e13 | swap | launch | swap: 4e13e236 | skip |  | GP2 only: <= 0.06 ms/frame, fps cannot resolve it (gpu-post-runtime.md s7) |
| 54 | X_cf79 | swap | launch | swap: cf792d56 | skip |  | GP2 only: <= 0.06 ms/frame, fps cannot resolve it (gpu-post-runtime.md s7) |
| 55 | X_e6ef | swap | launch | swap: e6efc8ff | skip |  | GP2 only: <= 0.06 ms/frame, fps cannot resolve it (gpu-post-runtime.md s7) |
| 56 | X_f11a | swap | launch | swap: f11afe97 | skip |  | GP2 only: <= 0.06 ms/frame, fps cannot resolve it (gpu-post-runtime.md s7) |
| 57 | M_GPUP | measure | launch | GPUProf=2 | low |  | per-pass ms; digest the GP2 lines |
| 58 | M_SP | measure | launch | SP=1 | low |  | hot functions; not compared |
| 59 | M_WAKE | measure | launch | WakeStats=1 TS=1 SPR=1 TileStats=1 MemStats=1 | low |  | counters only |
| 60 | BEST | combo | launch | all KEEP of the Ally verdict | med |  | must beat the best single item; else bisect |
| 61 | BEST_lowrisk | combo | launch | all KEEP of the Ally verdict | low |  | safe fallback set |

Notes: PC-only items (not listed): L1, WL, FG, X1, DS, TG (Ally uses L1W / TG_W with `L1OnWine = 1`). `GPUProf = 2` and `SP` are measurement runs (their overhead
is not compared). Run order = table order, but do the risk=high items (AS0, TG_W, R2b, R1g) last; a failing item must not stop the rest.

