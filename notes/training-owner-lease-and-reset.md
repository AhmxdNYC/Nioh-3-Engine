# Training owner lease and in-place reset boundary

2026-10-11, Codex. Bounded static continuation of reviewed disposal note b1aba70. Only retained dump slices and existing code/notes; no candidate edits, native calls, build, game or device operations.

## New concrete result

The resource owner has a real **native shared-control-block lifetime**, not just a raw pointer. A matching weak-to-strong acquisition and release algorithm is present in the native resolver. That closes a mechanical allocation-lifetime contract for a future adapter, conditional on obtaining the native weak pair safely. It does **not** close the requested logical owner epoch: the owner initializer can mutate identity/state without freeing the allocation, but its sole observed direct caller uses a fresh unpublished owner. No published-owner reset is demonstrated by that callsite. Therefore the overall arbitrary-spawn backend remains unadmitted; this note narrows the remaining proof to actual pair acquisition and reset/teardown serialization rather than proposing another observer.

## Allocation, ownership transfer and release

`E307B8` obtains the native allocator via12F1F4 then tail-calls1BED5F4. That requests **3E0 bytes**, alignment10, tag35 through allocator virtual+38, then calls owner constructor1BF71B8. This size covers the already-established resource-count map at owner+3C8.

`1BE1BF0(dstPair,srcRawPointer)` constructs ownership, rather than acquiring a reference to an arbitrary borrowed pointer. It allocates a **18-byte control block**, initializes strong count+8 and weak count+C to1, writes vtable **3A892F0**, stores the owner pointer at control+10, publishes `{owner,control}` to dstPair and clears srcRawPointer. Its control vtable first entry is1C01DC4: load control+10, tail-callE312A4 ->1BEDD1C ->1BF9B10. This reaches the resource teardown previously traced through1C10570 ->1C08D1C, then frees the owner through its allocator. Never call1BE1BF0 on an already-native-owned raw owner: that creates a second owner/control block and risks duplicate destruction.

`1BF6694` places this pair at the start of its containing state object. `1C07F0C` allocates another raw owner, initializes it through1C2608C, and inserts a pair in a list at containing-state+10 throughE2AAA0 ->1BE1998 ->1BE1BF0. Both call1BF9784 on the temporary raw-pointer holder afterward; after a successful move that pointer is null and no owner destructor runs. Hence a local cleanup callsite is **not** automatically a native owner-epoch end. This refines the earlier destructor call-chain evidence.

Native **F262C(control)** atomically decrements strong+8; only previous count1 calls vtable+0 (owner disposal), then decrements implicit weak+C and calls vtable+8 only when that reaches zero. F25FC is the pair wrapper reading pair+8. The aliasing strong-pair constructor1BE1D00 increments control+8 and publishes a pair, but assumes an already-valid owned source. It is not a safe upgrade of a stale/weak pointer.

## Existing native weak lookup and non-resurrecting upgrade

`135E7C(context,outWeak,key)` searches context+20's keyed container. On a matching entry, it copies node+8/+10's object/control pair and increments **weak count+C**, including balancing its temporary weak reference. It returns an empty pair for absence. The inspected body does not acquire a container lock. Atomic reference counts protect the control block **after a valid pair has been acquired**; they do not make concurrent container traversal safe.

`8F3F48(contextHandle,key)` demonstrates the correct upgrade:

1. Calls135E7C using `*contextHandle` and owns the returned weak pair.
2. Reads strong count+8 and uses `lock cmpxchg` to increment only while the count is nonzero (8F3F81..90). Zero means expired; no resurrection.
3. Releases its owned weak count at8F3FA4, retaining the successful strong pair temporarily.
4. CallsF262C to release that strong reference **before returning its raw pointer** (8F3FDB..DE on the selected path).

Consequently8F3F48 remains a borrowed-pointer resolver, not a lease-returning API. An adapter may reuse the native weak-lookup plus nonzero-CAS algorithm only under proved container serialization, retaining the resulting strong reference until its explicit release. It must not call8F3F48 and pretend the returned raw pointer remains pinned. The fallback root pointer on lookup failure is also not selected-owner success.

The dynamic-create wrapper calls8F3F48 at1C0C62D, then independently reloads the global context and uses its actor registry at+48. This caller does not hand a retained resource-owner reference to the mod. Any proposed owner mapping must bind the actual resource-owner pair, requested owner placement, actor-registry context and current script state; a matching scalar alone is insufficient.

## Identity initialization and the remaining logical-epoch question

