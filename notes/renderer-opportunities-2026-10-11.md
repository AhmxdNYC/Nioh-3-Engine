# Renderer opportunities and new native findings - 2026-10-11

Private successor research. Existing decompilation plus bounded Capstone reads
of the inert v2.02 dump; no Ghidra, game launch, installed edits or shader build.
Default picture remains unchanged. User now explicitly wants both same-picture
opportunities and separately labelled optional quality tradeoffs considered.

## New work actually completed

Compute PSO trim calls Release at2EFA3FA BEFORE removal2EFA407. Native lookup
58C0D8 copies24 bytes on hit and leaves the input untouched on miss; equality
is hash32 and8 key bytes at+8, excluding padding. This independently confirms
the contract in handoff-codex-compute-cache.md. Inference: invalidating only
at tree removal is insufficient to cover a release-time reentrant lookup;
real reentrancy is not demonstrated. Begin mutation exclusion before release,
keep it through removal/clear, and preserve the native lock/lifetime boundary.

New tools/bench/compute_pso_shadow.h is an OFFLINE compare-only core, not an
installed hook. It always invokes stock once and returns its boolean/full
output; no cached native pointer is dereferenced or served. An explicit cache
lifetime token and nested mutation scopes suppress comparisons/refills during
mutation or for another cache. The caller must eventually prove those tokens
and hook boundaries; this core does not magically discover native lifetimes.

MSVC /W4 /WX test_compute_pso_shadow.py PASS159 checks:49 stock lookups,
17 predictions,14 equal results,3 deliberately detected stale-result cases,
21 suppressed comparisons. Covers hash collision, key padding versus full
output padding, miss nonmutation, unchanged-size replacement, nested mutation,
cache-address/shader reuse and independent caches. These synthetic cases
validate the core behavior, not native mutation coverage or FPS improvement.

Frame-overlap helper audit resolves12B0D0/12AF40 as acquire/release SRW on
resource+198: mode1 shared, mode2 exclusive, other modes no helper locking.
Imports and bounded native instructions independently checked by
 tools/bench/audit_renderer_cache_locks.py (PASS).
2F06AB4 brackets152D48 bitmap writes and token98 emission with those helpers.
152D48 updates byte cells conditionally; its read export shows no wait/call.
Inference: these resource-local helpers alone do NOT serialize the context
stream cursor3C40 or establish producer closure. Caller modes/outer locks
remain the exact next FO proof; no demonstrated race or safe flush claim.
Native43-byte window hashes are emitted by the audit, not copied game bytes.

## Ranked opportunities

| Priority | Opportunity | Picture and evidence | Next concrete gate |
|---|---|---|---|
| 1 | Fog parallelization | Intended same-picture; full finite proof PASS, native acceptance open | Ally queue1954: exact shader execution, actual FG state, image/noise comparison, then profiler-off ABBAAB |
| 2 | CPU compute-PSO lookup memo | Intended same-picture; new shadow core PASS, no runtime integration or gain | Trace complete mutation/lifetime callers and integrate compare-only at native lookup under its existing lock; never serve hits yet |
| 3 | Meshlet LOD uniform-wave work (lodu) | Existing distinct pack provenance PASS; no new measured gain | Exact d5a1a069 execution and moving/streaming native comparisons; separate from conflicting mshlod pack |
| 4 | Meshlet culling20/33 | Guarded uniform G1M-row load reduction is an unbuilt proposal | Exact MinRun1 pass costs first; request1965 piggybacks already planned Ally profile; old MinRun4 costs22/24 are not20/33 savings |
| 5 | Optional fog/cloud/shadow/LOD reductions | Explicit quality tradeoff, not same-picture; existing XPRE1 historical PC+3.14%, Ally+4.16%, pc2 neutral in recorded scenes | Isolate individual existing ExtraPerf levels after current test, measure visual loss and frame-time benefit; retain user defaults and restoration |
| 6 | Frame overlap/effect readback | Experimental; FO remains NO-GO/defaultOFF | Complete producer closure, stream bounds/overflow and resource retirement; replay cost reduction helps stock without requiring overlap |

Historical FO8 pc2 gain was only about0.4%: drain wait largely moved to the
effect GPU-to-CPU sync. FO11's+11.74% was under an artificial8ms replay delay,
NOT a native-device FPS gain. Two-frame effect readback changes timing and
could change visible effects; any future test belongs to the optional/experimental
category until equivalence and behavior are established. No claimed15-25% win.

GPU profiling historically put USER G-buffer around7.54ms versus culling2.88ms
and fog2ms, with asynchronous overlap/grouped-pass limits. This motivates
material/culling research but is not an additive savings budget. Never add
quality-preset percentages to constant-upload or fog results.

## Current device work

Ally acknowledged1962 and owns both locks for fog validation on fixed USER
Engine1.7.44/GHDM; root1963 requires fresh entry backup before changes. New
culling row request1965 adds no launch/config change. No result received yet.
pc2 AABB queue1955 still awaits ACK/recovery gates. Root stays game-free and
continues offline rendering research. Historical default archive arithmetic
reproduced; exact sample slices requested1964 remain pending.
