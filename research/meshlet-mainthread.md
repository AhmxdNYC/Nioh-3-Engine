# Main thread: meshlet/texture streaming bookkeeping under render-frame-begin (Nioh3+5D214)

Static analysis only (Ghidra decompiles, dump disassembly, r5/r6/r7 profiles). Nothing was attached, read live,
written or applied. Date: 2026-10-06. Scripts and raw outputs stay in the private dev repo.

## TL;DR

- **5D214 is ~10 % of the main thread, and nearly all of it is in three callees:**
  - **4BBEC**, ~6.6–7.2 %. About 2/3 of it is **material texture streaming**: 1E7050 → 128EF0 / 1290B8.
    This is *not* meshlet LOD (the CPU does no meshlet LOD; that runs on the GPU in `MeshletResolveLOD`).
  - **4AA34**, "MeshletCheckRequestLoad", 2.2–2.8 %. It builds per-entry dispatch arguments for a GPU compute pass.
  - **419A8**, 0.8–1.1 %. A per-entry flag shift register.
- **It is a full O(N) scan every frame, and that is by design (GPU-driven streaming).** The GPU decides residency
  every frame, so the CPU re-issues per-entry dispatch arguments every frame. The engine already has:
  - per-frame dedupe stamps;
  - a change-detection path for the object buffers;
  - dirty bits for the uploads;
  - a per-frame tile-request budget.
  
  The remaining cost is **cache misses on scattered objects**, plus locked RMWs on the flag words.
- **Ranked candidates** (all offline-verified; the S1E7 switch is off by default):

