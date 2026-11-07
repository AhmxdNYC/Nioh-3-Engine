# 1.8.7 release notes (release candidate, not published)

Compared public 1.8.6 source `pc/release-186` `f60c56f9` with branch `cx-pc/release-187-prep` (= `origin/pc/final-187-79191` `43a7cc7f` + one version-string commit). Nothing is published: no GitHub release, no site change, no stable.json.

EngineFix now says `MOD_VERSION "1.8.7"` (`mod/Nioh3EngineFix/src/dllmain.cpp`).

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
- Training at a Picture Scroll no longer crashes. The log line for that request read past the name table (crash #79507 on the main PC). Fixed offline; unproven in the game.
- The Saves tab is only on the mod menu tab strip where its page can stay open, so the bumpers no longer land on a page that closes. Unproven in the game.
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
- Shading is one On/Off switch in the mod menu (Optimizations > GPU). On is the preset: whole-surface layouts, the coarse rate, and both character protections on. There are no other levels or value rows for players. It ships Off. Measured at +4 to +12% fps by the lead (device-specific); the picture check in game is still open.

## Release candidate build (not published)

Built offline from `cx-pc/release-187-prep` (`43a7cc7f` + version commit `face24af`). Nothing has run in the game. Packages sit in `D:/nioh3-re/rc187/v1.8.7` (script `D:/nioh3-re/rc187/repack.sh`, same shape as 1.8.6). Updater and the camera ini are the 1.8.6 files (their source is unchanged).

| File | 1.8.7 candidate | public 1.8.6 | bridge |
|---|---|---|---|
| EngineFix | `77bfd278` | `1f8d71b6` | #6150 |
| Boss Rematch | `f391f3b2` | `fe7ec45d` | #6151 |
| Training | `3622a1b3` | `04781485` | #6152 |
| CamKiProbe | `455cd2f7` | `7482c03d` | #6153 |
| Setup.exe | `d4e8accb` | `fc2efec2` | #6154 |
| Linux pyz | `f2f84880` | `6806b4c0` | #6155 |
| full-install | `37dee7ae` | `ee7bf920` | #6156 |
| combined | `a9317070` | `83053563` | #6157 |

Also: Updater `61474cd1` (unchanged), catalogue `2156cc6a`, arenas table `bb7b1125`. Camera has no source change; its hash differs only because the build is not byte-for-byte repeatable. The full-install check printed PASS (28 files).

Offline checks: rematch harness all passed; mod menu harness all passed; catalogue `--check` in sync; Boss Rematch python tests 11 of 12 pass. `test_sites.py` fails the same way on earlier bases (probe sites match 2 to 3 times) and was not changed.

## Still needed before anyone can publish

1. Gates: a pc fresh install and an update over the human's ini files with Setup.exe, and an Ally boot of the Linux pyz (do not skip it again). Not run.
2. The lead's condition: the crash fix is in the human's game and the pc2 retest passes.
3. `stable.json` (6 names) with the 1.8.7 news line, then GitHub tag and site `stable.json` / `version.json`. Not done, not touched here.

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

## Not done in 1.8.7

- Dojo support is not in this build.
- Yin/yang soul cores are not done.
- Four rare crash reports with unknown cause (#30, #31, #33, #34) are not explained.
- Leave from a Picture Scroll abandons the mission.
- A queued exit during a load is untested.
- Shading ships off by default.

## Unproven in game at the time of writing

Everything new since 1.8.6 was tested offline only from this build: Picture Scroll fights straight into the waves, the Fight again loading screen, Training at a scroll and its crash fix, empty-bag Return, favorites, the mod menu bumpers and Saves tab, the shading switch, the render distance slider, and the catalogue and arena data moves. Re-check this list before publishing.

## Known issues

- Soul cores and the speed presets were kept out of public 1.8.6. They are in this build.
- A lower render distance changes how far the world draws. The default stays 1.00.
- The Ally Linux package check was not run for 1.8.6, and nothing in this draft replaces it.
