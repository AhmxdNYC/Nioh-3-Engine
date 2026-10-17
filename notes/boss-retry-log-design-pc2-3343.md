# Boss retry lifecycle: log-only experiment design

2026-10-14, pc2; Task #3271, export delivery/continuation #3343.

**Verdict:** the 13 additional decompiles support a concrete first-entry/two-retry observation plan. They do not establish a complete resident-arena reset or a safe loader bypass. Retain native mission loading and measure script lifetime, owner resource release, boss construction and player recovery separately. No instrumentation, build, game launch, installed-file change or save access was performed for this design.

## Evidence and changes to the earlier report

Bridge file416 was downloaded with SHA256 verification: `5cad28bc00d2798c105a834a796ec987fedf033ec79aec78100256a87cdc5a65`, 8,388 bytes, `loader-decomp2.tgz`. All 13 requested files are present. In this report, `decomp2/` means the archive root; pc2 retains it at `C:/nioh3-re/bridge-files/task3343/`. PC states the source executable is the same `D:/nioh3-re/Nioh3.dump.exe` used for earlier exports. Export headers span October 6, 10 and 14; this is not a fresh executable-hash or live-byte verification.

This report supersedes the missing-decompile status for these 13 RVAs in `notes/boss-only-load-pc2-3271.md`. Other targets there remain notes/symbol evidence. The preference for shipped duel missions where available, followed by research into resident-arena re-entry, is unchanged.

1. **A selected cell is real; a complete selected-cell workload is still unknown.** `223B8F0` checks area/object readiness and player availability before progressing (`decomp2/0x223b8f0.c:49`, `:58`). One path resolves the start/warp record; another obtains coordinates through a separate record (`:76`, `:94`). It conditionally calls `3A1700` with the selected position (`:125`), then dispatches to `1FCECE4`, `1FCEE1C` or `1FC4208` (`:130`, `:134`, `:142`), with another conditional call to `1FCCFF4` (`:138`). It finishes by writing sequencer state 5 (`:213`). This is a state-advancing function, not a callable streaming query. A cell-centered request does not reveal all cells scheduled downstream. No inference of arena-only residency or speedup follows from the selected cell alone.
2. **The Emaki filter is narrowly placed.** `1C1E470` first processes a separate `mission+0x60` resource (`decomp2/0x1c1e470.c:97`), then `mission+0x6C` (`:117`). Within that second path, mission `0xCC96` compares the row ID to Emaki record `+0x10` (`:130`) before building a type-6 descriptor (`:168`, `:180`, `:201`). This is not a general enemy-table filter. The precise gadget-kind 907/909 predicate remains prior-note evidence because callee `3B2648` is not exported here; do not claim those constants are visible in this function.
3. **Owner release drains counted references.** `1C10570` performs several mission-ID-based cleanup calls using owner `+0x120`, then enters `1C08D1C` (`decomp2/0x1c10570.c:15`). The latter walks owner `+0x3C8`, calls `1C1DF80(id,0)` once per positive count and decrements each count, then clears the container (`decomp2/0x1c08d1c.c:14`). This proves reference-drain behavior in this owner path. It does not prove physical asset eviction; other owners or caches may retain resources.
4. **Script replacement, completion and owner shutdown are distinct.** Launch looks up a name in manager `+0x40`, replaces its execution slot and starts its named entry (`decomp2/0x94b04.c:35`, `:49`). Replacement calls release, installs a context at slot `+8`, and assigns an owner through the context-associated object (`decomp2/0x94d80.c:15`). Release nulls slot `+8` (`decomp2/0x95244.c:11`). The completion predicate checks context byte `+1` plus matching execution values (`decomp2/0x282574.c:16`). Owner removal tests `+0x134`, launches `mission_end` once using `+0x142`, waits through `23BD710`, then proceeds with container/refcount removal (`decomp2/0xe22d10.c:43`, `:72`, `:87`, `:257`). Neither a completion result nor a release call proves cancellation of already queued gameplay operations. Name-key duplicate behavior remains supported by the earlier static note, not independently re-exported here.
5. **Boss resource readiness is not a reset.** `669388` resolves wrapper `+0x30` and tests resolved object `+0x2C8` (`decomp2/0x669388.c:16`). `669110` checks it, then further gates, and skips allocation if wrapper `+0` already contains a live object (`decomp2/0x669110.c:26`, `:37`, `:45`). Its final state write is 2 even on the live-object branch (`:132`). A successful readiness poll, a call return or state 2 alone cannot establish a fresh boss or restored HP/AI.
6. **Recovery submission is not recovered control.** `1C4D608` builds a request, submits it, performs cleanup and returns whether a handle field is nonzero (`decomp2/0x1c4d608.c:25`, `:31`, `:35`). `1C45DD4` inserts into one of two paths, optionally under a critical section, and returns its second argument (`decomp2/0x1c45dd4.c:16`). Observe native submission and subsequent player state separately; neither routine is an admitted synchronous resurrection API.

