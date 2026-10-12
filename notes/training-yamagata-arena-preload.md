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

## RC362 destination construction and producer gate (bounded follow-up)

A genuinely different candidate is RC362, The Demon's Regimen: mission C362,
start 1107817, native boss placement 1102178, character scalar 131751 and
parameter 23230. Its current supported entry is Fight now (native atFight), not
Shrine. This is an occupied Hanzo encounter. A Yamagata child here would initially
be a second actor, not a clean Yamagata-only arena; Hanzo's script, AI and mission
completion remain independent blockers. Killing/removing Hanzo is not a proved
way to retain his actor-owned construction context.

The retained metadata file
`data/extract/lfm/archive_03/C4/E1CBAE0841F7ECE87BD581BA0998F242FE69C4449DB57A473D072E0EB85FAC`
has header 8, count 1022 and stride 398h. Record 60 has key+64=307259 and
parameter+8=22178 (Yamagata); record 102 has key 131751 and parameter 23230
(RC362 Hanzo). Both have words +382/+384 = 0/6. Native 134358 performs the
key-to-record lookup in the table selected by 1342E0; the latter takes no
character scalar argument. Native wrapper registration 4BC904 uses these counts
for child wrappers. Thus the dynamic route can source Yamagata's character
metadata without borrowing source placement 65092 across the load. It still
requires the destination's actual registered, unused child and live owner.

Native DynamicCreate 1C0C5E0..1C0C745 checks the character resource's +2C8
ready byte (1C0C668), resolves the destination owner ID through 13684C, requires
wrapper[0] live, constructs a fresh request with 683FE4, and derives its owner
field +60 from destination live+B0->+10. It does not clone an absent source
placement record. Resource-ready is not script/AI readiness, nor proof that this
owner survives the deferred consumer.

Two optional request arguments must not be guessed as zero: request +70 is an
initial action override (65AD6B -> 66C330 -> 66C4A8); request +DC passes through
66AD88 to live+3B0->+50->+190C, whose gameplay meaning is unresolved. Constructor
683FE4 initializes both to -1 (684059 and 6840C2); the consumer skips those
branches at the native defaults. This narrows a future constructor-backed
adapter, but does not authorize directly calling the higher DynamicCreate with
invented parameters.

The exact producer is not a general thread-safe queue. 683F74..683FA6 resolves
manager 4745348 and calls 9DCE70 with manager+BC0. The latter validates that the
character metadata exists, then increments a global serial using ordinary
load/inc/store at 9DCEB3..9DCECC. 9DCF6C..9DCFDF allocates/copies a node,
increments the list count and writes four linked-list pointers without a lock
in that function. No lock is acquired in 683F74 or 9DCE70 either. This establishes
an external serialization prerequisite, not that the whole engine is unlocked.
An arbitrary addon worker must not become another producer. The native producer
phase and its ordering against actor-owner teardown still need closure.

There is also a concrete exhaustion trap: 1C1CAB8 scans and reserves owner+D0
bits from metadata+382+1 up to min(metadata+384+1,16). When every eligible bit
is occupied it returns the loop limit, not an explicit failure sentinel.
1C1CC10 still shifts that result by 28 and ORs the owner ID. For RC362's 0/6
metadata, exhaustion can therefore produce slot 7 without reserving any slot.
Nonzero generated ID is not a reservation receipt. DynamicCreate also returns
the generated child ID after discarding 683F74's enqueue result. Any future
transaction must distinguish registration/free-slot reservation, enqueue token,
materialization and owned disposal, including rollback of a reservation after
failed enqueue.

These facts make a destination-owned prototype more specific, but do not supply
a clean arena or a lifetime-safe producer seam. The next implementation-enabling
edge is an existing destination script phase that owns both producer ordering
and a live construction owner without simultaneously starting Hanzo's encounter;
it must be established before adding a new native call. The current entrance-hold
prototype deliberately avoids this entire dynamic allocation path.

## Cleaner destination and native update seam

The narrow comparison now favors **native Training Ground 0x6F24 (28452)** over
RC362 for a future independent-arena prototype. `notes/rematch-fight-index.csv`
provides start 606826, default index 0, at 76559/3555/56559 yaw -157; alternate
608718 has condition 908 and is not a substitute. This is static placement data,
not a current-journey clear receipt or an accepted addon catalogue route.
`Mission::IsTrainingMission` actually recognizes 6F24 (see battle-scrolls.md).

