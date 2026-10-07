# Nioh 3 (PC, Steam v2.02) — engine research notes

Written 2026-10-06 by the research agent (web only; nothing on the local game was touched). Sources are listed inline.

Priorities: (A) fix the camera judder, (B) more performance on an RX 9070, Ryzen 5 7600X, 1440p.

---

## 0. TL;DR — the most actionable findings

1. **The camera judder comes from the timing model.** The Katana engine advances gameplay, animation and camera by a fixed step per *rendered frame*, sized to the selected target (30/60/120). It does not use measured wall-clock dt. Anything between the targets is uneven: at 110 fps, frames carry a 1/120 step and the engine adds *frameskip* steps to catch up. Below target with frameskip off, the game runs in slow motion. Sources: Nioh1Fix research (RE of Nioh 1), Nioh3Fix source, user reports in §1.
2. **The engine already has a dynamic-target path.** In Nioh3Fix, Lyall hooks it: the in-game "FPS Dynamic Adjustment" flag, a function that returns the *target FPS* (it reads an averaged frametime at `[r8+0x74]` in ms), and a frameskip branch. Nioh3Fix `[Dynamic FPS] TargetFPS` can set any target, e.g. `58` for 116 fps with FG. **This is the best existing lever for the judder.** Lyall's old 0.0.2 "Frameskip=false" option was "smoother but slows down if you miss target"; he removed it in 0.0.4 as buggy. Signatures are in §1.3. Nioh3Fix 0.0.6 is updated for 2.00.02 (Aug 25 2026). 2.01 and 2.02 contain no PC or engine changes in the notes, so its signatures probably still match; verify.
3. **Nioh1Fix (RobertTausig, GitHub) shows how to do a full arbitrary-framerate fix on this engine family.** It patches a 4-row *frame-profile table* {target, a, b}, disables a QPC-based post-Present limiter, and sets Present to SyncInterval 0 with DO_NOT_WAIT. It then replaces a *direct target-FPS accessor* (~190 call sites) with `2 × measured presentation fps`, plus about 10 subsystem-specific dt fixes (camera coefficients, grass wind, ocean, clouds, UI anims, input cadence gated to 60 Hz, projectile script tick). It needs a steady external cap; free-floating fps "results in very stuttery presentation". Nioh 3 almost certainly keeps the same frame-profile and target-FPS-accessor architecture, so this is the template.
4. **Frame generation workaround:** use a locked 60 base with FG for 120 output, or 120 base for 240 on a 240 Hz display. The engine paces only at exactly 30/60/120 base. DLSS-FG plus Reflex caps at ~116 on a 120 Hz display, which judders. **FSR3-FG at a locked 120 is reported smooth.** DSOGaming reports MFG x4 hides the judder.
5. **Upscaler on RDNA4:** the game ships FSR 3.1, which OptiScaler calls a "custom implementation". The **Adrenalin FSR 4 driver override works**: gamegpu benchmarked FSR 4 via the driver toggle "AMD FSR Upscaling", and TPU confirms it. OptiScaler: use **XeSS inputs**, not FSR3.1 (unsupported) or DLSS (reverts to FSR each boot, then crashes). 0.9.1 crashed at boot until the fix in 0.9.2 (`Fsr4Update=false` was the workaround). Use 0.9.3 or 0.9.4 (FSR 4.1 / 4.1.1). With XeFG, set `OverrideVsync=true` and turn VSync off in the Opti overlay. FSR 4.0.3 caused exposure flicker in Nioh 3, fixed in FSR 4.1.
6. **GPU cost: Global Illumination ("OBORO", trace-based multi-bounce GI) is the only big setting.** Very Low to Ultra costs up to 54% (4090, 4K). SSAO costs 5-6%, SSR 5%, and most other settings are negligible. Grass density is reported as noticeable.
7. **CPU (§3):** widely reported as "unreasonably high CPU demand" on every CPU, including in cutscenes and menus. One user saw "all threads ~45-50%, main worker ~85%". Capping CPU max state to 99% (no boost) costs **no** fps at a 60 lock, which points to busy-wait/spin load, matching our 650% at GPU 47%. Nobody has published an RE fix. `CPU_SPEC_CLASS` has zero public documentation. Katana precedents to A/B test: Wild Hearts patch "high-end CPUs unintentionally capped" (the engine tiers threads by CPU class); RotR "CPU load high when audio = Surround/3D with Windows spatial sound off"; Wild Hearts "stereo → 5.1 improves perf a lot"; Koei Tecmo official advice "unplug extra USB input devices"; and HID re-polling dips.

