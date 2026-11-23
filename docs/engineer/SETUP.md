# Developer setup: toolchain, builds, install, logs, releases

**What this is.** An end-to-end setup guide for a new engineer on the Nioh 3 (Steam, v2.02) ASI mod project.
It covers what to install, how each of the four mods is built, where files go in the game folder, how settings
and logs work, how to put a dev build over a release and roll back, and how releases are packaged.
**Truth source.** The current source tree (release 1.8.x). The `mod/` folder in this public repository is an older
copy, so where a file or feature exists only in the current source it is marked.
**Read next.** [CONTRIBUTING.md](../../CONTRIBUTING.md) (decompiling workflow), [PUBLIC_EXCLUSIONS.md](../../PUBLIC_EXCLUSIONS.md)
(what must never be committed), [docs/code-map.md](../code-map.md), [research/README.md](../../research/README.md).

---

## 1. The four mods at a glance

All four are 64-bit DLLs renamed `.asi`. They are loaded by an ASI loader (section 3) into `Nioh3.exe`. All patching
is done in memory at run time; no game file is changed.

| Mod | Language / compiler | Source | Role |
|---|---|---|---|
| `Nioh3EngineFix.asi` | C++17, MSVC + MASM (`ml64`) | `mod/Nioh3EngineFix/` | Engine fixes and CPU/GPU optimizations, overlay, the in-game mod menu (which also hosts the other mods' pages), crash guards, crash/report upload, update notice, save-slot helper. |
| `Nioh3BossRematch.asi` | C++17, MSVC | `mod/Nioh3BossRematch/` | Boss logger, Boss Matches (rematch / "Fight now"), Practice catalogue, Spirit camera add-on, soul-core forms. Has its own overlay and an optional local pipe for tests. |
| `Nioh3Training.asi` | C (clang via `zig cc`) | `mod/Nioh3Training/` | Training engine: arena swap, summon, drills, frame meter, style meter, boss vitals, player log. Driven by a command file and by the hosted menu. |
| `Nioh3CamKiProbe.asi` | C++17, MSVC | `mod/Nioh3CamKiProbe/` | Read-only probe (camera / Ki); it ships with a self-test executable that the build runs. |

Shared code lives in `mod/shared/` (stroke-font text, `known_build.h`, the version resource
`nioh3_version.rc`) and in `mod/*.h` at the top of `mod/` (boss move tables, menu navigation, the
BossRematch menu bridge header). `mod/settings/` is the settings catalog (section 6).

`mod/shared/known_build.h` and `mod/shared/nioh3_version.rc` are.

Every mod refuses to patch an unknown build. The check is the PE timestamp `0x6AA7C106` and `SizeOfImage`
`0x52FB000` (Steam exe) or `0x52C1000` (Steam wrapper removed); see `mod/shared/known_build.h`.
Individual patches also verify their own bytes, so a mismatch skips that patch instead of crashing.

---

## 2. Toolchain

### 2.1 Required

| Tool | Needed for | Notes |
|---|---|---|
| Windows 10/11 x64 | building and running tests | Builds are Windows-native. Linux/Proton is a *runtime* target only. |
| MSVC x64 (`cl`, `link`, `ml64`, `rc`) + Windows SDK | EngineFix, BossRematch, CamKiProbe, and the C++ test harnesses | Visual Studio or Build Tools with the "VC.Tools.x86.x64" component. Everything is built Release x64, `/O2 /MT` (static CRT, so no VC++ redistributable is needed on the player's PC). |
| Python 3.10+ (`py` launcher) | generators, checks, packaging, installer, most tests | Scripts are run as `py -I script.py` (isolated mode: no `sys.path` pollution). Standard library only for everything in `release/`. |
| `ziglang` (`pip install --user ziglang`) | `Nioh3Training.asi` only | Training is C and is built with `py -m ziglang cc`. No MSVC needed for it. |
| Your own copy of the game, v2.02 | live testing, and any step that needs a decrypted dump | See CONTRIBUTING.md for making the decrypted dump from your own install. Never commit it. |

### 2.2 Optional

- A disassembler (Ghidra, IDA, x64dbg) with the names from `symbols/symbols.tsv` loaded; see CONTRIBUTING.md.
- `capstone` and `numpy` (Python) for `gen_rvas.py` when it adds new pattern entries from a dump.
- PyInstaller (pinned in `tools/updater/build-windows.ps1`) only if you build `Nioh3Updater.exe` or the setup exe.
- `gh` (GitHub CLI) for `release/full_install_zip.py check-tag` and `release/publish_github.py`.

### 2.3 How the build scripts find the compiler

Each `build.bat` tries, in order:

1. `cl` already on `PATH` (run it from a "x64 Native Tools" prompt and you are done).
2. A portable MSVC wrapper at `tools/msvc/setup_x64.bat` if you have created one (the folder is not in the repo; it is
   gitignored). Do not make it a junction or symlink: deleting a worktree can then delete the target.
3. Visual Studio / Build Tools located with `vswhere` and its `vcvars64.bat` (EngineFix and BossRematch).

The BossRematch and CamKiProbe scripts also look for a second hard-coded wrapper path inside the maintainer's
checkout; ignore it, it simply does not exist on your machine. CamKiProbe's script has no `vswhere` fallback, so
build it from a native tools prompt.

If none is found the script prints `ERROR: no MSVC toolchain found` and exits 1.

---

## 3. How the mods load and where files go

### 3.1 Game folder layout

The game folder is the one with `Nioh3.exe` (Steam: Manage, Browse local files). A full install puts everything
**next to `Nioh3.exe`**:

```
Nioh3.exe
dinput8.dll                       Ultimate ASI Loader (ThirteenAG, MIT) - not built here, not in this repo
Nioh3EngineFix.asi / .ini         (+ .log, .rebarnotice after first run)
Nioh3BossRematch.asi / .ini / _bosses.tsv
Nioh3Training.asi / .ini + Nioh3Training_action_records.tsv, _actionsets.tsv, _arenas.tsv, _bgm.tsv
Nioh3CamKiProbe.asi / .ini
Nioh3FightRecorder.ps1            only used when Nioh3Training.ini [Record] Enable = 1
Nioh3Updater.exe                  standalone updater (optional)
Restore Vanilla Game.cmd, Put the mod back.cmd, restore-vanilla.sh, put-mod-back.sh
```

The authoritative file list for the installer is `FILES` at the top of `release/install_mod.py`. `Nioh3EngineFix.asi`
also uses folders created next to itself at run time: `Nioh3CrashReports\`, `Nioh3Reports\`, and (opt-in) `Nioh3EngineFix_pj\`.

### 3.2 Loading

`dinput8.dll` is a proxy: the game (or Windows) loads it in place of the system `dinput8.dll`, and it loads every
`.asi` in the game folder. Each mod's `DllMain` (BossRematch: `mod/Nioh3BossRematch/src/main.cpp`, then `addon::Boot`;
EngineFix: `mod/Nioh3EngineFix/src/dllmain.cpp`) starts a worker and waits for the game's code to be decrypted before
scanning (Steam's DRM wrapper keeps `.text` encrypted on disk and in memory until it has run).
Ini keys `DecryptWaitSeconds` (EngineFix default 60, BossRematch default 120) control that wait.

Things worth knowing:

- Each mod finds its own folder from its own module path, so INI/TSV/log files must sit next to the `.asi`.
- EngineFix allows one instance per process (a named mutex); a second copy logs "another instance is already active".
- Every game address is resolved by byte pattern at startup, not a fixed RVA, where possible. See section 4.3.
- Under Proton/Wine the loader is enabled with the Steam launch option `WINEDLLOVERRIDES="dinput8=n,b" %command%`.
  The setup program for Linux turns it on in the prefix instead (see `tools/setup/`, *source not yet in the public copy*).
- The mod menu hides its hotkey from the game by hooking the DirectInput vtable and a `WM_KEYDOWN` filter
  (see the top comment of `mod/Nioh3EngineFix/src/menu.cpp`).

---

## 4. Building

All commands are run from the repository root (or from the mod folder; the scripts `cd` to their own directory).
Outputs go to `bin\` (EngineFix, BossRematch, CamKiProbe) or beside the script (Training). `bin\` and `build\` are not
committed.

### 4.1 Nioh3EngineFix

```
mod\Nioh3EngineFix\build.bat
```

Output: `mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi` and a copy of the ini. The script is a gate as well as a build,
and it stops at the first failing step (`|| exit /b 1`). In order, it:

1. Locates MSVC (section 2.3).
2. Regenerates `src/patches_gen.h` from `patches.json` (`tools/gen_header.py`).
3. Checks the packaged menu defaults header is current (`tools/generate_menu_defaults.py --check`; see section 6).
4. Checks `rvas.json` against `src/rvas_gen.h` (`tools/gen_rvas.py --check`, warning only) and runs
   `tools/check_fixed_rvas.py` into `build\fixed_rvas.txt` (warning only).
5. Compiles and **runs** a series of offline harnesses (`tests\*_test.cpp`, `*_harness.cpp`): HUD patterns, extra-perf
   menu, PTS start guard, VRS tests, loop breaker, and more. No game and no GPU device is needed.
   `NIOH3_HUD_DUMP` may point at a decrypted dump for the HUD pattern test.
6. Assembles the `.asm` caves with `ml64`, compiles each `.cpp` with
   `/O2 /MT /GS /Gy /Gw /EHsc /std:c++17 /W4 /permissive- /DNDEBUG /DUNICODE /D_UNICODE`, compiles the resource file with `rc`,
   and links with `/DLL /DEBUG:FULL /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT /MACHINE:X64` against `kernel32 user32 shell32 winhttp advapi32 bcrypt version`.
7. Builds the Python save helpers (`tools/saves/build_package.py`, `build_live_package.py`) into `bin\save_helpers\`.
   These are only used when `[SaveFiles] Native = 0`; the default native path needs no Python.

Notes:

- `CMakeLists.txt` also exists (it lists the sources and the same flags, and generates `patches_gen.h`) but `build.bat`
  is the supported path; the object list in `build.bat` is the one that ships. If you add a `.cpp`, add it to
  **both** `build.bat` (compile line and link line) and `CMakeLists.txt`.
- `third_party/libdeflate` is compiled as C (C11) into the ASI.
- `mod/Nioh3EngineFix/.github/workflows/build.yml` is an older CI recipe that compiles only a few files; treat it as
  unverified for the current tree.
- A version string lives in `#define MOD_VERSION` in `src/dllmain.cpp`. `release/check_release.py` compares it to the built
  ASI. The log's first line prints it with the build date.

### 4.2 Nioh3BossRematch

```
mod\Nioh3BossRematch\build.bat
```

Output: `bin\Nioh3BossRematch.asi` plus `Nioh3BossRematch.ini` and `Nioh3BossRematch_bosses.tsv` copied into `bin\`.
It compiles `main sites hooks bosslog rematch training ui remote gamerva keepgear loot_clear` with the same MSVC flags
plus `/DBR_SOULCORE=1` (soul-core forms; the release gate refuses an ASI built without it), then links. `keepgear.cpp`
and `loot_clear.cpp` are.

Before compiling it runs `tools/check_fixed_rvas.py` (warn-only) to list hard-coded game addresses that are not found
by a byte pattern. Game sites are described in `sites.json`; `tools/derive_sites.py` derives patterns and `gen_rvas.py` +
`sites_gen.h` / `rvas_gen.h` are generated headers (check them in after regenerating).

BossRematch tests are in `mod/Nioh3BossRematch/tests/` (C++ harnesses with `run_*.py` wrappers, plus Python "offline"
replays that read the source) and `mod/tests/` (the menu-bridge ABI test, `run_rematch_bridge_tests.py`).
`run_rematch_bridge_tests.py` and `tests/run_*_harness.py` look for `tools/msvc/setup_x64.bat` and call it if present;
if you build from a native tools prompt, remove or adapt the `call "...setup_x64.bat"` line locally (it is a hard-coded
path in the bridge test runner).

### 4.3 Fixed RVAs, `Grva()` and `rvas.json`

A game patch moves code and data, so a hard-coded `base + 0x1234567` is a time bomb. The project resolves addresses
by byte pattern:

- `Grva("name")`-style calls (`src/gamerva.h`/`.cpp` in each mod) look up a pattern table generated from `rvas.json`
  by `tools/gen_rvas.py` into `src/rvas_gen.h`. Regenerating needs a decrypted dump of the running game (file offset == RVA).
- `tools/check_fixed_rvas.py` lists every raw `base + RVA` that is neither pattern-resolved nor in
  `tools/fixed_rvas_allow.txt`. Build scripts only warn; treat a new raw RVA as debt.
- `tools/aob_audit.py` audits the patterns for uniqueness.

### 4.4 Nioh3Training (C, zig)

```
pip install --user ziglang
mod\Nioh3Training\build.bat        (Windows)    or    sh mod/Nioh3Training/build.sh
```

It runs `py -m ziglang cc -target x86_64-windows-gnu -shared -O2 -fms-extensions -Xclang -fasync-exceptions -Wall src/training.c -o Nioh3Training.asi -lgdi32 -lmsimg32`,
then copies `data/boss-action-record.tsv` to `Nioh3Training_action_records.tsv`. `training.c` includes the headers in
`src/`, so a change to any `src/*.h` rebuilds it.
`mod/Nioh3Training/build.bat` is; `build.sh` is there and works from Git Bash.

Two flags matter:

- `-fasync-exceptions` is mandatory. Clang's SEH is call-only by default, so a `__try` around a plain load emitted no
  handler and the mod crashed (the comment is in `build.sh`). The `__try` helpers are `noinline` so every call site keeps its guard.
- `zig cc -O2` defines `NDEBUG`, which silently removes `assert`. The C tests under `mod/Nioh3Training/tests/` build
  with `-O0 -UNDEBUG` (or `#undef NDEBUG` at the top of the test); do not trust a pass from an `-O2` build.

`plug/hate.c` is a small probe plugin that Training can load at run time with the `plug` command
(`<game>\trplug\p<N>.dll`, exporting `Cmd`); it is a developer experiment, not part of a release.
`style/` holds the generators for the style-meter sprite atlas (`gen_style_atlas.py`).

### 4.5 Nioh3CamKiProbe

```
mod\Nioh3CamKiProbe\build.bat
```

Builds `src/probe.cpp` + `src/gamerva.cpp` into `bin\Nioh3CamKiProbe.asi`, then compiles a second
`probe_test.exe` with `/DPROBE_TEST` and runs it; a failing self-test fails the build. It is read-only.
The probe is not loaded by every install type; the release gate nevertheless requires it in the package so the camera
rows have something to load.

### 4.6 Tests, in one place

| Where | What | How |
|---|---|---|
| `mod/Nioh3EngineFix/tests/` | About 70 `run_*.py` wrappers around C++ harnesses, plus `test_patterns.py` (pattern scan of a dump) | `build.bat` runs a subset. `py -I release/check_release.py --full` runs `test_patterns.py` and every `run_*_harness.py`. |
| `mod/Nioh3BossRematch/tests/`, `mod/tests/` | Harnesses and offline replays | Run the `run_*.py` wrappers or the individual `*_offline.py` / `test_*.py` files. |
| `mod/Nioh3Training/tests/` | C tests and Python tests | Python tests compile with `zig cc`; run them with `py -I`. |
| `mod/shared/tests/` | stroke-font and `known_build` tests | `run_gdi_stroke_tests.py`, `known_build_test.c`. |
| `mod/settings/test_catalog.py` | Settings catalog guard (section 6) | `py -I mod/settings/test_catalog.py` (needs `git` history). |
| `release/tests/`, `release/test_publish_github.py` | Installer, restore, PDB scrub, publish guard | `py -I -m unittest` style or run directly. |
| `tools/updater/test_*.py`, `updater_selftest.py` | Updater | `py -m unittest discover -s tools/updater -v` |
| `tools/testhub/hub.py` | Catalog of test lanes (`offline`, `native`, `dump`, `release`, `device`...) | `py -I -B tools/testhub/hub.py` prints the catalog and runs nothing; `run offline` runs the five small checks. |

Device tests (anything that starts the game) are not part of any offline suite.
Several native tests need a decrypted dump (`NIOH3_HUD_DUMP`, `test_patterns.py`) and skip or fail without one.

---

## 5. Installing a dev build over a release, and rolling back

There are four mechanisms. Pick by how much safety you need.

### 5.1 Hand copy (fastest, no backup)

Close the game. Copy the freshly built `.asi` over the installed one; leave the `.ini` alone unless you changed a
default. EngineFix keeps its log open with `_SH_DENYWR`, so the game must be fully closed; a loaded `.asi` may not be
overwritten while the game runs. Run the game, read `Nioh3EngineFix.log`.

Keep the release `.asi` files you replace (copy them to a folder outside the game folder). Rolling back is copying them back.

### 5.2 `release/install_mod.py` (the player installer)

```
Install-Mod.cmd --game-dir "<Nioh 3 folder>"                       (Windows; wraps: py -3 -I install_mod.py %*)
sh install-mod.sh --game-dir "<dir>" --proton-prefix "<pfx>"        (Linux/Proton)
```

Options: `--game-dir` (required), `--save-root`, `--proton-prefix` (the game's `compatdata/<appid>/pfx`), `--backup-root`,
`--package` (default: the folder holding the script). It expects the package files next to it. What it does, in order:

1. Refuses if the game is running, if directories overlap, or if any path is a link / reparse point.
2. Creates `NIOH3-mod-backups\<UTC stamp>-<uuid>\` beside the save root and copies every save file there, verifying
   SHA256 and re-inventorying the source to prove it did not change.
3. Copies the package payload and the **previous** mod files (`mods-before\`) into the backup and writes `receipt.json`.
4. Takes an exclusive lock file `.nioh3-mod-install.lock` in the game folder, stages each file, and swaps it in with
   an atomic replace. Existing `.ini` files are **preserved** (new defaults are installed only if the file is absent).
5. On failure it rolls back only the files it replaced and that are still unchanged; conflicts are left alone and
   marked in the receipt. An interrupted run leaves the lock; inspect the receipt before deleting it.

It never restores or deletes saves. The save root on Windows is `%LOCALAPPDATA%\KoeiTecmo\NIOH3` (on Proton you must
pass the prefix or `--save-root`; nothing is guessed). The backup-and-install behaviour is documented for players in
[release/USER_GUIDE.md](../../release/USER_GUIDE.md); do not duplicate it here.

To roll a dev build back: install the previous release package with the same command (the installer never downgrades
silently, it just installs the package you point it at), or copy files from `mods-before\` in the newest backup.

### 5.3 Turning the mod off: Restore Vanilla

| Action | How | What it does |
|---|---|---|
| Windows, game closed | `Restore Vanilla Game.cmd` (in the game folder) | Renames `dinput8.dll`, `Nioh3EngineFix.asi`, `Nioh3BossRematch.asi`, `Nioh3Training.asi`, `Nioh3CamKiProbe.asi` to `*.mod-off`. Saves and `.ini` files are untouched. Refuses if `Nioh3.exe` is running or a `.mod-off` already exists. |
| Turn back on | `Put the mod back.cmd` | Reverses the rename. |
| Linux / Proton | `restore-vanilla.sh`, `put-mod-back.sh` | Same, for a shell. |
| In game | Mod menu, first page, **Reverse Mods**, press A twice (within 5 s) | Same rename from inside the game (`reversemods` namespace in `mod/Nioh3EngineFix/src/menu.cpp`); takes effect at the next start, pressing it twice again puts the mod back. It rolls back a partial rename. |

These are the quickest way to bisect "is it the mod or the game". `release/tests/test_restore_vanilla.py` covers the scripts.

### 5.4 Standalone updater

`Nioh3Updater.exe` (`tools/updater/updater.py`, frozen with PyInstaller) downloads the files listed in a manifest,
verifies SHA256, backs up what it replaces to `nioh3-update-backups\<id>\`, journals the swap, and can roll back:
`Nioh3Updater.exe --game-dir "<dir>" --rollback <BACKUP_ID>`. Linux uses `tools/updater/update-linux.sh` with Wine from a Proton
install. Details: [tools/updater/README.md](../../tools/updater/README.md). For development you can point it at your own
manifest with `--manifest HTTPS_URL` or `--config` (`[Update] ManifestUrl`).

---

## 6. INI files and how settings flow

### 6.1 The files

| File | Owner | Sections (from the shipped ini) |
|---|---|---|
| `Nioh3EngineFix.ini` | EngineFix | `General`, `Overlay`, `Menu`, `Patches`, `ExtraPerf`, `VRR`, `Tuning`, `Latency`, `GpuOpt`, `Profiler`, `ShaderSwap`, `ShaderHot`, `Memory`, `Report`, `Debug`, `Rematch` (legacy), `Update`, `SaveFiles`, `Perf` |
| `Nioh3BossRematch.ini` | BossRematch | `General`, `Training`, `Rematch`, `Menu`, `Remote`, `Debug`, `Spirit`, `SealHits`, `SealDur`, `SealLastHit`, `SealLastHitA`, `SealHitsA`, `SoulCore` |
| `Nioh3Training.ini` | Training | `ArenaSwap`, `Summon`, `Return`, `Fade`, `Record`, `Debug`, `Training`, `Practice`, `StyleMeter`, `Report` |
| `Nioh3CamKiProbe.ini` | CamKiProbe | `CamKiProbe` |
| `Nioh3EngineFix_live.ini` | EngineFix (`[Debug] LiveCtl = 1`) | Control file for one-launch A/B tests; name set by `[Debug] LiveCtlFile`. |
| `Nioh3EngineFix_skip.ini` | EngineFix | Skip-effects list; name set by `[Debug] SkipFxFile`. |

The shipped ini files are heavily commented: every key has its meaning, default and the reason for the default.
Read the comment before changing a default. Conventions:

- `1` = on, `0` = off. EngineFix keys documented as `-1` mean **auto by OS** (Windows vs Wine/Proton), resolved
  at startup; the log line `ini keys set to -1 (auto)` lists what each one became.
- Most keys are read **once at startup** with `GetPrivateProfileIntW/StringW`; the comment says "Restart the game to apply".
- A missing key means the default compiled in. Existing player ini files are preserved by installers and the updater,
  so a changed default in the repo does **not** reach an existing install unless a migration code path is added
  (EngineFix has `LoadFixDefaultV`-style migrations in `dllmain.cpp`; see also `btnstyle_migrate.h`).
- `[General] Enabled = 0` turns a mod off while still loading it; the log says so.
- Training `[Report]` and EngineFix `[Report]` differ: EngineFix posts to a report website; Training's `Url` is a placeholder
  that just creates a local zip (per the comment in the shipped ini).

### 6.2 The settings catalog (menu <-> ini)

The mod menu rows that edit ini keys are generated from one reviewed file:

```
mod/settings/catalog.json  --generate_catalog.py-->  mod/<Mod>/src/settings_catalog.h
```

- `catalog.json` has one object per mod (`Nioh3EngineFix` ~349 settings, `Nioh3BossRematch` ~24). Each setting has `section`,
  `key`, `label`, `default`, `kind` (`Number`, `Choice`, `Manual`), `lo/hi/step`, `choices`, `mode`, `live`, `legacy`,
  `group`, `experimental`, `help`. Top-level lists: `groups` (ini sections shown as menu groups), `groupLabels` (plain-English names),
  `codeDefault` (keys whose default lives in code, not the ini) and `iniOnly` (keys deliberately not in the menu).
- `mode` maps to `setting_ui::Apply` in `mod/settings/model.h`: `NextLaunch` (startup preference, edit then Save then restart),
  `OverlayLive`, `MenuLive`, `TuningLive` (apply immediately).
- `py -I mod/settings/generate_catalog.py` rewrites the generated headers; `--check` fails if they are stale. **Edit
  `catalog.json`, never `settings_catalog.h`.**
- `mod/settings/store.h` is the runtime store: `Load` reads every key with `GetPrivateProfileStringW`; `Change` marks a
  value edited; `Save` hands only changed, non-`Manual` values to a sink that writes them with `WritePrivateProfileString`.
  `Manual` kinds (free-form paths and lists) are read-only in the menu and never rewritten.
- `mod/Nioh3EngineFix/tools/generate_menu_defaults.py` embeds the shipped ini values (plus Training `[Practice]`) in
  `src/menu_shipped_defaults.h`. It backs the menu's "reset to defaults" so a reset never copies a player's edited ini.
  `build.bat` fails with "packaged menu defaults stale" if you forget to regenerate it.
- `py -I mod/settings/test_catalog.py` is a guard test. It compares the protected parts of the menu source (the item enum, the page
  order `kOrder`, the experimental ini keys, the Remote command lookup, and the add-on provider callbacks) with the **last
  released commit** pinned in `RELEASE` at the top of the file. Rows may be *added*; removing, renaming or reordering a
  shipped row fails. Update `RELEASE` when you ship (see section 9).
- A second checker, `release/check_release.py`, lints the ini (every key read in source is documented, every ini key is read).

### 6.3 Release profiles

`release/profiles.json` holds per-device overrides applied on top of `mod/Nioh3EngineFix/Nioh3EngineFix.ini`.
`release/make_profiles.py` writes `release/profiles/Nioh3EngineFix.<NAME>.ini` (generated, gitignored; today one profile, `ALL`).
Each override has a `status`: `kept` (measured and safe), `provisional` (recommended, still being measured; blocks `--final`),
`off`. The profile text also feeds `PROFILE.txt` in the legacy EngineFix-only zip.

### 6.4 Run-time flow, short version

`ini on disk` -> read at startup by each mod -> `Store`/menu values in memory -> menu edits apply live (or are queued for
next launch) -> **Save to ini** writes only edited keys back with the Windows profile API (comments are preserved; edited
lines become `key=value`). Some rows write immediately (for example `AutoContinueLoad`, and the BossRematch and Training
rows that say "Saved."). Frame generation is status-only: the mod never changes the game's own settings.

---

## 7. Logs and files the mods write

| File | Written by | Where | Notes |
|---|---|---|---|
| `Nioh3EngineFix.log` | EngineFix | next to the `.asi` | Opened `"w"` each start (previous run is overwritten); `[General] Log = 0` disables. Each patch is `applied` or `SKIPPED` with a reason; statistics lines every 30 s. First thing to read and the first thing to attach to a bug. |
| `Nioh3BossRematch.log` | BossRematch | next to the `.asi` | More detail with `[Debug] BossLog`, `RematchLog`, `InvLog` (all default 0). |
| `Nioh3Training.log` | Training | game folder | Commands, probes (`FrameProbe`, `IFrameProbe`, `KiPressProbe`, `PostTrace` ... all default 0), `KILOG` lines. |
| `Nioh3Training.cmd` | developer, read by Training | game folder | Command file, one command per line, consumed on the game thread (see the top of `training.c`). |
| `Nioh3CamKiProbe.log` | CamKiProbe | next to the `.asi` | Append mode. |
| `Nioh3PlayerLog.txt` (+ `.old`) | Training | next to the `.asi` | `[Record] PlayerLog = 1` (button edges `PAD`, action ids `ACT`, `SET` header, `WPN`), `PlayerClock = 1` adds one `CLK` line per second (game speed vs wall clock, fps). New file per start. Off by default. EngineFix's Report attaches it. |
| `Nioh3CrashReports\pending\crash-*.json` | EngineFix `crashupload.cpp` | beside the `.asi` | Written on a crash; contains no user names or paths. Posted to `[Report] CrashUrl` unless `CrashUpload = 0`. |
| `Nioh3Reports\report-<time>.json` | EngineFix `reportsend.cpp` | beside the `.asi` | Local copy of what the menu's Report box sent: your note plus tails of the mod logs. |
| `Nioh3EngineFix.rebarnotice` | EngineFix | beside the `.asi` | Marker so the Resizable BAR notice shows once. Delete to see it again. |
| `Nioh3BossRematch_favorites.txt` | BossRematch | beside the `.asi` | Menu favourites. |
| `Nioh3EngineFix_mlp.csv`, `Nioh3EngineFix_cbvtrace.bin` | EngineFix profiling | beside the `.asi` | Only with the matching `[Profiler]` / `[Debug]` switches. |
| `Nioh3EngineFix_pj\` | EngineFix PSO journal | beside the `.asi` | Only with `[Patches]` PJ options on. |
| `Nioh3ModEvents.jsonl` | none | - | Referenced as a tail source by crash/report code but **no mod writes it** (see the comment in `crashupload.cpp`); do not rely on it. |

Named pipes (all local only, off by default) are for scripted tests:
`\\.\pipe\nioh3vpad` (EngineFix virtual pad, `vpad.cpp`) and `\\.\pipe\nioh3bossrematch` (`[Remote] Pipe = 1` in BossRematch).
`[Remote] HostedFightTest` needs `Pipe = 1` and a live EngineFix host.

Logs are plain text and may contain your Windows user name through game paths. Redact before posting; the report box sends
no names or Steam id by design, but a raw log pasted by hand is yours to scrub.

---

## 8. The in-game mod menu, basics

The menu belongs to `Nioh3EngineFix.asi` (`menu.cpp`, `menu_pages.h`, `osd.cpp` draws into the game's own present).
BossRematch and Training do not draw their own full UI when EngineFix hosts them: EngineFix queries an in-process
ABI (`mod/rematch_menu_bridge.h`, `mod/Nioh3EngineFix/src/menu_rematch_host.h`) once the first `Present` has happened, and
shows their pages as **Boss Matches**, **Training** and **Player**. The BossRematch standalone window (`[Menu] Hotkey = F7`,
View + D-pad Up gesture) is suppressed while hosted.

| Control | Input |
|---|---|
| Open | **F10** (`[Menu] Key`), **Escape three times** (`KeyTripleTap`), or **triple-tap Start** on a pad (`Gesture = TripleTap`, `HoldButton = Start`, `TapWindowMs`) |
| Move / change | D-pad or stick / Left, Right; **A** (Enter) toggles or opens; **B** goes back; Y/Triangle closes outside edits |
| Close | **B**, the same gesture, or the "Close" row |
| Always-visible rows | Mod menu (jump to top), Close, **Report** |

Layout: `[Menu] Layout = 0` (default) has two pages, Optimizations (picture-preserving) and look-changing options, with Boss
Matches, Training, Display Overlays and Settings on the first screen. `Layout = 1` is the older grouped layout. Rows
marked `* restart` apply at the next launch. **Save to ini** writes edited keys. **Reverse Mods** is on the first page (section 5.3).
The top-left overlay (fps, frame-generation fps, frame time, game speed) is `[Overlay] FpsCounter`; the startup popup is
`[General] StartupPopup`.

Menu code has offline tests (`tests/menu_harness.cpp`, `run_menu_harness.py`, `menu_*_test.cpp`) that build the real
model; they prove the logic, not rendering on a device.

---

## 9. The auto-updater and the update notice

Two separate pieces:

1. **Update notice in the mod** (`mod/Nioh3EngineFix/src/updatecheck.cpp`, `updatecheck_core.h`). `[Update] Check = 1` (default)
   and `[Update] Url` (default is the project website's `version.json`). A background WinHTTP worker (HTTPS only, redirects refused,
   no cookies, 8 s total limit, 512-byte body cap, one check per hour) fetches a tiny JSON and compares numeric
   `major.minor.patch`. If the remote version is newer, the menu shows
   `Update x.y.z available: Mod Menu > Update + Page`. It **never downloads or replaces code**. The row `Update + Page`
   opens the fixed website and selects `Nioh3Updater.exe` in Explorer (under Wine it shows the folder path and website instead,
   and launches no Windows shell UI). `[Update] Check = 0` disables all requests.
2. **The updater program** (`tools/updater/`, section 5.4). Reads the manifest `updates/stable.json`
   (schema 1: `version`, one-line `news` of at most 240 characters, 1 to 9 files from the fixed allowed names, each with an HTTPS URL and
   SHA256), downloads to staging on the target volume, hash-checks everything **before** touching an installed file, swaps with atomic
   replaces under a journal, preserves existing INIs, refuses to run while the game is running, and never starts or stops the game.
   A numeric downgrade is refused. After an update that changed files it sends only the old and new version numbers
   (`[Update] CountUpdate`, `--no-count`).

`tools/updater/stage_site.py` stages immutable payloads plus a manifest (it deploys nothing). `site/` holds the website API
and `version.json`.

---

## 10. How releases are packaged (high level)

A release is one set of files published to the public GitHub repository, plus the same manifest on the update site.

```
build 4 ASIs  ->  release gates  ->  full-install zip + combined (update) zip + Nioh3Updater.exe + SHA256SUMS
              ->  publish to the public repo  ->  update-site manifest  ->  bump the catalog pin
```

| Step | Script | Notes |
|---|---|---|
| Lint / regression | `release/check_release.py` (`--full`, `--strict`, `--br PATH`) | ini lint, source keys vs ini, profiles, `MOD_VERSION` vs built ASI, changelog mention, and with `--full` all harnesses. `--br` checks only the BossRematch soul-core gate. |
| Profiles | `release/make_profiles.py` | Section 6.3. |
| Legacy EngineFix-only zip | `release/package.py` (`--final`, `--full`, `--allow-stale`, `--out`) | Builds `Nioh3EngineFix-<ver>[-rc].zip` with `dinput8.dll`, the ASI, the profile ini, README/USER_GUIDE, `PROFILE.txt`, `SHA256SUMS.txt` and optional shaders. `--final` refuses provisional entries. The loader and shader replacements are local-only inputs, never committed. |
| Combined offline candidate | `release/package_final.py --loader PATH` | Builds `Nioh3-<ver>-combined-rc.zip` from the four ASIs plus updater and re-verifies every archived hash. Treat it as an older flow: it asserts `[SaveFiles]` defaults that the current ini no longer has, so verify before relying on it. |
| Fresh-install zip | `release/full_install_zip.py build / check / check-tag` | Rule: every GitHub release carries `Nioh3-<ver>-full-install.zip` that installs from scratch (loader, four ASIs, inis, installer, `INSTALL.txt`, `SHA256SUMS.txt`). `check` and `check-tag` refuse a zip that is missing any `REQUIRED` file. |
| Public assets | `tools/updater/pack_public_assets.py` | Builds the asset folder (combined zip with `Nioh3Updater.exe` at its root and also as its own file, the `.tsv` lists, `stable.json`). The player settings files are deliberately **not** in the update zip, so unzipping it over the game folder cannot overwrite them. Refuses if `news` does not start with the new version or repeats the previous release's. |
| Publish gate | `tools/updater/updater.py: publish_gate` | Refuses a package missing the four ASIs or `Nioh3CamKiProbe.ini`, or a BossRematch ASI built without the `SoulCoreForms` marker. |
| PDB path scrub | `release/scrub_pdb.py` | Writes a distinct PE copy with PDB pathnames replaced by a bare file name, so no home path ships in an ASI. Refuses a result that still contains path markers. |
| Publish | `release/publish_github.py --tag vX.Y.Z --notes FILE --dir OUT [--dry-run]` | The only allowed target is the public repository; it refuses any other `--repo`, uploads every file in `SHA256SUMS.txt`, marks it Latest and re-checks the GitHub digests. A bare `gh release create` from a private checkout goes to the wrong repository. |
| Source export | `tools/export_public.py` | Dry run by default: overlays the mod source onto a worktree of the public branch and gates for binaries, personal paths, newer-in-public files and soul-core code. A separate manual step pushes. |
| Setup programs | `tools/setup/` (`build-setup.ps1`, `build_linux.py`) | One-click `Nioh3-Setup.exe` / `.pyz` embedding the full-install zip. |

Per release, also: add a bullet changelog entry (the `MODLOG.md` entry is checked by `check_release.py`), and
**bump `RELEASE` in `mod/settings/test_catalog.py` to the commit you shipped**, otherwise the catalog guard compares against stale history.

Never ship: game files, a decrypted dump, unscrubbed ASIs, or save data (see PUBLIC_EXCLUSIONS.md). Third-party binaries
(the ASI loader) are credited and licensed separately in the zip (`LICENSE-UltimateASILoader.txt`).

---

## 11. First hour checklist

1. **Read** CONTRIBUTING.md, PUBLIC_EXCLUSIONS.md, this file, then `docs/code-map.md` and `docs/subsystems/README.md`.
2. **Install** Python 3.10+ (with the `py` launcher) and an MSVC x64 toolchain (VC tools + Windows SDK). Open an
   "x64 Native Tools" prompt. Check `cl`, `link`, `ml64`, `rc` and `py -V`.
3. **Install zig** for Training: `pip install --user ziglang`; check `py -m ziglang version`.
4. **Confirm your game build**: Steam v2.02 (PE timestamp `0x6AA7C106`). A different build means the mods will do nothing on purpose.
5. **Back up your saves** (`%LOCALAPPDATA%\KoeiTecmo\NIOH3`) by hand, once, before the first run of any mod.
6. **Build everything** from a clean tree, in this order, and read the first failure rather than the last:
   `mod\Nioh3EngineFix\build.bat`, `mod\Nioh3BossRematch\build.bat`, `mod\Nioh3CamKiProbe\build.bat`, `mod\Nioh3Training\build.bat` (or `build.sh`).
   Expect the EngineFix script to run many offline harnesses; that is normal.
7. **Run the cheap checks**: `py -I mod/settings/generate_catalog.py --check`, `py -I mod/settings/test_catalog.py`
   (needs git history), `py -I tools/check_fixed_rvas.py`, `py -I release/check_release.py`.
8. **Install a dev build**: close the game, run `Install-Mod.cmd --game-dir "<Nioh 3 folder>"` from a package folder (or hand-copy
   your four `.asi` files plus `dinput8.dll` once). Keep the release `.asi` files somewhere safe for rollback.
9. **Launch and verify**: after about 3 s of rendering a popup says the mod loaded. Open the menu with F10. Open
   `Nioh3EngineFix.log` and confirm each patch line says `applied` (a `SKIPPED` line has the reason).
10. **Practise the escape hatch**: press **Reverse Mods** twice in the menu (or run `Restore Vanilla Game.cmd`), start the game
    as vanilla, then put the mod back. You will use this to bisect regressions.
11. **Make one safe change** (a log line or a default in the ini), rebuild, and watch it appear in the log.
    If you touch a menu row, edit `mod/settings/catalog.json`, run `generate_catalog.py`, then `test_catalog.py`.
12. **Pick a first task** from CONTRIBUTING.md: naming functions in `symbols/symbols.tsv` is the lowest-risk start.

---

## 12. Open problems / known gaps

Collected from comments in the source and the project notes. None of these is a promise; treat each as "needs verification
before you rely on it".

- **No single one-command build.** Four `build.bat`/`build.sh` scripts, each with its own toolchain discovery; CamKiProbe has no
  `vswhere` fallback, and some test runners hard-code a `tools/msvc/setup_x64.bat` path (`mod/tests/run_rematch_bridge_tests.py`).
- **Two source lists for EngineFix** (`build.bat` and `CMakeLists.txt`) must be kept in sync by hand; the bundled GitHub workflow is outdated.
- **Raw fixed RVAs remain.** `tools/check_fixed_rvas.py` and `tools/fixed_rvas_allow.txt` track them; the audit is documented in the
  private notes `notes/aob-audit.md` and is only warn-only at build time. A game patch will break any site that is not pattern-resolved.
- **Version coverage is single-build.** v2.02 only. The Steam-wrapper-removed exe is recognised by `known_build.h` but is a secondary path.
- **Ini defaults do not reach existing installs.** Installers and the updater preserve player ini files, so a default flip needs an explicit
  migration in code or it only affects fresh installs.
- **Stale package script.** `release/package_final.py` encodes older `[SaveFiles]` assumptions; `release/package.py` is the EngineFix-only
  legacy zip. The path actually used for releases is `full_install_zip.py` + `pack_public_assets.py` + `publish_github.py`.
- **Version string.** `MOD_VERSION` in `mod/Nioh3EngineFix/src/dllmain.cpp` reads 1.8.38 in the tree this guide was written from, one
  behind the "1.8.39" release label. Unverified which is canonical; run `check_release.py` to see what the gate thinks.
- **Hosted menu on handhelds.** The notes on the hosted Boss Matches/Practice menu say offline coverage does not prove rendering or
  gesture behaviour on a handheld; live device verification was required and the original failure that motivated hosting was not
  causally diagnosed.
- **Linux/Proton:** the updater and installer paths are tested with fakes (`test_linux_launcher.sh`); desktop behaviour (browser,
  file manager) needs a real session. `Open update page` under Wine only shows the website and folder path.
- **Save-slot helper.** `[SaveFiles]` has a native path (default) and a legacy Python path; the live-title import and named-slot
  features are described in the notes as partly experimental. Check the ini comments before enabling anything beyond the defaults.
- **Local pipes and probes** (`[Remote]`, `LiveCtl`, `Nioh3Training.cmd`, `plug/`) are test tooling with no security model beyond
  "local only"; they are off by default and must stay off in a release ini.
- **Not verified for this guide:** that `build.bat` for Training and `tools/setup` build cleanly on a fresh machine; the exact
  Ultimate ASI Loader behaviour for subfolders (this project always installs to the game root); Proton launch-option details beyond
  the one line shown in the user guide.
