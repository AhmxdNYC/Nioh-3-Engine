# Meshlet CPU candidates: M1 lifecycle, M2 ordering, narrow M2S

October 13, 2026. Read `notes/cpu-stacks-current-2026-10-13.md` for the actual current profiles. Their meshlet capacity-list loop samples nominate work; they do not measure savings. This audit did not access the game or change an installed file.

## What was previously tested and why these remain off

M1 and M2 are experimental defaults of zero in the shipped INI and `Config`. `notes/worker-threads.md` explicitly describes static work and an own-process offline harness, not completed gameplay A/B. Its 6,000 M1 native cases include deliberate stale results; its suggested core savings are estimates, not observations. The checked PC matrix report `captures/tm/results_pc_1716.md` and targeted retained matrix report search contain no M1/M2 gameplay CPU result. `notes/remaining-work.md` still says built/off, need PC A/B. This is no evidence that either patch was rejected for a measured slowdown; equally it is no evidence of a measured benefit. LK is a separate set of 18 rows and does not already enable M2.

## M1's old lifetime argument is incomplete

The sum reads device+67A8/67B0/67B8 heads, mode+67C0, each node's capacity+14 and next+30. Existing M1 keys only head0 and a TSC bucket and publishes head/value in separate stores.

An actual mutator now located in the existing decompiles:

- `0x87583C` calls `0x875890` to allocate a new segment, finds the type's current tail, writes `tail+30 = new`, and `new+38 = tail`. Existing head does not change.
- `0x875890` computes growth size, creates the heap/segment, initializes its next/previous links, and advances the pool's page-base counter. Growth may be refused according to the pool limits, but is not universally absent.
- Streaming insertion paths `0x250D08`, `0x250EC8`, `0x2F0FB7C` call this append function after lookup miss. The exported callers of `0x87583C` also include `0x492310`, `0x4926E4`, `0x492A80`, `0x492CC4`, `0x2F0EE8C`.
- `0x4906E8` is the previously known exclusive entry-eviction user. Its shown body does not unlink segments. This does not establish that all other destruction/unlink/reset paths are absent.

Evidence: exact field-displacement/LEA xrefs in `D:/nioh3-re/meshlet-cpu-audit-20261013/pool-field-xrefs.txt`; exported functions in `D:/nioh3-re/decomp`. The direct field scan includes aliases such as `device+67A8` passed as a pool pointer, so scanning only literal displacement stores cannot enumerate all mutators.

The old claim that the list never changes during play is therefore unsupported. Complete constructor/destructor/reset/mode/capacity mutator closure remains unresolved. An unlocked native walk alone does not prove permanent immutability: mutation may happen at native quiescent points. Neither one TSC bucket nor an invocation lifetime proves correctness across growth or quiescent replacement.

Additional source-level gaps:

- The key omits the pool identity, mode and two secondary heads.
- Separate head/value stores are not a transactional publication. Same-tag writers for different heads can interleave `headA, headB, valueB, valueA`; headB then pairs with sumA. x86 store ordering alone does not prevent this. Reachability of simultaneous distinct heads is not established; this is a conditional counterexample to the old proof, not observed corruption.
- The refresh tags the sum after walking. A delayed refresher can publish an older sum with a newer TSC bucket. The old wall-time staleness bound is not a lifetime proof.

Minimal corrected design, deferred rather than installed: identify every pool mutation/lifetime transition first; bind cache to pool identity plus a native generation covering all heads/mode/capacities/links; use coherent publication (one writer/sequenced snapshot, or per-thread entries with a shared generation); check generation around the original walk, reject changing/unknown generations, and retain the native walk as fallback. Invalidate before any native free/reset and publish a new generation only after native construction/mutation completes. An epoch hook on append alone is insufficient. No TTL fallback, or invocation-local hoist across issue-load calls, is accepted as exact equivalence.

## M2 ordering screen and the testable subset

The original offline verifier establishes matching memory operand/value, zero source, dead flags and branch boundaries. It does **not** automatically establish the absence of a StoreLoad dependency across threads.

| Original M2 site | Review |
|---|---|
| 48FB9, object+B8 | Crosses an indirect object call earlier; no complete caller/callee StoreLoad proof produced here. Keep full group off. |
| 49046, entry flags | Prior loop iteration can write object bookkeeping at49091/49095/490B1; consumer/publication graph not closed. |
| 49261, page claim bitmap | If already claimed, path skips the write-CAS49280. Keeping that CAS does not preserve a barrier on every path. |
| 4933D, entry flags | Follows possible IssueLoad and non-atomic488/490 updates; full publication graph not closed. |
| 493DC, completion seed | Unconditionally reaches retained locked CAS493E9; only register instructions intervene. Suitable narrow candidate. |

The sibling early-exit path at49412 has the same seed pattern and unconditionally reaches retained locked CAS49428. It also preserves its preceding output store4940D. At either site, a plain aligned seed read can be stale, but the retained CAS validates it; failure supplies the current value and retries. There is no intervening publication, memory read, helper call or branch that bypasses the retained CAS. That CAS retains the StoreLoad barrier before any subsequent memory access and before completion becomes visible. The bit9 update, failure loop and all real work remain unchanged. This argument is confined to these two seed sites, not the entire original M2 group.

## Candidate and concrete gameplay comparison

Isolated branch `codex-meshlet-seed-20261013`, based exactly on hosted/MW commit592a888, at `D:/nioh3-wt/meshlet-seed-20261013`.

- New `[Patches] M2S = 0` default; set1 only at a fresh launch for this trial. Keep `M1=0`, full `M2=0`; preserve current WG/LK/MW, all other switches, settings and native save progress. Do not copy the repository's default INI over device settings.
- M2S changes493DC and49412 only, in one existing frozen `ApplyBatch`. M2e's existing row is reused, so the manifest has no overlap. Exact dependency bytes pin both retained CAS/retry regions. Runtime must log `M2S: applied (2 site(s), ...)` and the explicit M2S ON line before classifying an ON run.
- If full M2 is explicitly nonzero, its existing five-site behavior takes precedence and M2S is skipped/logged. A trial must reject that configuration; it is not the two-site candidate.
- No menu/MW/hosted-rematch implementation changed. No hot toggle is added. The old full-M2 menu row does not describe M2S.
- Compare same candidate binary with M2S0/1/0 across fresh launches in the same scene/route and fixed cap. Capture actual process CPU plus aligned frames, RAM, available GPU/VRAM and available sensors through UO session_capture. Record missing sensors. Use gameplay, including streaming/travel, and check visible behavior; do not introduce artificial memory/noise launch gates. CPU/FPS deltas remain unaccepted until real results.
- This subset removes two seed RMWs per relevant invocation, not the hot capacity loop or all page RMWs. No numerical benefit is predicted and a negligible result is plausible.

Validation: exact original-vs-patched machine code executed in an isolated native harness,40,002 equivalence/contention cases PASS. The harness exercises both exits, random/edge words, unchanged output and bit preservation under concurrent OR writers. `test_patterns.py`: ALL258SITESOK, including exact bytes, unique patterns, generated header and dependency ranges. Retained locked CAS ordering is a source argument, not proven by stress success. Build/pins and a source patch are delivered separately; no gameplay acceptance follows from these checks.