Retained script A47086B78FCD has no dedicated incumbent boss in its
mission-specific section. It does have native training behavior: gadget-selected
Create calls for 605895/E08A, 602231/E270, 608021/E442 and 601766/E614, plus
607917 in a separate demonstration loop. For example E03C..E058 tests gadget
603945 status against 2 before the first creation. These native selectors must
remain untouched; the map is not empty. Setup also changes training HUD/geometry
visibility, supplies tutorial items and replenishes gauges. These are native
Training Ground semantics, not a neutral arena stripped of scripts. The next
bounded prototype should first enter this exact native route and verify return;
a Yamagata request still requires destination resource ownership and disposal.
No arbitrary clear flag, start condition or actor ID is authorized by this data.

RC362 is worse for replacement: B023CC05F9E5's mission-specific section waits
on box 1103060 (CED8), obtains the native boss placement, changes its Algo/aware
state, waits on the character group, then requests action 152 (CF70) and boss
BGM/voice. Deleting or freezing Hanzo does not cancel these script waiters or
establish clean encounter ownership. No exact native stop-and-replace protocol
was found in this bounded script review.

The dynamic consumer's update phase is now exact: call 39E5D6 in
GameModeUpdateMain 39E4E4 invokes 5BC970, which resolves character manager
4745348 and calls 5BD168. At 5BD1A0 it drains manager+BC0 through 5BCBAC;
that iterates pending nodes and invokes 65A8A8 at 5BCBEE. The character task graph
allocation/enqueue follows later in 5BD168. Correction after tracing the actual caller: 565288 calls 39E4E4 first;
565294 then calls 5A29F0; the addon pump at 56529C precedes its original callee
3B29E4. Thus the addon pump is AFTER the world update/queue drain, not before it.
The retained game-logic audit places the previous MissionThread join at frame
start and the next kick at frame end. None of this by itself proves producer
exclusion or prevents owner teardown between phases. The main update runs on
KIDSTaskScheduler0; calling it the operating system's main thread would overstate
what is proved. Native VM DynamicCreate callbacks remain indirect table entries.

A superficially simpler producer, 1C0C478 (called by state callback 21BFA48), is
**rejected as an arbitrary boss constructor**. Although it preserves the enqueue
token and needs no live parent actor, its A5D770 helper maps selectors 0/1/2/3 to
fixed actor IDs 1/10/11/12, and all other selectors to zero. It is not an allocator.
Reusing that path with a guessed selector could target an occupied special slot.
The destination-owned child route's reservation/resource/lifetime requirements
therefore remain; no native construction call is ready merely because a cleaner
map and the queue-consumption phase have been identified.

## Training Ground admission and the unselected-parent limit

For the first native scene switch, retain the current offline/queue/attempt gates
and request mission6F24/start606826 with the actual current MissionState+10 byte.
Use the existing exact-entry return transaction. Static replay-list eligibility
and a default start do not prove that this user's current journey cleared it.
Pinned clear helper702B64(db,id) finds the mission node, invokes2853FC(db), then
reads node+38+selectedIndex and ANDs bit1. The selector helper reads db[0]; only
selector6 substitutes current MissionState+10. Other selectors1..5 select their
own journey (unrecognized values fall back to1). Therefore a future explicit
Training Ground preset must verify current-selector provenance, or refuse, before
using that clear bit as current-journey evidence. Do not alter the clear DB to
make it pass. Frozen addon catalogue has no admitted 6F24 preset, so this is a
concrete next scene-entry implementation, not an already available pipe command.

Keeping native training gadgets unselected rules out borrowing an unmaterialized
dummy as DynamicCreate's parent: 1C0C67E..1C0C68D explicitly requires wrapper[0]
nonzero. Registering six child wrappers does not supply a live parent. Starting a
dummy just to obtain its context would violate the selected empty-training-area
scope and introduce its own native respawn script.

The separate native CreateDirect path can avoid an incumbent parent, but cannot
be reduced to the existing child queue call. At238CB02 the VM wrapper extracts
destination script owner+90; +120 supplies ownership to the temporary registered
wrapper at238CCAF..238CCC3. It allocates a root ID through1C1CB40, constructs and
initializes the live object through2BFA04 and the following component operations,
then attaches the native tracked reference and registers via4BC904 before
destroying the temporary wrapper. No cached source VM owner is valid here.

