# Compute cache: constructor, duplicate insertion and preserved reset

2026-10-11. Read-only native continuation of `compute-cache-teardown-boundary.md`.
No game, hook, ASI change, benchmark or new shadow core. Serving cache remains
NO-GO, but this closes three concrete ambiguities in the invalidation contract.

## Constructor and successful initialization are distinct boundaries

Actual SynCtx constructor **2EFA840**, called by outer constructor **2EE0144**,
sets vtable409A680 and zeros cache root/free/block/allocator/count fields at
2EFA8ED..2EFA910; capacity+808 at2EFAB95 and lock pointer+818 at2EFABAC.
It does not create a usable cache lock. Exact whole1600-byte constructor hash
is pinned in the new audit. Do not admit a cache simply on constructor return.

Actual dispatcher creation **2EED444** passes outer context+10 to
**2EFEA14** at2EED462. In that initializer, config+49!=0 and current capacity
context+808==config+44 skip reinit1899BD4 entirely (2EFF3C2..2EFF3FB).
Both paths then call **7507C8** at2EFF402 and store its result in context+818
at2EFF407. Null branches to failure2EFF999. The helper allocates38 bytes,
initializes the lock at object+10, and increments its reference count.

Thus reset can preserve the *same actual tree*, root, entries, count and
capacity while replacing the lock object. Together with the known teardown
flag that skips clear but releases+818, this is a concrete preserved-cache
lifetime path, not merely a hypothetical remove/reinsert sequence. A lock
address can also be allocator-reused; neither lock address nor tree identity
is a sufficient epoch. Retire at initializer entry and bind only on successful
complete initialization, including preserved-tree paths. Never bind merely
when the allocation at2EFF407 succeeds: later initializer operations can fail.

## Duplicate insertion is not replacement

58BA50 initializes insertion count zero and existing-record pointer zero in
its local argument block. In58BAA0 the equality call58C114 branches at58BAFD
to an **out-of-line cold block19D9806**, which only stores the existing record
address to argument-block+18 and jumps to return58BC52. It does not overwrite
the24-byte record. Wrapper58BA50 returns insertion-count!=0, therefore false
for this duplicate case. A naive contiguous hot-body review misses this cold
edge; the audit binds it separately.

This rules out duplicate insert as an in-place replacement in this path.
The earlier remove/reinsert and shader-address reuse concern remains valid.
Do not equate helper58BAA0's true return with wrapper insertion success or
transfer ownership of a rejected new PSO: the known callers release it when
58BA50 returns false. No universal claim about all indirect/native writers.

## Reentrant and reset synchronization boundaries

Known runtime lookup/create/insert paths58BE90 and58C000 hold the native
CS; preload2EFE1CC creates outside it but performs lookup/insert inside it.
Shader removal189912C removes before Release, whereas trim2EFA3A0 releases
before removing. Clear2F00AC8 calls allocator+58 at2F00AEC before root/count
zeroing at2F00AF7..2F00B0C, with no local CS acquisition. Reinit1899BD4 calls
clear before replacing its allocator/allocating nodes. Consequently mutation
exclusion must start at trim/clear/reinit entry, before external callbacks,
not after remove or after a count changes. Actual callback reentrancy is not
established; a serving memo must nevertheless preserve the stock behavior if
it occurs. The existing shadow revision repair remains necessary.

Reset has more synchronization than the prior note resolved:2EEC4C8 calls
**2EEDDEC**, which loops on DispatcherRingBusy(outer+3BE0) until false, then
tail-calls **2F00A74(outer+10)**. That leaf chooses2F01108 when context+580
is nonnull (close command list, fence poll/wait path), otherwise2F14328 on
context+5A0 (ring finalize/idle wait and command allocator release). These
are actual dispatcher/command-work drains, not a native compute-cache CS.
They do **not yet prove** that another producer cannot enqueue after the
busy test, or that all preload/shader-cleanup threads have joined.

Known destruction remains2EFB19C ->2EFB020 ->2F00BC4; outer destructor
2EE09FC also reaches2EFB020. Known reset is2EEC4C8 ->2EEDEF8 ->2F00BC4.
Teardown retires before any release even when its preserve flag is nonzero.
Entry instrumentation of common primitives covers known leaf wrappers;
hooking only an outer reset/destructor misses the other paths.

## Exact remaining gate and next implementation choice

There is no defensible live serving cache yet. The next useful closure is
**preload scheduling/cancellation across GfxDeviceTeardownForReset and
GfxDeviceCreateDispatcher**, specifically how2EFE1CC's owning task prevents
concurrent entry or an in-flight post-create lock acquisition while+818 is
released/replaced. Dispatcher-empty and GPU-fence completion are insufficient
substitutes. Resolve that task owner/stop boundary before any cache hook;
do not add another observer just to rediscover hit rates.

A future adapter needs explicit monotonic epochs at constructor/init/teardown,
bounded mutation scopes covering insert/remove/trim/clear/reinit, and native
lock-held admission. It must never extend a PSO lifetime by inventing AddRef
semantics or serve during teardown. These requirements are necessary, not
proof that the complete adapter is safe or faster. No FPS gain is asserted.

Verification: `tools/bench/audit_compute_cache_lifetime.py <private dump>`
checks13 immutable native windows (including constructor and cold duplicate
branch), rejects13 altered-byte fixtures, and checks14 named instructions.
The dump remains private; no native game bytes are committed.
