# Release draft: byte-pattern auto-detect for all mods, no soul-core forms (pc #59065, human #58436)

DRAFT - nothing is published. Branch `ally/release-aob` (nioh3-engine).

## What the branch is

- `origin/main` (a7f4a01f, the 1.7.86 tree)
- + `ally/aob-bossrematch` (aob-tool + pc/attackcancel-58429 b7b98a2c + BossRematch Grva + typo fix 37198b46), merged clean
- + port of public `Nioh-3-Engine` deb036c4 (= released v1.7.87, "each patch turns on from its own bytes"). That commit
  only exists in the public repo; 4 conflicts (autocont, bootnotice, psomemo, scenemirror + dllmain) were the
  `knownBuild` gates vs the Grva wraps: gates removed as in 1.7.87, addresses kept as `Grva()`, `gamerva::Init` kept.
- + soul-core forms compiled out of BossRematch (`BR_SOULCORE=0`, addon.h)
- Versions: EngineFix 1.7.88, BossRematch 1.4.23 (Training/CamKiProbe have no version string).

## Changes vs the last release (v1.7.87, 2026-10-29 8:23 pm EST, Nioh-3-Engine deb036c4)

1. Every fixed game address in every mod is found by byte pattern at game start (`Grva()` + per-mod `rvas.json` ->
   `src/rvas_gen.h`, made by `tools/gen_rvas.py` from the decrypted v2.02 dump):
   - EngineFix 505 uses / 210 RVAs (157 code, 41 data, 11 imports, 1 none; 185 uses allowlisted = profiler labels)
   - Training 401 uses / 218 RVAs; BossRematch 154 uses / 121 RVAs; CamKiProbe 31 uses / 15 RVAs (CamKiProbe is not a release asset)
   - Known build (v2.02): `Grva` is the identity, zero cost; the startup log line `game RVAs (rvas.json, N entries): ... 0 missing`
     verifies every entry once (BossRematch: once the player exists).
   - Another game build: each address is looked up by its pattern; a missing / non-unique pattern returns 0 and that
     feature stays off (callers read nothing) instead of writing to a wrong address.
   - Build-time check: `tools/check_fixed_rvas.py` (0 RAW for all 4 mods; warn-only in BossRematch build.bat) and
     `mod/<mod>/tools/gen_rvas.py --check` (all 4: 0 missing).
2. BossRematch: fixed a wrong global (0x45B3E30 -> 0x45B9E30, data-DB table read; disasm of 74C274 proves it).
3. BossRematch hook installer: call retarget writes only the rel32 when the 5-byte call straddles a 16-byte line
   (part of the pc soul-core line, kept: it is generic).
4. Training: `[Practice] PlayerInfiniteSoulCore` (default 0) and `SoulCoreMin` (default empty) came in with the merge.
   They only put soul-core ITEMS back (practice cheat like PlayerInfiniteNinjutsu), no form code. Kept, default OFF -
   pc decides whether to strip them too.

## Soul-core forms: how they are off

`BR_SOULCORE` (addon.h, default 0; dev line builds with `/DBR_SOULCORE=1`):
- main.cpp: `[Spirit] FormKey/Body` no longer turn on the SC hook group, `bosslog::FormKeyInstall()` is never called
  -> no seal hooks, FormHide, FormSlot0, AttackCancel, SealEnd*, OrbSkip, FastThrow, FormFollow, exit smoke.
- rematch.cpp: the "Soul core tuning" row is not added to the Training hub; pipe command `spirit ...` answers
  "not in this build"; `[Spirit] Body` is ignored.
- Nioh3BossRematch.ini: the [Spirit] section says its keys are ignored in the release build (all its enables are 0 anyway).
- The code itself stays in the binary (unreachable); no source deleted so the dev line merges cleanly.
- [Debug] InvLog=1 still installs the log-only SC probes (debug switch, default 0).

## Updater path (how 1.7.86 / 1.7.87 users get the new one)

1. In game, EngineFix polls `https://nioh3-mod-feedback.vercel.app/version.json` every hour (updatecheck.cpp,
   `[Update]` URL overridable). Newer -> HUD "Update X available: Mod Menu > Update + Page"; the Update page opens the
   website (Windows) or shows the game folder + site (Wine). It never downloads by itself.
   **FOUND: version.json still says 1.7.86 (checked 10-31 5:05 am EST) while v1.7.87 is out - 1.7.86 users got no
   in-game notice for 1.7.87. Publishing must bump version.json, last.**
2. Website -> `/downloads/Nioh3Updater.exe` (tools/updater, PyInstaller). It finds the Steam game folder, waits for the
   game to close, reads `/updates/stable.json` (schema 1: name / sha256 / https url), falls back to
   `github.com/AhmxdNYC/Nioh-3-Engine/releases/latest/download/stable.json` if the site manifest is unreadable (public
   repo 388ba7c8; the private tools/updater lacks this fallback), downloads `/releases/<ver>/<file>`, checks sha256,
   backs up to `nioh3-update-backups/<id>` (rollback `--rollback ID`), replaces the ASIs, keeps existing INIs.
   Note: the GitHub copy of stable.json still points its file URLs at the website, so the fallback only covers the manifest.
3. Linux: `tools/updater/update-linux.sh --game-dir ... --proton-dir ...` runs the same exe under Proton's wine.
4. Existing INIs are kept -> the new [Spirit] comment line only reaches fresh installs (keys are ignored anyway).

Publish order (when the human says go): MSVC build -> `tools/updater/stage_site.py --version 1.7.88 --payload ...`
(immutable /releases/1.7.88/ + /updates/stable.json) -> deploy site -> GitHub release v1.7.88 on Nioh-3-Engine (3 ASIs,
stable.json, combined zip) -> public repo export -> bump `/version.json` to 1.7.88 last.

Test plan (pc2): install v1.7.87 release ASIs -> launch -> confirm the HUD notice once version.json says 1.7.88
(or point `[Update]` at a staging URL) -> run Nioh3Updater.exe -> confirm 3 ASIs replaced (sha), INIs kept, backup made,
log shows 1.7.88 / 1.4.23 + `game RVAs ... 0 missing` for EngineFix, Training, BossRematch; soul core LEFT = vanilla.

## Draft release notes (v1.7.88)

> Every game address the mods use is now found by its own bytes when the game starts - EngineFix, Training and Boss
> Rematch alike. On the current game (v2.02) nothing changes and it costs nothing. After a game update, each feature
> whose bytes are still there keeps working; one whose bytes moved stays off instead of touching the wrong place.
> Also: a Boss Rematch data read used a wrong address (fixed). Update with Nioh3Updater.exe or the in-game Update page;
> your settings are kept.

Checks done on ally: zig compile of BossRematch (9 units + link), Training (link), EngineFix (51/52 units; dllmain
needs MSVC's _udiv128, same as main). MSVC build + in-game run still to do (pc / pc2).