Root-ID allocation is now mechanically bounded:1C1CB40..1C1CC0E cycles4000..4999
in manager[0]+16B8, with 1000 occupancy entries at+16BC. For nonzero recorded
character scalar it checks both registry135B78 and typed live lookup20A93C before
reuse; on success it writes the requested scalar into that slot. After1000 busy
checks it returns0. These are global manager-owned reservation writes without
local locking, not a mod-owned free list. A failed factory/registration must not
be treated as no mutation. The exact destination script-phase borrow, complete
initialization and registered-root disposal remain to be closed before invoking
this adapter. This is preferable research to pretending a special player slot
or unselected dummy is a valid arbitrary enemy owner.

### Completed-request removal drain located

The previously open BD0 disposal edge is now traced through actual instructions.
At5BD194, before pending creation drain5BD1A0, native5BC9A0 processes manager+BD0.
It acquires list+10 via48B44, copies tracked live pointers to a temporary list,
clears the original list via5BCAC0 and unlocks viaB61524, then invokes2BF7E8 for
each retained pointer at5BCA73. The copying phase includes native temporary-list
sorting/iteration cold paths; no mod should reproduce its containers manually.

2BF7E8 resolves the current manager and calls2BF818(manager+B98,live). The latter
computes the factory bucket from live type/ID, optionally informs the native AI
removal queue, and calls2BFC90. That helper finds the exact live+E0 intrusive node,
unlinks it, decrements bucket count, marks the node self-linked and calls2BF8E4.
The manager's overall count is decremented only when the bucket count changed.
2BF8E4 reaches2BF90C, which invokes2BF948(live+B580),6639B0(live) teardown, then
tail-calls the factory allocator virtual+58 with the live pointer. This is an
actual live-object destruction path, not merely a pending token removal.

This closes the native backend for a late completed-token cancellation. It does
not prove immediate completion when68157C returns: destruction is deferred to
the next BD0 drain. Nor does it itself release a parent reservation bit, unregister
all wrappers or establish that an old pointer belongs to the current epoch. The
adapter must preserve token/actor identity until native tracking is invalidated
and its own reservation is reconciled; no immediate second spawn is authorized.
For root-ID Direct construction, disposal still needs its own registration and
root occupancy/mission-owner bookkeeping rather than assuming the child bitmap
protocol applies.

Additional admission byte check: mission-master archive_03/52/BD29D455...E4FC2,
record25 (stride198h), contains id+158=28452 and default-start override+90=0.
Thus ById start0 would use native default-point fallback; it does not encode
606826 in the mission record itself. The explicit606826 comes from the retained
point-table index0. Verify that registered destination point rather than treating
an unrelated mission-master field as its source.

The Direct root wrapper's owner key also has a real cleanup consumer: script
cleanup1C10570 calls1C10324(owner+120,true) at1C105CE, before resource-map drain.
1C10324 walks the character registry and compares wrapper+24 with that key. For
matches on this forced owner-cleanup path, it calls4BDEB8, clears wrapper+D0,
marks wrapper+EF=1 and registry+B8 dirty. It does not immediately erase registry
entries in this function. The later dirty sweep must be closed before claiming
root-ID reuse or registry disappearance. This identifies why destination-owned
Direct registration can participate in native mission teardown, but the mod must
never invoke owner-wide cleanup to remove its one boss: native training actors
share that owner key and would be affected.

## Registry erase closure and bounded reuse predicate (2026-10-12)

The final dirty-registry sweep is **968D2C -> E25370**, not either of the proposed
erase helpers.968D32 tests registry+B8;968D6E invokes E25370 and968D73 clears B8
only after it returns. E25370 walks the registry's16-byte ID/pointer entries and
E253DD tests each wrapper+EF. In the selected erase path, E25F78..E25F81 detaches
any remaining tracked live reference through2F65D4; E25F86..E25F94 obtains the
native allocator and frees the exact wrapper via virtual+58. E25FB0..E25FC4 shifts
subsequent ID/pointer entries down, and E25FCA..E25FCD decrements registry count.
The loop continues at the same entry after erasure. This is actual pointer
removal/free, not merely a dirty marker or a presumed next-frame event.

Candidate-name corrections:2BC958 resets position-related wrapper fields from
its placement record (or native zero vector);2BD1B8 resolves a placement table
through8F3F48/2BD1D8. Neither is an erase helper.1C1030C calls E4DD50 conditionally;
it is not the registry sweep.2BCA24 is a larger lifecycle routine which can mark
wrappers and calls the actual sweep at2BCC37.

