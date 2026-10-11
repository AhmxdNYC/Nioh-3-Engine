# Compute cache teardown boundary

Native review against retained v2.02 dump, 2026-10-11. No installed hooks.

2F00BC4 saves its third argument r8b in r14b. At2F00D46 it tests that
flag; nonzero branches past compute trim/clear to2F00D6B. This destination
still loads context+818 and, when nonnull, calls256810 then zeroes the
pointer. Thus the compute-cache clear is conditional while lock-pointer
cleanup is not conditional on that flag. A lifetime epoch tied only to
clear/remove would miss this boundary. Retire comparison state at teardown
entry regardless of the flag; do not infer cache preservation implies a
usable lock or ongoing context lifetime. Follow-up read of256810 resolves
this as reference-count release under a global critical section, invoking
vtable+18 only at zero. It does not prove unconditional destruction of the
lock object; the context unconditionally drops its own nonnull pointer.

Native direct destructor caller2EFB020 passes zero for both flags at2EFB03B/
2EFB03E and calls teardown at2EFB046. Wrapper2EEDEF8 retains caller flags,
clears stream metadata and tail-calls teardown at2EEDF89 with context+10.
Its retained direct caller is GfxDeviceTeardownForReset2EEC4C8. Context
base offsets must therefore be normalized before indexing observer state.
1899BD4 clears/reinitializes the cache;2EFDFDC is another14-byte wrapper
calling it. An indirect caller inventory and synchronization at reset
remain open. This is bounded evidence, not complete lifetime closure.

Verification added to tools/bench/audit_renderer_cache_locks.py: exact
decoded flag propagation, branch target, clear call and lock cleanup sites.
No CPU/FPS claim; compare-only core remains offline and serving hits stays
NO-GO pending mutation/lifetime and native integration proof.

## Reentrant comparison repair

Added actual offline core cases where the stock callback completes a mutation
scope, retires the cache, or rebinds using the same caller token. Before repair,
the first case failed: admission was cached before invoking stock and the
outer callback compared/refilled after its scope had changed. Every lifetime/
mutation boundary now increments an internal revision; a changed revision
suppresses both comparison and refill after stock returns. Native return value
and all output bytes are preserved. MSVC /W4 /WX suite now168 checks PASS,
including9 new reentrant checks. This is a demonstrated prototype defect,
not an observed game race. Native integration remains uninstalled.

## Direct-edge inventory

audit_compute_direct_edges.py scans executable sections for call/tail-jump
candidates and verifies instruction boundaries using pdata function ranges.
It confirms three lookup/insertion callers (58BE90,58C000,2EFE1CC), two
remove callers (189912C,2EFA3A0), three trim callers, two clear/reinit/teardown
callers each, and one pdata-covered shader-cleanup caller2EE7F64. Report kept
under scratch/compute-direct-edges.json. Indirect references are not covered.

One extra target outside pdata resolves to12-byte leaf2EFD04C:
`lea r8,[rcx+7D8]; jmp189912C`. The direct-edge scan finds no direct caller
to that leaf, and a raw absolute-pointer search found no reference using
the dump image base. Its runtime reachability is unresolved; absence from
the old decompiler caller list is not proof that it cannot execute. Hooking
only known outer callers would require resolving this edge. Instrumenting
the common cleanup function would cover either entry, but still needs the
complete native locking/lifetime contract.
