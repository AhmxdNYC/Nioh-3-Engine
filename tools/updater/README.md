# Standalone updater

Python 3.10+ with no runtime dependencies. Offline tests: `py -m unittest discover -s tools/updater -v`.
This task does not install anything in the real game directory or modify an ASI.

## Manifest contract (schema 1)

Publish `/updates/stable.json` on the Vercel site over HTTPS. Publish immutable payloads first, then atomically publish the manifest. Return `Content-Type: application/json` and `Cache-Control: no-cache` for the manifest; immutable versioned URLs may be cached. All URLs below are placeholders, not deployed services.

```json
{
  "schema": 1,
  "version": "1.2.3",
  "news": "Improved input display and crash reporting.",
  "files": [
    {
      "name": "Nioh3EngineFix.asi",
      "sha256": "REPLACE_WITH_64_LOWERCASE_HEX_DIGITS",
      "url": "https://YOUR-SITE.vercel.app/releases/1.2.3/Nioh3EngineFix.asi"
    }
  ]
}
```

`version` is the latest bundle version, numeric major.minor.patch; compare numeric tuples in the menu. `news` is one line, maximum 240 characters. Between 1 and 9 unique files are allowed: `Nioh3{EngineFix,BossRematch,Training,CamKiProbe}.{asi,ini}` and `Nioh3 Mod Website.url`, with exact case. The website shortcut is shipped from `release/Nioh3 Mod Website.url`, opens `https://nioh3-mod-feedback.vercel.app/`, and uses the same manifest name/SHA256/HTTPS-download-URL fields as other files. Include it in the hosted manifest when publishing; it is backed up and restored by rollback. URLs must be HTTPS, including redirects; credentials and fragments are rejected. Manifest limit: 64 KiB; each payload: 128 MiB. Compute SHA256 from exact published bytes. The publisher and HTTPS endpoint are the trust root; a hash delivered by the same endpoint is integrity checking, not an independent release signature.

## Double-click update (Windows)

Run `powershell -ExecutionPolicy Bypass -File tools/updater/build-windows.ps1`.
The script creates `.venv-build`, installs pinned PyInstaller 6.16.0, and builds
`tools/updater/dist/Nioh3Updater.exe` (console enabled). Python is only needed to build.
The public 1.7.90 asset folder is `py -I tools/updater/pack_public_assets.py` (zip plus the exe as its own file). It does not publish.
Double-click that executable. It discovers Steam and additional libraries via
`libraryfolders.vdf`; missing/ambiguous installations prompt for the folder containing
`Nioh3.exe`. It waits for the game to close, downloads from the default manifest
`https://nioh3-mod-feedback.vercel.app/updates/stable.json`, verifies hashes, backs up,
updates, and waits for a key. It never closes or launches the game. Existing INIs stay intact.
Use --manifest HTTPS_URL to override the manifest, or --config PATH to read [Update] ManifestUrl. Explicit --manifest takes priority. Invalid/missing explicitly selected config fails before any update; URLs remain HTTPS-only.
File download URLs still follow the HTTPS-only manifest contract.

Rollback (use the backup ID printed after updating or the folder name under
`nioh3-update-backups`):

```powershell
.\Nioh3Updater.exe --game-dir 'C:\Games\Nioh3' --rollback BACKUP_ID
```

Scripted runs may add `--no-pause`.

## Linux / Steam Deck / Bazzite

Close Nioh 3. Keep `Nioh3Updater.exe` in the game folder. From a desktop terminal,
run this command, substituting your Steam library and selected Proton directory:

```sh
sh tools/updater/update-linux.sh --game-dir '/path/to/steamapps/common/Nioh3' --proton-dir '/path/to/steamapps/common/Proton 11.0'
```

The launcher uses that Proton installation's `files/bin/wine` (or older
`dist/bin/wine`) directly. It does not invoke `proton run`, which failed on Ally
while copying amdxcffx64.dll. It reuses the game's existing
`steamapps/compatdata/3681010/pfx`; if compatdata lives in another library, add
`--prefix '/path/to/compatdata/3681010/pfx'`. It refuses a missing prefix instead
of creating one. `PROTON_DIR` and `WINEPREFIX` are also accepted as defaults.
`timeout` must be available; path conversion is limited to 15 seconds.

For rollback, append `-- --rollback BACKUP_ID`. For unattended use, append
`-- --no-pause`. `--updater '/path/to/Nioh3Updater.exe'` selects a separate copy.
The executable verifies downloads, preserves existing settings, and writes its
normal `nioh3-update-backups` directory. Exit status is preserved. Launch the
game after the updater finishes. If your game-launch helper copies mods from
another directory, sync those files first so it does not undo the update.

Under Wine, the in-game **Open update page** action launches native `xdg-open`
for the fixed website and the converted Unix game folder, on the existing
background worker. It does not wait for either desktop application. The folder
opens without selecting the executable; Windows still selects the updater in
Explorer. A working desktop session and default browser/file manager are needed;
a headless test can check process arguments but cannot prove visible windows.
The action does not execute the updater or close the game.

