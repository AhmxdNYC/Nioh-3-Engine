# Yamagata alternate arena and native character preload

2026-10-11. Bounded offline evidence against retained v2.02 dump; no game,
resource request, mutation, build or actor transfer. Current addon e9941b4.

## A second native Yamagata encounter exists, but a second arena is not proved

`boss-master.csv:20,54` and `rematch-enemy-index.csv` join:

| Mission | Placement | Character scalar | Parameter | Area |
|---|---:|---:|---:|---:|
| 1CBC, Escape from Hitokoto Slope | 65092 | 307259 | 22178 | 75 |
| 7F16, The Battle of Hitokoto Slope | 1010621 | 307259 | 22178 | 75 |

The first records in archive_03 enemy files58/E391DE6B5... and5A/A6FA6A800...
were independently read (header00220420, stride138h, record offset8).
Record+40 fourfloats are respectively
`336153.3125,10106.8701171875,206735.9375,-90` and
`335410.625,10104.7587890625,206733.828125,90`.
These are nearby world placements, not evidence of different geometry.
Mission table records34/72 also share area75. This supports an alternate native
encounter for the same character/parameter, not an independent arena picker.
7F16 is absent from the frozen addon catalogue, has no admitted start tuple here,
and its first-play variant/progression script must not be silently substituted.

R0F7F and R0B0B are multi-boss scripts; the first mapped R0F7F Yamagata scalar598115
is different from307259 (`boss-map-all.csv`). Hanzo RC362/RE18C are different bosses.
The old preset note's Shrine request is corrected: frozen ExecDirect892 rejects
atFight Shrine. AtFight Fight now follows the native start without warp/spawn.
None of these routes currently proves moving the intended Yamagata into a new arena.

## Concrete preload input and scoped accounting now identified

Addresses are RVAs, independently decoded using retained .pdata ranges.
No VM wrapper is a native API taking scalar IDs.

- VM Load::Character A429F4..A42A7C parses one argument and obtains the script-
  associated owner through reader+158 -> +8 -> +8 -> +90. It calls
  **A42A7C(owner, uint32 characterScalar)** atA42A46.
- VM Load::CharacterPlEnemy23A22EC..23A23A0 uses that same owner. It calls
  2BD1D8(owner), then2F730C(table,placement). On a found enemy record it passes
  **record+4** toA42A7C. Thus the input for the native Yamagata records is307259,
  not a guessed CN_MASAKAGE asset hash, action ID or placement ID.
- A42A7C..A42ABF calls3F9E58(characterScalar,2,0) atA42A93. It then looks up
  owner+3C8's ordered map viaA42B00, key is signed32 record+1C. Existing entries
  increment count+20 atA42AB6. Cold block1A6425C calls1BFAB8C to insert/access
  the value and writescount1, then returns toA42AB9.
- 3F9E58..3F9FC0 builds a dependency set via3FB62C, requests its resource keys
  through3F9C10, and for mode2 also calls3F9CD8. It handles additional dependencies.
  This is resource-request work, not a synchronous ready/constructed Character
  receipt. Its scope does not create an actor, navmesh or boss script.
- Matching native **1C217A8(owner, uint32 characterScalar)** is called by both
  UnloadCharacter23A2A40 and UnloadCharacterPlEnemy23A2ACC. It finds the owner+3C8
  entry; only if present calls1C1DF80(characterScalar,0), decrements count+20,
  and removes the key through1C112BC when count<=0. No key means no release.

This is an actual paired resource-accounting route and removes the guessed asset-ID
input problem for a future adapter. It does **not** yet authorize retaining a source
owner across mission change: owner resolution, destruction/drain and exact resource
readiness are still needed. A duplicated request must be balanced exactly once in
its original live owner; releasing through a destination owner is not equivalent.

Load::UnloadStage23A2B80 checks globalstage nonnull and stage+11F1==0 before
FB5F98(stage,id). FB5F98 checks stage+504!=-1, operates on stage+500 via4A9D3C,
and releases its local shared pair viaF25E4. That path does not establish that a
separate character preload lease survives a stage/mission transition.

## Next executable-path proof

