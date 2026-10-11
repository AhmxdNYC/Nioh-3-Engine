# Credits and sources
Every mod, tool, article and thread this work used, and what it was used for.

## Nioh 3 mods and tools (Nexus / GitHub / Codeberg)
- **[Nioh3Fix](https://www.nexusmods.com/nioh3/mods/14)** by Lyall ([source on Codeberg](https://codeberg.org/Lyall/Nioh3Fix)): its 23 byte patterns gave us verified entry points into the engine (LOD, framerate, resolution); its Dynamic FPS hook pointed us to the frame-timing code.
- **[Optimization modification and FG x4 unlock](https://www.nexusmods.com/nioh3/mods/36)**: three config.xml presets; showed which settings keys the engine reads.
- **[Reduce Effects](https://www.nexusmods.com/nioh3/mods/10)**: ReShade + ShaderToggler groups; shader hashes for fog, grass, rain, afterimages and other effects.
- **[Loose File Loader](https://www.nexusmods.com/nioh3/mods/90)** by Kassent: loads modded files from `<Nioh3>/mods` without repacking; studied for the game-data workflow.
- **[Nioh3FileLoaderLogger](https://www.nexusmods.com/nioh3/mods/62)**: logs which files the game loads; studied to find render data files.
- **[Nioh 3 DLL Loader (Nioh3ModLoader)](https://www.nexusmods.com/nioh3/mods/45)**: dinput8 DLL mod loader; the standard way to load native mods, including on Proton.
- **[Nioh 3 Mod Manager](https://www.nexusmods.com/nioh3/mods/84)**: noted as the community mod-install route.
- **[Cheat Engine table](https://www.nexusmods.com/nioh3/mods/55)**: checked; has a VSync toggle and speedhack, no real fps unlock.
- **[RDBExplorer](https://github.com/MrIkso/RDBExplorer)** by MrIkso: reads and repacks Nioh 3 archives (.rdb/.fdata); used to look for tunable render data.
- **[OptiScaler](https://github.com/optiscaler/OptiScaler)** and its **[Nioh 3 wiki page](https://github.com/optiscaler/OptiScaler/wiki/Nioh-3)**: FSR4 through XeSS inputs; the wiki gave the AMD-safe settings (DLSS inputs crash, XeFG needs `OverrideVsync`). [Issue #976](https://github.com/optiscaler/OptiScaler/issues/976) covers related reports.
- **[SCSKiller](https://github.com/BlueHeisenberg/SCSKiller)** by BlueHeisenberg: shader pre-compiler; evaluated and not needed, since the game compiles all shaders at boot ([issue #66](https://github.com/BlueHeisenberg/SCSKiller/issues/66): engine not supported yet).
- **[universal-modder](https://github.com/rehan-remade/universal-modder)** by rehan-remade: AI modding workflow and safety rules (backups, anti-cheat checks), plus the camera-interpolation precedent from its Assassin's Creed notes.

## Same-engine games (Team Ninja / Koei Tecmo)
- **[Nioh1Fix](https://github.com/RobertTausig/Nioh1Fix)** by RobertTausig / Quabai ([Nexus](https://www.nexusmods.com/nioh/mods/38), [research notes](https://github.com/RobertTausig/Nioh1Fix/blob/main/docs/research.md)): the only real fps unlock with a game-speed fix in this engine family; the blueprint and checklist for our real-time frame step.
- **[Wo Long mod #18](https://www.nexusmods.com/wolongfallendynasty/mods/18)** and **[#22](https://www.nexusmods.com/wolongfallendynasty/mods/22)**: Wo Long framerate and cutscene-cap mods; checked for how the engine's caps were handled.
- **[ResetEra: Nioh 2 PC performance thread](https://www.resetera.com/threads/nioh-2-complete-edition-pc-performance-thread.373551/)** and **[Rise of the Ronin PC thread](https://www.resetera.com/threads/rise-of-the-ronin-pc-performance-thread.1130673/page-4)**: engine history (CPU load, frame pacing) across games.
- **[DSOGaming: Rise of the Ronin performance patch](https://www.dsogaming.com/patches/rise-of-the-ronin-april-10th-patch-released-packing-performance-optimization-improvements/)**: fixed high CPU with surround audio, and the 33+ thread crash.
- **[Wild Hearts patch notes](https://primagames.com/gaming/wild-hearts-pc-patch-update-notes)**, **[eXputer](https://exputer.com/news/games/new-wild-hearts-patch/)**, **[Wccftech workaround](https://wccftech.com/wild-hearts-simple-workaround-performance/)**: same engine; "high-end CPUs unintentionally capped" and the audio-mode CPU workaround.
- Steam discussions: **[Nioh 2](https://steamcommunity.com/app/1325200/discussions/0/3879347633288749188)**, **[Wo Long 1](https://steamcommunity.com/app/1448440/discussions/0/3782498683145437645/)**, **[2](https://steamcommunity.com/app/1448440/discussions/0/3792632416036637960/)**, **[3](https://steamcommunity.com/app/1448440/discussions/0/3818528846669544233/)**, **[Rise of the Ronin](https://steamcommunity.com/app/1340990/discussions/0/600772666094279367)**, **[Dynasty Warriors: Origins](https://steamcommunity.com/app/2384580/discussions/0/734750050411921469/)**: fps caps, CPU usage and frame-pacing reports on this engine.

## Nioh 3 performance articles and threads
- **[DSOGaming: PC performance analysis](https://www.dsogaming.com/pc-performance-analyses/nioh-3-pc-performance-analysis/)**: camera judder unless locked to 60/120; CPU core requirements.
- **[DSOGaming: first PC benchmarks / no stutters](https://www.dsogaming.com/articles/nioh-3-does-not-suffer-from-stutters-first-pc-benchmarks/)**: confirmed no shader stutter; fps-lock workaround.
- **[Wccftech: graphics tuning guide](https://wccftech.com/how-to/nioh-3-pc-graphics-tuning-guide-how-to-get-the-best-experience-on-pc/)**: per-setting cost (GI the most expensive); optimized ≈ +25% avg / +14% 1% lows.
- **[PC Optimized Settings guide](https://pcoptimizedsettings.com/nioh-3-pc-settings-performance-optimization-guide/)**: cross-check of setting costs.
- **[TechPowerUp: Steam Deck / ROG Xbox Ally X review](https://www.techpowerup.com/review/nioh-3-steam-deck-rog-xbox-ally-x-performance-benchmark/3.html)** ([page 8](https://www.techpowerup.com/review/nioh-3-steam-deck-rog-xbox-ally-x-performance-benchmark/8.html)): handheld performance baseline.
- **[GameGPU: DLSS 4.5 vs FSR 4 benchmarks](https://en.gamegpu.com/test-gpu/action-fps-tps/nioh-3-sravnitelnye-benchmarki-c-nvidia-dlss-4-5-i-amd-fsr-4)** and **[older-hardware CPU/GPU tests](https://gamegpu.com/test-gpu/rpgrolevye/nioh-3-benchmarki-videokart-i-protsessorov-na-starom-zheleze)**: GPU/CPU scaling; the 60 fps engine lock and frame-gen behavior.
- **[GameGPU: 240 fps with a 60 fps engine lock](https://en.gamegpu.com/news/igry/kak-poluchit-240-fps-v-nioh-3-pri-loke-dvizhka-v-60-kadrov)**: showed that "240 fps" claims are frame generation.
- **[Digital Foundry coverage (via Multiplayer.it)](https://multiplayer.it/notizie/nioh-3-tecnicamente-miglior-gioco-pc-team-ninja-digital-foundry-non-mancano-i-problemi.html)**: technical analysis; remaining PC issues.
- **[ResetEra: Nioh 3 PC performance thread](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/)** (pages [3](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-3)–[7](https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/page-7)): community reports of high CPU load, USB polling dips, the "99% max CPU state" trick, frame gen and VRR behavior.
- **[PCGamingWiki: Nioh 3](https://www.pcgamingwiki.com/wiki/Nioh_3)**: fps caps (30/60/120, ×2 with FG), known fixes.
- **[jonathang.me: FSR 4.1 and Nioh 3](https://blog.jonathang.me/posts/fsr-upscaling-4-1-0-and-nioh-3/)**: FSR 4.1 / OptiScaler behavior in Nioh 3.
- **[CGWorld interview (Japanese)](https://cgworld.jp/article/202605-cgw333-nioh3-b.html)**: engine internals: meshlet LOD ("RADEN"), real-time GI ("OBORO"), virtual shadow maps.
- **[Koei Tecmo support: "The game runs slow"](https://support.koeitecmo.info/hc/en-us/articles/4405858545049--Steam-The-game-runs-slow)**: official advice (unplug unused USB input devices).
- **[Vortex Gaming post 1](https://vortexgaming.io/en/postdetail/658687)** and **[post 2](https://vortexgaming.io/en/postdetail/661018)**: community performance and settings reports.
- **[Nioh3.net PC settings guide](https://nioh3.net/guides/pc-settings/)**, **[Prima Games camera-stutter workaround](https://primagames.com/tips/nioh-3-camera-stutter-workaround-pc)**: lock-to-60/120 workaround and settings advice.
- **[vkd3d-proton issue #2845](https://github.com/HansKristian-Work/vkd3d-proton/issues/2845)**: Nioh 3 under Proton/Linux.
- **[Nexus Mods API](https://api.nexusmods.com/v2/graphql)**: used to list all 119 Nioh 3 mods when looking for existing fps unlocks.

## Tools
- **[Ghidra](https://github.com/NationalSecurityAgency/ghidra)** (NSA): disassembly and decompilation of the whole game.
- **[Iced](https://github.com/icedland/iced)**: x86 assembler/disassembler used by our external patcher.
- **[PresentMon](https://github.com/GameTechDev/PresentMon)** (Intel): the fps / frame-time / CPU / GPU numbers in the benchmarks.
- **[Radeon GPU Profiler / Developer Tool Suite](https://github.com/GPUOpen-Tools/radeon_gpu_profiler)** (AMD): the GPU frame capture.
- **[tinygrad `sqtt.py`](https://github.com/tinygrad/tinygrad/blob/master/tinygrad/renderer/amd/sqtt.py)**, AMD's open-sourced rocprof trace decoder, and Mesa's `ac_sqtt.h`: basis for our own RGP trace parser.
- **.NET 10 / F# Interactive** (Microsoft): runs the patcher under a signed host, so it works with Windows Smart App Control.
