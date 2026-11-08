# Collision masking investigation (#82077)

Base: combined 903df625 (cold-wait, equipped first-press, smoke fallback, idle learning, bind window).

Read-only existing pc log at 10:59:55.111 pm EST: 60B86 has one ignored collider at bind, actions change at +533/+1300 ms, no burst edges, despawn +3200 ms. This does NOT establish that damage is outside the pool: the previous implementation detects only aggregate zero/nonzero edges. Any persistent ignored collider keeps `open` true and hides later valid colliders, including existing tracked helpers. The log has no per-frame collider census, so the exact damage owner remains unproven.

Change: exclude appearance objects individually by collider address and owner, retain exclusion while observed, remove it when absent, and count only remaining attack colliders. Existing body filtering and seal/helper ownership are unchanged. No new actor is assumed owned from proximity, motion or FX alone. The existing helper list itself is populated by nearby 65A8A8 spawns; this patch does not strengthen that historical ownership heuristic.

Added bounded SealHitPool diagnostic: up to 64 changed-count lines per form, raw/appearance/attack/seal/helper/body. This can distinguish persistent-mask failure from missing-owner failure on the next pc test without pool scans beyond the existing bounded 4000-slot copy. No effect, timing, hit-count seeds or per-core tables changed.

Validation: 99 actual-tick checks including persistent appearance + later direct and tracked-helper bursts, filtered idle learning then next-use last-open exit, foreign owner rejection, pointer reuse after observed removal, unchanged 60B86 appearance-only fallback, and Doji last open 1.668 s. Additional 33 first-use +111 picker +23 smoke +29 equipment +357 rematch =652 unique checks; sc22 available replay passes. Release succeeds. Later 60B86 damage windows in the new tests are synthetic hypotheses, not recovered live timestamps or proof of projectile ownership.

Next: pc install candidate and inspect SealHitPool during 60B86. If attack rises while appearance=1, mask bug is confirmed live; first complete form learns and next exits at its actual last opening. If attack stays 0, obtain collision-owner samples/child spawn evidence before extending ownership. No game touched by cx-pc.
