# Nioh 3 Engine

Open-source engine fixes, a boss rematch mod, and reverse-engineering research for **Nioh 3 on PC (Steam, game version 2.02)**.
Not affiliated with Koei Tecmo or Team Ninja. No game files are included in this repository or its downloads.

| folder | what it is | status |
|---|---|---|
| [`engine-fix/`](engine-fix/) | **Nioh3EngineFix**: a native mod (ASI) that fixes engine problems in memory | 1.7.61, tested on AMD hardware |
| [`boss-rematch/`](boss-rematch/) | **Nioh3BossRematch**: a separate add-on to refight bosses you have beaten | 1.3.3 tested, 1.4.0 beta |
| [`symbols/`](symbols/) | the **symbol database**: about 16,000 named functions, globals and structs in `Nioh3.exe` v2.02, with x64dbg and IDA exports | ongoing |
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

## Reverse-engineering progress

About 6 % of the game's 211k functions have names (12,679 in `symbols.tsv`), but those cover essentially all of the
CPU time we profiled, and every GPU pass we captured maps to named engine code. The goal is understanding performance, not a full source rebuild.

![Performance-relevant decompilation progress](docs/progress/performance.svg)

More charts: [docs/progress/](docs/progress/). Subsystem write-ups: [docs/subsystems/](docs/subsystems/).
Function-region map: [docs/code-map.md](docs/code-map.md).

## Help decompile the game

Anyone can add names to the symbol database: no coding needed, just a disassembler and your own copy of the game.
Setup, priorities and how to send a pull request: [CONTRIBUTING.md](CONTRIBUTING.md).

## Building

Both mods build with MSVC (x64 C++ tools) and the Windows SDK: run `build.bat` in the mod's folder. Tests that check
byte patterns need your own copy of the game executable; nothing from the game is shipped here. See each mod's README.

## License and credits

Code and documentation in this repository: [MIT](LICENSE). The overlay font in `engine-fix/src/osd_font.h` is derived
from Inconsolata (SIL Open Font License 1.1). The release zips include the Ultimate ASI Loader by ThirteenAG (MIT).
Nioh 3 is a trademark of Koei Tecmo; this project uses no game assets.

This work builds on many community mods, tools and write-ups: [CREDITS.md](CREDITS.md).
