# Multi-actor hit sources (#82289)

Base: 3d72e837 / BR 39483dda, including 35fb8235 cold wait and c4383d2a float fix.

The observer previously admitted helper callbacks relative to its first tick, potentially excluding a first actor spawned before the observer started. It also keyed learning to the bound actor. The reported 16127 then C17E3 sequence therefore needs independent press-source membership and pressed-core identity.

This change anchors source admission to the press snapshot, retains all distinct spawn-note actors for that snapshot (not only the old eight-entry helper ring), and counts colliders from their union with the bound actor. Identity appearance exclusion (200 ms) and new-object clustering (100 ms) are unchanged. Counts use the pressed core; the post-exit actor identity guard still checks the actual bound actor. Physical binding, hiding and float code are unchanged.

SealHitSource logs admitted actor, per-actor raw census including zero, and the contributing actors at each visible-form burst. No live claim that 16127 owns attack colliders is made: both empty pools retain the existing idle fallback, not an invented timer or count.

Release build passed. 752 checks passed: last-hit 149, first-use 52, picker 112, smoke 23, ground/restore 23, equipment 36, rematch 357. Available sc22 replay passed (two optional historical fixtures absent).
Replay covers first actor at +.27 / bound actor +.38 before observer +.4, pressed-core learning, post-exit bound identity, stale actor rejection, and twelve sources beyond the old ring. Actual spawn recorder deduplication and snapshot rollover tested. Existing Doji/60B86/A2ACB coverage remains passing.

Next: pc live census for first/repeat Blood Edge Demon, verify 16127 versus C17E3 sources, then Doji/60B86/A2ACB and no float regression. If both source pools are empty, report the census; do not guess ownership. No game operation by this worker.