Ordering is instruction-backed: MissionManagerUpdate379620 obtains registry from
manager[0]+48 and calls968D2C at37974C, then2FEA98 and98F04C, then724044 at37976C.
724044 can itself call2BCA24 at72437F, including that second sweep. Consequently,
observing B8 clear before724044 is not proof the rest of the update cannot change
ownership again. No elapsed-time or dirty-bit-only completion rule is justified.

**4BDEB8 does not unconditionally clear wrapper[0].** Its hot path calls4BDEF4,
then clears state+50, flags+F6/+EE and bitmap+D0. Only E0!=0 takes cold branch
19B37B8: if live is nonnull it calls2BF7E8, then releases its character resource
pair and clears E0. The sweep similarly calls2BF7E8 atE25436 only in its E0-owned
branch; E0==0 skips toE25DE2. The later2F65D4 unlinks the wrapper's tracked-reference
node under its native lock and explicitly zeros wrapper[0],+8,+10 at2F6616..1E;
that operation alone does not destroy the live Character. Thus especially for a
Direct-created root, wrapper disappearance must not be equated with actor death
or factory removal.

A defensible completion check, inside an independently validated serialized
native epoch, must bind the original generated ID, old character scalar, native
request token if queued, and destination owner/manager identities. Require the
owned request absent (no late materialization), exact wrapper ID absent from
135B78/registry AFTER native sweep completion, and the typed live lookup20A93C
absent using the same old scalar/ID pair that1C1CB40 tests. A freed wrapper address
must never be read to obtain this evidence. The native root-ID allocator can
reuse a nonzero occupancy entry only after both registry and typed-live searches
miss; the occupancy scalar need not become zero. Let the allocator choose; do
not force its cursor or clear a reservation table entry. Any intervening epoch
change or reappearance refuses reuse. This is a native-backed predicate, not a
new live-tested observer or a permission to call the allocator from a worker.

Remaining implementation gate: a validated destination script execution/borrow
context and exact owned-live disposal when E0 is false. Owner-wide cleanup would
affect native training actors and is not an acceptable substitute. Registry erase
is closed; exclusive single-actor destruction/resource ownership is not silently
inferred from it.

Retained dump SHA256 windows (RVA/raw offsets):
-968D2C/50h:437346855cc4ac68a3c832552f05ecbf233fdcb53cdb531d977ec4c1a9039207
-E25370/C8Dh:9dc281648c0dab2fb22d1e75812598be1cec539a8be75ab29cff5e9942b5e396
-379734/3Dh:3b2750d658fd47cfa0296208082ae6264b0314781013fc74c05ac3c53e217eae
-4BDEB8/3Bh:22b0838d24fcab77e0267651927045111f338136ca277894fde87fdd60676a43
-19B37B8/24h:6218a1afa4a5d19fc4cf338fa56036707a75c61cc5ee186d841db1ab662a8c6c

## Exclusive shipped Character::Delete path (2026-10-12)

One concrete per-actor chain is now closed. The retained native-name table entry
397EF30 contains the string pointer for `Character::Delete`; its adjacent function
pointer resolves to **4BE654**, a VM wrapper (not a function taking a placement ID
in RCX). It builds the actual VM argument reader, decodes one ID, resolves the
registered wrapper with135B78, calls4BDEF4 to snapshot/reset wrapper flags, then
loads wrapper[0] and, if nonnull, calls **2BF7E8** at4BE6B3. Unlike4BDEB8, this
call is NOT conditional on wrapper E0 ownership. It therefore supplies the shipped
exclusive live-delete route needed by a Direct-created E0=0 actor.

The already traced backend removes that exact live+E0 node from its typed factory
bucket through2BF818/2BFC90, updates counts, and calls2BF90C. The latter first calls
**2BF948(live+B580)**: under the native tracking lock, it walks all tracking nodes,
zeros each node's pointer/links at2BF96C..973, then zeros the live tracking-list
head at2BF986. It next performs6639B0 live teardown and allocator virtual+58.
Thus registered wrapper[0] is invalidated via native tracking destruction even
for E0=0; wrapper deletion and script-owned resource release are separate steps.

