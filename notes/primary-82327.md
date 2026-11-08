# Primary actor and group exit (#82327)

Base b659838f (multi-source), retaining follow-ground fix. Primary is the first press source matching the pressed core, otherwise the first source. Later secondary spawn notes cannot overwrite it. FormHide4 capture uses the same selection and promotes an earlier fallback when the matching actor arrives. Follow, end guard, idle motion and ghost request use that primary.

Ghost exit snapshots all observed hit sources, including the primary: each actor has its own identity/root, hide deadline, fade baseline, x2 tail ramp, and 20-second lifetime. Hooks read synchronized snapshots; cleanup removes despawned/reused actors independently. New registered spawn clears an old ghost entry at that address. Secondary smoke queues append without resetting the primary effects or their times; capacity stays bounded.

Multi-source counting and its pressed-core learning key remain unchanged. Included seat0 burst replay coverage absent from the requested base. Offline tests execute extracted production selection, capture promotion, fade/rate/lifecycle and smoke queue code plus the full last-hit tick. Live appearance remains for the lead; companion does not touch the game.
