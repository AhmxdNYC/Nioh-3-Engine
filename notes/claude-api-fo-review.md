# Claude API review of experimental FO deferred commands

2026-10-11. Attributed suggestions; not a proven fix or runtime result. No credential retained. The final response hit its output cap and is incomplete; there is no automatic retry.

Lead assessment: the queue-full fallback and missing explicit Flush98 space check agree with the existing source audit. The review's assumption that original emitters check capacity is unsupported: the previously inspected7C50B4 emitter does not have its own room check. Its fake-buffer and reentrancy claims are hypotheses, despite the model's heading; verified12-byte emitter behavior is relevant evidence. Treat snapshot/swap advice as a starting point only: preserve object lifetime, epoch updates, GPU ordering and FO half-stream protocol. No additional fix, safety verdict or performance result follows from this review.

Setup: first call4725input+1600outputtokens exhausted budget in hidden thinking, no visible review. Second returned text but Windows pipe decoding failed; usage unavailable. Corrected Sonnet5.5 thinking mode and ASCII-safe JSON. Third bounded call below; no automatic retries configured.

Model: claude-sonnet-5-5; usage: {"input_tokens": 3246, "cache_creation_input_tokens": 0, "cache_read_input_tokens": 0, "cache_creation": {"ephemeral_5m_input_tokens": 0, "ephemeral_1h_input_tokens": 0}, "output_tokens": 1200, "output_tokens_details": {"thinking_tokens": 0}, "service_tier": "standard", "inference_geo": "global"}; stop: max_tokens.

**Scope:** read-only review of lines 615–695 and 2085–2107 only. I haven't seen `EndFrame`, `kWrite`, `F<>`, or the caller of `Flush98`. Anything depending on those is a hypothesis.

## Proven issues

**1. Queue-full fallback runs the emitter against the live `ctx` (658–662).** On overflow, `g_e98[I](obj, ctx, ...)` runs on the off-main thread, which is the exact race the deferral exists to avoid. The comment says "never seen", but the fallback is the old unsafe path. It does not lose records, but it is unordered and racy relative to main's stream writes. Also, `++s_98full` is the only trace. There is no log on first occurrence.

**2. Unbounded write into the stream in `Flush98` (681–685).** It advances `dev+kWrite` by 12 bytes per record, `n` times, with no bounds check against the stream/buffer end. `n` can be up to 16384, so up to ~196 KB. Whether the stream has room is unproven from the supplied lines. This is a real stream-bound issue: no capacity check exists in the shown code. The original emitters presumably check capacity internally. Flush bypasses that by writing directly.

**3. Faked emission goes into a 16-byte `buf` (664–668).** The emitter writes through `fake+kWrite` into `buf[4]`. The code assumes the emitter writes at most 12 bytes, then reads `buf[1]` and `buf[2]` (671). If the emitter ever emits a larger record or checks the bound against a different field, this overflows the stack. The only guard is the `rangesOk` hash on the code, which supports the assumption but doesn't prove it.

**4. Lock held across emitter call (657–674).** The emitter runs under `g_98lock`, and `g_98fake` is a single shared scratch buffer, so producers serialize. Not incorrect, but any emitter that blocks or re-enters `Fo98` deadlocks (SRW is non-recursive).

**5. Unsynchronized `g_98n` fast-path read (677).** The early `!g_98n` read is outside the lock on a `volatile`, not an atomic. On x86 it's benign in practice, but it is formally a data race. The re-read under the lock (679) is correct.

## Hypotheses (unverified)

- `g_98fake` is 0x6EC0 bytes but only `+0x6EB8` and `kWrite` are initialized. The emitter may read other `ctx` fields, leaving stale or zero values.
- `buf[3]` is never consumed, so if a record is 16 bytes, the dropped word is lost.
- Flush's "between tokens" ordering relative to `EndFrame` is unverified.

## Minimal, safe repair strategy

- **Do not hold `g_98lock` across `EndFrame`'s replay drain.** Flush should snapshot under the lock into a local array (or swap in a second buffer), release the lock, then write to the stream. Replay threads calling `Fo98` then never block on a main thread that is draining.
- **Bound-check in Flush.** Compute the remaining capacity (need the stream-end field, which I haven't seen). If insufficient, flush a prefix and retain the remainder in the queue, or follow the emitter's own grow/flush path.
- **Overflow:** instead of calling the emitter against live `ctx`, either block/spin until main drains (risky if main waits on replay) or grow the queue. At minimum, log once and keep the counter.
- **Make `g_98n` a `std::atomic<uint32_t