Use destination-owned preload, after native return/load settles, rather than
keeping a source actor or owner pointer alive across teardown. Trace the concrete
owner returned through current mission script context and its destructor/3C8 drain,
plus a matching ready predicate for3F9E58's character resource. Coordinate that
with the summoning lane's placement registration/construction/disposal contract.
Only then can an adapter acquire307259 in a destination, construct an exclusively
owned Yamagata there, and balance its resources on removal. No source modification
or live admission is supported yet; existing native return/load remains the path
for switching whole native encounters with their own enemies.

## Resource completion and owner teardown: bounded closure

Independent continuation, no native calls. The shipped placement construction path
669110 invokes **669388(wrapper)** before allocation. Exact body669388..6693B5:
load wrapper+30 resource key, call133440(global45B9E60,key), require nonnull
resource and **byte resource+2C8 !=0**. Native already performs this polling;
activation admission is not readiness. Preserve its pending behavior instead of
introducing a second loader or interpreting wrapper+50/E0 as completed loading.

133440 locks the resource map through its+20 critical-section owner, finds the
key, then unlocks before returning a borrowed pointer. This read is not an owned
resource reference usable from an arbitrary worker. Native134314 derives keys
from a character scalar plus300000000h, but can first substitute the scalar under
a game setting through1342E0/1344B4. Prefer the wrapper's actual+30 key; do not
hardcode307259+300000000h as universally equivalent.

The resource acquisition itself,3F9C10..3F9CA0, locks the global resource map,
finds or creates an entry through3F8DA4/3F9514, increments resource+2C0, then
unlocks and returns the entry. Existence/refcount is distinct from+2C8 readiness.
Readiness still does not establish live actor/AI/boss script completion.

**Owner map drain is now located:**1C08D1C..1C08D74 traverses owner+3C8 nodes.
For each positive count+20 it invokes1C1DF80(nodekey+1C,0), decrements and repeats;
then advances with1BFB560 and finally clears the map via1C0D2AC. The leaf
1C1DF80 sets EDX=2, R8D=oldEDX and jumps3F9900, exactly matching preload's
3F9E58(character,2,0) mode/placement scope.

One instruction-boundary-verified direct caller is1C10570..1C105F3, which runs
cleanup helpers using owner+120, clears owner+18 via99D10 and invokes1C28AF0,
then tail-jumps to1C08D1C. Owner destructor1BF9B10 starts by calling1C10570
before freeing its other containers, including+3C8 later at1BF9BE9. Thus
script-owner preload accounting is actually drained on that lifecycle, not an
asset lease that may be assumed to survive a map/mission switch. A manual unload
after native drain could double-release if it bypassed the keyed owner protocol;
never keep and use the old owner pointer after cleanup.

Implementation implication: the existing destination-placement path owns its
own3F9E58(character,2,placement) /3F9900 counterpart and polls669388. Do not also
add script-owner preload for that route. For a genuinely independent destination
actor, request resources in the destination owner only after its native load;
construct and dispose inside that owner's lifetime. The matching native resource
gate is identified, but establishing a valid destination owner borrow and the
fresh-child construction/disposal transaction remains the summoning-lane gate.
No resource reference crosses a mission transition in this proposed design.

## Predicate-hold peer review boundary

The first summon prototype can instead preserve the original encounter script.
Exact call139863 inside VM Player::IsEnterPlacementBox sends
RCX=1363D4(boxID), RDX=PlayerChar(0)+110, R8D=boxID to137B24;
AL is consumed at139868.137B24 looks up137C1C(owner,boxID), copies the16-byte
position and calls138214's shape predicate. Only this callsite's positive result
for owned fresh mission1CBC/start67064/box66599 may be held.

1363D4 resolves the keyed native entity/placement ownership, briefly upgrades
native weak references, derives the table at retainedobject+D8, releases its
temporary strong reference and returns that borrowed table; it may use2BEC64
fallback. This is NOT the VM's preload owner+90. No pointer should be retained by
the hook beyond the current native call.137C1C can use table's owner+480 placement
records or its native fallback; mission identity alone does not manufacture owner
lifetime. Original call first preserves native validity/geometry.

The hook must be active and armed before the request. Any read/epoch uncertainty
that permits a relevant native true through must permanently end that attempt's
hold: the script may already have passed its wait into rights/intro/Create.
Never recover from that gap by later advertising AwaitSummon. Releasing the hold
permits the original script to run; it is not a newly allocated actor, repeatable
respawn, or independent-arena transfer. Creation/AI/control completion still need
the existing attempt-bound native witnesses.
