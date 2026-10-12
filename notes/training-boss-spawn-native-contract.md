# Second selected enemy: native creation contract

2026-10-11. Read-only inspection of the retained `re/Nioh3.dump.exe`, existing `training-spawn-contract.md`, `training-mode-design.md`, `training-playable-next.md`, inventory and symbol table. No game, source ASI, deployment, bridge, save, controller or hold changes. All addresses below are RVAs in the retained image, not live callable addresses. The absent `D:/nioh3-re/decomp/0x1180740.c` was not invented; bounded Capstone inspection supplies the new evidence. PE section raw offsets were checked equal to RVAs before decoding.

## Result and next implementation

There is a real native asynchronous creation route below the VM, but **no admitted arbitrary `(enemyId)` factory yet**. The best next implementation is a **transactional dynamic-spawn request adapter with an offline native-ABI test double**, restricted initially to a second already-loaded character type and one existing owner in the current fixed arena. It must expose separate reservation, enqueue and completion results. Do not implement it as a call to `1C0C5E0` followed by `id != 0` success.

The new concrete blockers are loss of enqueue failure after reserving an owner slot, ambiguous/exhausted generated IDs, and unclosed release of a reservation when no child wrapper is ever registered. The path does not load arbitrary assets. This note supplies the exact request fields/copy boundary and failure cases needed to implement and test the adapter, rather than another observer. Native execution remains disabled until ownership and cancellation close. Choosing any boss and switching arenas remain unfinished features; mission replay is not equivalent to either.

The lower-risk alternative is `1180740` activation of a **second existing registered placement in that same arena**, if one is actually present, enabled, inactive, resource-ready and not owned by a competing mission script. That is a genuine second selected enemy only when the current arena's records provide one. There is no current evidence for such a second boss in the Yamagata arena, and no permission to substitute a catalogue ID from another mission.

## Why Character::Create cannot synthesize an enemy

The existing `training-spawn-contract.md` contract remains: `1180740(placementId, unused, exchangeFlag, exchangeValue)` searches loaded placement sets/fallback, requires enemy-record flag bit0, then requires `ActorFindById` to find an existing wrapper. It sets wrapper activation fields and conditionally invokes `20A05C` / `2869A0` / `669110`. Its true result is activation acceptance, not a completed live Character. Neither the catalogue boss ID nor the character-table scalar alone creates the required wrapper/resources.

The wrapper is manager-owned; its first pointer is the optional live Character. Lookup returns a borrowed pointer. Native mission reload remains the established high-level lifecycle, whereas deleting a group or killing a Character is not reward-free one-child cancellation.

## Native dynamic route: mechanical ABI and fields

`238CD5C` is the VM wrapper; `238CE8A` calls the independent function `1C0C5E0`:

```cpp
uint32_t DynamicCreate(uint32_t characterTableId,
                       const alignas(16) float* positionXYZW,
                       float yawDegrees,
                       uint32_t rawArg5,
                       uint32_t ownerPlacementId,
                       uint32_t rawArg7);
```

This spelling is descriptive, not a new callable declaration. ECX, RDX, XMM2, R9D and entry-RSP+28/+30 are the actual Windows x64 locations. VM args1..3 become XYZ and W=1. The first scalar reaches the character table/resource key path; the fifth native argument is resolved in the actor registry and supplies the owner. RawArg5/rawArg7 meanings remain unproved and must not be guessed. The second direct caller `23CD6C2` corroborates this register/stack layout and a position transformed from an existing actor, not arbitrary cross-mission placement support.

`1C0C5E0` does the following in order:

1. Resolves character-manager/owner context (`8F3F48`, then manager->[0]+48). `8F3F48` briefly obtains/releases a shared pair; this does not transfer a retain to the mod.
2. Calls position helper `2BC6E8`; creates the resource key through `134314`; looks up resource via `133440` and requires entry+2C8 != 0 (`1C0C65F..66E`). `134314` forms `0x300000000 + characterTableId`, possibly substituting an alternate character ID when the native mode gate is set. A new adapter must preserve this resolver, not hardcode that sum.
3. Looks up `ownerPlacementId` through `13684C(manager+48, ownerId)` and requires the resulting wrapper and wrapper[0] to exist (`1C0C674..68D`). This proves an existing live owner prerequisite. It does not prove that any actor type is a suitable owner.
4. Initializes a stack request with `683FE4`; generates/reserves an owner-derived ID with `1C1CC10`; fills fields below; calls `683F74`.
5. Discards the enqueue return and returns the generated ID in EBX (`1C0C715..71A`). Thus a nonzero ID can accompany a rejected request.

The request begins at that routine's post-prologue RSP+60. Its copied payload is 0x140 bytes. Exact assigned offsets:

