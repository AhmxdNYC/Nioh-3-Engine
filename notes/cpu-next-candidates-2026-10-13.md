# CPU follow-up, October 13

User explicitly requested continuing beyond profiling toward real CPU-saving candidates. Main game16044 remains untouched; existing WG/LK/MW settings retained. Current capture evidence is in cpu-stacks-current-2026-10-13.md. No new gain is claimed.

## Active implementation

- Renderer-drain agent: exact418F40 predicate is queue write/read mismatch OR in-flight work. Candidate pairs a wait with the dispatcher completion at1A7C4AF, after actual replay/submission returns. Queue reservations that have not incremented in-flight must still prevent successful drain. Native reset/init and completion races are being checked before a default-off build in a separate hosted592a888 worktree.
- Meshlet agent: old M1 cache key omits pool/mode/other list heads and has separate key/value publication. Found actual tail append0x87583C reached through streaming insertion0x250D08/250EC8/2F0FB7C, so an unchanged head does not prove unchanged capacity. Existing M1 remains OFF. Full M2 has memory-ordering questions beyond register/flag equivalence. Preparing a narrower two-site seed-read candidate493DC/49412: each read is immediately followed by the original write-CAS, preserving that synchronization.

## Job-queue candidate review

Existing J1 rewrites8 loads and remains OFF on main. Root reused the LK control-flow StoreLoad screen against the dump, then against effective J1+LK bytes (26 existing sites). Six rows had no within-function pending non-stack store/call;40481 and3E821 reach calls, requiring semantic review rather than treating the screen as whole-program proof.40481 selects return versus Sleep(0), not an indefinite wait;3E821 reads job completion after helping, with final locked decrement3E82C preserved. This analysis alone does not prove all calling-context ordering or gameplay gains.

Reproduction: D:/nioh3-re/cpu-next-20261013/j1-ordering.fsx and j1-ordering.txt; in-memory offline dump only, no game access. Old workers_live offline records cover values/registers/flags, not a universal concurrent proof.

Ally preparation requested in bridge3075 for existing J1 OFF/ON/OFF with matching CPU+frames+RAM/GPU, same loaded scene/camera/cap/config, current-entry restore, memory/noise telemetry only. Awaiting actual device ACK and source decision; no launch claimed. Keep M1/M2/new drain out of that comparison until separately reviewed. PC2 remains on the independent Yoritomo gameplay task; it must not receive concurrent CPU work.

## Rematch coordination during CPU work

PC2 admission3071 stopped before mutation/game because Windows PowerShell parsed UTC timestamps as local and compared them without normalization. Reviewed file386 changes exactly3 timestamp comparison lines;GO3080 authorizes the same unspent one game launch with new timestamps and exact new source pins. No CPU or gameplay acceptance follows from preflight.

## Concrete deliverables

M2S completed at isolated commita1b5fef0dffb7d071e865c4fe3b9f13e9d082802, EngineSHA53ebf47b9aab38ede3bfdcd1b35f5b38356973d88940c1be0fa0ee14521ee3ab. Root source-reviewed,40002 actual native equivalence/contention cases,258sites and build PASS. Bridge file388 SHA349bd1617574e7af4fac570f0799f85cf8faa340a6a473b4e42b6f377a75b28d. Message3088 queues comparison preparation AFTER PC2 completes current rematch restoration; no concurrent device task or gameplay claim. Detailed note meshlet-cpu-audit-20261013.md.

Existing J1 preparation contract file387 SHA1bc7c799005a8a6ec2e98bda22849a1a5fbcc2159c8ae009a2ad6f23c96c46ea: exact8 site bytes plus paired-frame/CPU OFF/ON/OFF protocol; Ally messages3075/3085 pending ACK at checkpoint. Source review permits focused experiment but is not universal concurrency proof. No new binaries/defaults installed on main.

PC2 STARTED rematch3080, reported3083: ownedgame15100/runner19616. CPU work must not interfere. Discord/phone delivery for3083,3086 CPU-start milestone,3089 M2S-ready milestone verified1/1, errorsnull.


RD completed at isolated commita52f82ae31fb96eacb91c8614e884246e9567308. EngineSHAe960be352980380291629bf398c1e39a50cc3124af90057ffbdd0887144193d2; bridge file389/archive4b5d27d556d5bb8b1074b4acab9b25a394e5280a97f160440bba8bb4c7bc9f1a. Root reviewed full native predicate, reservation gap, completion decrement+wake, reset/join, paired publication and both detour ABIs. Review caught JMP-entered thunk unwind issue; final code reuses native shadow space and describes inherited frame, validated using actual extracted cave at4 PCs.259site checks, native completion/race/100reset cycles, unwind checks, release build PASS. No game install or measured CPU gain. READY3091 queued automatic phone/Discord delivery. Full note renderer-drain-wait-candidate-2026-10-13.md; compare RD0/1/0 independently, not combined with M2S.

Both implementation agents finished. Root retains test coordination; PC2 currently has rematch then M2S preparation, Ally J1 still awaiting ACK. RD is ready for a separate main/device comparison; the still-running main manual game was not interrupted or changed. Source/build validation is complete; live optimization acceptance remains pending.
