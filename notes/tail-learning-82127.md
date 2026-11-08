# Post-ghost burst observation (#82127)

Base a8e731a6 / combined candidate. Existing mask fix is confirmed by pc's supplied live evidence: 60B86's later attack colliders are directly seal-owned (attack=2, seal=2 at +0.533), while appearance=2 remains excluded. No additional projectile ownership offsets were needed.

New learned sections are SealHitsA2 (attack-only) and SealHits2 (all colliders). Old sections are untouched but never read; old built-in seeds are removed so EVERY core relearns once. Doji also has one full learning use; with corrected count two, its exit remains the live second opening (1.517 s in the supplied replay).

At ghost exit, copy the filtered pool state, excluded collider identities, core ID, hit count, open state and known helper owners into an independent observer. It runs even after visible form reset or a new form. The observer performs no exit, hide, spawn, or despawn action. It raises the stored max only when additional post-exit bursts open, never just because an early rule fired. Snapshot attack-only mode selects the matching versioned section even if live settings change. Subsequent visible sessions read the corrected count.

Presence uses the existing bounded actor-table membership check: confirmed despawn removes the observer; unreadable membership/pool preserves edges and defers counting. Core-ID changes stop it. At most eight concurrent observers, with a 30-second guard, log their end; neither guard invents a completed count. Fresh helper candidates are collected only while the original form owns the player; after reset the observer retains direct seal and captured helpers rather than adopting a later form's helpers.

Validation: 114 actual-code hit/tail/settings checks, 33 first-use, 111 picker, 23 smoke, 29 equipment, 357 rematch = 667 unique checks, all pass. sc22 available replay passes. Tail fixtures cover 1->2->3 growth after player restoration, no repeated exit, no circular learning without new bursts, max-only behavior, unreadable pool/presence, preserved section, timeout, and a subsequent form. Release build passes. No game touched. Next pc first/repeat uses of both cores; inspect SealTailHits logs and new sections.
