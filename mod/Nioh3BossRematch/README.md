# Nioh3BossRematch

> The source in this folder is the final build (add-on 1.4.22; the separate Training add-on is in `mod/Nioh3Training`). The text below was written for 1.4.0 and still describes the core rematch flow; see the top-level README for the current feature list.

## 1.4.0 notes (BETA at the time)

**BETA, not fully tested yet.** Only a light in-game test of the new arena spawn has passed. The tested version is **1.3.3** (on the Releases page). Read "What is tested" before you use it on a save you care about.

A separate add-on for **Nioh 3 (Steam, version 2.02)**. It adds an overlay menu that lists the bosses you have beaten and lets you fight them again. It also has an optional boss-fight logger for modders.

It is **single-player, offline only**. It works with or without Nioh3EngineFix, and it needs no other mod.

## Back up your saves first

The add-on starts real missions on your loaded save. It does not write story flags or save data itself. The game, however, runs the mission normally, and it may save, award rewards or change progress the way any mission does.

Before your first use, copy your Nioh 3 save folder somewhere safe. The folder location depends on your Steam setup. Restore the copy if anything looks wrong.

## How to use it

1. Install (below). In `Nioh3BossRematch.ini`, set `[Rematch] Enable = 1`. It is **off by default** and installs nothing until you do.
2. Start the game offline and load a save.
3. Press **F7** to open the menu. On a controller, hold **View + D-pad Up** for 1 second. Press the same keys to close it. The pad is hidden from the game while the menu is open.
4. Pick a boss. Each row shows its route and whether you have beaten it on this journey. Routes in the catalogue:
   - **Replay:** the game's own Battle Scroll or shrine replay list. You pick the mission there.
   - **Direct start** (`DirectStart = 1`): starts the mission directly, as the shrine replay confirm does. Only missions you cleared on this journey.
   - **Hidden duels** (`HiddenDuels = 1`) and Picture Scroll records are listed separately.
   - Bosses with no route are listed greyed with the reason (`ShowAll = 1`).
5. **Fight now** (`FightNow = 1`): each beaten boss gets a line that starts its mission at the start point near the boss arena.
   - With `FightNowWarp = 1` you also get **Fight now (in the arena)** and **Shrine before the boss**.
   - **New in 1.4.0:** with `FightNowSpawn = 1` (the default value of that key, but it only matters when `FightNow = 1` and `FightNowWarp = 1`), you spawn in the arena from the first frame. There is no shrine stop first. A death reload puts you back in the arena. The change is in memory only.

The catalogue (`Nioh3BossRematch_bosses.tsv`) has 121 rows: 59 replay, 10 direct (duel-master records), 3 hidden duel, 20 Picture Scroll, 29 with no route. Only 16 rows have a stored arena point so far. Rows without one cannot use the in-arena option.

## What is tested

- **Passed:** offline harness tests (no game): catalogue merge, redirect and restore, lifecycle, hook chaining. Site-table tests against the v2.02 executable. A light in-game test of the 1.4.0 arena spawn.
- **Passed on 1.3.3:** an in-game self-test that ran Fight now on catalogued rows (33 pass, 0 fail in the project log).
- **Not done:** full per-boss validation of every row. Many rows are marked unverified, and by default they stay hidden (`ShowUnverified = 0`). A full test plan is still open.
- **Untested:** Proton and Steam Deck, and any use with a New Game Plus cycle you care about. Consider every route other than a plain replay experimental.

## Co-op and online

The add-on refuses to start a rematch in an online or co-op session. If an online session starts during Fight now, the arena spawn override is removed. If a session starts during a patched Picture Scroll, it logs a warning, because other players load their own map. Do not use it online. Online use is untested.

## Install

1. Install an ASI loader first (`dinput8.dll`, the Ultimate ASI Loader). Nioh3EngineFix ships one. Keep only one copy.
2. Copy `Nioh3BossRematch.asi`, `Nioh3BossRematch.ini` and `Nioh3BossRematch_bosses.tsv` next to `Nioh3.exe`.
3. On Proton, add the launch option `WINEDLLOVERRIDES="dinput8=n,b" %command%`.
4. A log, `Nioh3BossRematch.log`, appears next to the add-on.

**Uninstall:** delete the three files and the log. Or set `[General] Enabled = 0`.

If the game is not exactly v2.02, the add-on finds each game site by byte pattern. A group of features that cannot find its sites turns itself off and logs why. The other groups keep working.

## Settings that matter

All in `Nioh3BossRematch.ini`. Every key is described there.

- `[Rematch] Enable`, `DirectStart`, `HiddenDuels`, `ShowAll`, `ShowUnverified`, `FightNow`, `FightNowWarp`, `FightNowSpawn`. All off by default except `ShowAll` and `FightNowSpawn`.
- `ArenaCapture = 1` appends your position at each boss fight start to `Nioh3BossRematch_arenas.tsv`. Use it to add arena points.
- `SelfTest` plays real missions by itself on your loaded save. Back up first. Leave it empty.
- `[Menu] Overlay`, `Hotkey` (F7), `PadGesture`.
- `[Debug] BossLog`, `RematchLog`, `InvLog`: read-only loggers for modders. Off by default.
- `[Remote] Pipe`: a local named pipe for a test tool. Off by default.

## Build from source

You need Windows, MSVC with the x64 C++ tools, and the Windows SDK. Python is only for the tests and tools.

```
build.bat
```

It compiles `src\*.cpp` with `cl /O2 /MT` and links `bin\Nioh3BossRematch.asi`. It copies the ini and catalogue into `bin\`.

Tests:

- `tests\run_rematch_harness.py` (offline harness) still depends on helper scripts that are not in this repo yet, so it does not run here.
- `py -I tests\test_sites.py <path\to\Nioh3.dump.exe>` checks every site against the game executable. You supply the executable.
- `tools\derive_sites.py` rebuilds both `sites.json` and `src\sites_gen.h` from your game executable.

Notes on how the game replays bosses (Battle Scrolls, the shrine replay menu, the mission-start queue) are in the project notes. This README does not repeat them.
