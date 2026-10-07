# Nioh 3 FPS-unlock research (web survey, 2026-10-06)

Question: does any mod/tool unlock Nioh 3 (Steam, v2.00.0x) beyond 120 fps, or fix
"game speed tied to framerate"? Also: what exists for the sibling Katana-engine games.

## TL;DR

- **No true >120 fps unlock exists for Nioh 3.** I checked all 119 mods on Nexus (nioh3 domain,
  pulled through the Nexus GraphQL API), Codeberg, GitHub, PCGamingWiki and the ResetEra/Steam
  threads. Nothing removes the 120 cap with correct game speed.
- The only framerate tool is **Lyall's Nioh3Fix `[Dynamic FPS]`**. It is experimental and off by
  default. It changes the target that the in-game "FPS Dynamic Adjustment" option uses, from fixed
  tiers to any value from 30 to 500. It is **not** a dt fix: the sim still steps at the target, not
  at the measured time (see our `frame-timing.md` §0.4).
- Everything else that shows "240/480 fps" is **frame generation** (native FG x2 doubles the
  30/60/120 cap to 60/120/240; DLSS MFG x4 gives up to 480). That raises the displayed fps, not the
  simulation rate.
- **Best prior art for a real fix is Nioh1Fix (Quabai / RobertTausig, Sept 2026) for Nioh 1 on the
  same engine family.** It works on Proton. Its Windows build still has an "animations too fast"
  bug. Its method (hook the target-fps accessor, return a value from the measured frame time, then
  patch the per-render consumers one by one) is the same as our planned "variable-dt mode". Its
  research doc is worth reading in full (link below).
- Engine proof that it can work: Dynasty Warriors: Origins is a Katana-engine game that ships with
  arbitrary framerates up to 240. Nioh 3 / Rise of the Ronin / Wo Long do not.

## Nioh 3 results

### 1. Nioh3Fix (Lyall), the only fps-related tool
- Links: https://codeberg.org/Lyall/Nioh3Fix (mirror zff.dev/Lyall/Nioh3Fix, Nexus nioh3/mods/14)
- Version: **0.0.6 (2026-08-25)**, "Fixed broken pattern scans for version 2.00.02". It works on the
  current build. Issue #2 (LOD scans failing on 2.00.01) was closed as fixed in 0.0.6.
- Type: ASI plugin loaded by a **dinput8.dll proxy** (Ultimate ASI Loader + safetyhook).
  - **Proton:** supported. Add `WINEDLLOVERRIDES="dinput8=n,b" %command%` to the launch options.
- Other features: custom resolution, ultrawide/<16:9 HUD, movie crop, cutscene FOV, LOD/draw
  distance, intro skip, FG in menus.
- What the `[Dynamic FPS]` option does, from `src/dllmain.cpp` `Framerate()`:
  - Works only when the in-game "FPS Dynamic Adjustment" option is ON.
  - A mid-hook on the dynamic-FPS target pick (sig `45 89 ?? ?? 48 8B ?? ?? ?? ?? ?? 48 85 ?? 74 ??
    89 ?? ?? ?? ?? ?? 48 83 ?? ?? C3`, +0x10) sets `rax = clamp(1000 / [r8+0x74], 30, 500)`.
    `[r8+0x74]` is the averaged frame time. If `TargetFPS != 0`, that value replaces the computed
    one. So "TargetFPS = 0" means variable, and "58" means 116 fps with FG.
  - A "Menu Input" hook rescales menu repeat input by `round(60/fps*4)/4` so held inputs aren't
    dropped.
  - Setting address: sig `44 39 ?? ?? ?? ?? ?? 75 ?? 45 89 ?? ?? E9 ...` (+3, rel).
- History:
  - 0.0.2 (2026-02-06) added TargetFPS plus a **`Frameskip` toggle**. That toggle cleared ZF in a
    mid-hook at sig `F7 ?? ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? 4D 8B ?? ?? ?? ?? ?? 4D 85 ??`+7. Its ini
    comment: *"disable frameskipping ... can improve smoothness but causes slowdown if you cannot
    hit the target framerate"*. The old ini also warned of *"freezing when toggling frame
    generation"*.
  - 0.0.4 (2026-02-22) **removed the Frameskip option "due to bugs"** (commit 659aab9399).
- Known issues and limits:
  - Experimental.
  - Uses CPU-side frame time (`mon+0x74`), so it overestimates fps when the GPU is the bottleneck.
  - Game logic still advances by `1/target` per frame. If you set a target above 120 or between
    tiers and don't hold it, you get the same slow-motion/judder the vanilla tiers have.
  - Cutscene, physics and i-frame behaviour at non-tier rates are untested or undocumented.
- Raw code is reachable with `curl -A Mozilla/5.0` on `codeberg.org/Lyall/Nioh3Fix/raw/branch/main/...`
  and `/api/v1/...`. The HTML pages return anti-scraper junk to WebFetch.

