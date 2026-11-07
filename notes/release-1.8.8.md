# 1.8.8 release notes (hotfix candidate, not published)

Source: branch `cx-pc/release-188-prep` = `cx-pc/rematch-active-79884` `ce033d43` (public 1.8.7 `82dc959a` + hub header `528f8382` + title exit `231eaed0`, `334976a2`, `ce033d43`) + one version commit. EngineFix `MOD_VERSION` is "1.8.8" (the only file that carries the version, same as 1.8.7). Nothing is published: no GitHub release, no site change, no stable.json.

## What players get

- The Picture Scroll hub header says "active: training (no waves)" on a Training trip where the scroll's waves were skipped. It used to say "active: waves".
- Boss Rematch no longer stays active after Return to Title. After 20 seconds at the title (no player, no load, no mission) it goes back to idle, so a story mission started from the Boss Match list and then Continue no longer refuses the next Picture Scroll or fight with "rematch active".

Both were checked in the game on pc2 (#79845, #79992) on the development builds; this packaged candidate has not been run.

## Candidate build (not published)

| File | 1.8.8 candidate | public 1.8.7 | bridge |
|---|---|---|---|
| EngineFix | `f30fe7ef` | `77bfd278` | #6189 |
| Boss Rematch | `19adc4d6` | `f391f3b2` | #6190 |
| Training | `f57937d1` | `3622a1b3` | #6191 |
| CamKiProbe | `78302ba9` | `455cd2f7` | #6192 |
| Setup.exe | `33d7add0` | `d4e8accb` | #6193 |
| Linux pyz | `66f59d53` | `f2f84880` | #6194 |
| full-install | `cc545230` | `37dee7ae` | #6195 |
| combined | `1879010c` | `a9317070` | #6196 |
| Updater | `61474cd1` (unchanged) | `61474cd1` | #6197 |

Training and CamKiProbe have no source change from 1.8.7; their hashes differ only because the build is not byte-for-byte repeatable. Catalogue `2156cc6a` and arenas table `bb7b1125` are unchanged. Full-install check: PASS (28 files). Packages: `D:/nioh3-re/rc188/v1.8.8`, script `D:/nioh3-re/rc188/repack.sh`.

Offline checks: rematch harness 354 checks, 0 failed; mod menu harness all passed; transition cover tests passed; `catalogue --check` in sync; Boss Rematch python tests 11 of 12 pass (`test_sites.py` fails the same way on every earlier base).

## Still needed before anyone can publish

1. Gates: fresh install and an update over the human's ini files with Setup.exe, plus an Ally boot of the Linux pyz.
2. `stable.json` (6 names) with the 1.8.8 news line, then the GitHub tag and the site `stable.json` / `version.json`.

## Not changed from 1.8.7

The 1.8.7 "Not done" list still stands: Dojo support, yin/yang soul cores, crash reports #30, #31, #33 and #34, Leave from a Picture Scroll abandoning the mission, the untested queued exit during a load, and shading shipping off. One known limit of the title exit: after a native Fight now win, standing at the Proceed gate keeps the native replay pending and refuses a new fight until you leave the mission; that is old behaviour and was not changed.
