# Compute PSO cache: prototype gate - 2026-10-11

Finite read-only worker investigation; no implementation, game/harness execution or measured gain. This is separate from already built graphics PCM and current FO allocator tracing.

## Lookup and mutation evidence

- 58C0D8 returns false without changing output on miss; hit copies24bytes. 58C144 is B-tree **lookup**, not insertion. Equality uses u32hash and8bytes at+8; padding is ignored. Insert is58BA50->58BAA0.
- SynFlushComputeState58BE90 builds key from SynCtx+810, uses cache+7D8 and holds the CS referenced through+818. Getter58C000 and preload2EFE1CC also use that lock. Cache count+28, capacity+30; graphics PCM capacity+34 must not be copied blindly.
- Shader cleanup189912C removes entries and releases PSOs. Trim2EFA3A0, removal2EFC3D4, clear2F00AC8, reinit1899BD4 and teardown2F00BC4 invalidate contents/lifetimes. Generated C exports under D:/nioh3-re/decomp are the evidence; keep them outside Git.

## Verdict

Serving hits with graphics PCM's count/capacity/root invalidation is NO-GO for compute. Remove/reinsert can restore those fields between lookups; shader-pointer reuse can recreate the same key. This possible stale-PSO sequence is inferred from verified mutation paths, not an observed game failure. A bounded offline/compare-only prototype is justified by the existing profile; no FPS promise.

Before serving hits, prove mutation/lifetime invalidation (for example a generation updated by every relevant insert/remove/clear/reinit/destruction), preserve native locking, miss behavior, ownership/PSO creation and D3D12 ordering. Compare native equality fields while copying the complete24-byte result, including padding.

Meaningful production-code harness must cover equal-hash collisions, padding differences, misses, duplicates, same-root/same-count replacement, trim, pointer reuse, cache recreation, multiple caches and interleaved preload. Existing graphics harness covers refill with changed root, not the decisive same-root case. Its FrozenWrite5 also skips failed thread inspection; include that separate installer in a later failure-policy audit rather than claiming ApplyBatch repair covers all patch helpers.

First target test must be compare-only: proposed hits verified against stock boolean and all24 output bytes. Only after proof enable hits for repeated pc2 ABBA with actual lookup timings/hit/invalidation/drain counters, transitions/cleanup and visual checks; Ally no-regression before shipping.

Profile in notes/cpu-bound-midrange.md:59 attributes4% of dispatcher samples to compute lookup;1ms/frame is a model. Existing graphics PCM measured about+0.5%. notes/renderer/backend.md:234 already records built KL_Div=4 at+1.2% on pc2; validate existing release choices before duplicating that feature.
