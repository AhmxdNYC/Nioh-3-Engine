# 1.8.7 release notes (draft, not published)

Compared public 1.8.6 source `pc/release-186` `f60c56f9` with the merged candidate `origin/pc/final-187-79191` `ecbd9d4d` (the first 160 commits up to `551d4663` as before, plus 40 newer ones, 34 not merges). This file is notes only. No version bump, no installer, no Linux package, nothing published.

EngineFix still says `MOD_VERSION "1.8.6"` (checked on `ecbd9d4d`) (`mod/Nioh3EngineFix/src/dllmain.cpp`).

## What players get

Boss Match list

- The list cycles All, Multi, each era, and Picture Scrolls. The filter stays after the menu closes. Multi counts the roster members, not the row.
- Fight again says when that choice reloads the mission, and when it stays in the same arena.
- Held controller and keyboard repeats work on the Boss Match pages. If the menu refuses a press, it retries that press for 400 ms.
- The Idle row is hidden. Train here is no longer mixed into the move list.
- A duel row whose mission title is Dummy shows the real boss name.
- Four dojo lessons are marked in the list, and the lesson line is drawn on the play screen. The lesson cover drops if the teacher stays dormant or the player is hit.
- Picture Scroll Fight now starts that scroll's own record. A redirected map no longer sends every key to an empty byte. A load that never finishes can be left.
- Picture Scroll rows read "Picture Scroll - <place>" on the left, with the full label, so the 20 rows can be told apart. The Picture Scroll rows no longer show "Waves unverified" or grey out.
- Favorites: Right marks a row and Left unmarks it. Marks are kept by row id in `Nioh3BossRematch_favorites.txt`.
- The Training lists have the same list filter.
- Mod menu bumpers: LB mirrors RB around Home, the lit tab is the tab shown, hosted tabs keep their page, row and filter, and the Unavailable column fits.
- The Keian Noble Bouts row (RBAB8) is refused with the usual reason, because only one of its three fighters can be hit and the mission would not end.

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
- Rules that were written into the program (arena cells, wave wake step, scroll mission) now come from the catalogue and arena tables. This should change nothing for players; it was compared on 234 arena rows with no differences.
- Training no longer risks a crash about 40 seconds after start when a game thread ran a hook before it was ready (a hook set up in the wrong order). Unproven in the game.

Picture Scrolls (new since the first draft; offline only, nothing here has run in the game)

- Fight now on a scroll goes straight from the loading screen into the waves. The old 8 second fallback is gone. The screen ends when the first wave starts, when the search for the start finds nothing, or at a 75 second safety limit, and the log says which. All 20 rows have a recorded wake spot; the old walk with a 4 second dwell is the fallback.
- Fight again keeps the mod's loading screen during the trip home (it showed the game's tip screen for about 72 seconds before). The scroll's end object opens the mod's own page (Fight again / Leave).
- Training works at a scroll without starting its waves, and the end menu has Training rows. The scroll's waves are skipped for that trip only.
- An empty scroll bag no longer locks the shrine return. The trip save waits until the resume mission is home, so a restart does not reload the trip.
- A scroll run no longer binds to inventory slot 0; it uses the game's no-item path.

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
- Variable rate shading has a live page in the mod menu (Optimizations > GPU > Variable rate shading): one row per value plus Reset. Each row says live, next launch, or Selective only. A change in Whole frame mode reaches the next draw; Selective needs a restart. It also has a next-launch menu choice and a device check. The ini ships it off (`VrsCeiling = 0`, `VrsRate = 0`). Characters and unknown draws stay full rate when the audited world-only gate is used. Treat this as experimental. Do not turn it on in the public default.

## Candidate build (not a release package)

Built offline from `ecbd9d4d` (version still 1.8.6, no bump). None of this has run in the game. Bridge files: EF #6119, BR #6120, TR #6121, CamKiProbe #6122, catalogue #6123, arenas table #6124. Camera has no source change from public 1.8.6; its hash differs from the installed `7482c03d` only because the build is not byte-for-byte repeatable.

- Boss Rematch `fc95796d` (public 1.8.6 was `fe7ec45d`)
- Training `138e448c` (public 1.8.6 was `04781485`)
- EngineFix `9f09e937` (public 1.8.6 was `1f8d71b6`)
- CamKiProbe `6f8dc5b8` (public 1.8.6 was `7482c03d`)
- Catalogue `2156cc6a`, arenas table `bb7b1125`

Offline checks: rematch harness 350 checks, 0 failed; mod menu harness passed (settings pages, Boss UI, both controller menus); transition cover tests passed; catalogue `--check` in sync; Boss Rematch python tests 11 of 12 pass. `test_sites.py` fails the same way on the earlier base `63465f6d` (probe sites match 2 to 3 times) and was not touched. `test_verify_identity.py` was stale and is fixed on `cx-pc/final-187-tests-79308`.

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

- Nothing new since 1.8.6 has been proven in the game from this commit. Picture Scroll straight-into-waves, the Fight again screen, Training at scrolls, favorites, bumpers, and the live shading page are all offline-tested only.
- The Dojo work is not in this build.
- This draft is not 1.8.7 until the version bump, the pack, and the gates exist.
- Soul cores and the speed presets were kept out of public 1.8.6. They are in this source. The lead still chooses whether both go out in 1.8.7.
- Variable rate shading is off. Turning it on changes how the world is shaded and still needs a picture check.
- A lower render distance changes how far the world draws. The default stays 1.00.
- The Ally Linux package check was not run for 1.8.6, and nothing in this draft replaces it.
