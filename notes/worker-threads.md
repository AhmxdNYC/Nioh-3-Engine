# Worker threads: what they burn, what is waste, and the patches

Static analysis only: dump disassembly, Ghidra decompiles, and the r5 / r6-A1 / r7-A1 / ingame2 sampling profiles.
Nothing was attached, read live, written or applied. Date: 2026-10-06.
Scripts and raw outputs are in `re\scripts_workers\`. The live script is `re\scripts_workers\workers_live.fsx`.
Its `offline` run printed **ALL PASSED**; the output is in `re\scripts_workers\offline_out.txt`.

## TL;DR

The workers mostly block properly. None of the four thread types asked about is a spin like R1 / W3.
The waste that does exist is algorithmic redundancy plus "atomic loads" that take cache lines exclusive.

| rank | id | what | where | cost now (cores) | est. saving | patch | risk |
|---|---|---|---|---|---|---|---|
| 1 | **M1** | TS_MESHLET_REQ re-sums the streaming page pool's capacity by walking the pool's segment list **once per sub-chunk it looks at**. That includes chunks whose pages are already resident. The list never changes during play. | 48E90, loop 49150..4915A (36–42 % of 48E90) | 48E90 = 0.34 / 0.62 / 0.77 / 0.64 (r5 / ingame2 / r6 / r7) | **~0.10–0.25** | cave at 49131 (7 B): cache keyed on (list-0 head, rdtsc>>22) | low |
| 2 | **J1** | Job-system polls read the queue priority bitmaps and the job counter with `lock cmpxchg [m],0`. Every idle poller takes the line exclusive and bounces it between cores. | 411C0 (scan loop 41251..41290, hot RIP 4126B = 44–50 %), 40800, 403A0, 3E7CC | 411C0 = 0.16–0.19, 40800 ≈ 0.04 | **~0.05–0.10**, plus less line ping-pong for job pushers | 8 in-place rows, same length (`mov` or `mov`+`test`) | low |
| 3 | **M2** | Same idiom inside 48E90: page-claim bitmap and entry-flag reads | 49261 (hot RIP 49266 = 10–15 % of 48E90), 49046, 4933D, 48FB9, 493DC | ~0.04–0.1 | **~0.03–0.07** | 5 in-place rows | low |
| – | – | SynergisticDispatcherTask | 1A7C3E0 | 0.6 | 0 | **none**. It is a ring buffer with `WaitForSingleObject(sem, INFINITE)`, so it is not a poll. The work is real: the KTGL token stream is replayed into D3D12 (driver 26 %, D3D12Core 6 %, token handlers ~22 % of samples). | – |
| – | – | KIDSTaskScheduler0 condvar waits | 3D7CC8 (KTGL event), called from the frame step 5AA5DB … 5ACDC2 | 13 % blocked | 0 | **none**. These are frame-pipeline handshakes and block with INFINITE. Only J1 applies here. | – |
| – | – | CollisionUpdate | 43CEC → 319E00 → D80B0 | ~0.9 | – | **none built**. Every collider near the player is re-evaluated every frame by design; see §4. | – |

**Realistic total (M1 + M2 + J1): about 0.2–0.4 of a core of worker CPU, ≈ 2.5–5 % of the process's 8.5 cores.**

**Main thread:** no direct effect. [App]Game never runs 48E90, 411C0, 40800, 403A0 or 3E7CC (0 samples in all four
captures). The indirect gain is that the main thread's SMT sibling is free a little more often. With ~0.3 core removed
out of ~8.5 busy logical CPUs on 12, expect at most a ~1 % fps change, which is within PresentMon noise. Do not expect
an fps win.

**Power (handhelds):** the saved work is real CPU time (dependent pointer chasing and locked RMWs). It should show up as
lower package power at a fixed fps cap.

---

## 1. TS_MESHLET_REQ: 48E90 (Q1)

**What it is.** 48E90 is a job function, the per-entry "MeshletRequestLoad" step. It runs through 40800 @40E21 (a
devirtualised direct call), from 4A2D0 (the TS_MESHLET_REQ job, which fans out and waits in 3E7CC), and from
6B46F8 / 6B47A8. For meshlet entry `r9d` (M+568 table, 32 B entries) and each page `edi` of its range:
- it checks the resident / requested bitmaps;
- for each sub-chunk `k` with a non-zero size (`[rbp+40h]` 24-byte records → `[rbp+48h]` chunk table):
  1. **capacity check (the hot loop):**
     `rdx = [M+1C8h]` (= the graphics device `g`, same object as `[45BA588]`);
     `sum = Σ [node+14h]` over `node = [g+67A8h]`, `node = [node+30h]`. If `g+67C0h != 0` (per-type pools) only
     list 0 is walked; otherwise lists `+67A8/+67B0/+67B8` are walked. The hot path is the single-list one, 49150.
     Then `if (M+488h + chunkSize > sum * [M+134h] << 16 || M+490h + 1 > M+13Ch) { stop: bl = 1 }`.
  2. claim the chunk's 64 KB pages in the bitmap `[M+540h]` with CAS (loop 49240, atomic load 49261);
  3. if any page was newly claimed, 49D5C issues the load. It takes `g+6EA8h` shared and calls 116A24 → 251554 /
     2515B0 (the set-associative page lookup).

**The list.** It is the streaming page pool's **segment chain**:
- `node+10h` = bucket count; `+14h` = ways (entries per bucket); `+20h` = 64-byte entry array (indexed
  `bucket*ways+way`, see 4907BA); `+28h` = first page; `+30h` = next.
- The chain is sorted by `+28h` (117760 walks it as a sorted list).
- `+14h` sizes the entry array, so it cannot change for a live node.
- Every use found locks `g+6EA8h` *shared*: 49D5C, 49F4C, 128844, 129B70, 7A56B4, 7F49B4, 7F6CC4. The one
  *exclusive* user, 4906E8, evicts entries and does not relink nodes.
- 48E90 walks the chain with **no lock at all**. So nodes cannot be freed while workers run; at most the chain is
  built or grown, which in practice means at startup or a pool resize.
- No node-insertion site was found in the RVA ranges examined. That is consistent with "built once".

Correction to `notes\meshlet-mainthread.md` §3: it is not "four linked lists per request". It is **one** list on the hot
path (49150), or three on the shared-pool path. The four loops are four copies of the walk in the code.

**Why it is waste.** The sum is the same for every sub-chunk of every call. It is recomputed **before** the bitmap test,
so even chunks whose pages are already resident pay the full walk. The hot RIP 49153 (`mov rax,[rax+30h]` right after
`add ecx,[rax+14h]`) is 36–42 % of 48E90 self in all four captures. The per-chunk table miss 49108 is only 10–15 %, so
the walk costs about 3× a cache miss per chunk, which means a long chain.

**Can it be shortcut?**
- Checking the bitmap first would change which chunks hit the budget stop.
- Skipping empty lists does not help: the hot list is not empty.
- Caching the last result does help: **M1**.

### M1 patch: 49131 → cave
```
49131: 48 8B 96 C8 01 00 00        mov rdx,[rsi+1C8h]     ->  E9 <rel32 cave>  66 90
cave (data line 0: en @+0, kHead @+8, kVal @+10h; refresh counter @+40h; code @+80h):
  if (!en) { mov rdx,[rsi+1C8h]; jmp 49138 }           ; switch off = the original instructions
  tag = low32(rdtsc >> 22)                             ; ~0.9 ms windows at a 4.7 GHz TSC (~1.7 ms at 2.5 GHz)
  rdx = [rsi+1C8h]
  if ((([rdx+67A8h]) | 1) == kHead && hi32(kVal) == tag) { ecx = lo32(kVal); jmp 491AC }
  refresh: the original walk, instruction for instruction (49138..491AA), then
           kHead = head0|1; kVal = (tag << 32) | sum; ++cnt; jmp 491AC