| rank | id | what | main-thread saving (est.) | risk |
|---|---|---|---|---|
| 1 | **P1E7** | software prefetch in the per-texture loop of 1E7050 (next texture's streaming object, 5 hot lines) | **0.5–1.2 %** | low (only prefetches plus a load the original makes one iteration later) |
| 2 | **P4AA** | 3-stage software prefetch in the 4AA34 entry scan (entry line i+16, obj+150h line i+8, `*[obj+150h]` i+4) | **0.5–1.2 %** on top of A1 | low |
| 3 | **C419** | 419A8 rewritten: **one** `lock cmpxchg` per entry, **none** when the word doesn't change (was up to ~14 locked loads + 4 CAS loops) | 0.3–0.5 % on top of A2 (0.6–0.8 % without A2) | low (6,000 randomized native cases vs the original bytes, 0 mismatches) |
| 4 | **S1E7** | *experimental, switch default OFF*: time-slice the texture-group update (each group every other frame, half of them per frame) | ~1–1.5 % (less when combined with P1E7) | **medium**: halves texture-streaming request rate; slot re-acquire can be a frame later |
| – | (not built) | 4AA34 / 419A8 / 4BBEC every other frame | – | **do not do it**; see §3 |

- **Estimated total from the low-risk set (P1E7 + P4AA + C419): ~1.3–2.9 % of the main thread**, on top of A1/A2
  (an estimate from profiles, not a measured fps result).
  When CPU-bound, that is about the same in fps. With A + these, 5D214 should drop from ~10 % to roughly 6–7 %.
  The rest is necessary work.
- All four live in one cave, built by `meshlet_caves.fsx`. It has the same modes and safety model as
  `cpu_caves.fsx`: `offline` / `dry` / `apply` / `status` / `on|off` / `revert`, `FT_ALLOW_WRITE=1`, and
  `Mem.fsx safeWrite`. **`offline` was run: ALL PASSED**.

---

## 1. What runs every frame (Q1)

`4A529C` (render-frame begin) → `5D214(base, scene, viewIdx, isSecondary)`. Here `M = base+169DD0` is the meshlet
manager; the fields below are relative to M.

```
5D214  (normal frame, param_4 == 0)
 ├ 5A4DC / 387A40 / 96A1E4 / 93B720   setup; 0x16A0B8 ^= 1 (parity of the double-buffered GPU upload set)
 ├ 4BA8C  register new entries — GATED by pending counter M+4C4 (only runs while entries await bit 0x400)
 ├ 5B278 → 5B484, then tail-jmp 419A8   per-entry flag update (§1.1)
 ├ 4BBEC  scene meshlet buffers + texture streaming (§1.2)          6.6–7.2 %
 ├ 5D1F4 → 4AA34 "MeshletCheckRequestLoad" (§1.3)                    2.2–2.8 %
 └ if M+4C0 > 0: 5DA48 → one more compute dispatch (token 0x7E)
5DC18 (separate render step, from 4A4F94) → 4B104 "MeshletRequestLoad" (GenerateLoadRequestCommand), 0.6 %
TS_MESHLET_REQ workers (10 threads, ~78 % blocked): read the GPU results, load data, set entry bit 0x200 via CAS
(493E9/49428). They run concurrently with all of the above.
```

### Entry table and flags (`M+568`, 0x20 bytes/entry; `M+4B0` maps index → slot; `M+4C8` map length; `M+11C` capacity; `M+4C0` active count)

| bit | meaning (inferred) | writer |
|---|---|---|
| 0x2 | active entry | registration |
| 0x1 | ready (= 2 && !(4\|8\|0x10\|0x100) && 0x400 && (M+18C==0 \|\| obj+D0 & 8)) | 419A8 every frame |
| 0x20 | referenced this frame (a visible object uses it) | 4BBEC (atomic OR via 452050), cleared by 419A8 |
| 0x40 / 0x80 | referenced 1 / 2 frames ago (shift register) | 419A8 every frame |
| 0x10 | load in flight (excluded from both 4AA34 passes) | workers |
| 0x200 | set by TS_MESHLET_REQ worker (request taken) | workers (CAS) |
| 0x400 | registered | 4BA8C |

### 1.1 419A8 (0.8–1.1 %)
- Loops over slots 0..`M+11C` and stops after visiting `M+4C0` active entries, so **O(active)**, every frame.
- Per entry, in the original:
  - up to ~14 `lock cmpxchg [m],0` loads (the A2 idiom);
  - **4 separate CAS loops**: set/clear bit 0, bit7 := bit6, bit6 := bit5, bit5 := 0.
- Every active entry gets ≥3 locked RMWs per frame, even when nothing changes. Each one is a full fence, which kills
  memory-level parallelism, and each dirties a line that the workers then fetch back.
- There is no dirty mechanism. The shift register has to tick every frame.

### 1.2 4BBEC (6.6–7.2 % inclusive)
**Object part.**
- `*(scene+180h+view*8)+4498h` is the object list. For each object (45D5F8 filter) and each part (`obj+88h`, count
  `+80h`): if the part's entry lacks 0x20, it atomically ORs in 0x20 and calls 45B0E4/45D7B4. That is O(visible parts).
- **Existing change detection:**
  - The per-object buffers are recounted and rebuilt only when the counts outgrow the capacities (`46DD6C` checks,
    `+169EE8..169F00`).
  - Each upload uses a dirty bit at `scene+9B4h` (bits 1/2/4/8/10h). When it is clean, the upload is a single
    memcpy from a CPU shadow copy into this frame's upload buffer.
  - That copy is necessary because the buffers alternate by frame parity (`+16A0B8`).
- This part costs ~2 % (loops plus uploads), and it is not a target.

**Texture streaming (≈ 3.8–5.1 % self: 1E7050 + 128EF0 + 1290B8).** `1E7050(M, sceneBuf)`:
- memset the per-frame "visited" byte array `M+888h`;
- for every slot i < min(`M+124h`, `buf+48h`) whose visibility dword `buf+8 [i]` ≠ 0:
  - `slot = M+830h + i*40h`; `slot+10h` → param of type 0x14 → texture group G;
  - if the group stamp `G+20h` ≠ the global frame stamp `[45BA588]+6EB0h`, stamp it and, **for every texture
    tex in G (`G+28h[k]`, count `G+18h`)**:
    - `128EF0(tex+68h → +108h, g+74E0h, frame)`;
    - `1290B8(tex+68h, 0xFF, 0x10)`;
  - then the visited logic: `slot+18h` type 0x13 → ushort list → 129A28 checks.
- **128EF0** is a per-frame stamp (`+48h`) plus a check that the texture's slot in a block allocator (`1283A8` /
  `1286E8`, under a critical section) is still valid. It re-acquires or compacts the slot when it is not.
  - It is not an LRU. `g+74E0h+64h` counts textures in the current block.
