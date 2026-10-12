# Dynamic training children: completed cancellation and disposal boundary

2026-10-11, Codex. Static continuation of `training-boss-spawn-native-contract.md`; retained flat `re/Nioh3.dump.exe` and existing exports only. Frozen candidate e9941b4 untouched. No native calls, game, ASI changes or executable adapter. Existing acquire/release, registration and pending cancellation facts are not repeated here.

## Concrete result

The previously unknown **manager+BD0 completed-cancellation consumer is 5BC9A0**. It reaches the actual native Character disposal path rather than the previously traced EnemyForceKill entry; cleanup callbacks and reward-free behavior are not established by that distinction. The exact successful factory allocation and inverse manager removal are now joined. This closes where an accepted request's completed child goes after 681434; it does **not** turn queue absence, tracked-pointer null or a timeout into disposal completion.

A future adapter must serialize against this native phase and bind a successful disposal return to the original epoch/child identity before releasing its own resource count or permitting slot reuse. No callable `(owner,child,token)` operation with an epoch lease is yet proved. Accordingly no live backend or model-only 'safe spawn' helper is introduced.

## Completed cancellation queue and native scheduling

`681434(manager,token)`'s completed branch calls `681044(manager+BD0,live)`. The latter locks list+10 via48B44, constructs a tracking member with2F6594, inserts through6810DC, destroys the temporary tracking member, then releases viaB61524. It is a queue with tracked pointers, not transferred strong ownership.

`5BD168(manager,jobContext)` executes in this exact order:

1. 5BD194 calls **5BC9A0(manager+BD0)**.
2. 5BD1A0 calls **5BCBAC(manager+BC0)**, the asynchronous create consumer.
3. C30 and1168 consumers, then5BCE5C(manager,0).
4. It allocates/pushes character jobs, wakes the scheduler at5BD380 and waits on manager+14E8 at5BD38C.
5. Only after the wait does it run2F5FA0,5BD6E0,5BD630 and5BD3D0.

`5BC970` reads the same global4745348 and calls5BD168. Existing `39E4E4` export directly calls5BC970. Thus completed cancellation is drained **before new creation and before this routine's child jobs are dispatched**, not at an arbitrary worker callback. A completed cancellation added after the snapshot must wait for a later drain; an empty BD0 list is not proof that its extracted disposal batch has finished.

## What 5BC9A0 actually does

It creates a temporary plain-pointer list, acquires BD0+10, copies each queued node+10 live pointer into that list viaA0AF54, clears/destroys the tracked source list via5BCAC0/5BCAEC, and releases the lock at5BC9F3. It then sorts the temporary list (cold block19E0074, A35EC8/2023310), removes equal adjacent pointer entries (19E00B9 compares pointer values), and invokes **2BF7E8(live)** once per remaining pointer at5BCA73. Finally it frees temporary list nodes/sentinel.

The lock therefore protects list extraction, **not** the whole lifetime of the borrowed live pointers or subsequent destruction. Duplicate suppression is per extracted batch and raw pointer equality, not a persistent transaction ID, allocation generation or cross-frame guarantee. Appending a duplicate later is not authorized by this fact. Null tracking pointers may enter the temporary list; 2BF7E8/2BF818 tolerate null.

## Exact allocation/removal pair

Correction to the earlier spawn-contract note:2BF7E8 reads **4745348**, not4707D50. Its RIP load at2BF7EF resolves as2BF7F6+4485B52=4745348; this is the same manager family as BC0/BD0.

The already-known create consumer65A8A8 calls2BFA04 -> **2BFB88(manager+B98,request)**. The latter validates the character table and absence from its selected bucket, then calls2BFD80. That function obtains allocator `*(manager+B98)`, calls allocator virtual+28 for **B588 bytes** (tag35), clears live+B580, constructs the Character via2C19C4 and registers an additional native tracking path through2BFDE4. A nonnull result is linked into the bucket via2BFC68 using embedded node live+E0; manager count+20 increments. Null allocator/factory result is not successful completion, consistent with the create queue retry behavior already documented.

Inverse path from the BD0 consumer:

- **2BF7E8(live)** delegates to `[global4745348]+B98 -> 2BF818`.
- **2BF818** may first notify manager5978 depending on the algorithm component and2BF788. It recomputes the same bucket from live type/ID and calls2BFC90; it decrements manager+20 only if the bucket count actually changed.
- **2BFC90** requires nonnull live, embedded live+E0 not already self-linked, and actual membership in that bucket. Only then does it unlink, decrement bucket count, self-link live+E0, and call2BF8E4 -> **2BF90C**. A remover entry by itself can therefore be a no-op; do not acknowledge completed disposal merely because2BF7E8 was entered.
- **2BF90C** calls2BF948(live+B580), then the full Character cleanup **6639B0(live)**, then tail-calls allocator virtual+58 with the same live pointer. Allocation virtual+28 and release virtual+58 use the allocator held by the native bucket manager; they are not permission to manually free it.
- **2BF948** locks global4745360, walks all linked tracking nodes, zeroes each node's three pointers, clears live+B580, and releases. This occurs **before**6639B0 and allocator release. A wrapper's tracked live pointer becoming null is not proof of completed Character cleanup/free.

