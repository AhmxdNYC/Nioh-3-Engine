# Sequential multi-boss fallback — #71960

Base `origin/pc/near-boss-71644` 9f94c77e; branch `cxl-pc/fallback-multiboss`. Offline build only, no game/install/save writes.

## Behavior

Only the native-timeout fallback is armed with a generated roster. The first named boss replaces the single selected enemy in the temporary ArenaRow; subsequent bosses use the existing resource-loading summon path and the same saved arena boss position/facing. A confirmed kill must match the current tracked handle. It clears the old dynamic boss/HUD state, waits 1 second, then requests the next boss. It does not heal/warp the player or start another fight-history record between bosses. Retry returns to the first member. Player loss/quit cancels progression; quitting cancels a pending next-boss spawn. A native win callback cannot finish this fallback session before the final member has a confirmed kill.

Late-native dedupe applies the existing exact world/placement/character/live/player/tracked-actor guards to each roster member. Deferred deletion postpones the next dynamic spawn. No generic same-character removal, story flags, quest progress, mission-clear writes or native wave requests were added. Native route and single-boss dispatch retain their existing behavior; the existing arena TSV is unchanged.

## Generated data and limits

`tools/gen_fallback_rosters.py` joins catalogue boss names -> `notes/boss-names.csv` character identity -> same-mission `notes/rematch-enemy-index.csv` spawn key64 and placement. Runtime Training requires key64, not the name-table char_id. The header is generated, not a hand-maintained ID list. Each candidate and selection is recorded in `notes/fallback-rosters-71960.json`.

25 catalogue multi-boss rows: **23 resolved**, **2 unresolved**: **R0C12, R945F** (multiple distinct same-mission Hiruko boss variants). Unresolved rows explicitly refuse fallback rather than report a partial win. Equivalent placements with the same spawn key choose the catalogue placement when available, otherwise the lowest placement ID; all candidates remain in the evidence JSON. This deterministic placement choice and late-native behavior need live validation.

R0034 now requests Enenra (488671, placement74751), then Jakotsu-baba (947280, placement76781). These values are generated joins, not runtime hand lists. Order is the catalogue order and **sequential fallback policy**. The existing script audit (`notes/waves-71532.md` on cx-pc/waves-71532) does not establish simultaneous/sequential scheduling for every row, so this does not claim to reproduce native wave scripts or ordinary-enemy interludes.

## Offline validation

- `fallback_roster_data_test.py`: 5 tests pass, including source coverage, same-mission joins, ambiguity and correct spawn-key selection; generator `--check` passes.
- `fallback_sequence_test.py`: compiles actual production helpers with mocked game services. All23 resolved rosters progress through matching kills, exactly one final win; unrelated kills, intermediate native wins, loss/quit, deferred dedupe, native/single passthrough and unresolved rows checked. Source integration checks cover retry/stop/reset and entry selection.
- `native_entry_test.py`: pass.
- `native_entry_return_test.py`: pass using portable MSVC wrapper; initially unavailable `cl` was resolved by setting the toolchain environment.
- `build.bat`: Zig Windows DLL build; final result and bridge artifacts recorded below.

Live validation belongs to pc/0: begin with R0034, kill Enenra then Jakotsu-baba, verify no intermediate win, same spawn spot and one final win; retry/loss/quit between members; then a larger roster and late native duplicate case. No game was launched here. Resource loading/AI, mission-driven early return, corpse transitions and saved-progress invariance remain unproven in game.

Final build.bat exit0, TR SHA256 0aea2764c5173a7831195d1b0bde2fd5ddb20a6a127b5293afc705c345167323, ASI bridge#5419. No other component built or installed.