| Request offset | Proven value |
|---|---|
| +00 | characterTableId |
| +04 | generated child ID |
| +10..1F | caller's aligned XYZW |
| +20..5F | matrix produced by `5ECE50(yaw * native scale)` |
| +60 | dword from owner live Character +B0 -> +10 |
| +70 | rawArg5 |
| +76 | byte1 |
| +DC | rawArg7 |

All other fields come from `683FE4`, including identity/vector defaults and numerous -1/default flags. It is not a zero-initialized C struct. Do not pack guessed fields or read uninitialized padding as semantics. Existing native copy does copy the complete payload including padding.

## Reservation hazard: actual exhaustion and rejection cases

`1C1CC10(ownerId)` rejects IDs with high nibble nonzero, finds the owner wrapper, calls `1C1CAB8(wrapper)`, shifts its result left28 and ORs the owner ID. `1C1CAB8` reads character metadata words +382/+384, starts at `word382+1`, uses upper bound `min(word384+1,16)`, and searches owner wrapper bitmap +D0. On a free slot it calls `9FB208(bitmap, slot, true)` at `1C1CB2C`.

The exhausted path does **not** return a distinct failure. It reaches `1C1CB31` with the loop index at the limit; metadata lookup failure also returns the initial slot1 without a reservation. Therefore validate reservation provenance and a fresh free slot, not merely the generated ID:

- With upper bound16 and occupied slots through15, returned slot16 shifted in 32 bits becomes0, so the generated ID equals the owner ID. This is a concrete instruction-level counterexample; whether a shipped boss reaches this metadata/bitmap state is unmeasured.
- With a smaller upper bound, the returned index can encode a slot that was never reserved.
- `1C0C5E0` may reserve a valid slot, then `683F74` returns -1 (missing queue manager or failed table admission). The reserved ID is still returned. No undo exists in this inspected caller.
- A timed-out accepted request is not a rejected request. Clearing its bit or retrying could race a late child registration.

Do not label these observations a demonstrated shipped-game bug: native callers may rely on stronger preconditions. They are decisive reasons an unrestricted mod wrapper is unsound.

## Enqueue ownership is stronger than previously known, but not complete

`683F74(const Request*)` passes the request to `[global4745348]+BC0` through `9DCE70`, returning a request token or -1. The global address is derived from the actual RIP-relative load at683F7B. Missing manager returns -1.

`9DCE70` requires request+0 nonzero and `2BFC3C(1342E0(characterId))` success. The latter validates an index from `134430` against a table count; this is separate from the resource-cache +2C8 readiness test. It increments the native request counter, constructs a temporary queued record through `9DD14C`, inserts via `9DCF6C` and destroys the temporary's auxiliary member.

`9DD14C` copies all0x140 payload bytes into a0x160 queued record, writes token+140, state byte+144=0 and clears auxiliary storage+148/+150/+158. `9DCF6C` allocates a list node and calls `9DD080`; that performs another full0x140 copy plus token/state and copies the auxiliary member through `2F61B0`. Therefore the stack request/position buffer is **not retained by address by this enqueue chain**. Native list ownership of the copied request is demonstrated; deep resource retention, consumer completion and cancellation are not.

The existing cleanup/update routine `286124` supplies a real slot-release anchor: only after child wrapper[0] is null (`286479`), it derives an owner lookup ID from the child's +20, finds the owner, and calls `9FB208(owner+ D0, childId >>28, false)` at2864E4. The owner lookup mask is exactly **0x08FFFFFF**, not an invented0x0FFFFFFF normalization. Its full surrounding appearance/removal state machine matters. This is **not** a standalone rollback API and does not cover requests rejected before a child wrapper exists. No raw bitmap clearing is authorized by this evidence.

## Loading a second boss is a separate owned operation

Fresh `Load::Character` trace provides a useful lower native boundary: VM `A429F4` extracts its script/mission owner from the VM context and calls `A42A7C(owner, characterId)` atA42A46. `A42A7C` calls `3F9E58(characterId,2,0)`, then tracks a reference count in owner+3C8 keyed by that character ID. Existing entries increment +20; the cold insertion block at1A6425C inserts count1. This is a load request with script-owner bookkeeping, not a readiness predicate or free-standing global load.

`Load::CharacterPlEnemy` (`23A22EC`) resolves the owner's placement records through `2BD1D8` / `2F730C`, takes enemy record+4 and passes that to `A42A7C`. This establishes a concrete join from **placement ID to character-table ID**. It does not make that record available in another mission or transfer its assets/scripts/navmesh ownership.

Consequently, the first two-character adapter should initially require both character types already ready under the current mission owner. Supporting an unloaded second boss then needs the paired unload/reference-release path for owner+3C8 and consumer readiness/completion. Bypassing that owner list with an anonymous `3F9E58` call risks a reference leak or early unload.

