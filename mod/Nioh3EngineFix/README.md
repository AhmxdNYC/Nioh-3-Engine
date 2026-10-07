# Nioh3EngineFix

A small native mod for **Nioh 3 (Steam, v2.02)**. It fixes CPU and power waste in the engine and makes the game
run at real speed when your frame rate is below the in-game cap. It patches the game in memory only. No game
files are changed, and nothing is left behind when the game exits.

> **Status: built, but not yet load-tested.** The logic is a 1:1 port of memory patches that were applied and
> measured live on v2.02 with external scripts. This DLL has not yet been run inside the game. Read
> `Nioh3EngineFix.log` after your first launch and report what it says.

## What it does

| Patch | Effect | Who benefits |
|---|---|---|
| **W1** | Main-thread zero-fill loop on GPU upload memory no longer does a slow read-modify-write per element (it took 11-22 % of the main thread). | everyone |
| **W2** | Vertex quad writer does one store instead of a read-modify-write on GPU upload memory. | everyone |
| **VRR** | Variable-dt frame timing. In stock Nioh 3, the whole game slows down when you run below the FPS cap (e.g. 46 fps at a 60 cap = 77 % speed). With VRR, every frame advances by the real elapsed time, so the game runs at 1.00x speed at any frame rate inside the clamp. Works with the 30, 60 and 120 caps, with or without frame generation. | handhelds, VRR displays, anyone below their cap |
| **W3** | The window-message thread sleeps (≤1 ms, wakes instantly on input) instead of spinning a core at 26-62 %. | everyone, especially handhelds (power/thermals) |
| **R1** | The game thread waits for the frame-latency signal with a timeout instead of a busy poll. | GPU-bound / capped play (handhelds) |
| **L1** | The frame limiter sleeps on a high-resolution timer until ~1 ms before the deadline, then does its original precise spin. | capped play below what your CPU can do |
| **FG** | With in-game FSR frame generation on, its presenter thread spun on the clock between the real and the generated frame. It now sleeps until ~1 ms before, then spins the rest, so pacing is unchanged. Measured: about 0.2 of a CPU core less, no fps change. | frame-gen users, handhelds (power) |
| **P12** | On CPUs with 12 or fewer threads the engine gives two worker pools (KIDS_RESOURCE, StageUpdate) 3 fewer threads each. P12 keeps them (7 → 10 and 4 → 7 on a 6-core/12-thread CPU). It has to be written at startup, before the pools are created, which the mod does from its decrypt-wait loop. | 6-core desktops, Steam Deck, ROG Ally |

On an APU (ROG Ally / Steam Deck), every watt a spinning core burns comes out of the GPU's shared power budget.
W3, R1 and L1 hand those watts back to the GPU.

## Requirements

* Nioh 3 on Steam, **version 2.02**. The mod checks the executable and **refuses to patch any other build**
  (the log says `UNKNOWN BUILD`). After a game update, wait for an updated mod.
* Windows 10/11 x64, or Linux/Steam Deck via Proton.
* An ASI/DLL loader (see below). The mod is a plain x64 DLL with no runtime dependencies (static CRT).

## Install (choose one loader)

### A) Ultimate ASI Loader (recommended; also what Lyall's Nioh3Fix uses)
1. Get Ultimate ASI Loader x64 and put its `dinput8.dll` in the game folder (the folder with `Nioh3.exe`).
   If you already use **Nioh3Fix**, this `dinput8.dll` is already there.