- **1290B8** is a per-frame stamp (`+188h`), then a mip-tile pyramid walk:
  - inputs are the requested-level map `+120h` and the resident-level map `+D8h`;
  - it issues **at most 16 tile requests per texture per frame**, under a global budget (`g+7560h`, `+75A8h`,
    `+7590h`);
  - it has frame-number-based hysteresis (`+1B8h` + ushort `+1C0h`).
  - The streaming constants `vMLRemap` / `vMLIPkPrm` / `vMLClrPrm` are bound in 1E73C0, and the engine has
    `SamplerFeedback`.
  - **So this is feedback-driven material texture streaming.**

**Where the cost is (hot RIPs, r6-A1):**
- `128EF0` @128F03 (80 %): the first touch of `tex68+12Eh`.
- `1290B8` @1290F8 (42 %) and @12911E (21 %): first touches of `+90h` and `+178h`.
- `1E7050` @1E7167 (18 %, slot line), @1E71F9 (15 %), @1E721C (14 %).

In other words, roughly 3–5 cache misses per visible texture per frame. The tile loops themselves are cheap.

### 1.3 4AA34 "MeshletCheckRequestLoad" (2.2–2.8 %)
- **Gate:** it returns at once only when `M+4C0 == 0` (no active entries) or `M+1AC & 40000000h`.
- It runs two shader passes (`vtbl+A8h(pass+4)`), and **each pass scans all `M+4C8` map entries**:
  - **pass 0:** entries with 0x20 and 0x1 (referenced and ready) get size arguments (`e+8 → +30h → +190h …`);
  - **pass 1:** every active entry without 0x10 gets `count = *(u32*)[[e+10h]+150h]` and `idx | (ready&&ref ? 0 : 1<<31)`.
- Up to 0x200 entries / 0x1000 groups of 256 go into one dispatch (`vtbl+C8h` commit, token 0x7E), then token 0x8A.
- Hot: **4AC70 / 4AC76 = 66–74 %**, right after `mov rax,[rcx+150h]` and `mov r8d,[rax]`: two dependent misses per
  active entry in pass 1.
- With the A1 locked loads in front of them, consecutive entries cannot overlap.

**Is there a frame-skip, budget or dirty mechanism?**
- No frame skip anywhere.
- Budgets: only in 1290B8 (tile requests) and 4AA34's batch sizes.
- Dirty/pending gates: 4BA8C (`M+4C4`), the 4BBEC buffer rebuild, and the upload dirty bits.
- Per-frame dedupe stamps: group `+20h`, texture `+48h` / `+188h`.
- What runs unconditionally O(N) every frame: 419A8, 4AA34 (2 passes), 4BBEC's part loop and 1E7050.

---

## 2. Candidates (Q2/Q3): designs, bytes, hazards

All sites are checked against the unpacked exe dump. None overlaps the A/B/D/W1/W2/cpu_caves/ft_vrr sites; the script
checks this too. A whole-.text sweep found no direct branch target inside any patched range.

### P1E7: prefetch in the 1E7050 texture loop (rank 1)
- **Site.** `1E72C6: 4D 8B 34 24 4D 8B 83 C8 01 00 00` (`mov r14,[r12]; mov r8,[r11+1C8h]`)
  → `E9 <cave> 66 0F 1F 44 00 00`.
- **Cave**, with r12 = &tex[k], r15d = k, rdx = group:
  - if k+2 < `[rdx+18h]`: `prefetcht0 [tex[k+2]+68h]`;
  - if k+1 < `[rdx+18h]` and tex[k+1] ≠ 0 and `s = [tex[k+1]+68h]` ≠ 0: `prefetcht0` s+12Eh, +150h, +90h, +178h, +1B0h
    (the lines 128EF0 and 1290B8 stall on);
  - then the two displaced instructions, and `jmp 1E72D1`.
