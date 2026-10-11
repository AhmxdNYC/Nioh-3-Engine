# Nioh3EngineFix 1.7.61

A small native mod for **Nioh 3 (Steam, version 2.02 only)**. It removes wasted CPU work in the engine. It also keeps the game at real speed when your frame rate is below the in-game cap.

It patches the game in memory only. Close the game and the mod is gone. The one file it can change: if you change frame generation in the mod menu and press **Save to ini**, it also writes that one value into the game's `config.xml` (the old file is kept as `config.xml.Nioh3EngineFix.bak`).

On any other game version the mod checks the build and does nothing.

Performance numbers are not in this file. See [`../docs/BENCHMARKS.md`](../docs/BENCHMARKS.md).

## What is on by default

Each line is one fix. The names in brackets are the keys in `Nioh3EngineFix.ini`.

- **W1, W2:** Two engine loops wrote to GPU upload memory the slow way. They now write it the fast way. Helps the main thread on any PC.
- **VRR:** Variable frame timing. In stock Nioh 3 the whole game slows down when you run below your FPS cap. With this fix the game runs at real speed at any frame rate inside the limits set in the `[VRR]` section.
- **W3:** The window-message thread sleeps instead of spinning a CPU core.
- **R1:** The game thread waits for the frame-latency signal with a timeout instead of a busy loop.
- **L1:** The frame limiter sleeps on a high-resolution timer, then spins only for the last millisecond. Needs Windows 10 1803 or newer. It turns itself off where that timer is missing. Under Proton it is off unless you set `L1OnWine = 1`.
- **WK:** The job system does not wake a worker when no queue has work.
- **FG:** With in-game FSR frame generation on, its presenter thread sleeps instead of spinning. No effect with frame generation off. Under Proton it is off unless you set `FGOnWine = 1`.
- **WL:** The game never closed the swapchain's frame-latency handle, so it leaked one per frame on Windows. This closes it.
- **CBN, CBT:** Skip constant-buffer uploads whose bytes did not change since the last upload in the same command list. The GPU receives the same bytes. (Default since 1.7.60.)
- **W6, GF, MQ, EP, SR** are set to `-1` (auto). Auto means on under Proton and off on native Windows. The log says what each one chose.
  - W6, GF: wait for the GPU with a real sleep instead of a spin.
  - MQ: skeleton matrix maths in SSE.
  - EP, SR: faster file reads while loading. Proton only.

## Opt-in options worth knowing

All of these are off by default. Change them in the ini, or live in the mod menu.

- **Extra performance** (`[ExtraPerf]`, 1.7.59). These **change the picture** to gain speed: cloud steps, fog grid, GI resolution, shadow distance, shadow filter, LOD, terrain, particles, ambient occlusion, motion blur and bloom. `Preset = 1` is Performance, `Preset = 2` is Potato. With `Preset = 0` and every key at `0`, the game's own values are used.
- **Skip loading tip screen** (`AutoContinueLoad = 1`). The "press a button" page closes itself when the load finishes.
- **Frame overlap** (`[GpuOpt] FrameOverlap`). Experimental. It is built into the ASI but off by default; leave it at `0`. It has known safety problems and is not ready for use.
- **CBS, CBF** (constant-upload variants). Off. CBF is test-only.
- **WorkerCap = 10.** Handheld-only worker-pool cap.
- **VRROnline.** `-1` (default) pauses VRR by itself in online sessions. `0` turns the frame-timing fix off everywhere, offline too.
- Profiling and debug sections (`[Profiler]`, `[Debug]`, `[GpuOpt]` probes) are for modders. Leave them alone.

The ini documents every key next to it.

## Install - Windows

1. Steam: right-click Nioh 3, Manage, Browse local files. This opens the folder with `Nioh3.exe`.
2. Copy `dinput8.dll`, `Nioh3EngineFix.asi` and `Nioh3EngineFix.ini` into that folder.
3. Start the game. After about 3 seconds of rendering, a popup says the mod loaded and how many fixes are active.

`dinput8.dll` is the Ultimate ASI Loader. If another mod already put one there, keep only one copy.

Windows Smart App Control may block the unsigned DLL. If you see no popup and no `Nioh3EngineFix.log`, that is the likely cause.

