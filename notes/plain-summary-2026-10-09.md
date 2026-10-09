# Plain-language summary 2026-10-09

## 1. What the mod changes (shipped)

Think of the mod as a set of small repairs to how Nioh 3 uses your hardware. Nothing in the game files is changed; the fixes live in memory while the game runs. Each repair is a switch, and the release picks the right ones for your device.

1. Loading screens: the game made about 540,000 tiny file reads and slow wait-handles on Linux/Steam Deck/Ally. We pool and shortcut them. Ally load went 94 s to about 38 s. [EP, SR]
2. CPU waste: the game spun in busy-wait loops (like pacing a room while waiting). We make it sleep properly, so less heat and power, same fps. [W3, W6, GF, WL]
3. Game speed / smoothness: the game advanced a fixed time step per frame, so below the fps cap it ran in slow motion (0.90x). Now it runs at 1.00x. [VRR timing]
4. Stutter: first-time effects get built on the spot, which freezes a frame (a 'shader compile'). We record and pre-build them. [PJ, PJH]
5. GPU shaders (the graphics chip's small programs): same-picture faster versions of the shadow-depth pass and the volumetric fog pass. Ally +6 to +9% fps at max settings. [GHDM, fogpar]
6. Frame pacing and delay: lower queue depth and the option to present frames the fast way. [MaxFrameLatency, VrrPresent]
7. Linux / Steam Deck / ROG Ally (Proton): the OS-specific fixes switch themselves on only there. [EP, SR, W3_TimeoutMs, MQ]
8. Frame generation (fake in-between frames): fixes a crash when toggling it under Proton, and the fast-present path. [FGC, FGP]
9. Menu and settings: press F10 for a mod menu with live toggles, an overlay, and a Skip loading tip screen row. [AutoContinueLoad]
10. Safety net: one zip for every device, auto-detects Windows vs Proton, only works on game v2.02, and all else stays off by default.

Plain version: faster loads, calmer CPU, steadier pacing, and a few faster graphics programs. The bigger fps jumps come from settings, not magic.

## 2. Measured results

MEASURED (real runs). Short version: loads and delay got much better; raw fps did not move where the graphics chip is the limit.

PC, RX 9070, 1440p 240 Hz:
- Before 59 fps average, after about 103 (1% low 53 to 81). Honest note: most of that came from SETUP (monitor was at 60 Hz, wrong settings). The engine patches fixed slow motion (0.90x to 1.00x), cut stutter spikes (3-6 to 0-2 per 20-30 s) and lifted 1% lows about 10%.
- Mod vs stock, same settings: fps EQUAL. The graphics chip is about 90% busy, so CPU fixes cannot add fps there.
- Delay was worse in one modded run: the game itself sometimes sends frames in the slow way (+7 ms, 10-20% of frames never shown). The mod does not cause it; a fix is built (see message 3).

ROG Ally (Proton/Linux):
- Loading: 94-99 s down to about 38 s (event pooling then synchronous reads: 98.9 to 50.1 to 38.5 s, 0 failures).
- Shadow shader: 15.0 to 16.3 fps at max settings (+8.7%), better 1% lows. Fog shader: further +0.15 to +0.30 fps.
- CPU fixes: GPU clock goes up and CPU power drops, fps flat (GPU-bound).
- Frame generation at 34 real fps: shows 49 fps but real rate falls to about 24 and delay is worse. Only worth it at 40+ real fps.

pc2, RX 6650 XT, 1080p (mod 1.7.44 vs stock, PresentMon):
- Average fps 36.9 vs 36.8: no gain, GPU-bound.
- CPU time per frame 27.0 ms to 10.0 ms (big saving, but not the limiter).
- Display delay 79 ms to 41 ms median (95th percentile 103 to 55).
- Worse: frames over 100 ms went 39 to 94. About 80 were loading-screen frames on a cold shader cache, not gameplay; real gameplay hitches 4 vs 19. A warm-cache rerun is queued.

Tested and switched OFF (made it worse or nothing):
- Barrier batching: PC -7%.
- Frame-copy tweak B: first +15.6% could not be repeated, now off.
- Load queue cap LC: worse on Ally. Prefetch PF: neutral.
- GF/MQ on Windows: under 1%. Ally CPU bundle: dropped.
- Full fog rewrite: 0.3-0.5 ms slower on Ally; replaced by smaller fogpar.
- Lighting probe (GI) rewrite: dropped on the Ally, neutral on PC.

## 3. Built, waiting for a test

BUILT, WAITING FOR A GAME TEST. These compile and pass offline checks but have not run in a real game session yet, so treat as unproven.

- PJH, stutter fix, version 1.7.45: after one play session the game remembers its shader-pipelines, then hands them over ready next time. Should remove the freeze on first sight of an effect (boss-kill hitches were 100-450 ms). Same picture. [PJ, PJH]
- ESM1: renders one shadow map at 1024 instead of 2048 size. Should save about 0.2 ms on a strong GPU, less on mid-range. Tiny softness risk, off by default.
- VrrPresent: sends frames the fast way (no tearing wait) when FreeSync is on. Should recover about 7 ms delay and the 10-20% of frames lost when the game picks the slow mode. [VrrPresent]
- AS async slots: lets some graphics work overlap on the second queue, so the chip is idle less. Expected small gain, needs an A/B test. [AS]
- FGC / FGP (Proton frame-gen fix): guards a null texture that crashed the game when frame generation was toggled under Proton, and pauses it instead of rebuilding. The Ally passed this (#486, PASS).
- Faster loads, extra: PF prefetch (neutral so far), plus W1B/RPA and BPO CPU trims (part-bounds math). Expected to help weak CPUs only.
- Meshlet shader rewrite (MSV): proven identical offline, queued on pc2. Estimate 0.15-0.4 ms on a 6650 XT (about 1-2%).
- Boss Rematch add-on 1.0.0: separate mod, 96 bosses catalogued, not tested.
- IN PROGRESS (agents writing now): gpu-rewrite-round3 notes (meshlet and shadow rewrites). The volumetrics rewrite and game-logic audit notes do not exist yet.

Next steps: pc2 warm-cache rerun of the mod, PC run8c retest, VrrPresent A/B, and a final benchmark with you present. The release stays a release-candidate zip until those land.

## 4. Theories: why the game is so heavy for how it looks

THEORIES: why is Nioh 3 so heavy for how it looks? Each line is tagged MEASURED or THEORY.

MEASURED:
- About half of the GPU frame does NOT shrink when resolution drops (shadow maps, fog volume, culling, copies are fixed size). So turning the render slider down helps less than expected.
- On the RX 6650 XT (RDNA2) three costs stand out: meshlet culling (splitting models into small chunks and deciding what is visible) about 3.0 ms; volumetrics (fog and clouds) about 2.6 ms; memory copies and clears 1.7 ms. That is of a 23.2 ms frame. The RX 9070 does the meshlet part in 0.22 ms, so older GPUs pay a lot more.
- CPU per frame on pc2: 27 ms to 10 ms with the mod, and fps did not change. The game is GPU-bound.
- Shaders (graphics programs) are built the first time they are used, and the game waits for it. Real play had 247 such builds per session; the worst took 153 ms.
- About 17,000 shaders get rebuilt at every boot with no saved disk cache (the boot Compiling shaders screen).

THEORY:
- Tuned for the PS5: one fixed machine, so heavy work was sized for it, and other GPUs get no per-vendor tuning.
- The art style hides the cost: stylised, softly lit scenes spend a lot on lighting, fog and shadows you barely see as separate effects.
- An unpolished DX12 port: lots of redundant calls and waits (about 12-25k graphics calls per frame, plus busy-waits) that a console path does not hit.
- Next-gen checklist tech (meshlets, volumetrics, probes) in scenes that are not dense enough to need it, so the setup cost exceeds the benefit.
- Heavy asset reuse: the same models and materials repeat a lot. If so, waste may hide there. Being checked now: gpu-rewrite-round3 (in progress). Two more audits (volumetrics rewrite, game logic) are not written yet.

Take-away: the visible cost is mostly fixed overhead, not what you see on screen, which is why same-picture rewrites are the lever that is left.