2. Copy `Nioh3EngineFix.asi` and `Nioh3EngineFix.ini` into the game folder (or into a `scripts\` / `plugins\`
   subfolder).

### B) Nioh Native Mod Loader (Nexus mod #45, `Nioh3ModLoader`)
1. Put its `dinput8.dll` in the game folder. Create a `mods\` folder next to `Nioh3.exe`.
2. This loader only loads files ending in **`.dll`**. Rename `Nioh3EngineFix.asi` to **`Nioh3EngineFix.dll`**.
   Put it with `Nioh3EngineFix.ini` in `mods\` (or `mods\Nioh3EngineFix\`, as in the loader's README).
   The `.ini` must keep the same base name as the DLL and sit next to it.

Both loaders are `dinput8.dll`, so use only one of them. If the mod gets loaded twice anyway, the second copy
detects the first one and does nothing.

### Steam Deck / Linux (Proton)
Add this to the game's Steam launch options so Proton loads the loader's `dinput8.dll`:

```
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

Under Proton the mod detects Wine and **skips L1** by default. Wine may accept the high-resolution timer flag
without honouring it, and then the limiter would overshoot. You can try `L1OnWine = 1` and compare frame times.
Everything else is the same as on Windows.

## Online co-op: use VRR offline only
VRR changes the size of the per-frame simulation step. Other players' games don't do that, and nobody has tested
how the online sync code reacts. **For online co-op set `VRR = 0`** in the ini. The CPU/power patches
(W1, W2, W3, R1, L1) don't change game logic and can stay on.

## Settings (`Nioh3EngineFix.ini`)
* `[Patches]` turns each patch on/off: `W1 W2 VRR W3 R1 L1` (1/0). Changes apply on the next game start.
* `[VRR]`
  * `Tier30/60/120`: which in-game FPS caps get variable timing.
  * `MinXX/MaxXX`: the measured-fps clamp per cap. Below Min the game slows down again, like stock.
  * `MaxFps`: with the in-game cap at 120, raise the limiter to 144/165/240, or 0 = unlocked. This is
    **experimental**: script timers and frame pacing above 120 have not been fully verified.
  * `MenuQuantize = 1` only if menu key-repeat misbehaves. Leave it at 0 if you use Nioh3Fix.
* `[Tuning]`: W3/R1 wait times and the L1 margin. The defaults match the tested values.
* `[General] AllowUnknownBuild = 1` makes the mod try pattern matching on other builds. Each site is still
  verified, and anything unexpected is skipped. Not recommended.

## In-game mod menu
Change the fixes and the overlay while you play, with no restart. The menu opens at any time: in gameplay, in the
game's pause menu, or on the title screen.

| | Keyboard | Controller (ROG Ally / Steam Deck / Xbox pad) |
|---|---|---|
| open / close | **F10** | **hold Start (Menu) for 5 s** (a countdown shows top-right after 1 s) |
| move | Up / Down | D-pad or left stick up / down (hold to repeat) |
| change a value | Left / Right | D-pad left / right |
| toggle / run an action | Enter | A |
| close | Esc, Backspace, F10 | B, or hold Start 5 s again |

* A short Start press still goes to the game, so its pause menu opens as usual. While the mod menu is open the game
  sees a neutral controller and no arrow, Enter, Esc or Backspace keys. This lasts until you let go of the buttons
  after closing, so B doesn't also back out of the game's menu. The game never sees F10.
* While the game's pause menu is open, a reminder shows top-right: `F10 / Hold Start 5s: Mod menu`.
* All of this is configurable in `[Menu]`: key, hold button or combo (`Back+Start`, `L3+R3`, ...), hold time, text
  size and the reminder.

What it shows:
* **Live fixes, applied at once:** VRR timing, W3 message-pump wait (and its Proton sleep mode), R1 render-wait
  timeout (0 = stock busy poll), L1 frame-limiter sleep, F1 FSR frame-gen presenter sleep, WK job wake gate.
  A fix that isn't installed this session shows `not installed` or `off (Proton)`, with the reason.
* **Overlay:** FPS counter, frame time, speed factor, counter position, counter text size.
* **Next launch:** WorkerCap, P12, and under Proton also L1/F1 on Proton. These are marked `* restart` when changed.
* **Save to ini** writes everything to `Nioh3EngineFix.ini`. Without saving, changes last until the game exits.
* A description of the selected item, and a live stats line: W3 waits/s, L1 and F1 timer sleeps/s, WK wakes kept
  and skipped per second.

Notes:
* Turning a fix off in the menu restores the stock behaviour of that code path. The patch bytes stay in place, but
  the switch they check is off.
* The game's own XInput calls are used: the mod hooks the game's `XInputGetState` import and chains to the original,
  so the Steam overlay keeps working. The keyboard is read through the game's DirectInput reads. If either hook can't
  be installed, the menu polls the device itself and the log says that the game will also see those presses.
* Hiding F10 from Windows' F10 menu mode needs W3 (on by default). With `W3 = 0`, use the controller or another `Key`.
* Sony pads that the game reads over HID (DualShock/DualSense without Steam Input) are not seen by the menu. Under
  Steam Input (Steam Deck / Ally Game Mode) every pad is an XInput pad, so they work.
* If the overlay stops drawing, the menu closes by itself within 1.5 s and the game's input passes through again.
  Any error disables only the menu and is logged (`MENU: ...` lines).

Recommended in-game settings with VRR:
* Pick the FPS cap (30/60/120) you can *mostly* hold.
* **Required: set "FPS dynamic adjustment" to OFF.** With VRR the game sees your real frame rate, and Dynamic
  FPS reacts to anything below the cap by dropping to the 60 fps mode. Measured: 72.7 fps with it on vs 102.7
  off. VRR already removes the judder Dynamic FPS exists to hide. (The live scripts force it off automatically.)
* Frame generation is fine. VRR acts on the base frame rate.

## Is it working?
Look at `Nioh3EngineFix.log` next to the mod. A good run looks like this:

```
Nioh3.exe at ...: TimeDateStamp 6AA7C106 SizeOfImage 52FB000 -> known build (v2.02)
code is decrypted (...)
resolving sites:   ... OK at RVA ...
W1: applied ... VRR: applied (20 site(s)) ... L1: applied
done: N group(s) applied, 0 skipped.
```

Any line that says `skipped` names the patch and the reason. The mod never patches a site whose bytes it doesn't
recognise, so a skipped patch leaves that part of the game exactly stock.

To uninstall, delete the `.asi`/`.dll`, `.ini` and `.log`. Nothing else was changed.

## Compatibility
* **Nioh3Fix (Lyall) 0.0.6**: none of its hook sites overlap ours. Its Dynamic-FPS hook sits 0x1C bytes before
  one of our VRR sites, and its menu-input hook is at our optional `MenuQuantize` site (so keep that off).
* **OptiScaler / ReShade**: unrelated (graphics API layer).
* The Steam DRM wrapper (SteamStub) is **not** touched. The mod only waits until it has decrypted the game code.

## Known limitations
* The VRR step uses the previous frame's interval, like most engines. A single long hitch frame is followed by
  one long step.
* Per-frame script timers in the 30 cap, and anything above 120 fps, are only partly verified.
* Menu fades are frame-counted and take a little longer below the cap (cosmetic).

---

## For developers

```
src/dllmain.cpp       loader thread, config, AOB scan, verification, near page + trampolines, frozen writes,
                      W3 hookPeek and L1Wait (C++)
src/osd.cpp / osd.h   in-game overlay (D3D12, drawn at the bottom-most Present): FPS counter, startup popup
src/menu.cpp / menu.h in-game mod menu (input hooks, items, save to ini, pause reminder); drawn through osd
src/caves.asm         VRR caves (caveFps/Scale/Lim/Tier/Menu) and the caveL1 entry (ml64), port of the Iced caves
src/patches_gen.h     GENERATED from patches.json
patches.json          single source of truth: every site's pattern, offset, original bytes, kind, replacement
tools/derive_patterns.fsx  derives the patterns from the decrypted dump (Iced; rel32/RIP-disp wildcarded)
tools/gen_header.py   patches.json -> src/patches_gen.h   (--check verifies sync)
tests/test_patterns.py  verifies every pattern against the decrypted v2.02 dump (stdlib only)
build.bat / CMakeLists.txt / .github/workflows/build.yml
```

* **Build:** run `build.bat` (MSVC: cl + ml64 + link, `/O2 /MT`), or use CMake with the MSVC generator. The output
  is `bin\Nioh3EngineFix.asi`.
* **Test:** run `py -I tests\test_patterns.py <path\to\Nioh3.dump.exe>`. Each pattern must match exactly once in
  the executable sections and land on the expected RVA with the expected original bytes. The test also checks
  values the mod reads from those bytes: the PeekMessageA IAT slot, the limiter callee, GetDelta60, and GetFps.
  It also checks what the mod menu relies on: the XInputGetState IAT slot and the engine's stop-flag code.
* **How sites are patched:**
  1. Every enabled site is located by pattern and verified: unique match, original bytes, and the expected RVA on
     the known build.
  2. A 4 KB page is allocated within ±2 GB of `Nioh3.exe`. It holds the data the game reads RIP-relatively
     (`ticks120`, `ticks60`, `r1Timeout`) and one `jmp [rip]` trampoline per cave.
  3. Each group is written while every other thread is suspended. A group is only written when no thread is
     stopped strictly inside one of its sites.
* VRR is all-or-nothing: if one of its 20 sites is missing, none of them is written, because a partial install
  would run the game at the wrong speed. R1's two sites are also written together.
* Differences from the scripts:
  * W3 and L1's timer logic are C++ instead of hand-assembled bytes.
  * The timer is created up front. If that fails, L1 is skipped.
  * The optional P7 cave now calls GetDelta60 with proper Win64 stack alignment and home space. The script's
    version lacked both, but it was never applied.
  * Before the first frame, the tier defaults to 60. The scripts read it from the live game. P1 overwrites it on
    the first frame either way.
