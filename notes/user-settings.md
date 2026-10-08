# Settings YOU can choose to change (we never change them for you)

Date 2026-10-08. Nothing here was applied by us: no system, driver, BIOS, TDP, env-var or page-file change was made. These are options for you to try yourself.
Label meaning: **[M]** = measured in our own notes; **[C]** = community report (link); **[E]** = our estimate / not tested. Undo is always "put it back to what it was".
Sources are our notes unless a URL is given: `ally-gpu-budget.md`, `settings-cost.md`, `gpu-maxsettings.md`, `loading.md`, `load-memory.md`, `cpu-hotspots-ally.md`, `community-2026-10-06.md`, `ally-report-2026-10-06.md`, `frame-timing.md`.

## What matters most for the Ally 60 fps goal

1. The Ally is **GPU-bound and power-limited** [M]: GPU busy 87-98 %, socket pinned at ~26 W, GPU clock 1.4-2.2 GHz against a ~2.7 GHz max. CPU savings only help when they free watts for the GPU (W6 freed ~0.75 core and gave +37 % GPU MHz [M]).
2. Your current preset runs **~35 fps (28.7 ms GPU)**; 60 fps needs 16.7 ms, about **-42 %**. Settings alone will not get there [E]. The only measured jump toward 50 is frame generation (49 displayed, from 34 [M]), which crashes on Linux/Deck for some users [C], so treat it as risky.
3. So the order is: **power budget (TDP) and render scale first, GI level next, the rest is small.** The TDP row is the one big lever nobody has measured yet (all our Ally tests were at 25 W).

## Table 1: ROG Ally (Linux / Proton), ranked by expected benefit

