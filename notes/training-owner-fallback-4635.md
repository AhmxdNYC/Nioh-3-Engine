# Boss owner fallback when no live registered actor exists (bridge 4635)

2026-10-14, Ally. Scope: make a boss active where PickOwner() finds no live registered non-player placement (owner = player -> passive, e.g. Nuribotoke).
No game run and no code change in this pass: the candidate fixes below fail on static evidence, so there was nothing to test in game.

## Static findings
- `PickOwner()` (src/training.c ~168) returns a placement id only if the mission registry (c0+0x48) holds a live, non-player registered actor. Otherwise it returns 1 (player) and CreateDynamic yields a passive boss (ARENA WARNING path, ~line 317).
- Helper-spawn fallback does not work: the handle returned by CreateDynamic (`slot<<28 | ownerPlacement`) is NOT in the charMgr registry (notes/training-live-spawn-4318.md, despawn row). A spawned helper therefore cannot be used as an owner placement.
- Boss's own placement is refused as owner (training-fightnow-arena-4506.md), so a second CreateDynamic with self-owner does not help.
- Hate does not drive notice (training-aggro-fightnow-4616.md): changing hate is not the fix.
- The working owner in Rajomon is a level-placed live actor (placement 0x2538 = 9528, `actors` dump in /tmp/boss-4616/OFF-training.log). It exists because the level places it, not because the mod creates it.

## Conclusion
A PC-style case with no live registered placement in the mission has no valid owner id. Changing PickOwner cannot make that boss active.
Remaining options, all unverified: (a) find the actor field that stores the owner/team binding and write it after CreateDynamic (RE of the create path 0x1C0C5E0 around the owner argument); (b) pick a live registered placement from another mission record that is alive in the same world.
Needed to decide: PC's `actors` dump and registry (c0+0x48) listing for the Nuribotoke state, and the owner field offset.