### 2. Nexus nioh3/mods/36, "Optimization modification and FG x4 unlock on all GPUs"
- Author Velasquez. v0.1 (2026-02-12). 62 endorsements, 6.7k downloads.
- Not an unlock. It's a written guide: unlock FG x4 (MFG) on non-RTX50 GPUs, drop in FSR4, and
  paste config presets into `%LOCALAPPDATA%\KoeiTecmo\NIOH3`. It's frame generation only. The
  description reads as auto-generated. Low value.

### 3. Cheat tables
- **Nexus nioh3/mods/55** (monteduablo24, v0.2, 2026-02-15, game v1.03): has a "V-Sync Toggle"
  ("force V-Sync off for uncapped performance testing") and a Speedhack. Neither removes the
  120 cap or fixes dt. It's out of date for 2.00.
- **Nexus nioh3/mods/6** (darkksss1): data CE table, updated for v2.00.02. Nothing fps-related.
- Cheat Engine is Windows-only and awkward under Proton.

### 4. Nothing else
- No other Nioh 3 Nexus mod mentions fps/frame/uncap.
- GitHub "nioh3" repos: save editors, loaders (kassent/NIOH3-Plugin-Loader, Nioh3-Plugins:
  GameDumper, LooseFileLoader, UnlimitedTransmog, LivingArtifactCutsceneAlways), loggers, and
  trainer spam. Nothing fps-related.
- PCGamingWiki (via API wikitext):
  - 120 fps note: "Up to 120 FPS supported natively. Option to set cap to 30, 60 or 120 (doubled
    to 60, 120 and 240 when Frame Generation is enabled)."
  - Only Nioh3Fix is listed under Modifications.
- Community consensus (ResetEra perf thread pp.4-7, Digital Foundry via multiplayer.it, DSOGaming,
  Wccftech):
  - The game only works right at 30/60/120.
  - Below target without Dynamic Adjustment you get slow motion. With it on you get animation
    errors and judder.
  - VRR doesn't help. Shadows from artificial lights can vanish off-tier.
  - Steam threads say "no known way to remove the 120 cap".

## Sibling Katana-engine games

| Game | Tool | Author / link | Ver / date | What it does | Game-speed handling | Loader / Proton |
|---|---|---|---|---|---|---|
| **Nioh: CE** | **Nioh1Fix - Arbitrary Framerates** | Quabai on Nexus `nioh/mods/38`; source github.com/RobertTausig/Nioh1Fix | 1.9.0, 2026-09-17 (first 1.7.0, 2026-08-01) | **True unlock**: arbitrary fps (tested 45-135) with wall-clock-correct animation, camera, input and projectiles | Yes, it's the whole point (see below) | ASI via **version.dll**; **Proton is the primary target** (`WINEDLLOVERRIDES="version=n,b"`). **Windows build: "animations play too quickly", not ready.** Fix planned for 2.0.0 |
| Nioh 2 CE | Nioh2Fix (Lyall) codeberg.org/Lyall/Nioh2Fix | Lyall | 0.0.3 | Only **unlocks the 30 fps cutscene cap**. Gameplay stays at 60/120 | n/a | version.dll proxy, Proton OK |
| Nioh 2 | Nexus (644 mods) | - | - | No fps-unlock mod found | - | - |
| Wo Long | "FPS unlock for Steam ver" Nexus `wolongfallendynasty/mods/22` | Torofu | 0.1, 2024-03-16 | CE script that rewrites the fps-limit constants (`dd #120` per address) to any value | **Not handled.** Raises the cap only. Wo Long has the same fixed-step logic, so speed follows fps | Cheat Engine, Windows |
| Wo Long | "Remove 30FPS Cap from Cutscenes" Nexus `wolongfallendynasty/mods/18` | illusion0001 (github illusion0001/Windows-Game-Patches 1.0.9) | 1.0, 2023-03-12 | Removes the cutscene 30 cap | n/a | ASI + dinput8.dll |
| Wo Long | WoLongFix (Lyall) | codeberg.org/Lyall/WoLongFix | - | res/FOV/LOD/ultrawide only. **No fps option** | - | dinput8, Proton OK |
| Rise of the Ronin | RiseOfTheRoninFix (Lyall) | codeberg.org/Lyall/RiseOfTheRoninFix | - | res/FOV/FMV/aspect only. **No fps option** | - | dinput8, Proton OK |
| Rise of the Ronin | Nexus (72 mods) | - | - | Only a "Turbo" CE speed table and config presets. No unlock | - | - |
| DW: Origins | (native) | - | - | Ships with up to 240 fps on Katana. Reported as the first Katana game without the speed tie | Native | - |

There is no "RoninFix fps" mod and no "WoLongFix fps" option. Lyall never shipped a gameplay fps
unlock for any Team Ninja game. Nioh3Fix's Dynamic FPS target is the closest he got.

### Nioh1Fix method (most relevant prior art), from its `docs/research.md`
Nioh 1 v1.24.8.0. RVAs are Nioh 1 only, but the structure should carry over.

