# Final-release readiness (2026-10-08, ASI 1.7.40 on main, one universal zip)

Sources: AGENT_BRIEF, release/profiles.json + package.py + check_release.py, remaining-work 7-8, final-benchmark-protocol,
release-os-profiles, STATUS HANDOFF, run7-analysis, pc-cpu-next. Nothing was launched or built; `package.py` was run only
into a temp folder.

## Dry-run result
- `py -I release\package.py` -> OK, writes `Nioh3EngineFix-1.7.40-rc.zip` (9 files, 9.9 MB). The zip is an **-rc**.
- `py -I release\package.py --final` -> **REFUSED**. `check_release --strict` = 0 errors, 4 warnings (strict makes warnings fatal):
  1. provisional entries in profile ALL: `MQ`, `W3_TimeoutMs`.
  2. two keys read by src but not in the base ini as the scan expects: `[Patches] JT`, `[Profiler] JT_ReportSec` (JT = 0 exists at ini line 562;
     document/spell the keys so the scan finds them; src owner).
  3. six base-ini keys the src scan cannot find: `[Debug] TourDwellSec, TourLoadTimeoutSec, TourPasses, TourPointsPerStage, TourSettleSec,
     TourStartDelaySec` (tour.cpp has uncommitted edits in the tree; fix the read pattern or the scan, do not delete the keys).
  4. `mod\ally-package` asi differs from bin: retire the folder (remaining-work 9.4) or refresh it.
- Nothing else blocks `--final`: every other entry is already `kept` or `off`.

## Blockers
| # | item | why provisional / open | decided by | exact profiles.json edit |
|---|---|---|---|---|
| B1 | `MQ = -1` (provisional) | Proton side (bit-exact, CPU power only) never run on a handheld; Windows 0 measured (no gain) | Ally power/CPU pair with MQ on/off | KEEP: `"status": "kept"`. DROP: `"value": "0"`, `"status": "off"`, rewrite `why`, add `[Patches] MQ = 1` to `optin` |
| B2 | `W3_TimeoutMs = -1` (provisional) | Proton 3 vs Windows 1 is an estimate (cpu-hotspots-ally row 4, ~0.07 core), never A/B tested | Ally A/B W3_TimeoutMs 3 vs 1 (power, pump polls, input latency) | KEEP: `"status": "kept"`. DROP (flat): `"value": "1"`, `"status": "off"` (removes the auto, as proposed in release-os-profiles) |
| B3 | AABB, BPO, DQ, P1E7, MA, PJ (default 0, not in profiles.json at all) | run7 Block A had AABB ON the whole time (toggle bug, fixed in 1.7.40), so DQ/P1E7 were INVALID; MA (Block C) not measured; BPO new in 1.7.40; PJ needs 2 launches per device (stutter.md 6.1) | **PC re-test run8 on 1.7.40**: `abba2_pc.ps1 -Live AABB,BPO,DQ,P1E7 -LaunchFlags none -Quads 3 -MaxFps 240 -Input vpad` (Block A), then `-Arms BASE,MA,BASE,MA,BASE -Live none -MaxFps 240 -Input vpad` (Block C); PJ per stutter.md 6.1 | KEEP (rule: median >= +1 %, every valid quad > 0, p99 not worse > 2 %, mechanism read-back OK): append to `overrides` `{"section":"Patches","key":"AABB","value":"1","status":"kept","why":"PC run8 <delta> %, ..."}` and the same shape for each KEEP key. DROP or NEUTRAL: add nothing (stays 0), optionally an `optin` row. DQ costs nothing, keep on if neutral. If Ally disagrees for a key use `-1` only if the ASI has an auto rule for it (today it does not) |
| B4 | PP (profile says `off`) | STATUS/pc-cpu-next record PP as PC KEEP (+2.9 %, -0.26 ms) but the texture-sharpness gate (fast travel) is still open and the profile text says opt-in; inconsistent | one PC fast-travel sharpness check | KEEP: `"value":"1"` applies to the Ally too (weak at 16-30 fps), so either accept that or ask the src owner for a PP auto (Wine 0, Windows 1, as release-os-profiles proposes). DROP: no edit |
| B5 | Ally #326 power-shift / barrier results | bridge-only; no `#326` or "power-shift" text exists in notes, STATUS or MODLOG, so I could not read it (bridge not used) | Ally agent's #326 digest | Barriers: stays `0`/`off` unless the Ally shows a win (R2a/R2b are Wine-only; Barriers=1 was -7 % on PC); a Proton win needs a `-1` auto for Barriers (src owner). Power-shift: KEEP -> `optin` row (handheld) or auto key; DROP -> no edit. Treat as UNKNOWN until read |
| B6 | Tour leak check | `tools\tour\run_tour.ps1 -Passes 2` on PC, then Ally (notes\tour-mode.md); stability item remaining-work 6.2 | handle/VRAM trend in MemStats across passes | No edit on pass. A leak: set the offending flag `off` and rerun |
| B7 | fogpar shader `0xa9910db8` | kept from the Ally (+0.15/+0.30 fps), PC never measured; hash pinned in package.py | final PC benchmark must not regress | none; on a PC regression set `"status":"off"` |
| B8 | Final benchmark, latency, long session, smoke launch | remaining-work 8.2, 5.1, 6.2, 9.3 not run | user scheduling | none |