```
- **Registers.** The cave clobbers rax / rdx / flags and returns ecx, which is exactly the set the original 49131..491AB
  writes. The walker proves rax, rdx and flags dead at 491AC (`[ok]` in offline_out).
- **Site.** No direct branch in .text targets 49132..49137. The bytes after 49138 are unchanged, so a thread that is
  mid-walk during the write finishes the original walk.
- **Concurrency.**
  - kHead and kVal are plain aligned qword stores, written in that order (TSO).
  - A reader that sees a new head with an old value needs a tag match, which means the same ~1 ms window. Then the
    value is a valid sum of the same chain.
  - The data line is read-mostly: about one refresh per window per racing thread.
- **Semantics vs stock.** The capacity value can be **at most one rdtsc>>22 window old**, and any change of the
  list-0 head forces a refresh. That matters only if:
  - (a) a segment is appended behind the head: the budget is up to ~1 ms *more conservative*;
  - (b) a node's `+14h` changes in place: impossible for a live node, see above;
  - (c) a node is unlinked: the original would already be use-after-free-prone, since it walks unlocked.

  The budget is a throttle (stop issuing requests this round; retry next round), so a ≤1 ms lag cannot corrupt
  anything.
- **Native proof (offline).**
  - The original 0x7B bytes 49131..491AB are copied into this process and run against the cave, through a thunk that
    sets rsi and r13 = 0.
  - Inputs: random chains of 0–60 nodes × 3 lists, flag 0 / 1 / 80h, the switch off in 10 % of cases.
  - **6,000 cases, 0 mismatches.** 2,717 cache hits were verified equal.
  - 2,058 "stale within window" results appeared after deliberate in-place `+14h` edits. Those are the expected (b)
    behaviour. In all 62 window-expiry checks the cave was fresh again.
  - After every head change the cave was fresh at once.
- **Saving.** The walk is 0.12–0.32 core. The hit path costs one `rdtsc` (~36 cycles on Zen 4) plus three L1 loads,
  against a walk that is much longer than a cache miss. Net **~0.10–0.25 core**, spread over the 10 TS_MESHLET_REQ
  threads.

### M2 patch: 48E90 atomic loads → plain loads (5 in-place rows)
`xor eax,eax; lock cmpxchg [m],r13d` with r13d == 0 becomes `mov eax,[m]` + NOP. The architectural effect is identical
(eax = [m], memory unchanged); only the StoreLoad fence is lost, and no Dekker pattern depends on it. Proofs are
automated per row in `offline`:
- eax and r13d are proven zero by a forward dataflow from 48E90 (zeroed at 48F98 / 4940A; r13 is non-volatile across
  the calls);
- the flags are proven dead.

| rva | orig | new | what |
|---|---|---|---|
| 48FB9 | F0 44 0F B1 AD B8 00 00 00 | 8B 85 B8 00 00 00 0F 1F 00 | per call |
| 49046 | F0 46 0F B1 2C 21 | 42 8B 04 21 66 90 | per page, entry flags |
| **49261** | F0 45 0F B1 29 | 41 8B 01 66 90 | **per 64 KB page: claim-bitmap word.** 10 workers took these lines exclusive just to find the bit already set. |
| 4933D | F0 46 0F B1 2C 21 | 42 8B 04 21 66 90 | per page, entry flags |
| 493DC | F0 45 0F B1 28 | 41 8B 00 66 90 | per call, before the bts-9 CAS loop |

The CAS loops that do write (49280, 493E9, 49428) are untouched.

### Other TS_MESHLET_REQ costs (not patched)
- **2515B0** (0.08–0.15 core): the set-associative page lookup in 116A24.
  - It is a linear scan of `ways` 64-byte entries in one bucket, for each segment, for each page, so each way is its own
    cache line.
  - This is real work. A prefetch could help, but that is a future candidate.
- **Sleep(0) polls on one thread (TS_MESHLET_REQ9).**
  - (a) 117D6C `while (fence->GetCompletedValue() < v) Sleep(0)`. It is used when the caller (5C0D4 / 5C268) passes no
    event; with an event it already does `WaitForSingleObject(INFINITE)` (cold path 190A0E4).
  - (b) The job-wait helper 3E7CC → 403A0 404B9 `Sleep(0)`.
  - Together ≤ 0.1 core, mostly at 29 % busy, which means they yield more than they spin. Not patched: changing the
    job-wait Sleep(0) affects every job wait in the engine (see S1 in cpu-profile.md).

---

## 2. SynergisticDispatcherTask (Q2)

**What it is.**
- Thread entry: 2EEDFA0 (sets the name, then `1A7C3E0(this)`).
- The loop:
  ```
  for (;;) { WaitForSingleObject([this+10h], INFINITE);            // semaphore, 1 release per item
             if (read == write) return;                            // [this+24h] == [this+28h]: shutdown
             item = ring[[this+28h] % [this+20h]] (32 B); ++read;
             for (p = item.begin; p < item.end; ) call tokenTable[*(u16*)p](ctx, &p);   // 3C02220
             if (item.submit) { if (ctx+2458h) 7A728C(); 496078(); 4930C8(ctx+588h, ctx+5A0h, ..) }  // close/submit (inferred)
             --[this+30h]; }
  ```
- It replays the KTGL deferred command stream that the main thread records (`ctx+3C40h` tokens, see
  mainthread-drawprep.md) into D3D12 command lists.
- "Synergistic" is very likely a name inherited from KTGL's PS3/Cell era (SPU = Synergistic Processor Unit). Here it is
  just the render-backend translation thread. That is an inference, not proven.

**Why ~60 % of a core.** Real work. In r5:
- amdxc64 is 26.5 % of samples, D3D12Core 6.1 %, Nioh3 21.7 % (token handlers 1180D0 / 118604 / 1167D4 / 115xxx,
  B94250 memcpy);
- 119820 is a B-tree lookup with 128-byte `memcmp` keys (a state-object cache), 0.022 core;
- 58C144 is another memcmp loop.

**The WaitForSingleObject pattern is not a poll.** The timeout is INFINITE (`or edx,-1` at 1A7C3F9). "44 % of samples
waiting at 50 % busy" means it blocks often for short times: one semaphore release per ring item, and it goes back to
sleep as soon as it has caught up with the main thread. Kernel time is only 5 %.

**Verdict: necessary. No patch.** The only theoretical lever is fewer, larger items, which would mean batching on the
producer side. That changes latency and is not a byte patch.

---

## 3. KIDSTaskScheduler0 (Q3)

- **Where the 83 % goes.**
  - 61.5 % inclusive is the frame step 5ACF80 → 5AC664 / 5AC628 / 5AC9EC: game simulation.
  - Inside it, 38.8 % is in the job-wait helper 3E7CC → 403A0, which mostly *runs jobs* through 40800 while waiting.
  - Self time is spread thin: 6EFD0, 71C60, 6EA50, 66D00, 54360 … with nothing above 4.3 %.
- **The 13 % NtWaitForAlertByThreadId (SRW condition variable) samples.**
  - They come from `std::condition_variable::wait` (_Cnd_wait B618E8 → B616A4 `SleepConditionVariableSRW(…, INFINITE)`).
  - The KTGL wrapper is **3D7CC8**, an auto-reset event: mutex + condvar + flag `+99h` / auto-reset `+98h`, with 170
    callers.
  - Its callers inside the frame step are 5AA5DB, 5AAEE8, 5AB545, 5AB58D, 5AB60D, 5AB7F4, and 5ACDC2 (5ACDA4: wait on
    `[4745348]+1448h` when `+1408h` is pending; called from 5AC60F / 5AC620).
  - These are **frame-pipeline handshakes** between the game-update thread and the render / main thread. They are
    blocking waits with an INFINITE timeout. "78 % busy" only says that Scheduler0 was busy right before it blocked:
    it is waiting on the main thread, which is the bottleneck.
- **Is there a wake storm?**
  - Workers sleep on a per-worker event: 1A6BB84 sets bit 0 of `[w+10h]`, then `WaitForSingleObject(INFINITE)`.
  - Job pushes wake **one** sleeping worker, and call SetEvent only if that bit was set (40AC2..40B0A, 40CF0..40D2F).
  - That is a sound design. ZwSetEvent (0.06–0.08 core process-wide) is the cost of real wakes.
- **Spin-then-wait.** The only spins are the Sleep(0) job-wait polls (Scheduler0: 1.4 %, ZwDelayExecution at 91 %
  busy) and the bitmap polling (411C0 2.2 %, 40800 0.6 %). **J1** makes the polling cheap. Adding more spin before the
  condvar waits would cost CPU, which is the opposite of the goal.

### J1 patch: job-system atomic loads (8 in-place rows)

| rva | orig | new | in | flags |
|---|---|---|---|---|
| 4122D | F0 42 0F B1 1C 19 | 42 8B 04 19 66 90 | 411C0 first priority word | dead (4123C test) |
| **41265** | F0 42 0F B1 1C 19 | 42 8B 04 19 **85 C0** | 411C0 scan loop (hot) | ZF → 4126D jne; all flags dead at 413E0 / 41273 |
| 41397 | F0 42 0F B1 1C 19 | 42 8B 04 19 66 90 | 411C0 re-read | dead (413B3 test) |
| 414B6 | F0 42 0F B1 1C 19 | 42 8B 04 19 85 C0 | 411C0 re-read | ZF → 414CB je; dead at 41273 / 414D1 |
| **40899** | F0 42 0F B1 34 01 | 42 8B 04 01 85 C0 | 40800 own-queue priority word (hot RIP 4089F) | ZF → 408A1 je; dead at 41027 / 408A7 |
| 40481 | F0 45 0F B1 AF 54 04 00 00 | 41 8B 87 54 04 00 00 66 90 | 403A0 `sched+454h` flag, per empty poll | dead (test al,1) |
| 3E7F5 | F0 44 0F B1 71 34 | 8B 41 34 0F 1F 00 | 3E7CC job counter, first read | dead (via jmp → 3E827 cmp) |
| 3E821 | F0 44 0F B1 73 34 | 8B 43 34 0F 1F 00 | 3E7CC job counter, wait loop | dead (cmp eax,1) |

- **ZF equivalence.** `cmpxchg` with eax == 0 sets ZF = (`[m]==0`); `test eax,eax` after the load sets the same ZF.
  SF / CF / OF / PF differ, so the walker proves them dead at both jcc successors (no reader before an all-flags
  writer; a shift by CL is not counted as a writer).
- **Zero sources** are proven by the dataflow from each function entry:
  - ebx in 411C0: `xchg rbx,…` only on the exit paths;
  - esi in 40800: zeroed at 40870;
  - r13d in 403A0;
  - r14d in 3E7CC;
  - eax at every site.
- **Memory ordering.**
  - The job counter in 3E7CC is now read with a plain load. On x64, load→load and store→store order give acquire
    semantics: a waiter that sees the counter drop also sees the workers' earlier result stores.
  - The queue pops and pushes themselves stay locked CAS (41325 / 4133E / 41380 …, untouched).
  - A stale "empty" read is the same race the locked load already had.
- **Note.** cpu-profile.md W7 said a code-size-identical rewrite of 411C0 was "not possible byte-for-byte". It is
  possible: the 6-byte `lock cmpxchg [rcx+r11],ebx` holds a 4-byte `mov` plus a 2-byte `test`.
- **Saving.**
  - 411C0's hot RIP 4126B (44–50 % of 0.16–0.19 core) is the locked RMW stall. With shared lines a plain load
    mostly hits, so **~0.05–0.09 core**, plus ~0.01–0.02 core from 40800.
  - A further unquantified gain: job pushers (Scheduler0 and workers) stop losing the bitmap lines to idle pollers.

---

## 4. CollisionUpdate (Q4)

**The pipeline, per frame and per collision manager** (list `[45B9E58]+540h`, count `+560h`):
- **43CEC** (self 0.06 core, hot loop 43F35..43F61):
  - clears the work vector `mgr+150h`;
  - computes the grid-cell range around the player position `g9E58+1B10h`, plus the extra points in `+1B20h`;
  - for **every collider in every cell in range**: if `+A5h == 0`, sets `+A5h = 1` and pushes it.
  - Cells that left the range get `+A5h = +A6h = 1` (forced re-evaluation).
- **319E00** (job): N per-item jobs, then **5 strided jobs** (319D5C, stride 5). It waits in 3E7CC.
- **D80B0(mgr, out, start)** (0.37–0.42 core self, the largest worker hotspot after 48E90), for i = start, start+5, …:
  1. skip if `+A5h == 0`; otherwise clear it;
  2. owner lookups: an inline `std::map` find at `[[mgr]+30h]+28h`, keyed by collider `+0Ch` or `+08h`; then
     **D8D90**, a binary search in a flat array `[..]+180h` (the profiler files it as "D8C40(leaf?)" because D8D90 has
     no .pdata); then possibly a global map `[4759FC0]+28h`;
  3. for each part: D8740 hash lookup (D8700 leaf); **D89E0** distance test against the player (radius
     `max(16000, +98h) + part radius`) via **D8B70**, a part transform through a function-pointer table (0.18–0.20
     core self; hot RIP D8B7E = first touch of the part); D6E9C grid tests;
  4. emit a 0x30-byte change record **only when the part's active state flips**.
- **Hot RIPs:** D8291 (21 %, first touch of the D8D90 result `+10Fh`), D81BD (12 %, collider line 0), D83E0 (12 %,
  part array).
- All of these are first-touch cache misses. Nothing in this path spins or polls.

**Answer to Q4.** Yes: static colliders are re-processed every frame, but by design. Activation depends on the player's
distance and on owner state (`+10Fh` mode byte, `+1A68h` table, grid tests), and those can change any frame. There is
no dirty bit on the inputs, only on the collider (`+A5h`), and 43CEC sets that bit for everything in range every frame.

**What could be done (not built):**
- **C1, prefetch:** at D80B0's loop head, prefetch collider i+5 / i+10 (lines +0, +80h, +C0h) and its `[+88h]` part
  array. About 0.05–0.15 core of stall time; low risk; no fps effect.
- **C2, skip:** skip a collider when the player moved less than ε since its last evaluation *and* the owner state is
  unchanged. That needs per-collider state plus RE of every input to the active-state decision. A wrong skip leaves a
  collider active or inactive when it should not be (fall-through or invisible walls). **Not recommended as a byte
  patch.**
- **Effect:** CollisionUpdate runs inside the game-update job graph, not on the main thread. Making it faster shortens
  Scheduler0's frame step, which then waits longer on the main-thread handshake. So there is no fps gain, only CPU and
  power.

---

## 5. Other families (brief)
- **StageUpdate ×8** (0.85 core in total, ~0.25 busy): DBA70, 469C0, 484C0, DDC70, plus **411C0 0.06** (J1 applies).
- **EffectUpdateThread ×10** (0.5 core in total): ~85 % blocked in NtWaitForAlertByThreadId (condvar). Real work is
  spread (CE7E0, 219E80, C8958). Not examined further.
- **MissionThread** (17 %): mostly blocked (condvar); ~0.15 core of real work (20B900, 2866F0, 20A298).
- **Process-wide Sleep(0):** ZwDelayExecution is 0.13–0.17 core, of which the main thread accounts for 0.06 (W6 /
  7D9E90) and the workers the rest (404B9 job waits, 117D6C fence poll).

## 6. How to test (live, in a real game process; nothing here was run against the game)
```
cd C:\Users\Ahmad Hamza\nioh3-engine\re\scripts_workers
dotnet fsi workers_live.fsx offline                    # re-verify against the dump (no game access)
dotnet fsi workers_live.fsx dry                        # read-only: all 14 sites should be "original"
$env:FT_ALLOW_WRITE=1; dotnet fsi workers_live.fsx apply M1,M2,J1
dotnet fsi workers_live.fsx status                     # M1 refreshes/s should be ~1-3k/s (one per tsc window per racing worker)
# A/B without reattaching:
dotnet fsi workers_live.fsx off M1 ; ... ; dotnet fsi workers_live.fsx on M1     # cave switch only
dotnet fsi workers_live.fsx off J1 ; ... ; dotnet fsi workers_live.fsx on J1     # rewrites the 8 rows (frozen safeWrite)
dotnet fsi workers_live.fsx revert M1,M2,J1            # original bytes back; the M1 cave stays allocated
```
Measure with `patcher\cpuprof.fsx 20 <out>` at the round-5/6/7 spot, ON against OFF.

| signal | expectation with ON |
|---|---|
| 48E90 self | falls by ~40 %; loop 49150..4915A disappears from its hot RIPs |
| `<anon>` cave samples | appear, but are small |
| TS_MESHLET_REQ family CPU | −0.1 to −0.3 core |
| 411C0 self | roughly halves; RIP 4126B no longer dominates |
| process total | down by ~0.2–0.4 core |

- PresentMon fps / p99 should be **unchanged** (this is the check that nothing regressed).
- On a handheld, compare package power at a fixed fps cap.
- Watch texture/meshlet streaming after a fast travel (time-to-sharp) with M1 on and off. There should be no
  difference: the budget is unchanged apart from at most ~1 ms of capacity lag.

## 7. Files (`re\scripts_workers\`)
- `workers_live.fsx`: the M1 cave builder, the M2 / J1 rows, and the offline verifier:
  - site bytes and overlap list;
  - operand-equality check of every row;
  - zero-source dataflow;
  - flag-liveness walker;
  - whole-.text branch-target sweep;
  - M1 native equivalence test.

  It also has the live modes, with the same safety model as `patcher\cpu_caves.fsx` and
  `re\scripts_meshlet\meshlet_caves.fsx`.
- `offline_out.txt`: the last offline run (ALL PASSED), with the cave disassembly at a dummy address.
- `dis_411C0.txt`, `dis_D80B0.txt`: disassembly. `g_w1.txt`: decompiles of the pool users 49F4C / 128844 / 129B70.
  `g_coll.txt`: decompiles of 319E00, 319D5C, D80B0, D89E0, D8B70.
- `scan_imm.fsx` (immediate / displacement users), `scan_store.fsx` (writes to a displacement), `scan_rip.fsx`
  (RIP-relative / direct references): the static search helpers used here.