## Install - Linux, Steam Deck, ROG Ally (Proton)

1. In Desktop Mode: Steam, Nioh 3, Manage, Browse local files. Copy the same three files next to `Nioh3.exe`.
2. Steam, Nioh 3, Properties, Launch options:

```
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

Without this line, Proton ignores the loader and the mod never runs.

3. Keep in-game frame generation **off** under Proton. Switching it is reported to crash there.

## In-game settings

- Set **FPS dynamic adjustment** to **OFF**. The frame-timing fix needs it. The mod warns you when it is on.
- Pick an FPS cap you can mostly hold.

## Mod menu and overlay

- Open the menu with **F10**, **Esc three times**, or **Start three times** on a controller.
- Move with the D-pad or stick. Left/Right changes a value. A toggles. B (or Start three times) closes.
- Most fixes switch live. Rows marked `* restart` apply on the next launch. **Save to ini** keeps your choices.
- The overlay (top left) shows fps, frame time and game speed. Turn it off with `[Overlay] FpsCounter = 0`. Turn off the startup popup with `[General] StartupPopup = 0`.

## Optional shaders (not included)

The private builds also used two replacement shader files (shadow and fog). They are modified copies of the game's own files, so they are not redistributed here. Their measured gain was small (see the benchmarks).

## Resizable BAR

On a desktop graphics card with Resizable BAR (Smart Access Memory on AMD) switched off, the game reads per-frame data from system RAM. Turning it on in the BIOS is free. The mod shows a one-time notice if it is off (`[General] ReBarNotice = 0` hides it). Needs UEFI boot, so check `msinfo32` before disabling CSM. Handhelds and APUs are not affected.

## Is it working?

`Nioh3EngineFix.log` appears next to the mod after the first launch. Each fix is listed as `applied` or `SKIPPED`, with the reason. Include this log in any bug report.

## Uninstall

Delete `Nioh3EngineFix.asi`, `Nioh3EngineFix.ini`, `Nioh3EngineFix.log`, `Nioh3EngineFix.rebarnotice` (if present), and `dinput8.dll` (unless another mod needs it). Delete the `Nioh3EngineFix_pj` folder if it exists. On Proton, clear the launch option.

## Tested hardware

- **Windows 11, Ryzen 5 7600X + RX 9070:** played in game. Version 1.7.61 passed a short PC smoke test.
- **ROG Ally X, Bazzite + Proton:** played in game on earlier versions.
- **pc2 (Ryzen 5 5600X + RX 6650 XT) and the Ally with 1.7.61:** smoke tests are **still pending**. Treat 1.7.61 as untested there.
- **NVIDIA, Intel Arc, other AMD cards, Intel CPUs:** **untested.** A code review found no vendor-specific code in the default features. That is not a test.
- **Online co-op:** **untested.** VRR pauses itself online as a precaution.
- DLSS and XeSS frame generation: untested with the overlay. Its "FG FPS" label may be wrong with them.

## Known issues

- "Frame overlap" is experimental and off by default. Do not turn it on.
- Under Proton, frame generation can crash the game. Keep it off.
- Slow motion or judder below your cap usually means "FPS dynamic adjustment" is still on.
- A row marked `REFUSED` needs another fix switched on first.
- If a crash follows a change, set the last key you changed back to `0`. `[General] Enabled = 0` turns the whole mod off.

## Build from source

Source is in `src\`. You need Windows, **MSVC** (Visual Studio or Build Tools with the x64 C++ tools), the Windows SDK, and Python (optional, to regenerate `patches_gen.h`).

- `build.bat` finds MSVC, assembles the `.asm` files with `ml64`, compiles with `cl /O2 /MT`, and links. Output: `bin\Nioh3EngineFix.asi`. The static CRT means no Visual C++ redistributable is needed.
- Or use CMake with a Visual Studio (MSVC) generator. `CMakeLists.txt` needs the `ASM_MASM` language. Same output name.
- Test: `py -I tests\test_patterns.py <path\to\Nioh3.dump.exe>`. Every pattern must match exactly once, at the expected address, with the expected original bytes. You must supply the game executable yourself.

Patch sites live in `patches.json` (the single source of truth). Every site is checked byte for byte at runtime and skipped if it does not match.