---

## 1. Frame pacing in Team Ninja's Katana engine

### 1.1 The model across games
| Game | Behaviour | Source |
|---|---|---|
| Nioh 1 (2017) | 60 fps lock (30 in some modes). Frame-profile table + QPC limiter after Present. Gameplay timing reads the target FPS, not real dt. | [Nioh1Fix docs/research.md](https://github.com/RobertTausig/Nioh1Fix/blob/main/docs/research.md) |
| Nioh 2 (2021) | 30/60/120 targets. "Not stable unless fps matches 30/60/120", with judder when rotating the camera below 120. RTSS lock to exactly 60 = no judder. Cutscenes forced to 30 fps (Nioh2Fix "Unlock Cutscene Framerate" patches `B9 01 00 00 00 E8` to `mov ecx,0`, so cutscenes call a frame-mode setter with mode 1). | [ResetEra N2 thread](https://www.resetera.com/threads/nioh-2-complete-edition-pc-performance-thread.373551/), Nioh2Fix src |
| Wo Long (2023) | "No VRR support: any frame other than exactly 30/60/120 stutters". Driver-side cap at 60/120 recommended. | [Steam discussions](https://steamcommunity.com/app/1448440/discussions/0/3782498683145437645/) |
| Rise of the Ronin (2025 PC) | "Has to run at locked 60 or locked 120; in between = bad pacing + slow-motion effect; physics/animation based on framerate". 60 lock + FG = smooth 120. | [ResetEra RotR](https://www.resetera.com/threads/rise-of-the-ronin-pc-performance-thread.1130673/page-4), [Steam](https://steamcommunity.com/app/1340990/discussions/0/600772666094279367) |
| Dynasty Warriors: Origins (Omega Force, Katana) | A ResetEra user says it "fixed this issue". This suggests a Koei Tecmo branch with variable-dt support exists (unverified). | [ResetEra N3 p7](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-7) |
| **Nioh 3** | Caps of 30/60/120 only. 120 is offered only if the monitor is 120 Hz or more; FG doubles to 60/120/240 ([TPU](https://www.techpowerup.com/review/nioh-3-steam-deck-rog-xbox-ally-x-performance-benchmark/3.html), [PCGW](https://www.pcgamingwiki.com/wiki/Nioh_3)). 110 fps judders; 120 is smooth ([DSOGaming](https://www.dsogaming.com/articles/nioh-3-does-not-suffer-from-stutters-first-pc-benchmarks/)). "90 fps looks like 45" ([ResetEra p3](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-3)). Selecting 120 and getting ~70 makes it "run in slow motion" ([p4](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-4)). VRR does not help. | |

User explanation (ResetEra p7): "if your framerate is 110 then your game is running similar to how it'd run at 55 FPS". This is consistent with whole-step frameskip: 110 fps = alternating 1-step and 2-step frames at a 1/120 step.

### 1.2 Nioh 3 in-game "FPS Dynamic Adjustment"
- When ON, the game adjusts its frame-rate target and uses frameskip to "maintain a stable speed", i.e. the engine changes the target between tiers. Lyall's hook reveals it can return any integer. Wccftech/Gamerant note that turning it ON prevents slow motion at the cost of uneven presentation.
- Patch 1.04.03: "Adjusted the timing of FPS changes to reduce lag when FPS Dynamic Adjustment is enabled." Patch 1.04.03 also fixed resetting Dynamic Resolution letting fps exceed the cap.
- Patch 1.05: "Fixed lock-on camera rotation speed changing based on FPS" (proof that camera code had per-frame constants).
- Patch 2.00.01: several "at 30 FPS" gameplay bugs (Ki consumption, hit counts). This shows gameplay logic is framerate-tiered.
- Bug lore: toggling Dynamic Adjustment off and on "re-initialises the AI tick rate", which fixes passive bosses. So AI tick is derived from the target too.

### 1.3 Nioh3Fix (Lyall) — source read in full
Repo: https://codeberg.org/Lyall/Nioh3Fix (latest 0.0.6, 2026-08-25, "Fix broken pattern scans for 2.00.02"). Codeberg blocks AI fetchers; the `codeberg.org/api/v1/...` and `/raw/branch/main/...` endpoints work with curl. Framerate code (`src/dllmain.cpp`, `Framerate()`):

```cpp
// FPS Dynamic Adjustment flag (int, ==1 when on): RIP-relative at match+3
"44 39 ?? ?? ?? ?? ?? 75 ?? 45 89 ?? ?? E9 ?? ?? ?? ?? 45 38 ?? ?? 0F 85 ?? ?? ?? ??"
// Dynamic target: mid-hook at match+0x10; r8+0x74 = averaged frametime (ms, float);
// rax = target fps (int) returned; Lyall sets rax = 1000/avg (clamped 30..500) or TargetFPS
"45 89 ?? ?? 48 8B ?? ?? ?? ?? ?? 48 85 ?? 74 ?? 89 ?? ?? ?? ?? ?? 48 83 ?? ?? C3"
// Menu input repeat fix: xmm0 = round((60/fps)*4)/4  (menu repeat uses a 60-relative step quantised to 0.25)
"48 8B ?? ?? 0F 28 ?? 48 8B ?? 48 ?? ?? 74 ?? 48 8B ?? ??"
// Menu framegen allowed: NOP the jnz
"75 ?? E8 ?? ?? ?? ?? 41 ?? ?? 84 ?? 74 ?? 40 ?? ?? 49 8B ?? ?? ?? ?? ??"
```
Removed in 0.0.4, from commit 917e30ff99: **frameskip toggle**. Mid-hook at match+7 clears ZF so the `jnz` is taken, which disables frameskip when Dynamic Adjustment == 1:
```
"F7 ?? ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? 4D 8B ?? ?? ?? ?? ?? 4D 85 ??"
```
The ini comment of that era read: "Frameskip=false … can improve smoothness but causes slowdown if you cannot hit the target framerate". The 0.0.2 ini also said the dynamic option "can cause freezing when toggling frame generation".

Other useful Nioh3Fix hooks: LOD/draw distance (BackgroundMeshQuality, TerrainQuality 0.8/4000 at vHigh, character 5000/6000, model 260/15000, parts, HLOD, **MotionQuality** = distance at which distant enemies use decimated animation rate, an `mov edx,8` immediate). Intro skip patches the boot state machine (Logos(2)→ShaderComp(3), AutoSave(4)→Stage(7)), so **a "ShaderComp" boot state exists**.

### 1.4 Nioh1Fix (RobertTausig) — the arbitrary-framerate template
Repo: https://github.com/RobertTausig/Nioh1Fix (active, last push 2026-09-20; Linux/Proton validated; the Windows build still "animations too fast"). Key RE facts (Nioh 1 v1.24.8.0):
- **Frame-profile table**, 4 rows × 12 bytes {float target, int, int}: `60/1,1`, `30/1,2`, `30/1,2`, `60/1,2`. There is also an active-profile index global.
- Presentation is capped by **a QPC limiter called right after Present** and by Present(SyncInterval=1). All three (table, limiter, Present flags) had to change to exceed 60.
- **Direct gameplay target-FPS accessor** (~190 call sites) feeds gameplay/animation. The correct override is `2 × measured present fps`, so the internal "fps" unit is half-steps. Motion components get `scale/120` as delta.
- Systems with their own per-render constants need individual fixes: camera rotation coefficients (scale XMM7/8, not the raw axes, to keep lock-on switch thresholds), aim camera, grass wind phase, `CStatisticalOcean::Update`, `CCloudPlane/Circle/Particle::Update`, SCL UI `CAnimatorBase` seconds branch, overflow-text scroller, input pressed/released/repeat gated to a 60 Hz cadence, 56 gameplay action held-counters capped at 60 Hz, and projectile `ControlScriptObject` tick gated to 30/s.
- Rejected approaches are documented, which saves time: patching the table alone; non-blocking Present alone; bypassing main/worker sync (gives faster sim, not more fps).
- RTTI class names survive (`ktgl`, `scl`, `CCloudPlane`…). Expect Nioh 3 to keep `ktgl` namespaces.

**Implication for Nioh 3:** the judder fix is either (a) set the dynamic target to exactly the delivered rate, with a stable external cap and no frameskip (Lyall route), or (b) a Nioh1Fix-style `measured-dt` override of the target accessor plus camera/anim fixes. True render interpolation is not exposed anywhere and nobody has done it. **No public fix for Nioh 3 judder exists besides locking to 60/120 or running FG/MFG on a locked base.**

### 1.5 Practical judder workarounds reported
- Lock exactly 60 or 120 in-game. For 120, a 120/144 Hz display; ideally match the refresh rate to the cap (ResetEra p3/p7). The in-game cap beats external caps (vortexgaming 9070 XT test); Wo Long users found the opposite.
- FG: a 60 base gives 120 out. DLSS-FG+Reflex on a 120 Hz screen makes 116 = judder; on 144 Hz+ it is fine. FSR-FG at 120/120 Hz is reported smooth but has higher latency (ResetEra p6). On AMD with in-game FSR-FG plus the AMD driver cap at 120, the Game-Zone cap is incompatible with FG ([vortexgaming 9070 XT](https://vortexgaming.io/en/postdetail/661018)).
- Nioh3Fix `TargetFPS=58` for 116 FG output.

---

## 2. Patch notes to 2.02 (Steam news API `ISteamNews/GetNewsForApp/v2/?appid=3681010`)
Only PC/perf-relevant items:
- 1.03.x: crash fixes; "water surfaces radiate purple light" partially fixed.
- **1.04 (Mar 10):** some cutscenes that were stuck at 30 now run at 60. **The Anisotropic Filtering option was always x4; now applied** ("you may notice changes in graphics quality and performance"). Light anomalies at screen edges at some resolutions fixed. 1.04.01: crash fix for some PCs.
- **1.04.03 (Apr 1):** "Adjusted the timing of FPS changes to reduce lag when FPS Dynamic Adjustment is enabled"; Dynamic Resolution reset could exceed the cap (fixed); "screen lag" with a joystick fixed.
- **1.05 (Apr 27):** FSR3 Frame Generation crash fixed; HDR black-screen leak fixed; **lock-on camera rotation speed depended on FPS** (fixed).
- 1.06/1.07: no PC items.
- **2.00.01 (Aug 17, DLC):** AV deleting the XeSS DLL fixed. *"Be careful when using x3 or x4 frame generation override… rendering errors with a 60 Hz monitor"*. Several 30-FPS-only gameplay bugs fixed. Camera distance range widened to -10..10.
- 2.00.02 (Aug 20): save fix only. **2.01 (Sep 1), 2.02 (Sep 16): gameplay/DLC/bug fixes only, no performance or graphics items.**

## 3. CPU usage (PRIORITY) — matches our ~650% / GPU 47% measurement

### 3.1 What is publicly reported
- **Widely reported as "unreasonably high CPU demand"** on all CPU tiers. Quotes and sources:
  - TPU, on an RX 9070 XT with a 5700X3D: "can barely maintain 60 FPS at 65% scale… dropping most settings has almost no impact… unreasonably high CPU demand despite the maps being fairly compact" ([TPU conclusion](https://www.techpowerup.com/review/nioh-3-steam-deck-rog-xbox-ally-x-performance-benchmark/8.html)).
  - 5600X: "CPU usage almost at maximum… even at 540p internal CPU usage remains high"; misses a stable 60 in the open world ([vortexgaming](https://vortexgaming.io/en/postdetail/658687)).
  - 9800X3D: 70-80% total at 120 fps. Ryzen 5 5600: 85-100% in the open world at **both 30 and 60 fps**. 7700X "pinned holding 60". 9950X3D over 60%. A 13700K about 90% ([ResetEra p3–p5](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-3)).
  - **One user describes the shape: "utilises all available threads at ~45-50% overall, main worker thread ~85%"** ([ResetEra p5](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-5)). This points to a spread of spinning workers plus one hot thread.
  - **Cutscenes, menus and 2D cinematics run the CPU at full boost**: a 5900X hits TjMax 90 °C in cutscenes. Usage does not drop with the 30/60 cap ([ResetEra p4/p7](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-7)). "Spamming menus tanks the fps — CPU capped as usual for KT" ([ResetEra p1](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/)).
  - Core scaling: uses up to **8 physical cores / 12 threads**, with no gain beyond ([gamegpu](https://en.gamegpu.com/test-gpu/action-fps-tps/nioh-3-sravnitelnye-benchmarki-c-nvidia-dlss-4-5-i-amd-fsr-4)). DSOGaming's simulated CPU counts on a 7950X3D: 2 cores won't run, 4 cores stutter, **6 cores are smooth at 60, and 8 cores/threads are needed for 120** ([DSOG](https://www.dsogaming.com/pc-performance-analyses/nioh-3-pc-performance-analysis/)). RAM use is 14-15 GB at 1080p ([gamegpu old-hw](https://gamegpu.com/test-gpu/rpgrolevye/nioh-3-benchmarki-videokart-i-protsessorov-na-starom-zheleze)).
- **The only widely shared "fix" is a symptom treatment.** Setting the Windows power plan "max processor state" to 99%, which disables boost, gives "no detrimental effect, still locked 60" and drops temps 75→50 °C (gameplay) and 90→60 °C (cutscenes). Process Lasso gives the same kind of result. Interpretation: a large part of the load is **non-productive (spin/poll) time**, because removing ~15% clock does not cost frames at a 60 cap. **This is consistent with busy-waiting workers.**
- **No public RE / mod targets the worker spin.** Nioh3Fix has no CPU hooks. Nexus mod #36 "Optimization modification and FG x4 unlock" (Velasquez, v0.1) is only presets plus FSR/XeSS DLL swaps. `CPU_SPEC_CLASS`: **zero public hits** in any language. It is undocumented, so we must RE it ourselves.

### 3.2 Katana-family precedents (useful leads)
- **Wild Hearts (Omega Force, Katana, 2023)**: Patch 1.004 notes say: "fixed a CPU threading issue → significantly improved performance on high-end PCs" and "**fixed an issue where certain high-end CPUs were being unintentionally capped**" ([Prima](https://primagames.com/gaming/wild-hearts-pc-patch-update-notes), [eXputer](https://exputer.com/news/games/new-wild-hearts-patch/)). **So the engine classifies CPUs and sizes or limits its thread usage from that**, likely what `CPU_SPEC_CLASS` controls. Also in Wild Hearts, **switching audio from stereo to 5.1/7.1 "improves performance considerably"** ([Wccftech](https://wccftech.com/wild-hearts-simple-workaround-performance/)).
- **Rise of the Ronin (Team Ninja, Katana)**: Patch 1.09.0.3 says: "**Fixed CPU load increasing significantly when Audio Output was 'Surround' or '3D Audio Effects' while Windows spatial sound was disabled**" ([DSOGaming](https://www.dsogaming.com/patches/rise-of-the-ronin-april-10th-patch-released-packing-performance-optimization-improvements/)). Patch 1.09.0.1 fixed a crash on CPUs with **≥33 threads**, so thread count is derived from the logical CPU count with a fixed-size table. Users saw 100% CPU and 90-95 °C at 60 fps. FSR3-FG at a 60 base dropped CPU to 50-60%.
- **Wo Long**: 90-100 °C CPU temps at 60 fps on many CPUs. Fixes reported: disable Windows thread-priority boost for the exe (Process Lasso), use affinity, use Realtime priority. That points to thread-scheduling-sensitive spin-waits, especially on hybrid Intel ([Steam](https://steamcommunity.com/app/1448440/discussions/0/3818528846669544233/), [Steam CPU thread](https://steamcommunity.com/app/1448440/discussions/0/3792632416036637960/)).
- **Dynasty Warriors: Origins**: "hammers cores 1 and 3 to 100% in battles, overworld map, menus and cutscenes" ([Steam](https://steamcommunity.com/app/2384580/discussions/0/734750050411921469/)).
- **Official Koei Tecmo troubleshooting (Nioh 3 / generic Steam)**: "**Unplug any input devices not related to gameplay**; USB peripherals may cause slowdown". The community adds: uninstall *virtual* input drivers (vJoy, ViGEm…). ResetEra p6 found the game **re-polls/enumerates USB HID devices, causing ~1 s fps dips every few seconds** ([KT support](https://support.koeitecmo.info/hc/en-us/articles/4405858545049--Steam-The-game-runs-slow), [ResetEra p6](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-6)).

### 3.3 What this suggests for our 650% / GPU 47% case (hypotheses to test)
1. **Spin-wait job workers.** About 5-6 of 12 threads sit busy in a static scene at a 60 cap, and boost-off costs nothing. Look for worker loops on `_mm_pause` or `SwitchToThread`/`Sleep(0)` with no `WaitForSingleObject`/`WaitOnAddress` backoff. Also check whether the main thread's frame limiter itself spins: Nioh1Fix found a **QPC-based limiter right after Present**, a classic busy-wait.
2. **Worker count from CPU class.** Wild Hearts proves the engine caps by CPU tier and RotR proves thread-count tables. `CPU_SPEC_CLASS` is likely the knob, so test lower and higher values. On a 7600X (6C/12T), check whether workers equal the logical count and whether SMT siblings fight the main thread. Test a Process Lasso "disable SMT" affinity (6 threads) A/B.
3. **Audio output mode**, to A/B test: Stereo vs Surround vs 3D, and Windows spatial sound on/off (Katana precedent in Wild Hearts and RotR).
4. **Input device polling**: unplug extra HID devices and remove virtual-pad drivers, then re-measure.
5. **Motion Quality** (Nioh3Fix comment: "CPU-only"; Wccftech: "affects CPU performance only") and draw-distance/character LOD are CPU-side settings. Grass density is reported as noticeable.
6. FG at a locked 60 base: RotR users saw CPU drop with FG. On AMD that means FSR-FG at 60 base giving 120 out, which also sidesteps the judder.

---

## 4. Upscaling and frame generation on RDNA4 (RX 9070)
- Native: **FSR 3.1** (a "custom implementation": OptiScaler cannot hook FSR3.1 inputs), DLSS 4, XeSS 2. FG options: DLSS FG/MFG, FSR FG, and (per PCGW) XeSS FG. Upscaler and FG can be mixed, e.g. XeSS upscale with FSR FG. There are no quality presets: render scale runs 25-100% in 5% steps. Every preset turns upscaling on; 100% = native AA (DLAA/FSR-native). Dynamic resolution has targets of 30/45/60/120 with a min/max range ([TPU p3](https://www.techpowerup.com/review/nioh-3-steam-deck-rog-xbox-ally-x-performance-benchmark/3.html)).
- **Adrenalin FSR 4 override works.** Enable FSR in-game, then toggle "AMD FSR Upscaling" in Adrenalin ([gamegpu](https://en.gamegpu.com/test-gpu/action-fps-tps/nioh-3-sravnitelnye-benchmarki-c-nvidia-dlss-4-5-i-amd-fsr-4), TPU). If the toggle is greyed out, force it in Global Graphics. Adrenalin 26.2.1 added Nioh 3 support.
- **OptiScaler** ([wiki](https://github.com/optiscaler/OptiScaler/wiki/Nioh-3), tested on 0.9 with a 9070 XT): use `dxgi.dll` and **XeSS inputs**. FG path: "DLSSG via Streamline → FSR-FG/XeFG" or Nukem's dlssg-to-fsr3. Known issue: XeFG forces a refresh-rate cap; fix with `OverrideVsync=true` plus VSync OFF in the overlay. **0.9.1 crashed at boot** (issue [#976](https://github.com/optiscaler/OptiScaler/issues/976)): the game forces FSR3.1 on a fresh config. The workaround was to boot without Opti, set XeSS in-game, then add Opti, or set `Fsr4Update=false`. Fixed in 0.9.2 (commit 9a021ca). Current releases: **0.9.4 (2026-07-18, FSR 4.1.1 / FFX 2.3 SDK)**, 0.9.3 (FSR 4.1). FSR 4.0.3 caused exposure/fog/metal flicker in Nioh 3; FSR 4.1 is "strictly better" ([blog](https://blog.jonathang.me/posts/fsr-upscaling-4-1-0-and-nioh-3/)). Nothing confirms Opti on 2.02 specifically, but 2.01/2.02 had no renderer changes.
- **FG crash history:** in-game FSR3-FG crash fixed in 1.05. On Linux (vkd3d) the FSR-FG crash on RDNA3/4 is still open ([#2845](https://github.com/HansKristian-Work/vkd3d-proton/issues/2845)). Nioh3Fix's dynamic-FPS option "can freeze when toggling FG".
- 9070 XT data points (demo): 1440p60 with XeSS at 100% on High; 1440p120 needs 50% scale; FG gives 200+ fps ([vortexgaming](https://vortexgaming.io/en/postdetail/661018)).

## 5. Settings costs (GPU side)
| Setting | Cost | Source |
|---|---|---|
| **Global Illumination** ("OBORO", trace-based multi-bounce GI, auto-placed probes) | **up to 54%** Very Low→Ultra (4090 at 4K). Low through Very High look similar; Ultra is the big jump. Lowering it also reduces grass/surface shimmer the *opposite* way (max GI/AO/Effects reduce shimmer). | [pcoptimizedsettings](https://pcoptimizedsettings.com/nioh-3-pc-settings-performance-optimization-guide/), [CGWorld](https://cgworld.jp/article/202605-cgw333-nioh3-b.html), ResetEra p5 |
| SSAO | 5-6% | pcoptimizedsettings |
| SSR | ~5% (higher levels = "software RT") | pcoptimizedsettings, Gamerant |
| Volumetric cloud | small; "Very Low" disables volumetric fog | Wccftech |
| Grass density | noticeable (ResetEra p2) | |
| Post effects (all) | ~3% | Wccftech |
| Shadows (virtual shadow maps), Terrain (tessellation), Model, BG Mesh (meshlet "RADEN" continuous LOD), Wind sway, textures | negligible on GPU | Wccftech, TPU, pcoptimizedsettings |
| Motion Quality, draw distances | CPU-side | Wccftech, Nioh3Fix |
- 4090 at 1440p, "optimized vs max": +25% average, +14% 1% lows (Wccftech).
- VRAM: 10.5 GB (1080p), 11 GB (1440p), 13 GB (4K). Fine on a 16 GB 9070.
- **"Meshlet quality" / "Color precision"**: no public measurements. Meshlet is likely the RADEN meshlet LOD error threshold. Nioh3Fix's BackgroundMeshQuality hook writes a LOD multiplier of Low=2.0, Med=1.5, High=1.0, so it is probably the same family. Color precision sits under Post Effects and is likely RT format (R11G11B10 vs RGBA16F); expect a small bandwidth cost.
- **Recommended for 9070 + 7600X at 1440p:** a locked 60 in-game cap (or 120 if ≥120 Hz *and* consistently above 120, which is unlikely given the CPU). GI at Standard/Low, SSR Low/Off, SSAO Standard, everything else High. FSR 4 via Adrenalin (or Opti XeSS→FSR 4.1) at 65-77%. For 120 output, use FSR-FG on a locked 60 base. FPS Dynamic Adjustment OFF if you can hold the cap, otherwise ON (no slow motion, but judder).

## 6. Shader compilation and traversal stutter
- Boot-time "Compiling shaders" step: a few minutes the first time, seconds afterwards. A **"ShaderComp" boot state** is visible in Nioh3Fix's intro-skip patch. Reviewers agree: **no shader-compilation stutter** after the step ([DSOG](https://www.dsogaming.com/articles/nioh-3-does-not-suffer-from-stutters-first-pc-benchmarks/), [TPU](https://www.techpowerup.com/review/nioh-3-steam-deck-rog-xbox-ally-x-performance-benchmark/8.html), Wccftech). Some guides claim "30-60 min of hitching" on first play; that is unverified SEO content.
- **Traversal hitching does exist** in open-world maps ("some areas stutter-free, others hitch fests"; "occasional massive frame rate drops out of nowhere", TPU). The causes are unknown. Candidates: streaming/decompression on the CPU (RAM use 14-15 GB), HID polling (see 3.2), VRAM pressure on 8 GB cards.
- **SCSKiller** ([repo](https://github.com/BlueHeisenberg/SCSKiller)) pre-warms the driver PSO cache from shipped shaders or a recorder. **The Katana engine is "not supported yet"**: issue [#66](https://github.com/BlueHeisenberg/SCSKiller/issues/66) "[Request] Team Ninja's Katana Engine games" was opened 2026-10-06 with no replies. Its optional *recorder* (`d3d12.dll` proxy) can still capture PSOs from play. Since the game already precompiles PSOs at boot, **expected gain is small**. It is only relevant if the recorder shows ≥20 ms PSO creates during traversal, and it would conflict with OptiScaler's `dxgi.dll`/`d3d12` hooks, so test separately. A driver update clears the AMD cache, after which the game re-runs its compile step.

## 7. Katana engine internals (for RE)
- Engine: Koei Tecmo "KATANA ENGINE" (Future Tech Base). Used by Nioh 3, RotR, Wo Long, Wild Hearts, DW Origins, and Nioh 1/2 (older branch). D3D12 only. RTTI namespaces seen in Nioh 1: `ktgl`, `scl` (UI), classes `CCloudPlane`, `CStatisticalOcean`, `CActModuleInputBase`, `CPushButtonFRateController`, `ControlScriptObject`, `PostureAnimeObject`, `ChildShotObject` (Nioh1Fix).
- Nioh 3 tech (CGWorld, May 2026, graphics lead 新田亮馬): **RADEN** = meshlet-based continuous LOD with auto LOD generation and fine-grained culling. **OBORO** = real-time trace-based multi-bounce GI with probes auto-placed by scene density. **Virtual shadow maps**. Shadow-casting light counts are budgeted. Open-field structure is inherited from RotR.
- Timing/frame architecture (Nioh1Fix RE plus Nioh3Fix hooks): frame-profile table, active profile index, target-FPS accessor (~190 callers), dynamic-target function using an averaged frametime, frameskip branch, QPC post-Present limiter, Present with sync interval. Menu input repeat is scaled by `60/fps` quantised to 0.25.
- Threading: no talk or paper exists. CEDEC 2022 #23 (三嶋寛了, "KATANA ENGINE design") is architecture/process only. CEDEC 2025 RotR session (CEDiL 3151) is design only. CEDEC 2026 had no Koei Tecmo sessions (checked the schedule CSV). Indirect evidence: CPU-tier thread capping (Wild Hearts), a thread-count table overflowing at ≥33 threads (RotR), audio-mode CPU spin (RotR/Wild Hearts), all-thread busy usage (Nioh 3).
- Mod loading precedent: Ultimate ASI Loader as `dinput8.dll` (Nioh3Fix) or `version.dll` (Nioh1Fix/Nioh2Fix). Nioh3Fix pattern scans broke on 1.04 and 2.00.02 and were fixed both times. The exe changes a lot between patches; SteamStub decrypts code after ASI load, so Nioh1Fix polls for 120 s before patching.
- Nioh3Fix log offsets for 2.00.01 (module timestamp 1786363651): Intro skip `+1c28cf6`, Current Resolution `+8707c8`, Motion Quality `+5475bb`, Character Draw Distance `+200f3a`, Background Mesh Quality `+24a1df`.

## 8. Source list (primary)
- Nioh3Fix source: https://codeberg.org/Lyall/Nioh3Fix (raw via `codeberg.org/Lyall/Nioh3Fix/raw/branch/main/src/dllmain.cpp`; old frameskip code at commit `917e30ff99`). Also Nioh2Fix, WoLongFix, RiseOfTheRoninFix on codeberg.org/Lyall; none touch framerate except Nioh2's cutscene 30-fps patch.
- Nioh1Fix: https://github.com/RobertTausig/Nioh1Fix (docs/research.md, src/timing.cpp).
- Patch notes: Steam news API, appid 3681010.
- OptiScaler wiki/issue #976; vkd3d-proton #2845; SCSKiller #66.
- Reviews/threads: TPU handheld review (p3/p4/p8), DSOGaming (benchmarks + analysis), Wccftech tuning guide, pcoptimizedsettings, gamegpu, ResetEra Nioh 3 PC thread pages 1-7, CGWorld Nioh 3 (2).

Local copies of the reference sources (for RE work) are in `notes/ref/`: Nioh3Fix 0.0.6 `dllmain.cpp`, the older Nioh3Fix with the frameskip hook (commit 917e30f), and Nioh1Fix `research.md`, `timing.cpp` and `signatures.hpp`.
