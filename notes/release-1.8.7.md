# 1.8.7 release notes (draft, not published)

Compared public 1.8.6 source `pc/release-186` `f60c56f9` with `origin/pc/final-next` `551d4663` (160 commits, f60c56f9 is an ancestor). This file is notes only. No version bump, no installer, no Linux package, nothing published.

EngineFix still says `MOD_VERSION "1.8.6"` (`mod/Nioh3EngineFix/src/dllmain.cpp`).

## What players get

Boss Match list

- The list cycles All, Multi, each era, and Picture Scrolls. The filter stays after the menu closes. Multi counts the roster members, not the row.
- Fight again says when that choice reloads the mission, and when it stays in the same arena.
- Held controller and keyboard repeats work on the Boss Match pages. If the menu refuses a press, it retries that press for 400 ms.
- The Idle row is hidden. Train here is no longer mixed into the move list.
- A duel row whose mission title is Dummy shows the real boss name.
- Four dojo lessons are marked in the list, and the lesson line is drawn on the play screen. The lesson cover drops if the teacher stays dormant or the player is hit.
- Picture Scroll Fight now starts that scroll's own record. A redirected map no longer sends every key to an empty byte. A load that never finishes can be left.

Fight now and native missions

- Fight now on an unbeaten boss uses the real mission and skips the game's mission-clear write, so the story clear is not written. It refuses if that clear guard is missing, and it refuses the mission you are already playing. The old ini switch `IgnoreProgress` stays 0 and is not required.
- The one-mission native allowlist is retired. Mission Fight now loads the native mission, then enters the arena. A missing boss gets one arena fallback after 30 ready seconds.
- The loading cover stays up past 90 seconds while a load or entry is still pending, including while the load finishes. It drops when hazard recovery gives up, and on a lesson when the teacher never wakes or you are hit.
- After landing, settle is 1 second and the near-boss nudge is 1 second (was 3 s and 10 s) when no fight has started and the boss is out of duel range.
- A fall during Fight now sends you back. Dormant script bosses are left alone. Infinite-health practice no longer dies to the native hazard, and the camera resets. Repeated hazard recoveries are bounded so a native respawn and finisher can still happen.
- The boss door stays the right door when Training owns the destination.
- A multi-boss fight stays open until each generated member is defeated. The next member is requested after 2 seconds, not 8. An unmarked member can wake instead of blocking the next boss. A win is held across the next script phase, including a short load.
- Warp-setting wave starts are counted, and a queued exit is held through a short load.
- A Ki-break slot resets when a new boss reuses the same actor address.
- After a real kill, with nothing left alive, a stuck boss mode is released. The 8 second window and the "no early win" rule stay.
- A fighting roster body gets a boss bar when the script never showed one. If another boss's bar is up, the new request does not burn the 5-try cap (3 probes, then wait). A refused bar is retried every 1.5 seconds.
- The Death Veil entry cover from the Ally pass is in this line.

Soul cores (left out of public 1.8.6 on purpose; in this line)

- A form binds only an actor spawned after the press.
- No 8x speed leak, a quick release when no yokai appears, and a late or far seal does not grab the player.
- Orb-confirmed uses, no pop before a late yokai, and learning only from a complete form.
- A zero-motion spawn on the live seal is ignored. A motion-0 spawn under 1 m is kept. A body that was already kept stays kept when the spawn note is replaced. Seal actions are confirmed before the item animation is boosted.

Practice and reports

- Repeat last move, with guards against an unsafe leap capture and against cycling a death state.
- Crash uploads add a bounded register and stack dump, the settings, the Training log tail, and the list of loaded modules.
- The report fight timeline keeps stable boss identities.

Graphics (optional; public 1.8.6 shipped none of this)

- Default preset is still 0. Nothing here turns itself on.
- Render distance slider 0.50 to 1.00, separate from presets. 1.00 is the game. A lower value hides distant characters, entities, and grass.
- Extreme (preset 4) no longer crashes on boot and no longer leaves a brown sky or strobe. Extreme HQ (preset 5) adds the sharper fog, SSR, and sharpen options. Half-rate GI, cloud, and fog (XHALF) and the render-size cut (XRES) are the speed options behind presets 3 and 4.
- Variable rate shading has a next-launch menu choice and a device check. The ini ships it off (`VrsCeiling = 0`, `VrsRate = 0`). Characters and unknown draws stay full rate when the audited world-only gate is used. Treat this as experimental. Do not turn it on in the public default.

