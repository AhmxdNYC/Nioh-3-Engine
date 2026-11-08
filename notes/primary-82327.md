# Primary actor and group exit (#82327)

Base b659838f (multi-source), retaining follow-ground fix. Primary is the first press source matching the pressed core, otherwise the first source. Later secondary spawn notes cannot overwrite it. FormHide4 capture uses the same selection and promotes an earlier fallback when the matching actor arrives. Follow, end guard, idle motion and ghost request use that primary.

Ghost exit snapshots all observed hit sources, including the primary: each actor has its own identity/root, hide deadline, fade baseline, x2 tail ramp, and 20-second lifetime. Hooks read synchronized snapshots; cleanup removes despawned/reused actors independently. New registered spawn clears an old ghost entry at that address. Secondary smoke queues append without resetting the primary effects or their times; capacity stays bounded.

Multi-source counting and its pressed-core learning key remain unchanged. Included seat0 burst replay coverage absent from the requested base. Offline tests execute extracted production selection, capture promotion, fade/rate/lifecycle and smoke queue code plus the full last-hit tick. Live appearance remains for the lead; companion does not touch the game.

Validation: 229 last-hit + 24 primary/group/queue + 52 first-use + 23 ground + 112 picker + 23 cold-smoke + 29 equipment + 357 rematch = 849 PASS; available sc22 replay PASS. Release BR fbfd93d4fb247500baa81d1149845875261150329028a0592f5352e3110e9202 uploaded #6403. Source c24591f5. No live test.
