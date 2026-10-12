# Arena switching for practice: native contract and smallest next step

2026-10-11. Read-only source/catalogue/existing-decomp review; no new native call, build, device or game action. Current implementation reference is private candidate3db5bc4 (code equivalent0c4a334); earlier training design notes describe older code and are not current admission policy.

## What can currently be selected independently

The current native route selects a **mission + current journey + start/retry point**, not an arena geometry plus an independent enemy. `D:/nioh3-re/decomp/0x2239600.c` (`MissionRequestById`) copies the mission/start into a native request, derives mission-record data and uses mode2; its four inputs contain no proven separate enemy/asset selector. The byte-checked argument/record interpretation is in `notes/training-stage-contract.md:31`. `0x223af50.c` (`MissionLoadStep`) waits for native load completion and performs mission-specific manager/setup work; names in this export do not constitute proof of all asset lifetimes.

The existing addon `ExecDirect` calls that native wrapper and verifies the requested tuple; `TrackReplay` and `CallReturn` retain native lifecycle ownership. The currently implemented Practice action pins only R1CBC/0x1CBC/65092/67064; it does not expose another arena or an enemy-preservation option. Returning and then requesting another allowlisted native encounter is implementable using these primitives. A live same-place transfer of Yamagata into a different mission is not established.

Preserving the user's *logical selection* while showing a filtered list of compatible native encounters is possible UI behavior. Preserving the same Character object, exact enemy stats/loadout, or arbitrary enemy choice across a mission switch is a different, unproved contract. Do not label a native mission variant as the same configured enemy.

## Smallest destination and useful bounded pair

The lowest-complexity additional destination in the existing catalogue is **RC362 / Hattori Hanzo / The Demon's Regimen**, mission0xC362, placement1102178, start1107817. Its row is a native mission-start encounter approximately18m from the boss; no spawn-record patch or gate injection is needed to request it. This is a static candidate, not a runtime-safe acceptance result. It requires current-journey clear and its own actor/arena/controller/retry/return observations.

For a concrete future *same named boss, different native arena* selector, compare RC362 with **RE18C / The Way of the Ninja: Adept**, mission0xE18C, placement5043775, start5044637. Both list Hattori Hanzo and native mission-start encounters, but the catalogue coordinates differ substantially (RC362250806,9624,251043; RE18C76529,3555,56560). The coordinates identify catalogue data, not proven walkable placement. Prefer Adept to RD759 Novice, whose row lacks a placement/start descriptor. R9BE4 Veteran shares Adept's catalogue start coordinates and therefore does not demonstrate a different arena.

Crucially, `notes/boss-map-all.csv:78` maps1102178 to enemy-record scalar131751/PARALLEL_102_HANZO, while `:60` maps5043775 to339773/TEACHER_MISSION_HANZO_01_BOSS. Veteran at`:69` is another scalar865761. This evidence supports separate native encounter variants, not identical enemy configuration or transferable actor identity. For Yamagata, other catalogue missions containing his name (R0F7F and R0B0B) are multi-boss encounters; they are not a minimal single-enemy-preserving switch.

## Transition contract to implement first

Expose compatible **native encounter presets**, initially the already-reviewed route and at most one separately admitted destination. Switching must be a serial transaction: one native return; ordered return-request and native replay-exit evidence; stable exact original entry tuple with idle/nonquarantined state and ownership restoration; fresh current-journey destination eligibility/queue checks; one destination request; exact queue witness and observed load/settled destination; root visual arena/actor/control acceptance. Do not submit a mission switch from an active fight or retain actor pointers across it.

Mission0 can be a valid original overworld entry. The earlier training-design requirement for a nonzero return destination was disproved by retained C72E pilot193651-24196: return reached original0x0/stage4019512 and native TrackReplay reported exit. The new tools bind return to the original tuple rather than rejecting zero or accepting any foreign mission. A request acknowledgement, empty queue slot or global fight flag alone does not establish completed cleanup or readiness. Late/uncertain admission remains quarantined; no automatic repeated return/start.

For a compatible Hanzo preset pair, retain only a logical family selection across the two independent encounter transactions. Each destination supplies its own placement, scripts, stats and native rewards. User-facing labels should state the variant; do not claim a fully independent Enemy/Arena picker.

## Why arbitrary cross-mission enemies remain blocked

`notes/training-spawn-contract.md` documents the native activation lookup0x1180740: it resolves a placement record in loaded sets, requires an existing registered wrapper, and may begin deferred Character construction. Returning true does not prove a live ready boss. `ActorFindById`0x135B78 does not retain an object. Creation consumes mission/resource/placement ownership; no proven asset lease survives a mission transition.

`Character::CreateDirect`0x238C9BC and `CreateDynamic`0x238CD5C are VM wrappers, not APIs accepting an enemy ID directly. The lower dynamic candidate0x1C0C5E0 has mechanically identified arguments but unresolved record/resource selector semantics and lifecycle. `ResetReceivedDead`0x1180940 is also a VM wrapper; its downstream bit clear is not revive/HP/AI/script reset. Group deletion is not exclusive actor disposal. Player spawn override changes player coordinates/camera data only; it does not import enemy assets, AI/navigation, native gates or boss scripts.

A genuine independent enemy/arena implementation therefore needs: enemy definition/resource-handle selection and retention; destination-compatible collision/navigation/AI/script context; construction completion and unique live ownership; scoped cleanup with no pending callbacks; intro/gate/death/reward isolation; and failure/transition restoration. Existing notes do not close these edges, so copying a placement ID or holding an old actor pointer is not an admissible shortcut.

## Concrete next proof

First test the additional native RC362 preset only after the current hosted Yamagata acceptance, without spawn/warp or enemy substitution. Record destination/placement/start, visible boss/terrain/controller response, native retry and exact return. Separately, a bounded read-only native investigation should follow the proven lower dynamic-create caller0x238CE8A into0x1C0C5E0: identify the record/resource keys and which reference owns them through construction and disposal. Until those are proven, an independent enemy/arena switch remains unavailable; native compatible presets can still deliver useful arena choice with explicit limits.