- **Scratch** rcx, r10 and flags, **proven dead**: the walker followed every path from 1E72D1, including into 128EF0
  → 1283A8 / 1286E8.
- **Safety.** The one real load (`[tex[k+1]+68h]`) is exactly what 1E72D6 loads one iteration later, under the same
  bound and the same null check.
- **Limit.** The first texture of each group is not covered (there is no cross-group look-ahead). If groups are mostly
  1–2 textures the gain shrinks; the follow-up below helps there.
- **Saving.** About 2.2 % of the main thread sits on these first-touch misses; prefetching one texture ahead should
  hide 40–70 % of it.
- **Follow-up (not built).** At the slot loop head 1E7104 (6 bytes: `cmp dword [rbx+rbp],0; jne`), prefetch the slot
  line for i+4 and `[[slot(i+2)+10h]+4]` / `[[slot(i+2)+18h]+4]`. That covers 1E7167, 1E71F9 and 1E721C (~0.4–0.9 %),
  but visibility is sparse, so test it after P1E7.

### P4AA: prefetch in the 4AA34 scan (rank 2)
- **Site.** `4ABD2: 41 F7 86 AC 01 00 00 00 00 00 40` (`test dword [r14+1ACh],40000000h`, the loop head and back-edge
  target) → `E9 <cave> 66 0F 1F 44 00 00`.
- **Cave**, with r12d = i, r11d = n, r14 = M, `[rbp-55h]` = pass:
  - **i+16:** `slot = map[i+16]`; `prefetcht0 [M+568h + slot*20h]`;
  - **i+8:** if `(flags & 12h) == 2`: `prefetcht0 [[e+10h]+150h]`;
  - **i+4, pass ≥ 1 only:** same flags test, `o = [e+10h]` ≠ 0, `p = [o+150h]`, `prefetcht0 [p]`;
  - then the original `test` (it sets the flags that 4ABDD `jne` consumes), and `jmp 4ABDD`.
  - Each stage has the same bounds as the original: `< n`, `< 100000h`, `map ≠ -1`.
- **Scratch** rcx, rdx, r8, r9, **proven dead** from 4ABDD. Virtual calls on the path use the arities from the decompile.
- **Safety.**
  - The i+4 dereference runs under the original's own precondition for that dereference (pass 1, flags 2 && !0x10),
    a few iterations earlier, in the same function call.
  - Any free/reuse race on that object exists for the original's own dereference in the same function call; the
    prefetch only moves it a few iterations earlier. The worker code seen (48E90) only sets bits with CAS; where
    entries are retired was not traced. That is an assumption, not a proof.
- **Saving.** 4AC70/4AC76 ≈ 1.5–1.9 % today. A1 lets the out-of-order core overlap some of it; the prefetch should
  hide most of the rest: **0.5–1.2 % on top of A1**.
- **4B104** has the same loop shape (0.6 %): loop head near 4B2CA, `lock cmpxchg` loads at 4B319/4B32C/4B33F. The same cave could be added there, but it was not built.

### C419: single-CAS 419A8 (rank 3)
- **Site.** `419A8: 48 89 5C 24 08` (`mov [rsp+8],rbx`, the function entry) → `E9 <cave>`.
- **The cave reimplements the function:**
  - inlines 4A5768 (pure: `cap ≠ 0 && !(1AC & A0000000h)`), then the `1AC & 40000000h` check;
  - per slot: `f = [e]` (plain aligned load); if `f & 2`, count it as active;
  - `ready = (f & 51Eh) == 402h && (M+18C == 0 || obj(+8)->D0 & 8)`;
  - `new = (f & ~E1h) | ready | ((f & 60h) << 1)`;
  - if `new ≠ f`: `lock cmpxchg`, retrying with bits 5–7 recomputed from the fresh value and `ready` kept, exactly as
    the original's separate loops do;
  - stop after the `M+4C0`-th active entry.