## Candidate build (not a release package)

These are the hashes named for this draft. Camera is the same file as public 1.8.6.

- Boss Rematch `04194151` (public 1.8.6 was `fe7ec45d`)
- Training `2a22b522` (public 1.8.6 was `04781485`)
- EngineFix `bd6bd92b` (public 1.8.6 was `1f8d71b6`)
- CamKiProbe `7482c03d` (unchanged)
- Settings catalogue `da86d65d`

Not named, and not produced for 1.8.7: Updater, Setup.exe, full-install, combined zip, Linux pyz. Public 1.8.6 still has Updater `61474cd1`, Setup.exe `fc2efec2`, full-install `ee7bf920`, combined `83053563`, Linux pyz `6806b4c0`.

## Still needed before anyone can publish

Copied from the 1.8.6 ship log. None of these were done here.

1. Version bump. Set EngineFix `MOD_VERSION` from 1.8.6 to 1.8.7, then rebuild. The hashes above are the pre-bump candidate, so a bump makes new hashes.
2. Installer and repack, same shape as `D:/nioh3-re/rc186/repack.sh`: full-install, combined zip, Setup.exe, and `stable.json` with the 6 names (1.8.1 updater compatible).
3. Linux pyz, built and checked. Public 1.8.6 published while Ally pyz check #67424 had not been run. Do not skip that check again unless the lead says so.
4. Gates. 1.8.6 used a pc fresh install plus an update over the human's ini files, and an Ally boot. 1.8.7 has no gate yet.
5. Publish. GitHub tag and the site `stable.json` / `version.json`. Not done.

## Do not ship as player features

Debug or test only. Leave them out of the player changelog and do not flip their switches in the public ini.

- `da3a46e5` `[Dev] IgnoreProgress=1` test switch for starting an uncleared mission. The published ini keeps `IgnoreProgress = 0`. Unbeaten Fight now does not need it.
- `28686e09` test-only near-boss move in the native entry harness.
- `67a4cd9d` harness-only cover of one four-boss row.
- `62f4a9ae` harness-only BossAnchor fields.
- `cbdef651` harness fixture refresh.
- `3d09e4b9` pump-harness refresh.
- Compiled blobs only, not a package: `6949d971`, `850177f4`, `d6428e8d`. Many other commits also store `Nioh3BossRematch.asi` in git. The release build is a fresh compile, not those blobs.
- Variable-rate-shading capture and mock commits (`b747461d`, `2a0418b2`, `6300a166`, `c605cc7d`, `f5525748`, `81835266`, `9388b10b`, `09bf7db4`, `ce702b92`, `3260c91f`, `8386a19e`, `558bb2a0`, `64fd44ff`, `bfaab996`). The menu commit `f5157ec1` is the optional off-by-default setting above. The capture TSV and the ceiling mocks are test gear.
- Checkpoint and handoff notes with no player change: `5f159cb2`, `f82248b8`, `a6dae2fa`, `caa858a3`, `71ea9f0b`, `b9ad81b4`, `bcdad8fa`, `0a370d6b`, `70d6b027`, `beae6182`, `62070822`, `a7f27be0`, `bd769708`, `99ba2481`, `928e021d`, `cdae6b9e`, `61d07b05`, `eb97e527`. `a6974c72` is only the Extreme HQ bench recipe note.

## Commits not explained

None. All 160 commits are one of: a player change in the lists above, a merge of two of those commits, an internal checkpoint, a test or harness edit, or a compiled blob.

## Known issues

- This draft is not 1.8.7 until the version bump, the pack, and the gates exist.
- Soul cores and the speed presets were kept out of public 1.8.6. They are in this source. The lead still chooses whether both go out in 1.8.7.
- Variable rate shading is off. Turning it on changes how the world is shaded and still needs a picture check.
- A lower render distance changes how far the world draws. The default stays 1.00.
- The Ally Linux package check was not run for 1.8.6, and nothing in this draft replaces it.
