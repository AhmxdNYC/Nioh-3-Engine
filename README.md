# Nioh 3 Engine

Open-source engine fixes, a boss rematch mod, and reverse-engineering research for **Nioh 3 on PC (Steam, game version 2.02)**.
Not affiliated with Koei Tecmo or Team Ninja. No game files are included in this repository or its downloads.

| folder | what it is | status |
|---|---|---|
| [`engine-fix/`](engine-fix/) | **Nioh3EngineFix**: a native mod (ASI) that fixes engine problems in memory | 1.7.61, tested on AMD hardware |
| [`boss-rematch/`](boss-rematch/) | **Nioh3BossRematch**: a separate add-on to refight bosses you have beaten | 1.3.3 tested, 1.4.0 beta |
| [`symbols/`](symbols/) | the **symbol database**: about 16,000 named functions, globals and structs in `Nioh3.exe` v2.02, with x64dbg and IDA exports | ongoing |
| [`research/`](research/) | 46 reverse-engineering notes: the renderer (all 11 packages), frame timing, loading, jobs, profiles | ongoing |
| [`tools/progress/`](tools/progress/) | the scripts that build every chart below and the code map, from your own copy of the game | |
| [`docs/`](docs/) | [benchmarks](docs/BENCHMARKS.md), engine subsystem write-ups, code map, decompilation progress charts | |

## Download

Get the zips from the [**Releases**](../../releases) page:

- `Nioh3EngineFix-1.7.61.zip`: the engine fix with the ASI loader. Install steps: [engine-fix/README.md](engine-fix/README.md).
- `Nioh3BossRematch-1.3.3.zip`: the rematch add-on, **tested** (33/33 in-game checks). Back up your saves first: [boss-rematch/README.md](boss-rematch/README.md).
- `Nioh3BossRematch-1.4.0-beta.zip`: adds spawning straight into the boss arena. **Not fully tested yet.**
- `nioh3-symbols-v2.02.zip`: the symbol database and its x64dbg / IDA scripts: [symbols/README.md](symbols/README.md).

Each mod works on its own. They also work together and share one `dinput8.dll` loader.

## What the engine fix actually does

Honest summary. Every number is in [docs/BENCHMARKS.md](docs/BENCHMARKS.md) with its evidence.

- **Fixes slow motion below your fps cap.** Stock Nioh 3 runs the whole game slower when it cannot hold its cap
  (0.83-0.90x speed, which also causes the camera judder people report). With the mod the game runs at 1.00x speed
  down to a floor: 48 fps at the 120 cap, 24 at the 60 cap, 20 at the 30 cap (adjustable in the ini). Below the floor
  it slows down like stock. It pauses itself in online sessions.
- **Much faster loading under Proton (Linux).** A save load on the ROG Ally X went from about 96 s to about 38 s (measured across two builds).
  Only the Ally was measured; other Proton devices (such as the Steam Deck) are expected to gain too, but untested.
  Windows loading is unchanged.
- **Less wasted CPU.** Several engine threads spin a CPU core while they wait; the mod makes them sleep.
  This frees CPU time and power, which mostly matters on handhelds and slower CPUs.
- **Raw fps: small at best on a strong desktop.** With the picture unchanged, our Windows test PC (RX 9070, 1440p,
  GPU-limited) measured between no change and about +2.5 %, mostly within test noise. Do not install it for an fps
  jump there.
- **Optional Extra Performance presets** trade a little image quality for fps: about +3 % (Performance) and
  +10 % (Potato) on our test PC and the ROG Ally. Off by default.

The development notes once claimed "+74 %" and "+49 %". Those gains came from the test PC's setup (a 60 Hz desktop,
settings, and the in-game "FPS dynamic adjustment" option), not from the mod. Details: the "Numbers we no longer use"
section of the benchmarks.

### The free wins you can do yourself

- Turn **FPS dynamic adjustment** OFF in the game's settings. It can lock the game into its 60 fps mode.
- Make sure your Windows desktop runs at your monitor's full refresh rate. The 120 fps option is hidden below 118 Hz.
- Turn on **Resizable BAR / Smart Access Memory** in your BIOS if your GPU supports it.

## Tested hardware

Everything was measured on AMD hardware: a Ryzen 5 7600X + Radeon RX 9070 PC (Windows 11), a Ryzen 5 5600X +
RX 6650 XT PC, and an ASUS ROG Ally X (Bazzite / Proton). **NVIDIA, Intel Arc and Intel CPUs have never been tested.**
A code review found nothing vendor-specific in the default fixes, so they are expected to work there, but that is not
a test. Reports from other hardware are welcome; please attach `Nioh3EngineFix.log`.

## Decompilation progress