## Exact observation contract

All following hooks are proposals, subject to disassembly/ABI validation. Each wrapper must call its original exactly once, preserve native arguments and return state, and never invoke an extra native query to fill a log field. Reuse existing observers where present. Pseudocode expressions below describe candidate fields, not trusted C++ prototypes or approved pointer dereferences.

| RVA | Entry observation | Exit observation and interpretation |
|---|---|---|
| `223B8F0` | Sequencer identity/state, request generation, start identifier; only verified live scalar fields | Elapsed QPC and state transition to 5. Correlate nested cell lookup, not total load time. Early return is not failure or completion by itself. |
| `1C1E470` | Owner identity, mission `+0x120`, mission-record identity at `+0x128` | Duration; retain as a setup boundary. Do not count placements from this entry/exit hook or infer filtering outcomes without additional branch/callee instrumentation. |
| `1C10570` | Owner identity and mission; snapshot only while valid | Record cleanup return against the captured identity; never dereference the owner after the original. |
| `1C08D1C` | Owner identity; optional bounded, verified count snapshot of `+0x3C8` on the owning thread | Drain boundary. A nonempty valid pre-snapshot proves counted owner references were present; entry alone does not. Do not walk a freed or concurrently mutating tree. |
| `94B04` | Script manager and bounded copied name; owner only after its real argument convention is verified | Correlate nested replacement and named launch with a call sequence ID; do not assume the exported `void` return is correct. |
| `94D80` | Slot identity and old context at slot `+8`; verified owner argument | New context at the surviving slot, replacement result. Each observed replacement allocates a fresh trace generation even if the address repeats. |
| `95244` | Slot and old context; associate only with previously mapped task contexts | Release event and return; no old-context dereference. This also receives temporary slots from `94B04`, so not every call is a named-task disposal. |
| `E22D10` | Owner from `*arg2`, flags `+0x134/+0x142`, manager `owner+8` | Preserve native result and log against captured identity. A true result supports this removal-predicate path; it is not proof every resource or actor is gone. |
| `282574` | Mapped context generation | Native predicate result; aggregate repeats and emit changes. Do not call it independently. |
| `669110` | Registered target wrapper, existing object, verified placement identity, readiness/state fields | Object/state changes only while wrapper lifetime is established. Same object address does not establish a fresh actor; unknown lifetime remains unknown. |
| `669388` | Registered target wrapper and request identity at `+0x30` | Native readiness result; emit edges plus poll counts. Readiness does not establish spawn completion. |
| `1C4D608` | Native recovery call sequence, validated request/target correlation | Native boolean and duration; no retained pointer into its stack-local request objects. |
| `1C45DD4` | Submission manager and verified input/output roles, nested recovery call ID | Bounded capture of a validated handle value while storage is live. Never interpret the returned pointer alone as player recovery. |

Retain existing observations at `2239600`, `223AF50`, per-frame call site `56529C`, boss mode start/end `23A2DCC/23A2F70`, and gate/load readiness loggers. The additional `3A1700` entry/return observer must be filtered to the current `223B8F0` call context; capture its position input, stage/grid key and returned cell. These targets have not all been re-exported in file416. If verified branch observations already exist for `1FCECE4/1FCEE1C/1FC4208`, retain them; otherwise report downstream scheduling as unobserved, rather than expanding the first experiment with speculative detours.

The core timeline needs streaming, script, teardown, placement and recovery observations. `1C1E470` is a secondary setup-duration probe; defer it if it adds hook risk. This design does not require live tree scans or a new actor-destruction hook: missing lifetime evidence yields an inconclusive residency result, not a fabricated generation.

## Trace format and identity rules

Each event carries `{run, encounter, retry_index, owner_generation, context_generation, event_seq, call_id, parent_call_id, thread_id, qpc, event, rva, phase, validity_mask, payload}`. The header records QPC frequency, executable hash/module base, ASI hashes/source commits, enabled probes and catalogue row. Render unknown fields explicitly; never fill them from a prior retry.

- Use R7B35 / mission `0x7B35`, boss placement `375545`, start `374664`, box `371660`, door `62177372`. Map owner by native mission/placement observations, not only the global current-mission value, which can change during teardown.
- Separate attempt number from object lifetime. A native retry gets a new attempt index even if its owner survives. Mark an owner first seen at tracing start as lifetime-unknown; removal closes its interval. A reused address after removal starts a new trace generation. No observed removal is not positive residency proof without complete lifetime coverage.
- Map manager/name -> slot -> context from nested native calls. Preserve the mappings needed to recognize releases before new mappings replace them. Capture the target owner's start, clear and `mission_end` tasks; also retain other task-name events for that owner so unknown cleanup is visible. Globally record only coarse teardown candidates needed for correlation, without names from unrelated owners.
- Hook-side writes go to preallocated per-thread bounded buffers. No file writes, heap allocation, bridge calls, waits, locks or formatting in hot hooks. A separate consumer drains copied scalars/names; it never dereferences captured native pointers. Truncate strings with an explicit flag. Use reentrancy guards and emit event-loss counters.
- Never collapse away creation, replacement, completion transitions or release events. Aggregate repeated unchanged readiness polls with first/last timestamps and counts. Preserve per-thread order; use nested call IDs for causality, not QPC ordering alone across threads. Record late attachment, unknown mappings, buffer overflow and omitted probes in coverage metadata.