1. **Raise the cap.** Three changes:
   - Patch the 4-row frame-profile table, rows 0 and 3, from 60 to 120.
   - Disable the QPC busy-wait limiter that runs right after Present. This is the same kind of
     limiter as our §0.2.
   - Make Present use `SyncInterval=0` plus `DXGI_PRESENT_DO_NOT_WAIT`.
   - An external limiter is then **required** (driver/RTSS/MangoHud).
2. **Fix game speed.** Hook the direct target-fps accessor (about 190 call sites) and return
   `2 * measured_presentation_fps`, measured each frame with QPC from the previous interval and
   clamped to 30-2000.
   - Their experiments: returning the fixed target was too fast, returning stock 60 was about 2x
     too fast, and returning measured fps was better. 2x measured fps was the stable wall-clock
     match because Nioh 1's internal target is 120.
   - The two 30 fps profiles are left alone.
3. **Patch per-render consumers that skip the accessor:**
   - 3 motion-component deltas
   - camera rotation coefficients (scale XMM7/8, not the axes, so the lock-on switch threshold
     still works), aim camera
   - grass/bush wind, ocean, 3 cloud updates
   - SCL UI anim (Amrita gauge), overflow-text scroller
4. **Input.**
   - Gate transient pressed/released/repeat state at the original 60 Hz.
   - Cap the held-counter increments at 60 Hz so the dodge-vs-run classifier (held-counter
     compared against 2 and 30 frames) stays correct.
   - Gating the whole input pipeline broke attacks and menus, so it was rejected.
5. **Projectiles.**
   - Posture-anim steps are integer per call, so they use a per-object accumulator at 30
     steps/s.
   - The ControlScript tick (`+0x14/+0x1C` counters, the expiry predicate) is gated at 30 Hz to fix
     reach.
6. **Runtime.**
   - SteamStub decrypts code after the ASI loads, so it polls for 120 s at 250 ms.
   - Every signature is validated before the first write.
   - It watches the profile table and re-applies the patch if the game resets it.
7. **Known gaps:**
   - Fluctuating fps causes constant re-adaptation, which stutters. That's why a hard external cap
     is mandatory.
   - **Still untested:** cutscenes, physics, **i-frame timing**, focus loss, sustained uneven
     frame times.
   - Online play/anti-cheat is unknown.
   - Windows build is broken (anims too fast).

## Takeaways for our patch (`frame-timing.md` §5)
- Our "variable-dt mode" is new for Nioh 3. Nobody has published it. Nioh1Fix is proof it works on
  this engine family and a checklist of what else breaks.
- Expect the same secondary breakage once the `G+0x614` divisor is variable. Audit these:
  1. integer per-frame counters (our 7 `120/fps` timers, input held-counters / dodge-vs-run
     classifier, projectile/script ticks);
  2. camera-sensitivity coefficients vs the lock-on threshold;
  3. per-render env updates (wind/water/clouds) and UI anim/scroll.
- Measure the **presentation interval** (QPC around Present), as Nioh1Fix does. Don't use the
  CPU-only `mon+0x74` that Nioh3Fix uses, because it misreads GPU-bound frames.
- Keep a hard external cap (driver/RTSS) and use smoothed rather than raw last-interval dt. Raw
  per-frame adaptation is what makes Nioh1Fix stutter under jitter.
- Ship as an ASI next to Nioh3Fix's dinput8 loader. Nioh1Fix's version.dll proxy can coexist with
  it. Both proxies work under Proton with the matching `WINEDLLOVERRIDES`.

## Sources
- Nioh3Fix: https://codeberg.org/Lyall/Nioh3Fix (README, Nioh3Fix.ini, src/dllmain.cpp, releases,
  commit 659aab9399, issue #2); https://www.nexusmods.com/nioh3/mods/14
- Nexus nioh3 full listing via https://api.nexusmods.com/v2/graphql; mods 36, 55, 6 descriptions
- Nioh1Fix: https://github.com/RobertTausig/Nioh1Fix (README, docs/research.md, releases);
  https://www.nexusmods.com/nioh/mods/38
- Nioh2Fix / WoLongFix / RiseOfTheRoninFix: codeberg.org/Lyall/* (README + ini)
- Wo Long: https://www.nexusmods.com/wolongfallendynasty/mods/22 ,
  https://www.nexusmods.com/wolongfallendynasty/mods/18
- PCGamingWiki Nioh 3 (API wikitext): https://www.pcgamingwiki.com/wiki/Nioh_3
- ResetEra Nioh 3 PC Performance Thread pp.4,5,7:
  https://www.resetera.com/threads/nioh-3-pc-performance-thread.1419121/
- Digital Foundry summary: https://multiplayer.it/notizie/nioh-3-tecnicamente-miglior-gioco-pc-team-ninja-digital-foundry-non-mancano-i-problemi.html
- DSOGaming / Wccftech Nioh 3 PC articles (120 cap, MFG to 480, camera judder off-tier)
- Reddit could not be searched (domain blocked for the tool). The Steam guide 3661388328 returned 429.
