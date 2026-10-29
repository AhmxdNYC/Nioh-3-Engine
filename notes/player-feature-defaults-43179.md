# Player feature access audit (#43179)

Offline audit on 2026-10-28, branch cxl-pc/no-relaunch-43116. Installed player session was not changed.

| Feature | Stock access and action |
|---|---|
| Save manager | Enabled/RealRoot/LiveTitle default 1 with Python/helper discovery and verified initial library copy (d94e199f). Root Save files row now remains visible during gameplay. Selecting it outside idle title explains returning to title; file-operation guards remain unchanged. |
| Boss Matches / Practice | Fixed Rematch Enable default 0 to 1 in stock INI, both startup checks and settings catalogue. This loads catalogue and registers callbacks without an INI edit. Explicit 0 remains respected. |
| Frame meter / ring / size | Display Overlays > Frame meter contains enable/style/size rows unconditionally. Live setter persists changes; hook installs when enabled. The optional overlay may start off without hiding its controls. |
| Hit labels / stance / move repeat | Existing boss Practice menus expose the move list and training controls; hit labels are part of those lists, not a separate startup feature switch. |
| Death blow / kill animation / loot | Existing Boss Matches controls call the live Training setter and persist. Individual choices are not forced on; a reachable option is distinct from enabling every mutually exclusive behavior. |
| Controller / input / FPS overlays | Display Overlays subpages are unconditional. Controller and input panels can be enabled live; FPS defaults on. |
| Camera / time / weather | Existing Camera settings and Display Overlays controls remain available; Training addon supplies live APIs. |
| Update page | Root row unconditional, update check defaults 1; network is not required to show the entry. |
| Performance controls | Existing Settings CPU/GPU/Game/Frame Gen pages remain. Unsupported/platform-specific and uninstalled experimental hooks retain their guards. Frame Gen is native-game status/settings, not a mod toggle. |

Kept off: virtual pad, unknown-build bypass, test instrumentation, reserved Training mutation, unverified routes and experimental hook options. No blanket enabling of diagnostic or experimental INI keys.

Also reconciled two stale settings-catalog metadata rows with the existing generated header (loading prompt wording and input history 20 rows, maximum 32), preserving current behavior.

Validation: native save safety harness and both real menu harnesses, stock catalogue consistency, MSVC EngineFix and BossRematch builds. Fresh-install in-game acceptance remains pending; this task explicitly forbids touching the running game.
