# Training spawn contract: one native-arena boss

2026-10-11, Codex; static inspection only. Worktree/base `1641d3ffe350817f2a6209faaec1ef51ae600c9c`; read-only comparison with `rematch-instant` `b2cfe097ad93dd89b4753e49f43a2cfa93c8ea75`. No game, Ghidra, build, bridge, save/config or mod-source changes. This supersedes the proposed direct `ResetReceivedDead(id)`/Create re-arm in `boss-arenas.md` section 2 and `boss-fights.md` sections 3/9: reset is a VM wrapper, and activation acceptance is not a completed summon.

## Evidence and address convention

Inspected inert `C:/Users/Ahmad Hamza/nioh3-engine/re/Nioh3.dump.exe` with installed Python Capstone and manual PE-header/exception-directory parsing. Timestamp `0x6AA7C106`, SizeOfImage `0x52FB000`, preferred dump base `0x7FF74F410000`; section raw offsets equal their RVAs. Addresses below are executable **RVAs**, except explicitly named placement/mission/start/group IDs. No runtime address or compatibility claim follows from this one image. Direct-call inventory scanned `.text` E8 candidates and confirmed their instruction boundaries by disassembling enclosing `.pdata` functions; it is not a complete indirect-call inventory.

Existing exports `D:/nioh3-re/decomp/0x135b78.c` and `0x37a8d4.c` were inspected; exports for `0x1180740`, `0x1180940`, `0x20a05c` and creation/cleanup helpers were absent. Byte evidence below closes specific gaps without generating new decompilations. Some exported names/callees misdescribe split cold blocks; bytes take precedence.

## Existing-placement activation: ABI established, completion unproved

`0x1180740..0x1180832` is a native four-register routine, mechanically expressible as:

```cpp
bool ActivatePlacement(uint32_t placement_id, uint32_t unused_edx,
                       uint8_t exchange_flag, uint32_t exchange_value);
```

Windows x64: ECX is the placement ID; EDX is only saved to its home slot and never read; R8B/R9D are preserved and used conditionally; AL is the boolean return. No stack argument is read. Names for the latter fields are descriptive of the calling wrapper, not a decoded semantic contract.

- `0x1180768 -> 0x2C0308(out_shared_pair, placement_id)` scans placement sets at `[0x47098E0]`, checks six resource slots with `0xC92FC0`, and searches set+`0xB0` enemy records through `0x2F730C`. If no matching set exists, activation uses `0x2BEAEC` fallback. It then releases the temporary shared ownership via `0xF262C` and looks up the enemy record again. Record stride is `0x138` (`0x2F736C`); record byte+8 must have bit 0 set (`0x11807A6..AC`). This is distinct from record+`0x9C` boss-candidate bit 4.
- `0x11807B0..BA` reads charMgr `[0x45B9E40]`, calls `ActorFindById(charMgr, placement_id)` (`0x135B78`). Its export proves lookup in a sorted array of 16-byte `{id,...,pointer}` records, without retaining the returned pointer. This pointer is a **registered actor wrapper**, not necessarily a live Character: wrapper[0] is the live object used by other callers.
- Independent registration proof: `0x4BC904` allocates `0x158` bytes, constructs wrappers through `0x4BDCB4` or `0x4BBC48`, and inserts `{ID,pointer}` through `0x4BD688`. The latter writes 16-byte slots and increments registry+8 count (`0x4BD733..769`). Constructor `0x4BDCB4` zeroes wrapper[0], stores source enemy record at +`0x18`, placement ID at +`0x20`, owner scalar at +`0x24`, and initializes +`0x50`=0. `CreateDirect` passes charMgr->[0]+`0x48` to registration at `0x238CCEB..EF`. Constructor/resource provenance for this specific boss is not independently traced; do not infer arbitrary wrapper synthesis from these generic paths.
- If R8B is nonzero, writes wrapper+`0xF2` = R8B and wrapper+`0x138` = R9D. Always writes word wrapper+`0xDA` = 1 (also zeroes adjacent byte+`0xDB`). If wrapper+`0x50` == 0 and charMgr->[0]+`0x2679` != 0, calls `0x20A05C(wrapper, &zero_counter)` then `0x2869A0(wrapper)`. Finally clears wrapper+`0xEE`, returns AL=1. Missing record, disabled record or absent wrapper returns AL=0. The true return has no live-object/readiness check.
- `0x20A05C` changes wrapper eligibility/distance state, including byte+`0xF0`; `0x2869A0` tests +`0xDA`, +`0xF0`, +`0xF8`, appearance/session predicates and references before setting +`0x50`=1 and calling `0x669110`. `0x669110` tests resource-cache entry+`0x2C8` (`0x669388`), may call destruction path `0xE37F10`, or construct through several type-specific branches when wrapper[0] is null, then notifies wrapper listeners (`0x669990`). Thus activation can initiate construction; it is neither pure lookup nor guaranteed immediate spawn.
- `0xC92FC0` is a six-slot availability predicate, including disabled/empty cases; it makes **no model/motion/AI/navmesh readiness guarantee**. Native-arena placement resources are a prerequisite; complete asset retention and readiness remain unresolved.