A second native disposal route, **FEDBA0(bucket)**, walks an entire bucket, unlinks/self-links each live+E0 node and calls the same2BF90C atFEDBE8. It bypasses2BF818, so observing only the public remover misses bulk teardown. The common tracking-invalidate/Character-cleanup/allocator-release boundary remains2BF90C. Its completed return says that Character's native disposal returned; it does not identify whether the cause was the mod's cancellation or owner teardown without the surrounding epoch receipt.

The appropriate exact success receipt is normal return through the actual disposal path after6639B0 and allocator virtual+58, tied to copied pre-disposal child/owner/epoch identity. No post-free dereference is needed or allowed. An exception/epoch change before that return remains indeterminate. This is a concrete dependency for transaction completion, not a proposal to observe every Character forever.

## Serialization facts and remaining owner epoch gap

The current addon pump hooks PerFrameTickCall **56529C ->3B29E4 (FastTravelSeqTickCall)**. Exact preceding calls are565288 ->39E4E4 (world update), then565294 ->5A29F0, then the pump site. Thus the addon runs **after** the current tick's world update, BD0/BC0 consumption and5BD168 character-job wait, not before them. A cancellation it enqueues normally awaits the next tick's BD0 drain. This corrects any assumption that the generic per-frame hook itself is the creation-consumer boundary. The existing corrected thread audit (`game-logic-audit.md` section1) records previous MissionThread join at the beginning of the logic frame, MissionThreadArm in39E4E4, and actual MissionThread kick at the end of GameUpdateTick. Arming alone is not the kick; do not infer overlap simply from its name. The freshly checked5BD168 ordering excludes its own newly-dispatched character jobs from its disposal/create prefix and confirms they have been joined before the current addon pump is reached through this caller.

These facts identify a promising serialization phase. They do not yet prove that every registry/owner teardown writer or alternate GameModeUpdateMain caller is excluded at the addon hook. In particular, a retained script-owner pointer, locally incremented training generation or two matching pointer reads is not an owner lease. The adapter still needs one explicitly verified native owner phase that covers resource acquire, slot reservation, enqueue, cancellation and teardown, with cleanup bound to the same owner allocation epoch. Cross-epoch resource release must remain forbidden because native owner drain already releases owner+3C8 counts.

The script resource drain1C08D1C is tail-called at1C105EE by **1C10570(owner)**, after several registry/group operations using owner+120, owner+18 cleanup and1C28AF0. Its confirmed direct caller is **1BF9B10** at1BF9B2B, followed by further owner-member destruction. Therefore resource counts can be drained as part of owner cleanup independently of a mod cancellation receipt. This establishes a concrete owner-invalidation boundary to include in the serialization proof; the scheduling/complete callers of1BF9B10 are not yet closed here.

The next bounded implementation prerequisite is therefore a **phase-owned command adapter**, applied at the pre-job BD0/BC0 boundary only after tracing the owner registry/drain writers into that same task or a joined phase. Its completion record must distinguish pending-node erase, disposal queued, disposal entered/no-op, disposal returned, and owner teardown. Slot/resource release cannot be inferred from any earlier state. No raw list lock should be held across unknown native cleanup callbacks; the native consumer itself releases it first.

## Reproducible native windows

RVA/window sizes are bounded inspection windows, not all complete-function sizes. SHA256 of bytes in retained image:

| RVA / bytes | SHA256 |
|---|---|
| 681044 / 5D | 88ef732741094300845ce67bf3bd2dcca40f3ef907088059bf70f6b9444e1c13 |
| 5BC9A0 / FB | 65a866cdd2530c293f9b66b761c56a5516968a2ce20fcc77308d01926bc2590e |
| 19E0074 / AE | 501429fb9aad7e067b7e913b9839e1a03cddec993b582b7546698a3f2d0d2651 |
| 5BD168 / 266 | 834413486e9d360dcc2d1bd6a6b63d1520d36b658fac3ddfcbc2e8319116fe98 |
| 2BFB88 / B2 | 3968ba697ecf36fcddf2c33a31162416a038faedc97e7ffa7ae1e4572d27f355 |
| 2BFD80 / 61 | 91c1ecedd975ba7ba667d5f92bc274fb8b732e16bd8f0e2cd9b0c474a638dbc4 |
| 2BF818 / CC | 1779a8f0540e8674f5d669390d08c96e4c739391108433cc6ebeab85c1244497 |
| 2BFC90 / 4A | 67f6e1dbbfddcaee9203cc1bae74884298f50cf384cec5cfe48920ed5b7c22e9 |
| 2BF90C / 3B | 1501e6fd6372da06afb4f5ed704dfd1ae4df7e3e9c215e7b7ba8e690a1df3e8c |
| 2BF948 / 4B | 02799cc053841c3a9f4ebdae264091372c213e1cf3813ac398dd61cdd5de0821 |

Independent review: rendering_opportunities re-read the central dump paths and recomputed the5BC9A0,5BD168,2BF90C and2BF948 hashes. PASS for BD0/BC0 ordering, tracking invalidation before cleanup/release, pump placement, manager RIP correction and bulk/owner teardown joins. This review does not establish owner epoch exclusivity or admit a callable spawn adapter.
