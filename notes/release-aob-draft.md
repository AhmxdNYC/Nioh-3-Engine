# Release draft: byte-pattern auto-detect for all mods, no soul-core forms (pc #59065, human #58436)

DRAFT - nothing is published. RC = bridge #4473 (pc MSVC build). Assets staged by ally #59139. Branch `ally/release-aob` (nioh3-engine).

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

## RC results (10-31)

- ally/Proton (#59136): clean install over the ally install, INIs kept: logs EngineFix 1.7.88 / BossRematch 1.4.23,
  `game RVAs ... 0 missing` x3; Fight Now (Yamagata) OK; soul core LEFT x2 = vanilla; no "Soul core tuning" row; return to
  title OK; no crash.
- pc2 Windows updater test (#59106): pending - can point the updater at the staged manifest below.

## Release assets (v1.7.88, same layout as v1.7.87)

Bridge file #4477 `rel-1788-assets.tgz` (sha256 32104e3d2c06...). Built from #4473's binaries, nothing rebuilt.
Local copy on ally: `release/dist/v1.7.88/` (gitignored).

| file | sha256 |
|---|---|
| Nioh3EngineFix.asi | 45efd9609b48570a5ba3f5b0a08debb0ec170c05521aa997fd7f4c5a3a0d44c6 |
| Nioh3Training.asi | 02fed4cc2ec3d9232ca3203d8626075da3ed1866284edc5f15a7151fb7ae1f1c |
| Nioh3BossRematch.asi | 2aa508f5087fb01435e2d60ef31d3c427997bb1536b19f64ec9c39c05f7ff169 |
| Nioh3-1.7.88-combined.zip (the 3 ASIs at zip root, like 1.7.87) | 7b1467401b82e75e6a7de7fcad519abb340870dbcfb488bda8dd3faf390d5791 |
| stable.json (schema 1, site URLs /releases/1.7.88/<asi>; made by stage_site.py) | b24bbc1fe022e5537b37937ce040b20ee92c15a16c43cf55fc257ed9d5961d4e |
| site/version.json (178 bytes, <512 limit) | d0d3c6e63e305a95c8e47b64049a90ab622ebd6725a547c61e55459349041a1a |

Tarball layout: `v1.7.88/{3 ASIs, combined zip, stable.json, SHA256SUMS.txt}` +
`v1.7.88/site/{releases/1.7.88/<3 ASIs>, updates/stable.json, version.json}` = drop-in for the site folder.

## Publish commands (pc, only after the human says go and pc2's updater test passes)

Site folder = `C:\Users\Ahmad Hamza\nioh3-mod-site\site` (vercel project, deployed by pc for 1.7.86/1.7.87).

```powershell
# 0. unpack + verify (expect 7 OK lines)
$A = "$env:TEMP\rel1788"; mkdir $A -Force; py -m uo_bridge get 4477 --out $A\a.tgz; tar -xzf $A\a.tgz -C $A
cd $A\v1.7.88; Get-Content SHA256SUMS.txt | % { $h,$f = $_ -split '  '; if ((Get-FileHash $f -A SHA256).Hash.ToLower() -eq $h) {"OK $f"} else {"BAD $f"} }
# 1. site: immutable payload + updater manifest (NOT version.json yet)
$S = "C:\Users\Ahmad Hamza\nioh3-mod-site\site"
if (Test-Path "$S\releases\1.7.88") { throw "releases/1.7.88 exists - stop" }
Copy-Item -Recurse site\releases\1.7.88 "$S\releases\1.7.88"
Copy-Item site\updates\stable.json "$S\updates\stable.json" -Force
cd $S; vercel --prod --yes
# 1b. verify live: version 1.7.88 + each file's sha matches
curl.exe -s https://nioh3-mod-feedback.vercel.app/updates/stable.json
foreach ($f in 'Nioh3EngineFix.asi','Nioh3Training.asi','Nioh3BossRematch.asi') { curl.exe -sLo "$env:TEMP\$f" "https://nioh3-mod-feedback.vercel.app/releases/1.7.88/$f"; (Get-FileHash "$env:TEMP\$f" -A SHA256).Hash }
# 2. public source export (release-aob tree, soul core compiled out) -> Nioh-3-Engine main, tag v1.7.88
#    (same export path as v1.7.87 deb036c4; then:)
cd $A\v1.7.88
gh release create v1.7.88 --repo AhmxdNYC/Nioh-3-Engine --target main --latest `
  --title "Nioh 3 Engine Fix 1.7.88" --notes-file $A\notes-1788.md `
  Nioh3-1.7.88-combined.zip Nioh3EngineFix.asi Nioh3Training.asi Nioh3BossRematch.asi stable.json
gh release view v1.7.88 --repo AhmxdNYC/Nioh-3-Engine --json assets --jq '.assets[].name'
# 3. LAST: in-game notice (1.7.86/1.7.87 users see "Update 1.7.88 available" within an hour)
Copy-Item $A\v1.7.88\site\version.json "$S\version.json" -Force; cd $S; vercel --prod --yes
curl.exe -s https://nioh3-mod-feedback.vercel.app/version.json   # expect "1.7.88"
```

`notes-1788.md` = the "Draft release notes" quote above (without the `> `), plus one line:
"Soul-core transformations are not in this release (still in development)." plus the v1.7.87 line
"Download the three .asi files into the game folder, or let the in-game updater use the website."

Rollback: site - restore previous `updates/stable.json` (1.7.87 content above in GitHub v1.7.87 asset) + redeploy;
GitHub - `gh release edit v1.7.87 --latest` then `gh release delete v1.7.88 --yes`. Never overwrite /releases/1.7.88/ with
different bytes (stage_site.py refuses).

## Public source export (Nioh-3-Engine), dry run (ally #59153)

`python3 tools/export_public.py [--ref ally/release-aob] [--out DIR] [--allow-soulcore]` - never pushes.
Same path as v1.7.87 deb036c4: one new commit on `public/main` (curated history) in a temp worktree.
- Takes only code/data files of `mod/Nioh3{EngineFix,Training,BossRematch,CamKiProbe}` + `tools/check_fixed_rvas.py`,
  `tools/fixed_rvas_allow.txt` (BossRematch build.bat calls it). README/docs/release scripts/`tools/updater` stay as
  public has them (public updater.py has the GitHub fallback 388ba7c8 that the private copy lacks).
- Skips build output (`bin/`, `build/`, .obj/.lib/.pdb/.exp/.asi, build.log, plug/*.lib, mod .github) and the
  soul-core dev fixtures `tests/formhide4*`.
- Scrubs like the 1.7.86 export: `C:\Users\<name>\nioh3-engine` -> `C:\nioh3-engine` (53 lines), home dirs -> `~/`,
  Steam IDs -> 76561190000000000 (tour_harness.cpp had a real one).
- Gates (exit 1): public-newer lines missing from the ref, new binaries, new personal paths, version 1.7.88 present,
  soul-core form code (markers FormKey/FormHide/Seal*/OrbSkip/...; public 1.7.87 had 0).
- Result (10-31 5:24 am EST): 84 files, +13642/-633, 22 new files (gamerva/rvas per mod). Gates OK except:
  **soul-core: 549 markers** (bosslog.cpp 478 = ~4.3k lines of compiled-out form code, Nioh3BossRematch.ini [Spirit] 36,
  addon.h, soul_core_tuning_menu.h, rematch.cpp, main.cpp). `BR_SOULCORE=0` only cuts the entry points; the code stays
  in the source and the binary. DECISION for pc: (a) publish dormant code (`--allow-soulcore`; source = shipped binary),
  or (b) guard the form code with `#if BR_SOULCORE` on the branch (removes it from the binary -> rebuild + retest) and
  strip at export, or (c) public BossRematch = 1.4.22 + only the generic AOB delta (source != binary).
- FOUND + fixed on the branch: public 29215833 (frame-meter master switch hides player row / I-FR / Ki window,
  shipped in 1.7.86 and 1.7.87) was missing from the private line -> **RC #4473 Training 02fed4cc lacks it**.
  Restored in `practice_stats_game.h` (zig compile OK); Training must be rebuilt (MSVC) for the final assets.
- rvas.json files carry short byte patterns (signatures), like the patterns already public in source.
- Public README/USER_GUIDE still say 1.7.87; bump them in the same commit if wanted (not done by the script).

## Soul-core code compiled out (pc #59173 option b, ally 10-31 5:30 am EST)

- `#if BR_SOULCORE` around: bosslog.cpp form/seal/tenshin block + Register [Spirit] reads + SC probe hooks; rematch.cpp
  tenshin/spirit pipe commands + Soul core tuning page rows; addon.h decls; hooks.cpp InstallCallKeyOverride; main.cpp
  FormKey/SC install + [Spirit] InvLog trigger; sites_gen.h groups SC + ACANCEL (hand-added rows; derive_sites.py now emits
  the guard, but regenerating drops the hand-added ACANCEL rows - known, pre-existing).
- zig: BR_SOULCORE=0 1027072 B (was 1188352), 0 form strings; =1 builds (dev line unchanged). Exported public BR source
  compiles to the same 1027072 B.
- export_public.py strips `#if BR_SOULCORE` blocks, [Spirit]/[SealHits] ini sections, soul_core_tuning_menu.h, formhide4 tests:
  dry run GATES OK, 82 files +8712/-633, 0 soul-core markers.
- Training rebuilt with zig from f57ab0ec (frame-meter fix): bridge #4481 sha e58d4b16 (RC was 02fed4cc).
- NEXT: pc MSVC-builds BossRematch from ally/release-aob 53bb84ee -> ally reissues assets (zip/stable.json) -> ally+pc2 recheck.

## FINAL assets (10-31 5:45 am EST) - supersede #4477

Bridge file #4484 `rel-1788-assets-final.tgz` (sha256 299bd6c82253...), same layout as #4477. Publish commands above: use 4484.

| file | sha256 |
|---|---|
| Nioh3EngineFix.asi (unchanged, pc MSVC) | 45efd9609b48570a5ba3f5b0a08debb0ec170c05521aa997fd7f4c5a3a0d44c6 |
| Nioh3Training.asi (zig, f57ab0ec frame-meter fix, #4481) | e58d4b16c8f5ad769f29b3af742ddadde6e4a49484fba1ceebce816627cadbb6 |
| Nioh3BossRematch.asi 1.4.23 (pc MSVC from fad8466d, soul core compiled out, #4482) | 0afa302e01b92eeddded2851c15d8ca962acb2c3f0b10d4adc3076020068a6aa |
| Nioh3-1.7.88-combined.zip | f89d78b12e8e9c952c547f2e052eccdf0d9573e7a1ac256be01ca022d8995f34 |
| stable.json | 306e2bf9c3fe702bfa74dbe81642ae3c9280e4bf846a7b4e6c8c8059e0d1540c |
| site/version.json (unchanged) | d0d3c6e63e305a95c8e47b64049a90ab622ebd6725a547c61e55459349041a1a |

Ally/Proton recheck of these exact bits (captures/ally-59205): logs EngineFix 1.7.88 / BossRematch 1.4.23, game RVAs 0 missing x3
(EF 210, TR 218, BR 121); soul core LEFT = vanilla (yokai beside player), 0 form/seal/spirit lines in BR log; no Soul core tuning row;
Frame meter master OFF -> Player frame meter + I-frame lane OFF, meter gone, ini saved FrameMeter/PlayerFrameMeter/FrameMeterIFrames=0;
Fight Now Yamagata OK (arena swap, boss bar); return to title OK; no crash.

## v1.7.89 (prep only, 10-31 8:35 am EST)

Do not run the publish from this section. Same order as v1.7.88 above. Wait until both rechecks pass and the human says go.
Source branch for the notes: pc/release-1789 26dda5b4. User-facing text: notes/release-1.7.89.md.
Assets pc named for bridge #4522. Local copies in captures/rel1789 (not in git) hash to those same prefixes:

| file | sha256 |
|---|---|
| Nioh3EngineFix.asi (MOD_VERSION already 1.7.89) | 30f2f2f23a149303a09c86333b9282c0a2b26917802d5fd8402169632a8b4c0e |
| Nioh3Training.asi | 8b18ab03eaaf5df236154b17697e8cd14091a21792e707f239f1114bcba30679 |
| Nioh3BossRematch.asi 1.4.23 (same bytes as the 1.7.88 final) | 0afa302e01b92eeddded2851c15d8ca962acb2c3f0b10d4adc3076020068a6aa |

Publish order, unchanged from 1.7.88 except the version and the notes file:

1. Unpack bridge #4522 and check the three hashes above. Build the site drop with
   `tools/updater/stage_site.py --site <site folder> --version 1.7.89 --payload <each asi> --news` from notes/release-1.7.89.md.
   That writes immutable `/releases/1.7.89/` plus `/updates/stable.json`. It must not be copied over `version.json` yet.
2. Copy `releases/1.7.89/` and `updates/stable.json` into `C:\Users\Ahmad Hamza\nioh3-mod-site\site`, then `vercel --prod`.
   Verify live `stable.json` and each ASI hash. Leave the live `version.json` on 1.7.88 for this step.
3. Public export, soul core still compiled out: `tools/export_public.py --ref pc/release-1789 --version 1.7.89`.
   The script's default `--version` is still `1.7.88`; pass `1.7.89` or the gate looks for the wrong string.
   Then `gh release create v1.7.89 --repo AhmxdNYC/Nioh-3-Engine --target main --latest --title "Nioh 3 Engine Fix 1.7.89" --notes-file notes/release-1.7.89.md` with the combined zip, the three ASIs, and stable.json.
4. LAST: copy the new `version.json` (`"version": "1.7.89"`) onto the live site and `vercel --prod` again.
   Players on 1.7.88 then see the in-game notice. Check `https://nioh3-mod-feedback.vercel.app/version.json`.

Files that still say 1.7.88 and that this publish has to bump:

- `C:\Users\Ahmad Hamza\nioh3-mod-site\site\version.json` (live, `"version": "1.7.88"`). Bump this last.
- `C:\Users\Ahmad Hamza\nioh3-mod-site\site\updates\stable.json` (live version 1.7.88, news line, and the three `/releases/1.7.88/` urls).
- `tools/export_public.py` default `--version` is `1.7.88` (the gate requires that string next to a VERSION name). Pass `--version 1.7.89`.

Already 1.7.89 in this repo, so a publish must not treat them as still on 1.7.88:

- `site/version.json` (repo copy only; the live site file above is the one players read).
- `mod/Nioh3EngineFix/src/dllmain.cpp` `MOD_VERSION` is `1.7.89`. BossRematch `ADDON_VERSION` stays `1.4.23`.

Not 1.7.88, so they are not on this bump list. The public README (Nioh-3-Engine ffdd6bfe) still says version 1.7.87 on the EngineFix line. This repo's `README.md` and `release/USER_GUIDE.md` do not contain 1.7.88, and the 1.7.88 public tree has no USER_GUIDE version line. `staged-site/` in this repo is an old 1.7.85 drop, not the live site.

Rollback, if a publish is undone: put the 1.7.88 `updates/stable.json` back and redeploy; `gh release edit v1.7.88 --latest`, then delete `v1.7.89`. Do not overwrite `/releases/1.7.89/`. Put live `version.json` back to 1.7.88 last.

## 1.7.89 (draft only, 10-31)

Nothing here is published. Do not run the site deploy, `vercel`, or `gh release`. Source branch `pc/release-1789` at `26dda5b4`. Assets are bridge file #4522 (not unpacked in this prep).

| file | sha256 prefix |
|---|---|
| Nioh3EngineFix.asi | 30f2f2f2 |
| Nioh3Training.asi | 8b18ab03 |
| Nioh3BossRematch.asi | 0afa302e (same prefix as the 1.7.88 final BossRematch; ship it again inside #4522) |

Player notes for the GitHub release and `notes-1789.md`: `notes/release-1.7.89.md`.

Publish order (only after the human says go): MSVC is already in #4522, do not rebuild for this note -> `tools/updater/stage_site.py --version 1.7.89 --payload ...` (immutable `/releases/1.7.89/` + `/updates/stable.json`) -> deploy site -> GitHub release v1.7.89 on Nioh-3-Engine (3 ASIs, stable.json, combined zip) -> public repo export with `--version 1.7.89` -> bump `/version.json` to 1.7.89 last.

```powershell
# DRAFT. Do not run until the human says go.
# 0. unpack + verify
$A = "$env:TEMP\rel1789"; mkdir $A -Force; py -m uo_bridge get 4522 --out $A\a.tgz; tar -xzf $A\a.tgz -C $A
cd $A\v1.7.89; Get-Content SHA256SUMS.txt | % { $h,$f = $_ -split '  '; if ((Get-FileHash $f -A SHA256).Hash.ToLower() -eq $h) {"OK $f"} else {"BAD $f"} }
# 1. site: immutable payload + updater manifest (NOT version.json yet)
$S = "C:\Users\Ahmad Hamza\nioh3-mod-site\site"
if (Test-Path "$S\releases\1.7.89") { throw "releases/1.7.89 exists - stop" }
Copy-Item -Recurse site\releases\1.7.89 "$S\releases\1.7.89"
Copy-Item site\updates\stable.json "$S\updates\stable.json" -Force
cd $S; vercel --prod --yes
# 1b. verify live files. version.json must still say 1.7.88 until step 3.
curl.exe -s https://nioh3-mod-feedback.vercel.app/updates/stable.json
foreach ($f in 'Nioh3EngineFix.asi','Nioh3Training.asi','Nioh3BossRematch.asi') { curl.exe -sLo "$env:TEMP\$f" "https://nioh3-mod-feedback.vercel.app/releases/1.7.89/$f"; (Get-FileHash "$env:TEMP\$f" -A SHA256).Hash }
# 2. public source export from pc/release-1789 (soul core stays compiled out) -> Nioh-3-Engine main, tag v1.7.89
#    py -I tools/export_public.py --ref pc/release-1789 --version 1.7.89
cd $A\v1.7.89
gh release create v1.7.89 --repo AhmxdNYC/Nioh-3-Engine --target main --latest `
  --title "Nioh 3 Engine Fix 1.7.89" --notes-file notes/release-1.7.89.md `
  Nioh3-1.7.89-combined.zip Nioh3EngineFix.asi Nioh3Training.asi Nioh3BossRematch.asi stable.json
# 3. LAST: in-game notice (1.7.88 users see "Update 1.7.89 available" within an hour)
Copy-Item $A\v1.7.89\site\version.json "$S\version.json" -Force; cd $S; vercel --prod --yes
curl.exe -s https://nioh3-mod-feedback.vercel.app/version.json   # expect "1.7.89"
```

Rollback: put the 1.7.88 `updates/stable.json` back and redeploy; `gh release edit v1.7.88 --latest` then `gh release delete v1.7.89 --yes`. Never overwrite `/releases/1.7.88/` or `/releases/1.7.89/` with different bytes.

### Files a publish must bump

These still say 1.7.88. This prep does not change them.

- `tools/export_public.py` line 91: `--version` default is `1.7.88`. Pass `--version 1.7.89` or the export gate looks for 1.7.88.
- Live site `nioh3-mod-site/site/version.json`: `"version": "1.7.88"`. Bump this last.
- Live site `nioh3-mod-site/site/updates/stable.json`: `"version": "1.7.88"`, the news line, and the three file URLs under `/releases/1.7.88/`.

Also set these, even though they do not say 1.7.88:

- Public `README.md` line 17 still says version 1.7.87. Set it to 1.7.89 in the export commit. `release/USER_GUIDE.md` has no version line.
- Private `mod/Nioh3EngineFix/src/dllmain.cpp` already says `MOD_VERSION "1.7.89"`. Public `dllmain.cpp` still says 1.7.88 and is replaced by that export, not by a hand edit.