- **Why it's legal under concurrency.**
  - The four original RMWs become one atomic RMW. That is one of the interleavings the original already allows: the
    one where no worker write lands between them.
  - Worker writes (bit 0x200 via CAS) are preserved, because the CAS fails and retries.
  - Skipping the write when `new == f` is equivalent to the original's CAS writing back the same value.
- **Steady state.**
  - Visible entries: 1 CAS per frame (bit 5 is set every frame by 4BBEC).
  - Entries unreferenced for 2+ frames: **0 locked ops**, and no dirtied line.
- **Proof.** `meshlet_caves.fsx offline` copies the original 419A8 and 4A5768 bytes into its own executable memory.
  It runs them against the cave on **6,000 random states**:
  - capacities 0–300, random flag mixes with "ready" made common, `18C` on/off, `1AC` variants, `4C0` counts
    below/equal/above the real count;
  - 10 % of cases with the switch off (which exercises the jump back into 419AD).
  - Result: **0 mismatches** over the whole struct, entries and objects.
- **Relation to A2.** C419 supersedes A2. With C419 on, A2's bytes are dead code; with `off C419` they are live
  again, so both can coexist.
- **Saving.** 0.3–0.5 % beyond A2.

### S1E7: time-slice the texture-group update (experimental, default OFF)
- **Site.** `1E71CD: 48 3B 4A 20 0F 85 C2 00 00 00` (`cmp rcx,[rdx+20h]; jne 1E7299`) → `E9 <cave> 0F 1F 44 00 00`.
- **Cave:**
  - if the stamp is already equal: go to 1E71D7, as before;
  - if the switch is off: go to 1E7299, as before;
  - otherwise the phase flips once per distinct frame-stamp value (cave state, so it does not depend on how
    6EB0h increments);
  - groups with `parity(popcnt(G & 3F0h)) == phase` take the "already updated" path (1E71D7) **without** stamping;
    the rest are updated.
- **Guarantee.** Every group is updated at least every other frame, about half of them per frame, so there are no
  bursty frames. Scratch: rax and flags (proven dead; 1E7050 returns void, and its caller overwrites rax at 4CAE3).