## Stale docs (checked against the one-zip, auto-by-OS design)
`release\USER_GUIDE.md` matches the design (one zip, `-1` auto, Windows and Proton installs). Stale or missing:
- the file table and "Optional shader" mention only `0x0104840c.file`; the zip also ships `0xa9910db8.file` (fogpar). Add it to both.
- the online co-op line says set `VRR = 0`; since 1.7.27 `VRROnline = -1` auto-suspends VRR online (co-op test still open, remaining-work 6.5). Reword.
- the closing `-rc` sentence must go (or stay conditional) once the final zip exists.

Project `README.md` is stale for the one-zip design:
- "Install: use `mod\ally-package` (see `README-ALLY.txt`)": that folder is to be retired (9.4). Point to the zip and USER_GUIDE.
- "Ally = main target" / "Desktop PC" sections and the Ally/PC scope labels imply two profiles.
- "LC (cap 16) ... not yet tested" and "PF, LC, PJ, DQ, P1E7, GIMerge and wdo14 are built but not yet tested": LC was measured worse (off), PF neutral.
- version table stops at 1.7.19, and the 1.7.13/1.7.14 text ("untested", "not yet verified in the game") is old.
- "Ally load 118.7 s, suspect 120 s timeout" is superseded (about 37 s with EP+SR).
- shader line lists only GHDM; fogpar missing. Final benchmark tables (8.3, 10.2) not written yet.
- `mod\Nioh3EngineFix\bin\README.md` (shipped as README-MOD.md): remaining-work 10.3 sync with the newer keys not verified.

## Command sequence: all results in -> final zip -> final benchmark
```
# 0. edit release\profiles.json per B1-B5 (kept/off, values), nothing else
py -I release\make_profiles.py
py -I release\check_release.py --strict      # target: 0 errors, 0 warnings (fix JT / Tour / ally-package first)
# 1. src owner: bin = src (1.7.40 or later), build.bat, tests\test_patterns.py -> ALL N SITES OK
# 2. final zip
py -I release\package.py --final --full      # refuses provisional + any warning; runs test_patterns + 9 harnesses (~50 s)
#    -> release\dist\Nioh3EngineFix-<ver>.zip (no -rc); check PROFILE.txt has no RELEASE CANDIDATE line, SHA256SUMS.txt
# 3. smoke launch per device with the zip contents (15 min each), boot log shows the "auto <key>" lines
# 4. PC final benchmark, user present (take the lock: py -m uo_bridge.gamelock acquire nioh3)
powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\final_pc.ps1 -DryRun
powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\final_pc.ps1 -Configs STOCK,OPTIMIZED,OPTIMIZED+FG -Presets USER,MAX -Runs 3
powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\final_pc.ps1 -ReportOnly -Hwinfo f.csv -Rtss g.csv
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tour\run_tour.ps1 -Passes 2 -Input vpad   # leak check (check tour-mode.md for flags)
# 5. Ally: notes\final-benchmark-protocol.md with the zip installed: STOCK / ALL-KEPT x MAX / USER x 3 runs interleaved,
#    MangoHud + tools\testmatrix\ally_final\ (measure, checklist, latency_procedure), LOAD1/LOAD2, hitches, 240 fps camera latency
# 6. results -> notes\final-benchmark-results.md, README tables (10.2), Discord (webhook pasted in-session), retire mod\ally-package (9.4)
```
Caveat: `final_pc.ps1` OPTIMIZED uses the matrix KEEP set (`tm.py best`); it must equal what profile ALL resolves to on Windows, or the benchmark
measures something other than the zip. Compare with the boot log before the run.

## Time estimate
- Edits, make_profiles, check, package --final --full: about 1 h once results are in.
- PC run8: Block A ~1.5 h, Block C ~0.5 h, PJ ~0.5 h = about 2.5 h game (needs the game lock free of the Ally).
- Ally: #326 follow-ups, MQ and W3_TimeoutMs pairs ~1.5 h; tour ~0.5 h. PC tour 2 passes ~0.5 h. Smoke 0.5 h.
- Final benchmark: PC ~3 h, Ally ~3 h (serial: one Steam account, user present). Docs ~2 h.
- Total about 1.5 days wall clock with the user for roughly 6 h. Critical path: PC run8 -> flip -> final zip -> benchmarks.