`CreateDirect` is a less attractive first adapter: its VM wrapper includes generated-ID allocator `1C1CB40`, live factory `2BFA04`, positioning/orientation, many component initializers, script-derived mission ownership, temporary wrapper construction, `4BC904` registration and temporary destruction. `2BFA04` alone merely reaches manager+ B98 factory `2BFB88`; it does not reproduce that initialization/registration tail. Calling only that leaf is not a safe shortcut.

## Concrete adapter implementation and acceptance boundary

Build an isolated request planner/transaction model first, with no live calls wired. Use the actual0x140 layout and typed backend operations, preserving the native constructor and resolver rather than recreating guessed semantics. Its states should be `Preflight -> Reserved -> Enqueued(token) -> Registered(id) -> Live -> Removing -> Released`, with separate `RejectedBeforeReservation`, `RejectedAfterReservationNeedsNativeUndo`, and `Indeterminate` outcomes. An epoch change while queued is indeterminate until native ownership drains; it cannot erase a pending obligation.

Required bounded tests: metadata absent; high-nibble owner; all slots exhausted; occupied selected slot; generated ID equal owner; no resource; no live owner; enqueue -1 after reservation; acceptance with delayed registration; timeout then late completion; native load transition; removal before slot release; duplicate command; owner disappearance. Validate full payload fields and native default bytes using a mock constructor; test request copy surviving caller-buffer invalidation. This makes the next code unit concrete and reusable when the missing native backend edges close.

The next **native reverse-engineering edges**, in priority order, are (1) queue consumer and cancellation for manager+BC0, following token+140/state+144 and native list erasure; (2) child wrapper registration and transfer of the selected owner slot; (3) paired script-owner load release for owner+3C8. Those are actual prerequisites to implement undo, not more event logging. Do not expose arbitrary boss Spawn or Arena Switch until those gates pass. Arena switching additionally needs stage/streaming/navmesh and encounter-script ownership; this factory trace does not grant it.

## Reproducible byte provenance

SHA256 of exact RVA windows in the retained flat dump (sizes are inspection windows, not universal function extents):

| RVA / bytes | SHA256 |
|---|---|
| 1180740 / F3 | 2a0b3cb79cf2b3042f572909a499c41318ab0f597d5bec6115bd15bf40f49f6d |
| 1C0C5E0 / 165 | 6a946d5c5717c8a98fe272a753f4149b0175420a64dfc3b03d0d25cbba4f6f8f |
| 1C1CAB8 / 86 | 39e4ef04da1d01f0cce27e7b14f60aa95fae8aba16d07b08fb627aabd60991fb |
| 1C1CC10 / 3A | 18db1473cb173248a0d5f24e57d9fdc0e14b9b6e0147df79511fd355c2565772 |
| 683F74 / 32 | a12b041a45609b3ad2126d8f9670779114249d75ec79560371ab03e33711e53d |
| 9DCE70 / B6 | c5bcc4f8511f33767e5e921f6f131f0c0fa51f7726c191627b678b5eb38ce229 |
| 9DD14C / 9F | 6c131d8ca0634c6c5070094f695ba7d83e0d1876f0fdcabd5fbf1985bf10e89e |
| 9DD080 / BF | f1579ce2bf5c8c48ba8400b886ca324222d85e5bd53dfb2509ab47b5ea3d16d2 |
| A429F4 / CB | 040ebc4f257e3bceca035eff5ab3899ce1707f7e6a115f563ea9bd1b6e54dca6 |
| 23A22EC / B4 | 6295efb9ab804889378bc9423d19dd8f49b78737d7620c996a1fc7b808ae7718 |
| 286124 / 3F9 | f956eb74158e2f01564bd2dbf0f30c4f9d2f339c5300daa69df97ffad2008f62 |

## Follow-up: native resource pair closed; pending cancellation narrowed

The initial note was committed as `c478721`; this continuation closes the paired native resource release rather than building an abstract model. Still no live calls, game operations or ASI changes.

**Resource-load balancing is now established within a valid script-owner epoch.** `Load::UnloadCharacter` VM wrapper23A2A40 calls **1C217A8(owner, characterTableId)** at23A2A95. `UnloadCharacterPlEnemy` independently resolves placement record+4, then calls the same native at23A2B42. This exactly mirrors the two load wrappers.

`1C217A8` searches owner+3C8 with A42B00. Missing entry returns without unloading. A found entry calls **1C1DF80(characterId,0)**, decrements node+20, and erases the entry via1C112BC only when the count is no longer positive. 1C1DF80 is a short tail adapter to **3F9900(characterId,2,0)**, pairing the load's **3F9E58(characterId,2,0)**. Each owned count represents one native release, not one unload only when the script count reaches zero.

