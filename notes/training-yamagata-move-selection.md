# Yamagata selectable-move training: native dispatch contract

Private static investigation, 2026-10-11. User confirmed Yamagata Masakage. No live requests, ASI changes, or playable move-selection claim. Scope is the existing R1CBC encounter: mission 0x1CBC, placement 65092, start 67064. Raw IDs below remain private evidence, not public extracted content.

## Concrete dispatch choices

Retained `re/Nioh3.dump.exe`, RVAs (flat dump; image base 0x7FF74F410000):

- `ScriptBind_Character_RequestActionTrans` 2391EE0 parses placement and numeric action arguments, calls actor lookup 135B78, dereferences wrapper[0] -> live+3B0, then calls 9A732C(component, actionID) at 2391F62. The VM return is interpreter bookkeeping, not an acceptance receipt. Reviewer independently decoded 9A732C: `488b41084885c074068990e4020000c3`; it only writes ID to state `[component+8]+2E4` when the state exists. This overwrites any previous pending value, performs no ID lookup, and does not establish an accepted or completed move. Trace its consumer before implementing the adapter.
- `RequestActionTransRepeat` 2391FA0 is a different path: after four VM arguments it calls 1C20298 at 239206E with context, live pointer, two integer arguments, a parsed float and zero second float. Repeat semantics/ownership are not closed; unsuitable for the requested one-shot default.
- `SetActionID` 2392840 constructs a reference-counted command via 2385D0C -> 2382978 -> 117E7B8. Command vtable 4042D80, payload ID at +8, execute +10 -> 1C0A41C. Wrapper dispatch 66BBF4 executes against an existing live pointer and also maintains its deferred-command list; it must not be substituted as a lifetime-safe immediate call merely because an actor lookup succeeded.
- 1C0A41C resolves live+3B0 -> component+8 and calls **66C4A8(state, ID, 0, 0.0f)**. This is an invasive forced transition: virtual +100 first; clears state +118/+260/+290/+258; sets +340, +2C0=-1.0, +33C; virtual +F8(ID,0,-1,false); tail virtual +170(true). Actual state vtable targets and all consequences remain unclosed. No AlgoActive test in this thunk does NOT prove a disabled boss can safely execute one complete move. Prefer resolving the pending-request consumer before choosing this path.

## Identity and candidate data, not attack labels

Existing placement/enemy join (`notes/boss-map-all.csv`, `rematch-enemy-index.csv`) gives placement65092, character-table scalar307259, parameter ID22178. Parameter record60 at file offset0xD7A8 in `data/extract/lfm/archive_03/C4/E1CBAE0841F7ECE87BD581BA0998F242FE69C4449DB57A473D072E0EB85FAC` has set keys +19C=710932, +1A0=187449, +1A4/+1A8=0, +1AC=295544, +1B0=359555.

These resolve respectively to rows128/129/1/2 in `archive_03/23/5B761F23D263081C964209B5914CC6E3E8421698B18B8D70CE8358DAEECE72` (528 records,432bytes). The table link is exact; calling these fields a boss attack list or its values transition IDs is unproven. Rows128/129 word2 are2778/2793, also unlabelled and not admitted moves.

Root subsequently closed the exact primary-record association to CN_MASAKAGE (hashCF276D56): record60/key307259/parameter22178 contains that hash at+248/+254. See `training-yamagata-asset-binding.md` for retained-file SHA256 and variant distinction. Current installed package root index and `data/tools/n3rdb.py`/`n3kod.py` reproduce:

- Loader file KTID56A3D77E,100bytes, object692E6BCB: SceneDatabaseResourceNameHashArray=A0D00ABE; CharacterSettingObjectDatabaseResourceNameHash=82AE0FD7; CharacterSettingObjectNameHash=62E11679.
- Setting82AE0FD7,3412bytes, SHA256 `cbd96bcbf109dd0f30b4152a9afe1631958b2f07055192cc844a76a86aa8dbea`: MotorCharacterSetting62E11679 has78 action-object hashes and78 group IDs, all groups zero. First object hashes268DA6A9/B3C8582A/410309AB. Object hashes are NOT integer transition IDs or78 attacks.
- Scene hashA0D00ABE and first action object hash268DA6A9 are absent as file KTIDs from both root and system indexes. This does not prove missing content: object/resource names can require a separate name-to-container join.
- CE_MASAKAGE_00 (hash84806F6C, loader359C51F0) belongs to the distinct598115/20878 parameter variant; its loader is absent from these indexes. The CN association is now directly verified, but transition-ID mappings are still missing. Do not derive a move allowlist from object hashes or import this alternate variant's moves.

## Script restrictions actually supported

Decoded retained mission `data/extract/lfm/archive_09/C6/8008F3E2B14D*` with `scratch/dec_a.py`. Shared-library requests127 and150 occur in `Function::DuelCommonFlow`; request357 belongs to `sub_event_kusanagi_roop`. None is established as a Yamagata attack. Mission-specific event flags91/24 target67938/68061, not65092.

The0.35HP check in `tsukumo_keshin_tuto` (F76A..F778) leads to follower974821, tutorial notices, player Amrita fill and mission flags. It supplies no verified per-move phase restriction. Do not translate the old heuristic HP-threshold CSV into a phase selector. Main encounter intro still controls AI activation; the existing native entry/intro readiness gate remains required.

## Smallest implementation contract still to close

The intended UI is a boss-specific allowlist of labelled moves, a one-shot Perform action, and explicit unavailable reasons. Each allowed row needs a proven transition-ID -> actual Yamagata action/move mapping, supported phase/state, and a native accepted/current-action then completed-idle witness. No arbitrary integer text box, guessed attack labels, automatic repeats, or claim that absence of a pending request means completion.

Resolve state+2E4 consumer and its action-map lookup/clear semantics next; join that actual map to the exact loaded boss type. Pair with the sibling idle contract: AlgoActive=false resets more than a bit, and current state/request validity must be independent of null/default sentinel comparisons. Bind dispatch to the current encounter/actor identity on the owning update phase, refuse occupied request slots and stale/unknown reads, issue once, and retain uncertainty on lost completion evidence. Offline proof cannot supply actor lifetime exclusion or prove a visually complete move.

Independent bounded peer review (training_contract): PASS for exact9A732C bytes/store-only semantics and the explicit non-attacklist limits. Related getter1FFD990 ->624518 reads state+2E4; consumer/completion proof remains open. User then reprioritized summon and arena switching first; move selection is checkpointed research, not an enabled feature.
