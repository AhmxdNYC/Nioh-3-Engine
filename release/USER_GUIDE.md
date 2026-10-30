# Nioh3EngineFix - user guide

Engine fixes for **Nioh 3 (Steam)**. Each patch turns on only when its own bytes are in that game file. Everything is applied in the game's memory while it runs; no game file is
changed except the optional shadow shader below. Close the game and the mod is gone. On any other game version the mod
checks the build and does nothing.

## What is in the zip
| file | what |
|---|---|
| `dinput8.dll` | Ultimate ASI Loader (loads `.asi` mods; the same loader Lyall's Nioh3Fix ships - keep only one copy). Do not install this mod together with Nioh3Fix: both patch the same engine code |
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
2. Close the game. Extract this ZIP into a separate download folder. With Python 3.9+ installed, run from that folder:
   `Install-Mod.cmd --game-dir "C:\path\to\Nioh3"`
   This backs up the current `%LOCALAPPDATA%\KoeiTecmo\NIOH3` tree before installing. Use `--save-root "path"` if your saves are elsewhere.
3. Start the game. After about 3 s of rendering a status popup says `Nioh3EngineFix ... loaded - N/N fixes active`.
4. Windows Smart App Control may block unsigned DLLs. If nothing shows and there is no `Nioh3EngineFix.log`, that is the
   likely cause.

## Install - ROG Ally / Steam Deck (Proton)
1. Desktop Mode -> Steam -> Nioh 3 -> Manage -> Browse local files.
2. Close the game and extract the ZIP elsewhere. With Python 3.9+ installed, run:
   `sh install-mod.sh --game-dir "/path/to/Nioh3" --proton-prefix "/path/to/steamapps/compatdata/APPID/pfx"`
   Supply the game's actual prefix, or `--save-root "/path/to/KoeiTecmo/NIOH3"`. No prefix/account is guessed.
3. Steam -> Nioh 3 -> Properties -> Launch options: `WINEDLLOVERRIDES="dinput8=n,b" %command%`
   (without it Proton ignores the loader and the mod never runs).
4. Change frame generation in the game's Options. The mod shows its status and never changes or restores the game's settings; Save to ini saves only mod choices.

### Automatic backups on installation and update

Use the same installer command for updates. Before replacing any mod file, it creates a new UTC-timestamped backup in
`NIOH3-mod-backups` beside the save root, copies every save file, verifies SHA256 hashes and rechecks that the source did not
change. The printed backup directory contains `saves/`, previous mod files and `receipt.json`. Use `--backup-root "path"` to
choose another directory outside the game/save/package folders. Existing INIs are preserved. Optional shaders are not installed.

A missing/empty save directory, running game, incomplete backup or checksum mismatch stops installation. Backups are never
reused, automatically deleted or automatically restored. If installation fails after a mod replacement, only this installer's
unchanged replacement files are rolled back; conflicting files are left alone and marked for review in the receipt. An interrupted
process, conflicting rollback, game starting mid-install or failed final receipt can leave `.nioh3-mod-install.lock` in the game
directory: inspect its receipt and finish recovery before removing that lock. Loaded mod files are not rolled back while the game is running.
For manual save recovery, close the game, preserve your current saves separately, then copy the chosen backup's `saves/` contents
to the receipt's `save_source` directory. Older backups are never silently restored over newer progress.

Downloading a ZIP cannot read or back up local saves. Automatic protection applies when this installer activates an install/update;
manual drag-and-drop, third-party mod managers, optional shader copying and already-installed releases bypass it. Keep the game
closed and avoid starting it or syncing/changing saves during installation; the checks do not suspend Steam Cloud or lock the game process.
The installer does not change Steam settings, launch the game or modify `Nioh3.exe`.

### Automatic backups on installation and update

Use the same installer command for updates. Before replacing any mod file, it creates a new UTC-timestamped backup in
`NIOH3-mod-backups` beside the save root, copies every save file, verifies SHA256 hashes and rechecks that the source did not
change. The printed backup directory contains `saves/`, previous mod files and `receipt.json`. Use `--backup-root "path"` to
choose another directory outside the game/save/package folders. Existing INIs are preserved. Optional shaders are not installed.

A missing/empty save directory, running game, incomplete backup or checksum mismatch stops installation. Backups are never
reused, automatically deleted or automatically restored. If installation fails after a mod replacement, only this installer's
unchanged replacement files are rolled back; conflicting files are left alone and marked for review in the receipt. An interrupted
process, conflicting rollback, game starting mid-install or failed final receipt can leave `.nioh3-mod-install.lock` in the game
directory: inspect its receipt and finish recovery before removing that lock. Loaded mod files are not rolled back while the game is running.
For manual save recovery, close the game, preserve your current saves separately, then copy the chosen backup's `saves/` contents
to the receipt's `save_source` directory. Older backups are never silently restored over newer progress.

Downloading a ZIP cannot read or back up local saves. Automatic protection applies when this installer activates an install/update;
manual drag-and-drop, third-party mod managers, optional shader copying and already-installed releases bypass it. Keep the game
closed and avoid starting it or syncing/changing saves during installation; the checks do not suspend Steam Cloud or lock the game process.
The installer does not change Steam settings, launch the game or modify `Nioh3.exe`.

## In-game settings that matter
- Set **"FPS dynamic adjustment" OFF** (the mod's frame-timing fix needs it; a warning shows when it is on).
- Pick an FPS cap you can mostly hold (Ally: 30 or 60; PC: 60 or 120). When the CPU can't keep the cap, stock slows
  the game down; with the mod it keeps real speed (down to a floor). When only the GPU limits, stock already runs at
  real speed and the mod changes nothing there.
- **Online co-op:** the frame-timing fix (`[Patches] VRR`) was tested offline only. By default (`VRROnline = -1`, auto) the mod pauses it by itself while you are in an online session and brings it back afterwards, so you do not need to change anything. If you want it never active online, set `VRROnline = 0`. Online co-op itself is still untested.

## Biggest free fps boost: Resizable BAR
If your PC has a desktop graphics card and **Resizable BAR** (AMD calls it **Smart Access Memory**, SAM) is off, the game
reads a lot of its per-frame data from system RAM over PCIe instead of from the card's own memory. Turning it on is a
BIOS setting, costs nothing and gave the largest GPU-time drop we measured: on an **RX 6650 XT** (Ryzen 5
5600X, 1080p HIGH) the GPU time per frame at the same spot went from **32.5 ms to 24.0 ms (-26 % GPU time)**; measured fps there rose only about +6 % because the CPU became the limit (slow-link test PC; size on a normal PC unmeasured); the
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

## Turn the mod off / back on (normal game)
- In game: Mod menu first page > `Reverse Mods`, press A twice. The next start is the normal game; saves and settings stay.
  Press it twice again (before starting the game without the mod) to put it back.
- Windows, game closed: double-click `Restore Vanilla Game.cmd` in the game folder; `Put the mod back.cmd` turns it on again.
- Linux / Steam Deck, game closed: run `sh restore-vanilla.sh` in the game folder; `sh put-mod-back.sh` turns it on again.

## Uninstall
Delete `dinput8.dll` (unless another mod needs it), `Nioh3EngineFix.asi`, `.ini`, `.log` (and the `Nioh3EngineFix_pj` folder
if you used PJ); on Proton also clear the launch option. If you installed the shaders, restore the originals or verify the game files in Steam.

## More
Full option reference: `README-MOD.md` (in the zip). What was measured on which device and why: the project README.
