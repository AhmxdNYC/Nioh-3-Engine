# Nioh3EngineFix - user guide

Engine fixes for **Nioh 3 (Steam, v2.02 only)**. Everything is applied in the game's memory while it runs; no game file is
changed except the optional shadow shader below. Close the game and the mod is gone. On any other game version the mod
checks the build and does nothing.

## What is in the zip
| file | what |
|---|---|
| `dinput8.dll` | Ultimate ASI Loader (loads `.asi` mods; the same loader Lyall's Nioh3Fix ships - keep only one copy) |
| `Nioh3EngineFix.asi` | the mod |
| `Nioh3EngineFix.ini` | settings tuned for this device (PC or handheld zip); `1` = on, `0` = off |
| `optional\shader_dx12\0x0104840c.file` | faster shadow shader, same picture (see "Optional shader") |
| `README.md` | this guide; `README-MOD.md` full option reference; `SHA256SUMS.txt` checksums; `PROFILE.txt` what this device profile changes and why |

Pick the zip for your device: **PC** (Windows desktop/laptop, usually limited by one CPU thread) or **ALLY**
(ROG Ally / Steam Deck class handhelds on Bazzite/SteamOS + Proton, limited by the GPU). The only difference is the ini.

## Install - Windows PC
1. Steam -> right-click Nioh 3 -> Manage -> Browse local files (the folder with `Nioh3.exe`).
2. Copy `dinput8.dll`, `Nioh3EngineFix.asi`, `Nioh3EngineFix.ini` into that folder.
3. Start the game. After about 3 s of rendering a status popup says `Nioh3EngineFix ... loaded - N/N fixes active`.
4. Windows Smart App Control may block unsigned DLLs. If nothing shows and there is no `Nioh3EngineFix.log`, that is the
   likely cause.

## Install - ROG Ally / Steam Deck (Proton)
1. Desktop Mode -> Steam -> Nioh 3 -> Manage -> Browse local files.
2. Copy the same three files next to `Nioh3.exe`.
3. Steam -> Nioh 3 -> Properties -> Launch options: `WINEDLLOVERRIDES="dinput8=n,b" %command%`
   (without it Proton ignores the loader and the mod never runs).
4. Leave frame generation OFF on Proton: switching it is reported to crash there. The mod keeps it off in `config.xml`.

## In-game settings that matter
- Set **"FPS dynamic adjustment" OFF** (the mod's frame-timing fix needs it; a warning shows when it is on).
- Pick an FPS cap you can mostly hold (Ally: 30 or 60; PC: 60 or 120). With the mod the game runs at real speed below
  the cap (no slow motion).
- **Online co-op:** the frame-timing fix (`[Patches] VRR`) is tested offline only. Set `VRR = 0` before playing online.

## Mod menu and overlay
- Open: **F10**, **Esc three times**, or **triple-tap Start** on a pad. D-pad/stick to move, Left/Right to change, A to
  toggle, B (or triple-tap Start) to close. The game ignores the pad while the menu is open.
- Every live fix can be switched on/off; rows marked `* restart` apply on the next launch. **Save to ini** keeps choices.
- Top-left overlay: fps, frame-generation fps, frame time and game speed (1.00x = real time). Turn off with
  `[Overlay] FpsCounter = 0`; startup popup with `[General] StartupPopup = 0`.
- Loads: the "press button" tip page closes by itself when the load is done (`[General] AutoContinueLoad = 0` to keep
  the press).

## Optional shader (same picture, less GPU time)
Measured: Ally +6.4 to +8.7 % fps at MAX, PC about +3 %. With the game closed:
1. In the game folder open `shader_dx12\` and **copy the original `0x0104840c.file` somewhere safe**.
2. Copy `optional\shader_dx12\0x0104840c.file` over it.
3. Undo: put the original back, or Steam -> Properties -> Installed Files -> Verify integrity.

## Is it working?
`Nioh3EngineFix.log` appears next to the mod after the first launch. Each fix is listed as `applied` or `SKIPPED` with the
reason. Every 30 s it prints `stats` lines. Send this log with any report.

## Problems
| symptom | do this |
|---|---|
| no popup, no log | PC: Smart App Control / antivirus blocked the DLL. Proton: launch option missing. Wrong game version (log says so) |
| crash or freeze | set the last fix you changed back to `0`, or `[General] Enabled = 0` to rule the mod out; send the log |
| slow motion or judder below the cap | "FPS dynamic adjustment" is ON in the game options |
| a fix row says `REFUSED` | it needs another fix on (the menu text says which) |
| visual glitch | turn the newest experimental row off in the menu, note which one, report it |

## Uninstall
Delete `dinput8.dll` (unless another mod needs it), `Nioh3EngineFix.asi`, `.ini`, `.log`; on Proton also clear the launch
option. If you installed the shader, restore the original or verify the game files in Steam.

## More
Full option reference: `README-MOD.md` (in the zip). What was measured on which device and why: the project README.
A zip named `-rc` is a release candidate: some defaults in `PROFILE.txt` still wait for the final device test.
