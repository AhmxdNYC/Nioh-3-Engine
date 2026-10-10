# Nioh3EngineFix - user guide

Engine fixes for **Nioh 3 (Steam, v2.02 only)**. Everything is applied in the game's memory while it runs; no game file is
changed except the optional shadow shader below. Close the game and the mod is gone. On any other game version the mod
checks the build and does nothing.

## What is in the zip
| file | what |
|---|---|
| `dinput8.dll` | Ultimate ASI Loader (loads `.asi` mods; the same loader Lyall's Nioh3Fix ships - keep only one copy) |
| `Nioh3EngineFix.asi` | the mod |
| `Nioh3EngineFix.ini` | settings for every device; `1` = on, `0` = off, `-1` = auto by OS (chosen at startup) |
| `optional\shader_dx12\0x0104840c.file` | faster shadow shader (GHDM), same picture (see "Optional shaders") |
| `optional\shader_dx12\0xa9910db8.file` | faster fog shader (fogpar), same picture (see "Optional shaders") |
| `README.md` | this guide; `README-MOD.md` full option reference; `SHA256SUMS.txt` checksums; `PROFILE.txt` what this device profile changes and why |

There is one zip for everything: any Windows PC, and any Linux device running the game under Proton (Steam Deck, ROG Ally on
Bazzite/SteamOS, Linux desktops, other handhelds). The mod detects Windows vs Wine/Proton at startup; the few settings whose
best value differs by OS are `-1` (auto) in the ini, and the log lists what each one resolved to. `PROFILE.txt` says what
each default is and where it was measured. Handheld-only extras (e.g. `WorkerCap = 10`) are off by default and listed there.

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
- **Online co-op:** the frame-timing fix (`[Patches] VRR`) was tested offline only. By default (`VRROnline = -1`, auto) the mod pauses it by itself while you are in an online session and brings it back afterwards, so you do not need to change anything. If you want it never active online, set `VRROnline = 0`. Online co-op itself is still untested.

## Biggest free fps boost: Resizable BAR
If your PC has a desktop graphics card and **Resizable BAR** (AMD calls it **Smart Access Memory**, SAM) is off, the game
reads a lot of its per-frame data from system RAM over PCIe instead of from the card's own memory. Turning it on is a
BIOS setting, costs nothing and is the largest single gain we measured on a mid-range PC: on an **RX 6650 XT** (Ryzen 5
5600X, 1080p HIGH) the GPU time per frame at the same spot went from **32.5 ms to 24.0 ms (-26 %, about +35 % fps)**; the
40-100x slowdowns of the meshlet culling and volumetric-cloud passes disappeared. The mod checks this once at startup and,
if it is off, shows a one-time notice (log line `RB:`; `[General] ReBarNotice = 0` turns the check off).

How to turn it on (wording differs per board maker; check your motherboard manual):
1. Update the motherboard BIOS and the graphics driver if they are old (ReBAR needs Ryzen 3000 / Intel 10th gen or newer,
   a 400/500-series AMD or 400-series Intel board or newer, and an RX 5000 / RTX 30 / Arc or newer graphics card).
2. In the BIOS: **Above 4G Decoding = Enabled**, then **Re-Size BAR Support = Enabled** (or Auto). On AMD boards it may be
   called "Smart Access Memory".
3. **CSM (Compatibility Support Module) = Disabled** (ReBAR needs UEFI boot; Windows must be installed in UEFI/GPT mode -
   check with `msinfo32` -> "BIOS Mode: UEFI" before you switch CSM off, or Windows will not boot).
4. Save, boot Windows. AMD: Adrenalin -> Performance -> Tuning -> Smart Access Memory should now say enabled. NVIDIA / Intel:
   GPU-Z -> "Resizable BAR: Enabled". The mod's log then says `RB: ... UPLOAD heaps in VRAM`.

Handhelds and APUs (ROG Ally, Steam Deck) share memory with the processor; this does not apply to them.

## Mod menu and overlay
- Open: **F10**, **Esc three times**, or **triple-tap Start** on a pad. D-pad/stick to move, Left/Right to change, A to
  toggle, B (or triple-tap Start) to close. The game ignores the pad while the menu is open.
- Every live fix can be switched on/off; rows marked `* restart` apply on the next launch. **Save to ini** keeps choices.
- Top-left overlay: fps, frame-generation fps, frame time and game speed (1.00x = real time). Turn off with
  `[Overlay] FpsCounter = 0`; startup popup with `[General] StartupPopup = 0`.
- Loads: turn on "Skip loading tip screen" in the mod menu (or `[General] AutoContinueLoad = 1`) and the "press button" tip page closes by itself when the load is done. Off by default (it only
  saves the press).

## Optional shaders (same picture, less GPU time)
Two replacement shader files are included. Both give the same picture; they do the same work with less GPU time. Measured on the ROG Ally: the shadow shader (`0x0104840c`, GHDM) ~+0.5 % at normal presets (precise re-test; older multi-launch tests showed +6.4 to +8.7 % at MAX) - fps at MAX; the fog shader (`0xa9910db8`, fogpar) a further +0.15 to +0.30 fps. On the RX 9070 PC the shadow shader gave about +3 %; the fog shader has not been measured on a PC yet. They are independent: use one or both. With the game closed:
1. In the game folder open `shader_dx12\` and **copy the original `0x0104840c.file` and `0xa9910db8.file` somewhere safe**.
2. Copy the files from `optional\shader_dx12\` over them.
3. Undo: put the originals back, or Steam -> Properties -> Installed Files -> Verify integrity.

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
Delete `dinput8.dll` (unless another mod needs it), `Nioh3EngineFix.asi`, `.ini`, `.log` (and the `Nioh3EngineFix_pj` folder
if you used PJ); on Proton also clear the launch option. If you installed the shaders, restore the originals or verify the game files in Steam.

## More
Full option reference: `README-MOD.md` (in the zip). What was measured on which device and why: the project README.
