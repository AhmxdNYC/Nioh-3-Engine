# Favorite-first lists (#83430 / human #83427)

1.8.12 candidate from 1.8.11 source 78685099; no installation or release changes. The existing eligible display rows are collected, stably partitioned into favorite/nonfavorite groups, then copied to the bounded menu output. Filters, settings and Back retain their positions; catalogue IDs/order are unchanged. This covers Bosses, Boss Rematch, Train here and Training arenas, including Scrolls and other filters. Bosses browse now recognizes favorite toggles (previously the row mapper omitted it). Existing separate B/T favorite namespaces and persistence remain.

UiChange rebuilds after marking/unmarking and relocates selection by the original item ID. Removing a row from the Favorites-only filter falls back safely because that row is no longer visible.

Release build PASS. All 28 BossRematch Python offline entry points attempted: 25 PASS, 3 pre-existing limitations below. New coverage: 26 rcore assertions (383 total harness checks PASS), 32 actual-source list/cursor cases PASS; both complete production controller-menu harnesses PASS. Tests include stable subgroups, promotion/demotion cursor, empty/all/none, separate B/T favorites, Scrolls filters on all 4 pages and sort-before-capacity truncation.

Rebase brought the upstream item29 stub repair; that suite now passes (runtime/build source unchanged). Remaining baseline limitations (unchanged inputs verified against 78685099): test_sites.py reports 10 ambiguous optional probe pattern checks; formhide4_offline.py requires absent g++, and manual MSVC attempt also exposes obsolete fixture stubs; empty_spawn_replay.py lacks Temp/h72299/BossRematch.log. These are not reported as passing and no unrelated fixtures were altered. Detailed logs retained in main checkout Temp/fav83427-tests.

BR SHA256: f536bbc90ca04ff8e91dde42beb5602e79a6f66d9ef2a3bca376890716538488
Next: pc owns 1.8.12 integration and live menu/screenshot validation after 1.8.11 gates; do not substitute this for current 1.8.11 assets.

Offline illustrative before/after preview sent to Discord; sample boss names, explicitly not gameplay.