**1C2608C(owner,newId)** operates on an existing owner address. Before resolving the new script/record it clears owner+30 through1C0D8C0, releases/reset members at+8,+148,+160, clears owner+128 and state bytes+13C/+13E, and conditionally replaces script/resource-associated fields. It finally writes **owner+120=newId at1C2622E**, then resets+134. It does not require the shared strong count to be1 and does not free the owner. Reference counts do not themselves serialize these field writes; whether native callers can perform them after publication must be established separately.

A bounded raw direct-call/tail-call scan found only1C07F3C ->1C2608C, inside the fresh-owner initialization path described above. This is **not evidence that a published owner is reinitialized during a training transaction**. No actual in-place-reset race is claimed. Mechanically the routine has no strong-count exclusion, so a broad proof must still establish that initialization is confined to pre-publication and that no other writer reuses the owner identity. The observed callsite is consistent with that confinement; remaining indirect references/writers are not closed here.

The destructor/resource drain remains1BF9B10 ->1C10570 ->1C08D1C. Retention can defer the destructor path, but may also delay native teardown and keep scene-owned references alive across a transition. Holding a lease across mission changes is not established as compatible; the adapter must close its transaction before the owner phase ends, or quarantine and let an explicitly proved native teardown path own the remaining obligation.

## Implementable boundary and precise remaining proof

The smallest useful backend contract now has two distinct capabilities:

- **OwnedOwnerLease**: obtained from an actually acquired native weak pair under its container owner; nonzero-CAS strong upgrade; copied object/control identity; exactly-once F262C release. Native control vtable and object association must match the proved resource-owner type. This capability pins storage only.
- **OwnerMutationPhase**: excludes published-owner identity reinitialization (if any),1C10570 teardown and owner/child registry mutation while reservation, enqueue/cancel and resource count changes execute. All native operations remain on that phase's owner thread. A lease alone cannot grant it.

The previous note closes that BD0 disposal precedes BC0 creation and this routine's character-job dispatch, with a join before the current addon pump. The existing frame audit closes previous MissionThread join / end-of-frame kick for its standard path. Neither proves every keyed-container/reset writer or alternate world-update path is excluded at the pump. The exact unresolved task is to trace **135E7C's keyed-container writers and the publication/identity writers around1C2608C** to a single native task/phase, or obtain an existing native lock/lease that spans them. Do not infer that atomic shared counts or the BD0 list lock serialize the script-owner map.

Once that phase is closed, allocation/reservation/enqueue can be a synchronous command applied there; cancellation retains its token and slot obligation until the common completed-disposal return established in b1aba70. Only after request/child drain may the adapter release its resource acquisition, then its owner strong reference. Owner-reset/teardown must either be excluded until this finishes or explicitly take over the obligation. No safe executable adapter is claimed yet, and no standalone model that assumes away the missing phase is added.

## Native byte provenance

| RVA / bytes | SHA256 |
|---|---|
| 1BED5F4 / 42 | 58ce65e290a37b7e09bec859b12ad2b0a2c97493b4cb32a25c4d8be008bb5653 |
| 1BE1BF0 / 66 | 2885749c1c5689d93c9c9ec09d7be963b55936bb29cac45ee2932943d2dc3e66 |
| 1BE1D00 / 25 | 32b802ddef7ad8717b4ff37362b0541ecc4aad211a76e3adc9b38a45fb01f294 |
| 1C01DC4 / 9 | 128a25636c724807e673655e85555bce0b416e741fa3ca7cd74b8dce2c44a6a0 |
| 135E7C / 1A5 | c28ef31aa90fafa84f767d0d1375dc3b36a6be0277fca3f6ce51408b788a1f8e |
| 8F3F48 / B6 | 89ee78ab2397f79a09e9c0f5e1bae7b4465c6a2032ef2bd010c5cbb8da105b92 |
| F262C / 3D | ecc1384a308322a788a70b6c2e709057847620b1650cfb21db458f4c255fa8d2 |
| 1C2608C / 1BC | aa2b594ec5afafdcad9ba106fa59b89c2d6c660466a1872a47e7afa358481c63 |

Independent review: rendering_opportunities recomputed the1BE1BF0,135E7C,8F3F48 andF262C windows and checked allocation-before-initialization at1C07F0C. PASS for native control-block transfer, weak reference, nonzero strong upgrade and release-before-raw-return facts. Safe keyed-pair acquisition and full owner-epoch serialization remain open; no published-owner reset race is claimed.