The release backend enumerates the same resource/dependency key families, calls3F9A8C, and updates the type2 bookkeeping via3F9A28. Inside3F9A8C, an unfinished resource increments pending-release counter+2C4; a ready resource decrements refcount+2C0 through3F9A74 and performs destructor/cache removal only at the native release condition. Therefore this is a native-managed release even before resource readiness, not a requirement to manually free or wait then discard the resource.

**Owner cleanup is also located:** 1C08D1C walks owner+3C8; for each entry it calls1C1DF80(characterId,0) repeatedly while node+20>0, decrementing after every call, then clears the map through1C0D2AC. A mod must not release an old count again after this owner drain. No retained script-owner pointer or synchronization lease has been established; this pair does not make cross-epoch raw pointer calls safe.

This closes the API/pairing edge for a future `AcquireCharacter`/`ReleaseCharacter` adapter. Preflight still needs a genuine native script owner, one game-thread mutation owner, build-pinned sites, successful load accounting and readiness. Pending child requests/Characters must cease using resources before the adapter voluntarily releases its count. Actual asset completeness, enemy-specific navigation/scripts and owner lifetime remain separate gates.

### Queue cancellation and creation consumer: actual behavior

The token-based helper **681434(manager, const uint32_t* token)** operates on the same manager+BC0 queue. It first calls5BB144, which finds the token at list-node+150 (=payload+140). If the record has a nonnull pointer at node+158 (=payload+148), it erases the record and returns that pointer. 681434 then appends a tracked pointer to manager+BD0 through681044. If no pointer is returned, it calls **5BA648(queue, token)**: that finds the token, unlinks the queue node, decrements queue count, destroys its tracked member and frees the node. There is no owner-slot lookup or bitmap release in this pending-node erase chain. This is a concrete pending cancellation mechanism, **not yet a complete dynamic-child rollback**. Meaning/final drain of the BD0 list is still unproved, so do not claim completed-child destruction from this wrapper alone.

The queue consumer is **5BCBAC(queue)**. Records with state byte+144=0 call **65A8A8(queue, payload)**. Completed records with null tracked pointer are erased via its cold branch19E0122; completed nonnull records remain. The creation routine:

- Uses supplied request+4 ID, or generates a fallback only when zero.
- Calls live factory2BFA04 at65A97A; a null result returns without setting completion, so that record remains eligible for retry on later consumer ticks.
- Performs actual position/component/AI initialization; request+76 becomes Character+AB96.
- At65AF88 looks up the supplied child ID through135B78; **only if a wrapper already exists** does it call669A10(wrapper, live), set wrapper E5/E7 for request+76 and notify669990. This inspected path does not register a missing child wrapper itself.
- Stores a native tracked reference to the live Character at request+148 and then sets completed byte+144=1 at65B1F6.

The tracked-reference helpers2F6594/2F65C0 maintain linked tracking nodes under a lock; this is **not proven strong shared ownership**. Do not retain the raw pointer returned by5BB144 as a mod-owned actor. The native consumer can create a live object even if no matching registry wrapper exists; future tests must distinguish live factory success from wrapper registration and slot release.

Consequently the immediate next reverse-engineering unit is narrower: trace **who registers owner-derived child IDs before this consumer** (or whether native dynamic children deliberately bypass wrappers), and how a rejected/erased pre-child reservation is reclaimed. The normal286124 release requires a child wrapper, while5BA648 does not supply one. This is a real remaining ownership gap; the newly closed resource pair does not hide it. The copied request, cancellation token, ready/failed-retry state and owned resource balance can now be implemented behind a disabled backend with exact contracts once that final reservation edge is solved.

Fresh continuation byte windows:

| RVA / bytes | SHA256 |
|---|---|
| 1C217A8 / 58 | 16da2564fb00e3d14dcf93738ab4e530aa8534f52b40ba776578190f96d565d6 |
| 1C08D1C / 58 | 297fc9dff4321c24637f031b6ee62bd488afea423510a5b48ef388942fbea31b |
| 681434 / 55 | 496a4e8b414ee99acf68c7e22497d2536b0658be8df5127c866b3e7ba3230e96 |
| 5BA648 / 48 | 32388dafe133f5a41abd5a8ee4e4d1fa239fe5896783606a6fbc6844fd4f20d6 |
| 5BCBAC / 49 | b9a470281e23043e54300e49f7137f1ad0c88a9300ab46419585472053c6f8e1 |
| 65A8A8 / 978 | a55d5418b820a7919b74a388776a7d2040cec85fe922d18ea4ef118aa4633ac2 |