![functions named](https://img.shields.io/badge/functions%20named-6.00%25%20(12679/211206)-orange) ![perf CPU time named](https://img.shields.io/badge/perf%20CPU%20time%20named-100%25-brightgreen) ![GPU passes mapped](https://img.shields.io/badge/GPU%20passes%20mapped-59/59-green) ![renderer understood](docs/progress/renderer_badge.svg)

**In plain words:** 12,679 of the game's 211,206 functions (6 %) have real names so far. Those names cover all of the
CPU time we measured in our profiles, and every GPU pass we captured (59 of 59) points at the engine code that runs it.
The full-game chart stays mostly grey on purpose: we name code that matters for performance first, not the whole game.
This is understanding the engine, not a full source rebuild. Charts as of 2026-10-11, regenerated from `symbols.tsv`.

### The whole game

Every function in `Nioh3.exe`, grouped by what the code is for ([docs/code-map.md](docs/code-map.md)). Block size =
code size; green = has a real name, grey = not named yet.

![Full game decompilation progress](docs/progress/full_game.svg)

### The code that costs CPU time

Only the functions that show up in our CPU profiles (ROG Ally + PC, self time). Block size = share of profiled time.

![Performance-relevant decompilation progress](docs/progress/performance.svg)

### GPU time

One GPU capture on the ROG Ally, the 118 most expensive shader buckets (20.7 ms of a 30.1 ms frame). Area = time per
frame. Green = shader/pass identified; blue = a faster replacement we kept in private tests (not distributed); red = a
rewrite we tried and dropped. These are ranking numbers from a profiler, not isolated costs.

![GPU time identified](docs/progress/gpu.svg)

### The rendering engine

The renderer on its own: 54,295 functions in 11 packages. **20.7 % of its code (by bytes) is understood.**

![Rendering engine decompilation: % understood by package](docs/progress/renderer_understood.svg)

| Package | Functions | Code | Understood (bytes) |
|---|--:|--:|--:|
| 01 backend (D3D12) | 4,931 | 1,494 KB | 12.1 % |
| 02 materials | 6,694 | 1,575 KB | 9.4 % |
| 03 lighting | 4,932 | 1,002 KB | 26.4 % |
| 04 post-processing | 7,479 | 1,683 KB | 11.6 % |
| 05 frame graph | 212 | 181 KB | 60.4 % |
| 06 resources | 235 | 61 KB | 78.2 % |
| 07 draw lists | 120 | 109 KB | 97.2 % |
| 08 meshlet / geometry | 11,443 | 1,792 KB | 8.5 % |
| 09 volumetrics | 7,041 | 2,830 KB | 42.9 % |
| 10 effects | 5,613 | 1,312 KB | 11.3 % |
| 11 UI | 5,595 | 944 KB | 12.9 % |
| **Total** | **54,295** | **12,983 KB** | **20.7 %** |

Core functions per package (bar = named; the thin bar = nearby functions only guessed to be renderer code):

![Renderer decompilation progress](docs/progress/renderer.svg)

Per-subsystem write-ups (what it does, data structures, hot addresses, threads, costs):
[docs/subsystems/](docs/subsystems/).

## Currently working on

As of 2026-10-11. Nothing here is in the downloads until it is tested.

- **Confirming 1.7.61 on more hardware.** Smoke tests on the second PC and the ROG Ally are queued; results pending.
- **Frame overlap** (recording the next frame's GPU commands while the current one is still being replayed).
  An experimental build is not safe yet: an offline audit reproduces queue-overflow and stream-boundary bugs, which
  are being fixed one at a time. Its only big number so far (+11.7 %) came from an emulator, so there is no real gain
  to claim yet.
- **Small renderer CPU wins** on the replay thread, each proven byte-for-byte and A/B tested before it ships.
- **Boss Rematch:** full per-boss validation of the catalogue, and testing of the 1.4.0 in-arena spawn.
- **Training mode** (switching or summoning bosses inside an arena): researched and planned only; no code yet.
- **Decompilation:** the least understood renderer packages (meshlet, materials, effects, post-processing).
  You can help: [CONTRIBUTING.md](CONTRIBUTING.md).

## Help decompile the game

Anyone can add names to the symbol database: no coding needed, just a disassembler and your own copy of the game.
Everything we know is public: the names ([`symbols/`](symbols/)), the reasoning behind them ([`research/`](research/)),
and the tools that draw the progress charts ([`tools/progress/`](tools/progress/)), so you can pick up exactly where we are.
Setup, priorities and how to send a pull request: [CONTRIBUTING.md](CONTRIBUTING.md).

## Building

Both mods build with MSVC (x64 C++ tools) and the Windows SDK: run `build.bat` in the mod's folder. Tests that check
byte patterns need your own copy of the game executable; nothing from the game is shipped here. See each mod's README.

## License and credits

Code and documentation in this repository: [MIT](LICENSE). The overlay font in `engine-fix/src/osd_font.h` is derived
from Inconsolata (SIL Open Font License 1.1). The release zips include the Ultimate ASI Loader by ThirteenAG (MIT).
Nioh 3 is a trademark of Koei Tecmo; this project uses no game assets.

This work builds on many community mods, tools and write-ups: [CREDITS.md](CREDITS.md).