Two confirmed direct callers of activation:

| Caller | Exact caller data | Result use |
|---|---|---|
| VM `Character::Create`, `0xA1D204`; call `0xA1D248` | Decode VM arg0 with `0x139938/0x139904`; ECX=arg0; R8D=0; R9D=0; EDX parser scratch, ignored by native | Ignores AL; returns VM bookkeeping AX, so script wrapper return is not native success |
| VM `Character::CreateExchangeHead`, `0x238CED4`; call `0x238CF33` | Decode arg0/arg1; ECX=arg0; R8B=1; R9D=arg1; EDX ignored | Ignores AL; VM bookkeeping |

Main and rematch-instant both resolve `Character::Create` as FIGHT **hook**, not `fn:` callable site (`sites.json`, `src/sites_gen.h`). `bosslog.cpp:97..108` observes four raw entry scalars and writes a placement/time ring; it does not prove live count, allocation, returned handle, actor lifetime or success.

## Reset/death and cleanup: narrow contracts only

- Binding pair at `.rdata` `0x397F7A0` is `{name@0x3BFB5E0 = Character::ResetReceivedDead, fn=0x1180940}`. At `0x1180959..976` it takes **VM context RCX**, dereferences context+8 and parses VM data with `0x13BDB0`. Calling `0x1180940(65092)` as a native is invalid.
- After wrapper lookup, reset follows `wrapper[0] -> live+0x3B0 -> component+8`; if each exists, `0x11809B2` ANDs dword component-data+`0xD8` with `0xFFFFFFFB`, clearing only bit 0x4. No HP restoration, animation/corpse repair, AI restart, placement reposition, actor construction, mission/script rewind or gate reset is in this routine. Zero confirmed direct E8 callers; binding-table invocation explains reachability.
- `0x7E11F0(wrapper)` reads wrapper+`0xE3` if wrapper[0] is null; otherwise tail-jumps `0x4892A8(live Character)`. The prior named `Duel::IsAliveBoss` path uses this predicate; absence and death must not be inferred merely from a Create entry or pointer disappearance.
- VM `Character::DeleteGroup` (`0x238D114`, pair `0x397EF40`) decodes positive **group ID** then calls native `0x1C101A4(uint32_t group)`. That native scans loaded placement sets and every enemy record, selects group membership via `0x2BE43C`, looks up wrappers by record ID, calls `0x4BDEF4(wrapper)`, and passes nonnull wrapper[0] to `0x2BF7E8`. It is a group-wide operation, not one-placement cleanup.
- Leaf `0x4BDEF4..0x4BDF25` clears wrapper word+`0xDA`, byte+`0xE2`, and updates +`0xE3` from live+`0x3A0` state. `0x2BF7E8` routes live-object removal through manager `[0x4707D50]+0xB98 -> 0x2BF818`, which updates an object bucket and other component/manager notifications. Complete disposal, outstanding callbacks, wrapper retention and reset of boss-specific script state are not proved. Do not transplant leaf flag writes as a despawn implementation.
- Export `0x37A8D4.c`/symbol notes call `0x19906E9` a despawn routine. Bytes show `0x37A947` **jumps** into that cold block; it consumes parent RSI/RBX/EDX, calls `0x10E3C80` using spawn-table+`0x220`->`0xC0`, marks parent node+`0x2C`=-1, then jumps back to `0x37A915`. It has no standalone ABI. Also no evidence ties this generic spawn-table owner to the selected boss.
- VM `Character::EnemyForceKill` (`0x238D504`) calls death component `0x786C84` with explicit death flags. That is a death path, not proven reward-free cleanup; do not use it as a substitute for disposal.

Ownership conclusion: native mission/placement/Character managers own these objects; an observer owns no game pointer. Wrapper/table/live pointers sampled in one epoch are diagnostic identities only. Invalidate on loading, mission/retry identity or session changes; do not cache for later dereference, retain, delete or write without an independently proved lifetime contract.

## Dynamic/direct creation: wrappers are not callable APIs

`Character::CreateDirect` pair points to `0x238C9BC`; `CreateDynamic` pair `0x397EF20` points to `0x238CD5C`. Both construct a VM argument reader (`0x139938`) and require real context. Direct additionally obtains mission/session data from decoded context, checks resource entry+`0x2C8`, constructs/registers through many helpers (`0x2BFA04`, `0x4BC904` etc.); no compact native adapter is established.

Dynamic does expose one lower native candidate: `0x238CE8A -> 0x1C0C5E0`, second confirmed caller `0x23CD6C2`. Mechanical signature:

```cpp
uint32_t CandidateDynamicCreate(uint32_t raw0, const float* xyzw,
                               float yaw, uint32_t raw4,
                               uint32_t raw5, uint32_t raw6);
```