Implementation follows Wine's [start /unix handling](https://github.com/wine-mirror/wine/blob/master/programs/start/start.c)
and [wine_get_unix_file_name allocation contract](https://github.com/wine-mirror/wine/blob/master/dlls/kernel32/path.c).
Offline launcher checks: `sh tools/updater/test_linux_launcher.sh` (fake Wine,
no real prefix or network). Real desktop verification remains required on Ally.

Executable end-to-end check:
`py tools/updater/test_exe_e2e.py`. This launches the actual frozen EXE with `--self-test`.
The EXE creates its own temporary fake Steam library and fake 1.7.81 manifest/payloads,
runs the double-click entry point, verifies discovery/wait/pause, installs two files,
preserves settings, checks backup/rollback, and rejects a corrupt hash. Transport and
process detection are injected **only inside this self-contained temporary fixture**;
there is no HTTP bypass or external fixture path. This proves the packaged workflow,
not live TLS/hosting. The website must be deployed separately by pc before real updates work.

### Two-step player copy

In-game pop-up:
> Update available. 1. Download Nioh3Updater.exe from the mod website.
> 2. Close Nioh 3 and double-click the updater. When it says Ready, launch the game again.

Website:
> 1. Download Nioh3Updater.exe.
> 2. Close Nioh 3, then double-click the updater. It finds your Steam installation,
> checks the download, and keeps a backup and your settings. If asked, select your
> Nioh3 game folder. When it says Ready, open the game again.


## Safety and recovery

The updater refuses if Nioh3 is running or process enumeration fails, checks again before each swap, and never starts/stops the game. Windows uses tasklist; Linux scans ps command lines for Nioh3/Proton. Existing INIs are preserved and backed up; release defaults install only if absent. All downloads are staged on the target volume and hash-verified before any installed file changes. Each file swap uses atomic replace. A UUID folder under `nioh3-update-backups` contains old files, checksums and a journal. New files are removed by rollback. Repeat the explicit rollback command after a crash; a prepared journal blocks another update. Backups are not automatically deleted.

An exclusive `.nioh3-updater.lock` prevents concurrent updater instances. After a hard kill, verify the recorded updater PID is dead before manually removing this stale lock, then roll back the prepared backup. If the game starts during installation, recovery can be deferred until it closes; never force-close it. Multi-file installation is journaled, not a filesystem-wide atomic transaction. Do not launch Steam/the game while updating. No save files are accessed. Run as the ordinary user with write permission to the installation directory.

## Mod menu integration plan (no installed code changes)

1. Release embeds its bundle version and a fixed trusted HTTPS manifest URL. On a background worker, opt-in check once per session (timeout 5 seconds, 64 KiB limit, TLS verification and HTTPS-only redirects). Cache locally for 6 hours; offline or malformed responses leave the menu usable. Use the same schema/version rules; never perform network I/O on the render thread.
2. Display `Update available: 1.2.3` plus the news line only when the remote numeric version is newer. Keep a `Check for updates` row for manual checks. Do not write downloaded code or replace loaded ASIs from the mod.
3. Selecting Update explains that the game must close. The mod passes only its validated installation directory as separately quoted arguments to the bundled updater using ShellExecute/CreateProcess (no shell command string from the manifest). Linux/Proton needs a native desktop launcher supplied by packaging, not an assumption that Wine can execute shell scripts.
4. Recommended first integration: open the local updater instructions/shortcut, let the player exit, then run it. The double-click CLI now waits for process exit; transaction functions continue to refuse if it restarts. Do not label an in-game click as an automatic installation until that launcher exists and is tested.
5. On next launch, show the compiled version. Never claim update success merely because a process was spawned. Publisher integration and real Windows/Linux installation smoke tests belong to the lead; executable packaging and temporary-copy tests are complete.

## Staged release handoff (task 40367)

`site/updates/stable.json` and `site/releases/1.7.81/` match the separately approved fake update (task 40375): byte-identical EF549f11cd, TR5774223a, BR793c10d4. No actual code upgrade is advertised here. The website shortcut is available as an extra unreferenced payload. The staging directory is also copied to `nioh3-mod-site/site`; no deployment performed. New real release selection and hidden test URL are owned by pc.


## Player test package (task 40389, override 40411)

Copy `Nioh3Updater.exe` into the COPY of the game folder first, then double-click it. No Python, configuration file or command-line arguments are needed. It prefers the game beside the EXE, falls back to Steam detection, waits for the game to close, updates from the public `/updates/stable.json`, and leaves its result visible. The public test release is 1.7.81 with four exact ASIs. Its EngineFix reports 1.7.81 so the notice clears on next launch. No saves are accessed; existing player mod INIs are preserved.

The optional `Update Nioh3 Mod.bat` and `Nioh3Updater.ini` provide an alternative configurable launcher targeting their own folder. They use the same public URL; the normal EXE workflow needs neither file. `--manifest` and `--config` remain available for future controlled tests; explicit CLI URL wins. Player config files are not downloaded or overwritten.

If an install swap fails after files changed, the existing verified backup is restored automatically. If the game starts during recovery, it is not force-closed: the prepared backup remains for explicit rollback after closing the game. Roll back with `Nioh3Updater.exe --game-dir . --rollback BACKUP_ID` from that same folder. Never launch the game during an update. pc owns deployment and the real player test; no website UI changes are part of this task.

### Release candidate settings and Linux desktop

In `Nioh3Training.ini`, put `FrameMeterStyle=1` under `[Practice]` for Ring,
or `FrameMeterStyle=0` for Row. `[Training]` does not read this setting.
On Wine/Linux the in-game action launches no external app: it shows the website URL and game folder in a non-modal 20-second notice. Open the website from the desktop after leaving the game. Windows behavior is unchanged.
The updater reads the installed EngineFix binary version and refuses a public
stable downgrade; explicit backup rollback remains available.