| # | Setting | Where / how | Expected effect | Risk / downside | Undo |
|---|---|---|---|---|---|
| 1 | **Render scale (REN_RAT) lower** | In-game graphics. Only multiples of 5 work (65, 70...); 67 is silently ignored [M] | Biggest single lever. FSR3 cost fits 2.56 + 4.43 x r^2 ms [M]: 100 % = 7.0 ms, 50 % = 3.65 ms. Whole frame scales with pixels (100 % was 18 fps vs 34 fps at 50 % [M]) | Softer picture; FSR hides most of it at 70-85 % [E] | Set the old value back |
| 2 | **TDP higher (e.g. 28-30 W)** | Windows: Armoury Crate manual mode. Bazzite: Quick Access performance slider, or Handheld Daemon / SimpleDeckyTDP | GPU is clock-starved at 25 W [M], so more watts should lift fps roughly with GPU MHz (+37 % MHz after W6 gave a measurable gain [M]). Gain at 30 W: **not measured** [E]. Ally X community runs use 30-35 W [C: [TPU](https://www.techpowerup.com/review/nioh-3-steam-deck-rog-xbox-ally-x-performance-benchmark/7.html)] | Battery drains fast, heat and fan noise, Z1E gains little above ~30 W | Slide back to 25 W |
| 3 | **GI (global illumination) no higher than HIGH** | In-game graphics | GI HIGHEST is the only step whose cost explodes: ~+11-14 ms on the Ally vs HIGH [E, from PC RGP scaled]. Skip if already at STANDARD or lower | HIGH vs HIGHEST is nearly invisible [E] | Raise it again |
| 4 | **Cheap-look trims**: motion blur OFF/LIGHTEST (1-2 ms [E]; PC -0.35 ms [M]); post-effect resolution STANDARD when render scale < 100 (0.3-0.8 ms [E]; PC -0.39 ms [M]); volumetric STANDARD not HIGH (HIGH ~2.4x [E]) | In-game graphics | About 1-3 ms together [E] | Motion blur is taste; POSTEFFECT STANDARD can soften the image | Switch back |
| 5 | **FSR, not XeSS** | In-game upscaler | XeSS is ~1 fps slower than FSR 3.1 on the 780M [M] (33.3 vs 34.2). The game may default to XeSS on AMD Linux [C: [Proton #9473](https://github.com/ValveSoftware/Proton/issues/9473)] | None | Pick XeSS again |
| 6 | **Limiter in Steam/gamescope as well as the in-game cap** (Quick Access framerate limit at the same tier) | Steam Quick Access > Performance > Framerate Limit | In-game cap busy-waits on the CPU; one PC report says an external limiter on top cut CPU ~20 % [C: [Guru3D](https://forums.guru3d.com/threads/nioh-3.456603/page-2)]. On the Ally freed CPU watts should help the GPU [E] | Game paces cleanly only at exactly 30/60/120, and artificial-light shadows can vanish off those tiers [C]. Keep both caps on the same tier | Set limiter to Off |
| 7 | **VRR on, panel at 120 Hz (or 60 Hz if you hold 60)** | Quick Access display settings | Smoother pacing when fps wobbles; no fps change [E] | Slightly more panel power at 120 Hz [E] | Set refresh back |
| 8 | **GPU clock floor (manual GPU clock)** | Quick Access > Performance > Manual GPU Clock | Loads sit at ~800 MHz GPU for the first 25-30 s while the CPU takes the power [M, loading.md]; a floor *may* shorten loads. In play it fights the power limit, so probably no gain [E] | Higher idle power and heat; can steal watts from the CPU | Turn manual clock off |
| 9 | **More memory for the GPU (UMA 6 GB)** | Ally BIOS > Advanced, or Armoury Crate memory profile | The community 30 fps recipe uses 6 GB [C: [RPGSite](https://www.rpgsite.net/feature/19502-nioh-3-steam-deck-full-game-recommended-settings-rog-ally-ps5-performance)]. Our loads show zram swap bursts (+907 MB in 10 s [M]) and VRAM 6.3-6.7 GB at the load clear [M], so a bigger GPU share may cut stalls [E] | Less RAM for the OS and game (16 GB total); a BIOS change | Set the BIOS value back |
| 10 | **Steam launch options** (already in use): `WINEDLLOVERRIDES="dinput8=n,b" %command%` for the ASI loader; your `VKD3D_SWAPCHAIN_LATENCY_FRAMES=3`; ntsync/fsync already on | Steam > Properties > Launch Options | Needed for the mod to load [M]. Do not add `PROTON_FSR4_UPGRADE`: FSR4 targets RDNA4, the 780M is RDNA3 [E] | A typo stops the mod loading | Delete the option |
| 11 | **Proton Experimental, not GE** | Steam > Compatibility | Community: Experimental beats GE 10-29 [C: [Steam thread](https://steamcommunity.com/app/3681010/discussions/0/759554462772937119/)]. We run Experimental 11.0 [M] | Can regress on updates | Pick another version |
| 12 | **Keep the game's Frame Generation OFF on Linux** | Graphics > Frame Generation | FG gives +15 fps displayed [M] but crashes, then the game won't start until the config is deleted [C: [vkd3d-proton #2845](https://github.com/HansKristian-Work/vkd3d-proton/issues/2845)]; adds input delay [E] | See left | Delete config.xml, or turn it off in menu |
| 13 | **Do not clear the shader cache** | Steam > Shader Pre-Caching on | First compile is long (hangs near 15395/17041 reported [C]) | Disk space | n/a |

SMT off or fewer cores is **not** advised on the Ally: the game wants ~8 threads [C], and the CPU is not the limit there [M].

## Table 2: PC (Windows 11, 7600X + RX 9070, 1440p), ranked by expected benefit

PC is **CPU main-thread bound by a hair and GPU-balanced** (CPU 8.7-9.3 ms, GPU 8.7 ms, ~109 fps [M]). GPU-only settings buy nothing until the CPU is lowered.

| # | Setting | Where / how | Expected effect | Risk / downside | Undo |
|---|---|---|---|---|---|
| 1 | **GI no higher than HIGH** | In-game | HIGHEST costs +1.8 ms GPU (2.5 vs 0.7 ms [E, RGP]), enough to make the PC GPU-bound (~95 fps instead of ~109) [E] | Nearly invisible [E] | Raise it |
| 2 | **Free space on C: and fix the page file** | Settings > System > About > Advanced system settings > Performance > Advanced > Virtual memory. Keep >= 20-30 GB free on C:, or set a fixed size (e.g. 16-24 GB) on the **SSD**, never on the HDD D: | The game commits a 6 GiB arena at start and never releases it [M, load-memory.md]; a nearly full C: can starve a system-managed page file and hit commit limits (hitches or crashes) [E, general Windows behaviour]. Not measured on this PC | A fixed size can be too small; a page file on the HDD makes hitches worse | Re-tick "Automatically manage paging file size" |
| 3 | **Keep the game on C: (SSD), off the HDD D:** | Steam > Library > Storage | The game streams from a 112 GiB `root.rdb`, 95 GiB of it streamed assets [M]; an HDD cannot feed that [E]. It is already on C: | C: stays nearly full (see row 2) | n/a |
| 4 | **External limiter on top of the in-game one** (Adrenalin Frame Rate Target Control at the same 60/120) | Adrenalin > Gaming > Graphics | One report: ~20 % lower CPU [C: [Guru3D](https://forums.guru3d.com/threads/nioh-3.456603/page-2)]. Our own A/B is planned, **not yet measured** [E] | Cap must match a tier (30/60/120). RTSS/Afterburner overlays are linked to freezes and crashes [C], so prefer Adrenalin's own limiter | Turn it off |
| 5 | **Pick a cap the PC can hold** | In-game max FPS | A cap far above what you get (120 while getting 60-110) makes the CPU work harder via frame-skip catch-up [C: [Steam](https://steamcommunity.com/app/3681010/discussions/0/780947379776040327/)]. A held 60 is smoother [C] | Lower peak fps | Raise the cap |
| 6 | **VRR (FreeSync) on, in-game VSync on, cap ~3 fps below refresh** | Adrenalin > Display; Windows display settings | Tear-free with the game's tier pacing [E, frame-timing.md] | Slight latency vs VSync off | Turn off |
| 7 | **FSR 4 via the driver** (Adrenalin FSR 4 upgrade for FSR 3.1 games) or OptiScaler | Adrenalin > Gaming > Graphics, per-game profile | Better image at the same render scale, so you could drop REN_RAT a step for the same look [E]. Our tests used OptiScaler XeSS->FSR4 [M]. Whether Adrenalin lists Nioh 3 is **not confirmed** in our notes | Override may not apply; OptiScaler is a third-party DLL | Disable the override / remove the DLL |
| 8 | **Game Mode on, High performance power plan** | Settings > Gaming > Game Mode; Power options | Avoids downclock between frames [E] | Slightly more idle power | Back to Balanced |
| 9 | **Process Lasso: priority High for Nioh3.exe** | Third-party tool | Anecdotal stutter/CPU fixes on some Ryzens [C: [Steam](https://steamcommunity.com/app/3681010/discussions/0/759554831917629321/)]; others saw nothing | Another resident tool | Close/uninstall |
| 10 | **If you get DEVICE_REMOVED / HUNG**: overlays off, RAM at stock instead of EXPO (3600 -> 3200), GPU max clock 100-200 MHz lower | BIOS, Adrenalin tuning | Community crash fixes [C: [Steam](https://steamcommunity.com/app/3681010/discussions/0/759554462772968080/)]. A stability fix, not a speed-up | Slightly slower RAM/GPU | Re-enable EXPO / reset tuning |

Not recommended: **SMT off** (8-thread runs were worse, the 7600X is 6C/12T [C]); capping CPU boost (cooler but slower, for temperature only [C]). Windows update KB5124010 and RGB/fan drivers using inpoutx64.sys are reported crash causes [C: [Windows Latest](https://www.windowslatest.com/2026/10/03/microsoft-confirms-windows-11-26h2-is-crashing-some-games-and-apps-promises-to-fix-it-in-the-next-update/)].

## Top 5
- **Ally:** (1) lower render scale to 65-70, (2) try 28-30 W TDP and log it, (3) GI at HIGH or below, (4) motion blur / post / volumetric trims, (5) FSR not XeSS plus a matching external limiter.
- **PC:** (1) GI at HIGH, (2) free C: and a fixed SSD page file, (3) keep the game on the SSD, (4) external limiter at a tier you hold, (5) a cap you can hold with VRR on.

Not tested by us: the TDP gain, page-file effect, external-limiter effect, GPU clock floor, UMA size, Adrenalin FSR4 override. Treat them as experiments you run yourself; our final benchmark keeps system settings fixed.