- **Hazards (why it's off by default):**
  - (1) Tile requests per visible texture are issued at half rate. Their budget is per call (≤16) and per frame
    (global), so **streaming throughput for the visible set roughly halves**. Sharpening after a teleport or camera
    cut can visibly take longer.
  - (2) 128EF0's slot re-acquire or compaction can lag one frame. Whether a texture with a stale slot makes the GPU
    write feedback into the wrong slot for that frame is unknown. That would mean spurious requests, not corruption.
  - (3) The `M+888h` visited array is unaffected (it is still rebuilt every frame).
  - The frame-window hysteresis in 1290B8 uses absolute frame numbers, so it is unaffected.
- **Saving.** About half of 128EF0 + 1290B8 + the texture-loop part of 1E7050, so ~1–1.5 %. It is less once P1E7
  has removed the misses.

### What must NOT be frame-skipped (hazards for Q2)
- **419A8.** It is a per-frame shift register (bits 5→6→7), plus bit 0 (ready). Skipping it stretches the
  "referenced 1/2 frames ago" history and delays readiness. Eviction and request decisions downstream read those bits.
- **4AA34 / 4B104.** They record GPU dispatches (`MeshletCheckLoadRequestCommand` / `MeshletGenerateLoadRequestCommand`)
  whose output the request pipeline and the TS_MESHLET_REQ readback consume. Skipping a frame means stale or duplicate
  requests, depending on how the buffers are cleared, which is unknown without shader analysis. They also emit tokens
  (0x7E / 0x8A) and increment the dispatch counter at `ctx+2A70h`. The remaining CPU upside after A1 + P4AA is only
  ~0.5–1 %, which is not worth it.
- **4BBEC / 5D214 as a whole.** The upload buffers are double-buffered by the per-frame parity toggle (`+16A0B8`), and
  the token stream (0x82 / 0x83), the global `+6EB8h` counter and the frame stamps all advance here.
- **Moving work to a job/worker.** 4AA34, 4B104 and 4BBEC write into the main thread's KTGL command stream
  (`ctx+3C40h`) and a shared compute context, so they can't move. The texture part (128EF0 / 1290B8) does no token
  writes, and its allocator is already under a critical section, so it is the only candidate. But it would need a
  submit-and-join on the engine's job system inside 4BBEC (new RE, an invasive cave), and the visited array would
  need a sync point. Not recommended as a byte patch.

### Data races checked
- Workers touch entry flags only through `lock cmpxchg` (bit 0x200).
- C419 keeps every flag write as a CAS on the full word, and the plain loads are aligned dwords (atomic, acquire on
  x64). The only thing lost is the StoreLoad fence that the read-only `lock cmpxchg` loads used to provide. No Dekker
  pattern exists here (the main thread never stores and then re-reads a worker's flag to decide).
- P4AA and P1E7 issue only loads and prefetches. S1E7 changes main-thread-only state.

---

## 3. Other observations
- **TS_MESHLET_REQ workers** (~13 % busy each, mostly 48E90). The loop 49150..4915A walks one list (`+67A8h`) when
  `g+67C0h != 0`, else three (`+67A8h/67B0h/67B8h`), summing `+14h` **for every request**; 36-42 % of 48E90 self in our
  r5/ingame2/r6/r7 captures, about 3.6 % on a 32-thread NVIDIA PC (outside data). Samples after a load mark where a
  cache miss lands, not the removable cost. (An earlier version said "four lists".) That
  costs worker CPU, not the main thread, so it was left alone.
- The A1 / A2 / A3 rows in `research/mainthread-drawprep.md` remain valid. C419 replaces A2's effect when it is on.

## 4. How to verify
1. Offline (done): `dotnet fsi meshlet_caves.fsx offline` should print `OFFLINE CHECKS: ALL PASSED`.
   It checks the site bytes, the overlap list, the decoded patched sites, the register-liveness proofs, the
   branch-target sweep, and the C419 native test.
2. Live, in a real game process:
   - `dry`;
   - then `$env:FT_ALLOW_WRITE=1; dotnet fsi meshlet_caves.fsx apply C419,P4AA,P1E7,S1E7`, one apply per process.
     S1E7 stays OFF until `on S1E7`.
3. Run `status` and check that **C419 calls/s ≈ fps** and **CAS writes/s ≈ visible entries × fps**. If CAS writes/s is
   about entries × 3 × fps, bit-churn assumptions are off; it is still correct, just less gain.
4. A/B without code changes:
   - `off C419,P4AA,P1E7` against `on …`;
   - PresentMon at the round-6/7 shrine spot: average fps, plus p50/p99 frametime;
   - `cpuprof.fsx` main thread: self of 419A8, 4AA34, 128EF0, 1290B8, 1E7050, and 5D214 inclusive.
   - Expect 4AA34 to fall below ~1.2 %, 128EF0 + 1290B8 to fall by roughly a third or more, and 419A8 to fall to
     ~0.3 %.
5. S1E7, separately:
   - `on S1E7`; `status` shows frames/s ≈ fps and groups deferred/s;
   - visual check: fast-travel or teleport and record time-to-sharp textures against `off S1E7`; also watch for
     texture shimmer or mip flicker while panning.
   - Keep it only if there is no visible difference and the profile gain beyond P1E7 is worth it.
6. Revert: `revert C419,P4AA,P1E7,S1E7`. The cave stays allocated, and threads inside it return to valid addresses.

## 5. Files (private dev repo)
- `meshlet_caves.fsx`: the cave builder, the offline verifier (byte checks, liveness walker, branch
  sweep, C419 native test), and the live modes.
- The last offline run, with the cave disassembly at a dummy address.
- Decompiles studied: 4BBEC, 5DC18, 4B104, 5B278, 5D1F4, 4BA8C, 5D160, 5C708, 695328, 1E73C0; 1286E8, 1283A8,
  129A28, 4A5768, 5B484, 5D214, 419A8, 4AA34, 1E7050, 128EF0, 1290B8, 4DB6C/4DD4C.
- Disassembly of 5D214, 1E7050, 128EF0, 1290B8 (head), 419A8, 48E90 (worker).