## One proposed run, three attempt windows

After separate instrumentation/deployment/runtime authorization, pin the executable and candidate ASIs, attach logging before the first mission request and use the existing authorized solo/offline rematch path. Keep the candidate's gameplay behavior fixed for the run. Do not mix a retry repair into the logging patch or compare binaries with different gameplay behavior as if only logging differed.

1. Enter Yoritomo normally and observe native load, intro, boss-mode start and independent control-ready state. Label this first entry.
2. Lose through ordinary play and choose the native retry. Observe the complete loss -> retry -> boss-ready/control-ready sequence. Do not synthesize script calls or gate rights to rescue an empty arena.
3. If the fight resumed, lose and retry once more. If that succeeds, finish normally and observe native clear and exit. Stop on the known retry failure and preserve its incomplete timeline. No second fight is invented to satisfy the plan.

Record request-to-control for entry, and loss/retry-choice-to-control for retries (their start anchors differ). Also record loading edges, observed native request time when present, streaming dispatch, load/setup boundaries, recovery submission, alive-state edge, control-ready edge, boss-mode start, clear and teardown. Mark a missing native mission request on retry as absent rather than borrowing the first-entry timestamp. Report cover, intro and control intervals individually; overlapping intervals must not be added as independent costs. Hook duration measures that call only, not asynchronous downstream work.

For each attempt, produce one row with: anchors; load/cover/intro/control times; stage and selected cell; observed owner/context/wrapper changes; reference-drain evidence; boss resource-ready and object-change events; player recovery submission/completion evidence; first failed readiness gate; trace coverage/dropped counts; native result (fight or failure). Unknown stage-resource lifetime remains unknown even if all pointer addresses match.

## Decision and acceptance criteria

| Observation | Allowed conclusion / next step |
|---|---|
| Arena-centered cell lookup, but no downstream cell enumeration | The request is centered on the arena. No claim that only one cell loads or that broad world loading is absent. If cell workload remains material, next static exports are `1FCECE4`, `1FCEE1C`, `1FC4208`, `1FCCFF4`. |
| Owner cleanup and counted-reference drain on retry | Reject reuse of that owner's previous encounter state. Assets might remain cached, but that is distinct from a live owner and working encounter. |
| Old start released, clear task still active or unmapped | Do not propose start-only re-entry. Resolve native clear-task completion and outstanding gameplay requests first. |
| Ready resource or existing boss object, without a fresh encounter/control transition | No fresh boss/reset success. Identify the native placement/actor reset boundary before any behavioral experiment. |
| Recovery submission succeeds, alive/control evidence absent | Request accepted/constructed only; recovery completion is unproven. |
| Owner, actor and scene/resource lifetime positively preserved with native cleanup and player recovery complete | Candidate evidence for a separate row-specific native re-entry design; still requires gate, phase, pending-operation and reward reset obligations. |
| Most elapsed time occurs after measured readiness during intro/control sequencing | Investigate that measured interval; do not infer disk/cell savings or shorten player protection from this result alone. |

Accept the diagnostic run only with attributable attempt timelines, no dropped lifecycle events, known probe coverage, and preserved native call semantics. A naturally reproduced empty-arena failure can be useful diagnostic evidence. Log-only intent alone does not prove zero runtime perturbation: report logger cost/overflow and compare with the existing observer baseline when an equivalent run is separately authorized. No speedup target or resident-reset success is claimed by this design.

## Implementation admission and next ownership

PC owns review and any separately scoped implementation. Before writing detours, obtain matching executable identity plus address-labelled instructions/caller samples, validate prototypes/return values and relocation spans, and reconcile the existing hook inventory. Concrete conflicts: `94B04` is exported as two-argument `void`, while `E22D10` calls it with four arguments and tests a return; `94D80` exposes five arguments while its caller prints two; `1C45DD4` exposes four arguments while its caller prints three. All export header signatures are generic. Do not copy these declarations into hooks or guess a missing ABI.

After ABI validation, validate pass-through call/return preservation and event decoding offline, including same-address reuse, temporary release slots, missing mappings and overflow. Keep emitted bytes bounded and no native-memory reads on the consumer thread. Only then request the separately scoped native observation run. Remaining executable/ABI and residency evidence debt is recorded in universal-optimizer/TECH_DEBT.md.