ECX=VM arg0; RDX points to local 16-byte `{arg1,arg2,arg3,1.0f}` (constant at `0x3BD41E0`); XMM2=arg4; R9D=arg5; stack arg5=`VM arg6`, stack arg6=`VM arg7`. `0x1C0C5E0` loads original entry-RSP+`0x28`/+`0x30`, checks resource entry+`0x2C8` and an owner resolved from raw5, obtains a generated placement-like ID from `0x1C1CC10(raw5)`, builds a request and calls `0x683F74 -> 0x9DCE70` enqueue machinery. Semantic meanings of raw IDs, request retention, destruction, slot limits and failure cleanup are unresolved. The generated ID is returned even though `0x683F74`'s enqueue result is ignored, so nonzero does not establish accepted creation. Not implementation-ready for arbitrary enemies.

## Selected boss and native-arena dependencies

Use **R1CBC / Yamagata Masakage**, mission ID `0x1CBC` (7356), placement ID **65092** (decimal data, never an RVA), start/retry point **67064**, box/gate **66599**. Catalogue `Nioh3BossRematch_bosses.tsv:3` says route R, cleared replay, verified=1, seen=`-`, arena confidence high; these metadata are not new runtime validation. Arena `{337691,10244,206714,yaw -89.2}`, named `CH02_MAIN01_HITOKOTO_BOSSAREABOX`, gate_kind=box. Do not call a door-gadget native with this box ID.

`boss-map-all.csv:54` deterministically joins script prefix `8008F3E2B14D`, enemy table `/58/E391DE6B5D1512DC19F7FAAD9F36988554494F218B569120554E6CE57E5B35`, placement65092 / char-table scalar307259, first boss-candidate record. `boss-scripts-b.csv` independently records `boss_id()=[65092]`. Character-table scalar307259 is not an established asset-hash join; `boss-char-assets.md`'s setting/model/motion chain alone does not supply a ready Character.

For this script, `boss-fights-table.csv` reports three BossBattleMode calls, two ends, five flags `{3,23,139,154,473}`, six SetMissionClearFlag call sites, three EndMissionClearState calls. `boss-scripts-b.csv` imports `Load::Character`, `Load::WaitLoadEnd`, `Load::StageStreamingEnd`, `Load::UnloadStage`, `Player::GetRetryPointID`; `sub_event_boss_clear_check` includes IsAlive, BossBattleModeEnd, five flag-set calls, clear-gadget creation/access and InterruptionSummon. These are whole-script counts/imports, not proof that every call belongs to this boss path. `boss-defeat-checks.csv:4` attributes `SetStoryProgress(1050)` to that function. Flag values/order, per-boss gate synchronization, intro state, save/autosave and reward policy remain unbounded. No flag clearing or save writes are proposed.

Main `rematch.cpp:569..652` requests the native cleared replay on the current journey and validates free queue slot, mission+4, start+0xC and mode+0x14=2. `:1165..1181` gates MissionRequestReturn on offline/nonloading state; request is not completed exit. `bosslog` reward/inventory/flag/trophy/save-path hooks observe effects; they do not suppress them. Native reload is the current lifecycle owner to reuse, subject to runtime proof, not a reward-free training contract.

Read-only `git diff main rematch-instant` shows six SPAWN sites and start-record override: `WarpPointFind(0x137DCC)`, record+`0x40..0x4F` position/yaw and +8 camera flag bit0x4, applied during load and restored conditionally on identity/value equality. This means **player spawn**, not enemy creation. Source comments mention player trials but were not independently validated here. Separate branch changes must not be silently imported as boss readiness/cleanup evidence.

## Smallest implementation-ready next unit

Implement an off-by-default **read-only R1CBC observer on the existing game-thread pump**, then obtain authorized native-replay runtime evidence separately. Reuse the existing Create observation hook and mission-request/exit state; no activation/reset/dynamic/death/cleanup calls and no memory writes are required for this unit.

Publish `epoch`, mission/retry ID, offline/loading/queue state, Create **entry** placement/raw scalar args and chronology, BossBattleMode/end, observed flag/clear/reward/save events and request/exit status. Label counts as entries, never `spawn_success`/`live_count`. Unknown readiness/death/despawn remains unknown; transition ends the observer epoch even if no BossBattleModeEnd occurs. Future pointer inspection needs separate guarded sites and epoch-local reads, not arbitrary raw dereference.

Acceptance evidence for one native replay: request accepted then consumed; expected mission/start load settles; expected placement activation entry correlates with exactly one visibly active boss (player evidence required); native trigger/gate, navigation and intro work; death/retry reload and explicit exit complete; no stale observer epoch on abort/online/load failure; log all reward/story/save effects. Reward-free sandbox, same-place revive, exactly-once disposal and arbitrary enemy support stay disabled until independent proofs close those contracts.
