# Nioh 3 PC: new community findings (as of 2026-10-06)

Web research only. Nothing was downloaded or run, and the game and its folder were not touched. This file only covers items that are **not** already in `notes/research.md` or the README credits.

Tags: **(a)** CPU main-thread / CPU load, **(b)** GPU, **(c)** handhelds, **(d)** stability. Evidence strength: **[measured]** means numbers or a repro were given; **[anecdote]** means one user's report with no data.

---

## 0. Status check: nothing new upstream

| Item | Finding | Source |
|---|---|---|
| Game patches | **No patch after 2.02.** The newest Steam news item is "Ver. 2.02 Update" (timestamp 1789611169 = 2026-09-16). Since then there have been only a sale and a bundle post. DLC 2 "Bloody Insurrection" is listed for the end of Feb 2027 (third-party roadmap; unverified). | [Steam news API](https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=3681010), [game8 roadmap](https://game8.co/games/Nioh-3/archives/576081) |
| Nioh3Fix | No commits since 0.0.6 (2026-08-25); no open issues. | [Codeberg API](https://codeberg.org/api/v1/repos/Lyall/Nioh3Fix/commits) |
| OptiScaler | 0.9.4 (2026-07-18) is still the latest release. | [GitHub releases](https://github.com/optiscaler/OptiScaler/releases) |
| Nexus | 120 mods total; the newest is #178 (2026-10-07). **No new performance, LOD, CPU or frame-pacing mod** since #14/#36. Loose File Loader (#90) updated to v1.3.0 for 2.02 (2026-09-17). | Nexus GraphQL API (`mods(filter:{gameDomainName:nioh3})`) |
| AMD / NVIDIA drivers | No driver release note mentions Nioh 3 since Adrenalin 26.2.1 (launch support). 26.9.1 and 26.9.2 have no Nioh 3 items. | [AMD 26.9.1 RN](https://amd.com/en/resources/support-articles/release-notes/RN-RAD-WIN-26-9-1.html), [26.9.2 RN](https://www.amd.com/en/resources/support-articles/release-notes/rn-rad-win-26-9-2.html) |
| PCGamingWiki | Nothing new: it lists only Nioh3Fix and the FOV mod, with no CPU fixes. | [PCGW wikitext](https://www.pcgamingwiki.com/wiki/Nioh_3) |

## 1. CPU / main thread (a)

1. **The in-game limiter costs more CPU than an external one.** A Guru3D forum report says: "hammers the CPU much more than forcing the framerate via RTSS". With RTSS/NVCP set to 30 *on top of* the same in-game cap, **CPU usage drops by ~20%**. It also reports that **the in-game 120 cap adds CPU load even when the game only reaches ~60.** — (a) **[measured, single source; page is Cloudflare-blocked, quoted via search snippet]** — [Guru3D Nioh 3 thread p2](https://forums.guru3d.com/threads/nioh-3.456603/page-2). This fits our spin-limiter hypothesis (QPC busy-wait after Present). It is cheap to A/B test: in-game 60 vs in-game 60 + Adrenalin FRTC/RTSS 60, then compare CPU%.
2. **Missing the target makes the CPU work harder (frameskip catch-up).** The reports: "with a 120 cap you get 45 fps where a 60 cap holds a constant 60" ([search summary of the QT3/Guru3D threads](https://forum.quartertothree.com/t/nioh-3-new-foes-old-engine/164740?page=5)); "If you hit 59 etc., you make your CPU work harder" ([Steam "This game is cooking CPUs"](https://steamcommunity.com/app/3681010/discussions/0/780947379776040327/)); "at a 30 cap the stutter is 100% gone, at 60 it's 58-60 and not smooth" ([Steam "Other fix to High CPU usage"](https://steamcommunity.com/app/3681010/discussions/0/780947379776113172/)). — (a) **[anecdote, consistent across sources]**. Implication: always measure with a cap the machine can actually hold, and set the cap to the rate it can hold.
3. **Thread shape.** One report has 2-3 threads at 80-100% at all times and most others idle (9950X3D + 5090). A counter-report on a 14700K has P-cores at 50-100% each and E-cores at ≤50%, for ~38% total. — (a) **[measured-ish, Process Lasso graphs]** — [Steam "Nioh 3's CPU scheduling problem"](https://steamcommunity.com/app/3681010/discussions/0/800091752203581046/).
4. **Affinity and SMT results:**
   - i7-12700F: removing the 4 E-cores from affinity got rid of the microstutter ([Steam stutter thread](https://steamcommunity.com/app/3681010/discussions/0/759554462772908209/)).
   - Dual-CCD X3D: restricting to CPUs 0-15 (one CCD) "fixed it" ([Steam](https://steamcommunity.com/app/3681010/discussions/0/759554831917720575/)).
   - **16 threads vs 8 threads:** the 8-thread run was worse; FG had to go off because the base fps dropped ([Steam "Abnormally high cpu temp?"](https://steamcommunity.com/app/3681010/discussions/0/726902998784027100/)).
   - Process Lasso with priority High, I/O priority High, and optionally "Disable SMT" was "almost fully fixed" for a 5900X ([Steam "PERFORMANCE FIX for AMD Ryzen users"](https://steamcommunity.com/app/3681010/discussions/0/759554831917629321/)). Another user saw no change or a negative effect.

   — (a) **[anecdote]**. For a 7600X (6C/12T), the 8-thread result and DSOG's "8 threads needed for 120" suggest **SMT-off will likely hurt**. Priority High is the safer test.
5. **Clock/boost caps for temps (refinements of the known 99% trick):**
   - A registry tweak unhides the "Processor maximum frequency" power setting to cap MHz instead of disabling boost; reported at ~-20 °C with minimal fps loss (ResetEra user cowbanana, [p7](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-7)).
   - Process Lasso per-process "Power Saver" profile ([Steam "CPU Temp Tip"](https://steamcommunity.com/app/3681010/discussions/0/759554462772960867/)).
   - MSI Claw 8 (Intel handheld): disabling Turbo Boost "stopped the 100% usage" ([Steam](https://steamcommunity.com/app/3681010/discussions/0/726902998784027100/)).

   — (a)(c) **[anecdote]**.
6. **The network-check loop burns CPU.** The demo, stuck offline in its "checking connection" loop at the title screen, ran at **90-100% CPU vs 40-60% online**. The user suspects a constant connectivity poll in the full game too. — (a) **[anecdote with numbers]** — [Steam, Wymn, 2026-02-09](https://steamcommunity.com/app/3681010/discussions/0/759554831917629321/). Easy test: compare CPU% online vs offline mode in the same spot.
7. **Windowed may load the CPU less than borderless.** — (a) **[anecdote, untested]** — [Steam "Performance settings tips"](https://steamcommunity.com/app/3681010/discussions/0/759554462772824964/).
8. **Streaming cost.** Users report 5-second dips to 20-40 fps when looking at far horizons, slow loading, and low SSD usage with high CPU. They attribute it to CPU-side decompression and streaming. — (a) **[anecdote]** — [Steam](https://steamcommunity.com/app/3681010/discussions/0/780947379776113172/), [Steam "Weird Frame Rate Cap Behavior"](https://steamcommunity.com/app/3681010/discussions/0/800091928768205357/). Relevant to the BG-mesh, terrain and draw-distance LOD knobs.
9. **Katana precedent, Rise of the Ronin 1.09.0.3:** "Dynamic Adjustment of FPS" was *added* by that patch, alongside the surround-audio CPU fix. So the Nioh 3 dynamic-FPS code is RotR-derived. — (a) — [Steam news RotR](https://api.steampowered.com/ISteamNews/GetNewsForApp/v2/?appid=1340990).

## 2. GPU / rendering (b)

1. **Shadows from artificial lights disappear when the frame rate is not on a 30/60/120 cap.** This was shown in BenchmarKing's video and repeated by guides. — (b) **[video evidence]** — [ResetEra p7](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-7), [VGTimes guide](https://vgtimes.com/guides/147899-nioh-3-optimization-guide-best-settings-for-maximum-performance.html). **Important for our real-time frame-step patch:** after unlocking, check that torch and lantern shadows still render. The shadow-light budget is probably tied to the frame tier.
2. **RDNA4 only: the martial-art/ninjutsu preview clips and tutorial videos stutter.** Reported on a 9070 XT (2026-09-02) and a 9060 XT on Adrenalin 26.8.1. A regression of a previously fixed bug since the DLC. The 30 fps cap fixes it; Resizable BAR and VRR made no difference. — (b) **[repro]** — [Steam](https://steamcommunity.com/app/3681010/discussions/0/582807198746341609/), [bug thread, suouta 2026-08-23](https://steamcommunity.com/app/3681010/discussions/0/726902731209521758/). Cosmetic; don't mistake it for an engine-patch regression.
3. **Cutscene A/V desync.** Max FPS 120 + cutscene FPS 60 desyncs audio in post-DLC 3D cutscenes; cutscene FPS 30 fixes it. FG + cutscene 120 also breaks sync. — (b)(d) **[repro]** — [bug thread](https://steamcommunity.com/app/3681010/discussions/0/726902731209521758/). This is another place where the timing model leaks.
4. **Default upscaler on AMD (Linux):** the game "seems to default to XeSS, which is much slower than FSR" on AMD. — (b) **[anecdote]** — [Proton #9473](https://github.com/ValveSoftware/Proton/issues/9473). On a fresh config, check which upscaler the game picked.
5. **Nexus #2 "No-HUD-No-Effects - Toggle Fog - Combat FX"** (ShaderToggler, v0.8, updated 2026-08-21) is not in our credits list. It has more effect/fog shader-hash groups than #10, which makes it a cross-check source for effect-cost tests. — (b) — [Nexus #2](https://www.nexusmods.com/nioh3/mods/2).
6. **Lossless Scaling FG at a 30/60 lock** is reported by several users as smoother than the in-game FG. — (b) **[anecdote]** — [Steam](https://steamcommunity.com/app/3681010/discussions/0/806844795448358450/).
7. (NVIDIA only, FYI) DLSS 4.5 DLAA gives microstutter; DLSS 4.0 preset K is smooth. — [ResetEra p7](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-7).

## 3. Handhelds and Linux (c)

| Finding | Source |
|---|---|
| Valve rating: **Steam Deck = Unsupported** ("graphics performance"), **SteamOS = Playable**, **Steam Machine = Verified** ("default configuration is performant"). | [Steam deck-compat API](https://store.steampowered.com/saleaction/ajaxgetdeckappcompatibilityreport?nAppID=3681010) |
| ProtonDB: **Gold**, 66 reports, **trending Silver**. | [ProtonDB API](https://www.protondb.com/api/v1/reports/summaries/3681010.json) |
| Steam Deck (SteamDeckHQ, 2026-02-04): 30 cap, 90 Hz, lowest settings, no forced Proton, **21-24 W**, 78-83 °C, 2.5 h battery. It still regularly drops under 30, CPU-bound. Some videos don't play. | [SteamDeckHQ](https://steamdeckhq.com/game-reviews/nioh-3/) |
| **Never enable FG on Deck/Linux:** it crashes, then the game won't boot until the config file is deleted. | [RPGSite](https://www.rpgsite.net/feature/19502-nioh-3-steam-deck-full-game-recommended-settings-rog-ally-ps5-performance), [Proton #9473](https://github.com/ValveSoftware/Proton/issues/9473), [vkd3d-proton #2845](https://github.com/HansKristian-Work/vkd3d-proton/issues/2845) (still open) |
| ROG Ally Z1E: 30 fixed, 1080p, FSR 3.1 dynamic res 25-100%, advanced and post settings at minimum, **25 W Turbo, 6 GB VRAM allocation**. Stable 30 over 8 h. | [RPGSite](https://www.rpgsite.net/feature/19502-nioh-3-steam-deck-full-game-recommended-settings-rog-ally-ps5-performance) |
| ROG (Xbox) Ally X: Very Low, 900p, 45-50% scale, 30-35 W, ~45 fps average. A 45 fps average is between engine tiers (judder/slow-mo), so TPU's alternative of a **30 lock at 65% scale** is the stable choice. | [TPU p7](https://www.techpowerup.com/review/nioh-3-steam-deck-rog-xbox-ally-x-performance-benchmark/7.html) |
| Linux: Proton Experimental beats GE 10-29. Shader compile hangs at **15395/17041** (NVIDIA 550/570 better; 590 has a VRAM-spike bug). CachyOS Proton + `PROTON_FSR4_UPGRADE=1` works on RDNA4 desktops. vkd3d-proton 3.01 and Mesa-git reduce, but don't end, GPU hangs. | [Steam "Playable Linux settings"](https://steamcommunity.com/app/3681010/discussions/0/759554462772937119/), [Proton #9473](https://github.com/ValveSoftware/Proton/issues/9473) |
| Proton: random **~500 ms stalls every few seconds** that don't happen on Windows; 100% CPU with asset pop-in on CachyOS. | [Proton #9473](https://github.com/ValveSoftware/Proton/issues/9473) |

## 4. Stability / crashes (d)

1. **Most relevant to our SKIN-SSS-off access violation: the character-render resource-lifetime crash family.**
   - **Linux repro:** "the simplest way to cause the GPU hang is to scroll the main menu back and forth quickly; the rendered character appears and disappears". The devcoredump shows a **GPU page fault reading address 0x0** (null VA). — (d) **[repro]** — [Proton #9473, user ipr, 2026-06-22](https://github.com/ValveSoftware/Proton/issues/9473).
   - **Katana precedent:** Rise of the Ronin 1.09.0.5 (2025-11-11) fixed a crash "after certain Windows updates… when **certain characters or equipment appeared on-screen**". — [Team NINJA](https://x.com/TeamNINJAStudio/status/1988139632907698645), [Steam news](https://store.steampowered.com/news/app/1340990/view/594038931768475802).

   Both point at per-character render resources (skin/SSS/material buffers) being referenced in the frame's command stream while null or freed. **Use the rapid menu-scroll as a quick repro for our crash with SSS off vs on.**
2. **DXGI_ERROR_DEVICE_REMOVED / HUNG fixes people report:**
   - AMD: cap the GPU max clock (2900→2700 MHz with power +10%, or set max frequency below the rated clock) ([Steam](https://steamcommunity.com/app/3681010/discussions/0/759554462772968080/), [Steam](https://steamcommunity.com/app/3681010/discussions/0/764058696424764012/)).
   - **Lower the RAM speed / drop EXPO** (3600→3200): "this engine is the ultimate test for RAM stability". The same fix is reported for RotR ([Steam](https://steamcommunity.com/app/3681010/discussions/0/568165608207681834/), [bug thread](https://steamcommunity.com/app/3681010/discussions/0/726902731209521758/)).
   - Cap fps at 60 **and cutscene fps at 60** ([Steam](https://steamcommunity.com/app/3681010/discussions/0/759554462772968080/)).
   - "The higher the fps, the faster it crashes" (7900 XTX, DEVICE_HUNG) ([bug thread](https://steamcommunity.com/app/3681010/discussions/0/726902731209521758/)).
   - A 9070 XT user saw it only at 100% GPU load ([Steam](https://steamcommunity.com/app/3681010/discussions/0/800092645914529456/)).

   — (d) **[anecdote]**.
3. **2.02 regression report (NVIDIA):** DEVICE_REMOVED and freezes since 2.02 on a 13900KF + 4090, with nvlddmkm Event 153 and a dwm crash in nvwgf2umx.dll. There is also an NVIDIA BSOD in `dxgmms2!VidSchUnwaitFlipQueue` (flip-queue/frame-sync path; 13600KF + 4080). — (d) — [bug thread, 2026-09-20 / 2026-05-29](https://steamcommunity.com/app/3681010/discussions/0/726902731209521758/).
4. **Freezes after the DLC with audio still playing:** removing **MSI Afterburner/RivaTuner** helped ([bug thread 2026-08-20](https://steamcommunity.com/app/3681010/discussions/0/726902731209521758/)). Disabling all overlays fixed a post-DLC fatal error ([Steam](https://steamcommunity.com/app/3681010/discussions/0/84032037661626332/)). — (d) **[anecdote]**. Keep RTSS out of crash repro runs.
5. **A 100%-reproducible 0xC0000005 on quit** (teardown) is reported by several users. Expect it, and don't confuse it with our in-frame crash. — (d) — [bug thread 2026-02-14](https://steamcommunity.com/app/3681010/discussions/0/726902731209521758/), [Steam](https://steamcommunity.com/app/3681010/discussions/0/759554831917612626/).
6. **0x80000003 (breakpoint/assert)** every time the user visits a shrine "blessing" (RX 6800). — (d) — [bug thread 2026-06-01](https://steamcommunity.com/app/3681010/discussions/0/726902731209521758/).
7. **Windows-side causes to rule out:**
   - **KB5124010** (2026-09-22 preview; 24H2/25H2/26H2): crashes in apps that use Windows' AC-3 decoder ([Windows Latest](https://www.windowslatest.com/2026/10/03/microsoft-confirms-windows-11-26h2-is-crashing-some-games-and-apps-promises-to-fix-it-in-the-next-update/)).
   - **KB5121003** (Aug): access violations in some games, traced to the third-party **inpoutx64.sys** driver installed by RGB and fan tools ([Windows Latest](https://www.windowslatest.com/2026/08/20/windows-11s-august-update-is-breaking-games-and-microsoft-is-investigating-after-a-studio-blamed-the-update/)).
   - Controlled Folder Access was blamed for 0xC0000005 ([Steam](https://steamcommunity.com/app/3681010/discussions/0/844004758333283555/)).

   — (d).

## 5. Watch list

- **Wo Long 2: Wings of Ember** (Katana; alpha demo Sept 2026, release 2027-03-04). This is the next Katana PC build; check whether its timing model differs. — [Gematsu](https://www.gematsu.com/2026/09/wo-long-2-wings-of-ember-launches-march-4-2027-alpha-demo-now-available)
- **KTFileLoaderLogger** (ren-, 2026-09-22): a multi-game fork of Nioh3FileLoaderLogger with per-game signature profiles for Nioh 3 (verified 2026-09-17, so 2.02), Wo Long, Wo Long 2 and RotR. Useful for comparing Katana builds. — [GitHub](https://github.com/ren-/KTFileLoaderLogger)
- **kassent/Nioh3-Plugins**: source of Loose File Loader, adapted to 2.0.2.0 (latest commit). Shows the current 2.02 signatures. — [GitHub](https://github.com/kassent/Nioh3-Plugins)
- **Avoid:** GitHub "Nioh 3 trainer / offline installer / engine pro" repos (many created 2026-10-06) are malware lures.

## Top 5 actionable

1. **A/B test the limiter.** In-game 60 alone vs in-game 60 + Adrenalin FRTC (or RTSS) 60, and compare CPU%. Guru3D reports ~20% lower CPU with the external limiter on top, which supports the busy-wait-limiter theory.
2. **Measure only at a cap the machine holds.** A cap above the delivered rate (120 while getting 60-110) *increases* CPU work through frameskip catch-up. For the frame-step patch, also check that **artificial-light shadows** survive off-tier frame rates.
3. **Crash repro.** Rapidly scroll the main menu so the character model appears and disappears, with SKIN SSS off vs on. A Linux devcoredump shows a null-VA read in exactly that scenario, and RotR had a "certain characters/equipment on-screen" crash. This matches our render-frame-begin access violation.
4. **Clean stability baseline before blaming our patches.** No RTSS/Afterburner, no overlays, stock RAM (EXPO is a common DEVICE_REMOVED cause), and confirm KB5124010 and inpoutx64.sys are absent. Expect the known exit-time 0xC0000005.
5. **Cheap CPU A/B tests:** online vs offline mode (a connectivity poll loop was seen at 90-100% CPU), windowed vs borderless, and Process Lasso priority High. **Don't** disable SMT on the 7600X: 8-thread runs were reported worse.
