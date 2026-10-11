# Next bounded frame-I/O opportunity: paired meshlet readback preparation

2026-10-11. Read newest STATUS18:34 and AGENT_BRIEF; existing native exports and
historical CPU samples only. No native-source changes, build, game, device or
installed action. Added `tools/bench/audit_frame_io.py` and five focused tests.

## Recommendation and evidence

The highest-priority *new bounded frame-I/O investigation* in the inspected
evidence is preparing the two same-job meshlet-request readbacks together,
recording their copies before one submit/wait, without changing feedback age.
This is not a proven FPS winner or a ranking above current Ally GPU work.
No prior implementation/rejection of this exact read-side pairing was found
in the inspected source and renderer/resource notes.

Native export `D:/nioh3-re/decomp/0x4a2d0.c:124-150` maps request-set+8 and+10,
both with map mode0. Both pointers reach worker state+5D8/+5E0 at212-213;
fanout occurs at324. The two map calls are sequential and the first must
succeed before the second is attempted. Partial-map cleanup is meaningful.

**A tempting paired-close optimization is now rejected, not left queued.**
I checked the retained flat PE's table at3BE29D0 after checking its raw-backed
section/RVA identity. Mode0 has read selector1 and close-copyback selector0.
Therefore the normal cleanup calls5CEE8 at354/364 DO NOT submit copyback for
these maps. Generic decompiler call presence alone would have misidentified
the bottleneck. The new audit emits this explicit rejection.

The actual read-side chain is5B3AC ->4D874 (read selector1) ->62E680 ->62E9B4
->62EA08 ->1A807F8 ->5C0D4(count1). Export1A807F8 explicitly constructs the
single-copy call;5C0D4 acquires a context, records copies, preserves dependency
waits, submits, polls117D6C, and returns the context.117D6C waits completion
before advancing/resetting its allocator ring.62E680 maps the staging only
after the read-copy helper returns. Thus each map can pay a separate synchronous
copy round trip before either pointer reaches the CPU workers.

Concrete next hypothesis: stage both readback allocations, preserve the two
ordered copy records/dependencies, submit/wait once, then expose both pointers.
Keep the original request-set/frame. Aliasing, map error cleanup, prior GPU
producer fences and source lifetime still need complete native proof. Do not
remove the existing wait or retain stale feedback another frame.

## Actual archived measurements

The new analyzer inspected four actual TSV profiles. These are worker-local
sample shares, not frame milliseconds or removable waits:

| Capture | 117D6C attributed samples / worker percent | 5C0D4 inclusive samples / worker percent |
| --- | ---: | ---: |
| cpuprof-r5.txt.tsv |164 /2.064%|260 /3.273%|
| cpuprof-r6-A1.txt.tsv |85 /2.113%|152 /3.778%|
| cpuprof-ingame.txt.tsv |125 /3.099%|170 /4.214%|
| cpuprof-ingame2.txt.tsv |210 /1.983%|352 /3.323%|

All these wait observations name `KIDSTS[s66:TS_MESHLET_REQ]9`. They establish
real work/wait in this path, but do not isolate paired reads from other
callers. They do not prove the main thread waits on this job. Self/attributed/
inclusive rows overlap and are never added. Absence is unobserved, not zero.
The audit emits input hashes and null for FPS savings, paired-flush frequency
and critical-path milliseconds.

## Smallest next implementation gate

First bind the exact native read-side instructions, two destination allocations
and dependency handles. Determine whether both reads share the source epoch
and whether any callback/failure cleanup requires the first map's completion
before preparing the second. Prove aliases, staging retirement and failure
behavior before replacing anything. The map-mode0 close-copyback hypothesis
already failed this static gate and should not be retried.

Only then implement an offline paired-read model using actual decoded copy
records. Compare stock versus proposed source/destination ranges, queue waits,
consumer-visible bytes and allocation retirement; inject first/second
allocation, submit and fence failures. A future root-authorized compare-only
sample should count eligible read pairs and time their waits/job join without
changing behavior. Measured critical-path benefit is required before native
mutation/A-B. No new observer was installed here.

## Avoid repeated rejected work

- FO8's roughly0.4% gain moved drain time to effect synchronization; FO11's
  artificial8ms delay is not a native win (`renderer-opportunities-2026-10-11`).
- Preserve effect readback age and existing meshlet feedback frame. Adding a
  frame of latency is not this proposal.
- CBV COPY-queue mirrors cost about15ms on pc2 (dllmain history); this proposal
  uses the already-existing copy context and combines existing operations.
- Emitter duplicate elimination remains blocked by0% emitter correlation in
  the old CBV trace (`cbv-emit-next`); replay equality is not removable emission.
- Native-Windows file event pooling/synchronous-read changes have little wall
  benefit; load I/O totaled3.2 CPU-seconds across threads in a25-second window
  (`pc-vs-ally-load`). Do not call GPU copy/readback waits disk I/O.
- No fog/LOD, material descriptor or compute-PSO work was duplicated.

## Verification

```powershell
python tools/bench/test_audit_frame_io.py
python tools/bench/audit_frame_io.py --decomp D:/nioh3-re/decomp --dump re/Nioh3.dump.exe captures/cpuprof-r5.txt.tsv captures/cpuprof-r6-A1.txt.tsv captures/cpuprof-ingame.txt.tsv captures/cpuprof-ingame2.txt.tsv
```

Five tests PASS (overlapping views, absent targets, duplicate attribution,
nonfinite percentages, negative counts); four real profiles parse; ten
native-export shape checks PASS; actual map-mode table rejects paired-close
copyback and identifies read-side preparation. These checks bind the cited exporter content,
not native ABI/lifetime correctness. No game bytes or payloads are emitted.