Typed-live lookup20A93C ->20A7E0 resolves through the same factory+B98 buckets and
compares the full live first qword (type plus ID). Adjacent native validator20A788
additionally compares a handle's dword+8 against live+18 at20A7C5..7CC. This proves
an existing instance discriminator, not a claim that it never wraps. An exclusive
adapter must bind destination manager/owner epoch, generated ID, exact first
qword, live pointer and that instance discriminator, and re-resolve/recheck all
of them in the same validated serialized callback immediately before disposal.
The VM Delete wrapper itself checks only the supplied ID; passing an old ID later
could delete a replacement. Never call it with a fabricated VM argument context.

Expected use ordering is now concrete: retain the destination resource lease;
exclude/cancel any owned pending creation; validate the current exact instance;
perform the shipped per-live disposal in a valid native execution context; verify
tracking invalidation and typed-live lookup absent; let genuine mission-owner
teardown remove the registered root wrapper on return; only then consider ID
reuse after registry absence. Do not use owner-wide cleanup as a substitute for
the single live deletion, and do not release character resources while the live
instance or a pending producer can still use them.

**Remaining blocker:** the validated destination script callback/borrow lifetime
and producer exclusion are still unproved for an addon-driven call. This bounded
chain proves the disposal operation and identity checks, not an immediately
callable training feature. Research stops at that context boundary; no native
mutation or fabricated callback was performed.

Byte anchors:4BE654/95h SHA256
82621c25f4c692cd2e8805ddd44b5b39b7ced9ae816248e6c14da67f0dcb8768;
2BF948/4Bh SHA256
02799cc053841c3a9f4ebdae264091372c213e1cf3813ac398dd61cdd5de0821.

## Destination callback borrow boundary (2026-10-12)

Bounded follow-up identifies an actual destination callback candidate, but does
not yet admit mutation there. TrainingGround script A47086B78FCD (full archive
path above), sub_event_syunobon_toujou at E03C/E04C, supplies gadget603945 to
`Gadget::GetStatusPlacement`; E050 compares the result with2 and E062 invokes
Time::DebugDispPoppingWait before the later native Character::Create605895 at
E08A. This is the shipped gadget-wait path, not permission to select its gadget
or spawn the native dummy. Its unconditional repetition throughout an arbitrary
addon request window is not established by these instructions alone.

The retained native-name table entry3980140 binds GetStatusPlacement to
**2E5598**. That wrapper builds its argument reader with139938, decodes arguments,
resolves the gadget through1182160 at2E5614, reads gadget+80 -> dword+18 if both
pointers exist, serializes the result through2E5A80, and returns. No owner lease,
producer exclusion, or actor creation is established by this getter.

The synchronous VM borrow itself is now instruction-backed. Dispatch13B18C
constructs a stack object at13B296..13B2A5: qword0=[VM], qword8=VM, byte10=0,
then passes its address in RCX to the resolved native function at **13B2A9**.
After return it validates stack-result accounting and may call140514 before
returning. Argument reader139938 stores that incoming pointer at reader+0 and
reader+128. Consequently CreateDirect238C9BC's reader+128 load at238CAD7,
then +8, +8, +90 at238CAF2/CAFE/CB02 resolves the current native callback's
VM state -> script owner, and238CB11 reads owner+120. This is a real, call-scoped
borrow; retaining the stack argument or its VM/owner pointers after callback
return is unjustified. Calling CreateDirect with the getter's argument frame
would also decode the wrong signature; no fabricated invocation is admitted.

**Exact remaining edge:** these instructions provide neither a lock nor a join
that excludes other native producers while the callback runs. The already traced
serial/list writers9DCE70/9DCF6C and root allocator1C1CB40 perform ordinary writes;
the post-world-update addon pump is not evidence of their exclusion. The specific
TrainingGround VM invocation must still be connected to its scheduling owner,
including exclusion of owner teardown1C10570/1C08D1C and all concurrent writers
to that destination manager. A gadget callback plus current mission/owner checks
cannot replace that proof. No defensible callable creation seam is closed this
pass; stop at this scheduler/lifetime edge rather than add a global lock or
borrow a guessed owner. No native calls or hooks were executed.

Retained dump SHA256 anchors:
-2E5598/D5h:cf370c919efbb2280cdf7c09f6c0b6c2ed1fd13470edac76ed27f4e3241a1494
-238CAD7/4Dh:c52c4f280e312de0b303a8a49040e6a6f8ce62285f358ccb1ec694510bad5f33
-13B277/76h:1831e92927bff168e1f2eb9c1f6d0fbb45229fffc3d4df9c5e89b149c62c12be
