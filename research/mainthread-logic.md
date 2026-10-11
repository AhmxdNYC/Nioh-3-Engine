# Main thread "non-render" work and the game-logic thread: where it goes, what is waste

Static analysis plus offline verification only (dump, Ghidra, the r5 / r6 / r7 / r9 / ingame2 sampling profiles).
Nothing was attached to the game, read live, written or applied. Date: 2026-10-06.
Scripts (`logic_live.fsx`, `frames.fsx`, `callgraph.fsx`) and large outputs (decompiles, call graph, disassembly) stay
in the private dev repo. The symbols named here are in symbols/symbols.tsv.

## TL;DR

1. **The premise that ~20 % of [App]Game is game logic is wrong.** [App]Game is a pure **render task-graph
   executor**. 3D1F20 → 112DD8 walks a node list and runs each node's task through 113204. The ~20 % between 112DD8
   and 113204 is render-frame-begin 4A529C, which has already been analysed. No script-VM, AI, animation or mission frame
   shows up on [App]Game in any capture. Game logic (frame step 5ACF80 → 5AC9EC → 5651D0 → 39E4E4, plus the object
   update jobs) runs on **KIDSTaskScheduler0** and the job workers.
2. **The profiles hide about a third of the main thread from its callers.** SceneRenderView 535188 has a
   **53,312-byte stack frame**, and `cpuprof.fsx` reads 32 KB of stack. So no sample under 535188 can unwind to 113204
   or to the thread root.
   - "113204 = 37 % ⊃ 535188 = 35 %" is false. They are disjoint: root-reaching (52–56 %) + 535188 (34–37 %) ≈ 89–92 %
     of samples in all 10 captures.
   - The real split of [App]Game is: draw walker 535188 ≈ 35 %, render-frame-begin ≈ 13–19 %, UI ≈ 6–7 %,
     effect GPU recording (EffectViewRecordSync 365AA98, particles.md section 6) ≈ 6–7 %, **other render-graph
     nodes ≈ 18–26 %** (never attributed before), and the 1 ms retry / QPC wait (D) 0–8 %.
3. **One real, patchable waste on the logic side:** `ObbToAabb` 6EA50 computes an OBB's AABB with 54 tiny calls and
   memory round-trips. It costs **6.5–8 % of KIDSTaskScheduler0** in every capture, plus ~0.02–0.04 core on the workers.
   - A bit-exact SSE cave makes it **~87 % cheaper**.
   - Verified offline: 300,000 random finite inputs, 0 bitwise mismatches; non-finite inputs and the aliasing fallback
     are also identical.

| rank | id | what | where | cost now | est. saving | patch | risk |
|---|---|---|---|---|---|---|---|
| 1 | **AABB** | OBB→AABB through 54 non-inlined vec calls + store/reload chains (≈150–270 ns per box) | 6EA50 (callees 6F060 / 6F010 / 6EFD0); 86 % of calls from 71C60 (per-part bounds), 38 callers | Scheduler0 6.5–8.1 % of a core; workers ~0.02–0.04 core | **~0.055–0.07 core on Scheduler0, ~0.02–0.035 on workers**; update-thread frame time −~0.5–0.6 ms at ~113 fps | 5-byte jmp at 6EA50 → cave (741 B code), runtime switch, alias fallback; **offline ALL PASSED** | low |
| 2 | RA | Rolling average re-walks a whole `std::list<float>` (≈fps nodes) every call | 12B440, loop 12B673..12B694 (hot RIP 12B67D = 100 %) | Scheduler0 1.0–1.4 % | ~1 % of Scheduler0 | **not built**: a running sum changes float rounding | low/medium |
| 3 | – | Per-node timing stats (2×QPC + spinlock + ring) and optional debug markers with `swprintf` | 113204 | not in any top-30 (<0.5 %) | ≤0.3 % | not built; needs one live read (`node+40h`, `node+4`) | low |
| – | – | Other render-graph nodes on [App]Game (18–26 %) | under 113204, outside 535188 | 18–26 % | unknown | **needs a re-profile with a deeper stack read** (§5) | – |

**Main-thread fps effect (honest).** AABB does not touch [App]Game. It shortens the game-update thread, which feeds
[App]Game.
- In r6/r7, [App]Game spent 5–10 % of its samples in the 1 ms "frame not ready" retry (candidate D in
  mainthread-drawprep.md). In those scenes a faster update can turn into fps.
- In r5-like scenes (render-bound, no D wait) expect **no fps change**. There the gain is ~0.08–0.1 core of CPU and power.

---

## 1. Accounting of [App]Game with the profiler blind spot removed

Frame sizes from .pdata unwind codes (`frames.fsx`):

| function | stack frame |
|---|---|
| BDB740 / 5B6670 / 134EBD0 | 48 B each |
| 27049CC / 1A74130 / 3D1F20 / 112DD8 / 113204 | 320 / 432 / 112 / 176 / 176 B |
| **535188 SceneRenderView** | **53,312 B** (`mov eax,0D000h; call __chkstk`, 7 pushes, rbp frame) |
| 4A529C render-frame-begin | 15,824 B |
| 365AA98 | ~16.5 KB (`auStack_4048`) |

`cpuprof.fsx` reads at most `stackBytes` = 32,768 B from rsp. To unwind 535188 it must read its saved registers and
return address ≥53 KB above rsp. That fails, the walk stops at 535188 (≥2 frames, so no fallback scan), and the sample
never counts for 113204, 112DD8, 3D1F20 or the root. (`frames.fsx big 24000` lists 75 functions with frames ≥24 KB;
any of them causes the same effect.)

Inclusive % per capture (main thread):

| capture | root | 112DD8 | 113204 (excl. 535188) | **535188** | 4A529C | UI 253C68 | 365AA98 | 113204 − UI − 365AA98 | not root, not 535188 |
|---|---|---|---|---|---|---|---|---|---|
| r5 | 56.1 | 58.4 | 37.4 | 35.4 | 18.7 | 7.2 | 6.3 | 23.8 | 8.5 |
| r6-A1 | 53.7 | 49.0 | 33.9 | 35.3 | 16.0 | 6.2 | 7.2 | 20.5 | 11.1 |
| r6-B1 | 55.6 | 48.0 | 34.5 | 35.3 | 13.9 | 6.5 | 7.0 | 21.0 | 9.2 |
| r7-A1 | 56.2 | 47.6 | 35.4 | 34.2 | 13.3 | 7.1 | 7.5 | 20.8 | 9.6 |
| r7-B1 | 51.7 | 53.3 | 36.0 | 36.9 | 16.2 | 7.8 | 7.4 | 20.8 | 11.4 |
| r9-all | 53.7 | 47.2 | 30.6 | 34.2 | 11.3 | 6.1 | 6.8 | 17.6 | 12.1 |
| ingame2 (pre-W1) | 67.7 | 70.0 | 56.2 | 24.4 | 12.9 | 31.5 | 5.8 | 19.0 | 7.9 |

Reading:
- 112DD8 − 113204 ≈ 4A529C in every capture (r5: 21.0 vs 18.7; ingame2: 13.8 vs 12.9). Render-frame-begin is
  therefore reached through one of the executor's own vtable calls in 112DD8 (begin/end hooks 0x110/0x118/0x120 or the
  every-4th-node hook 0x70), not inside 113204. This is inferred from the arithmetic, not traced.
- root − 112DD8 (up to ~8 % in r6/r7) is 1A74130's own QPC retry loop (D).
- The "113204 − UI − 365AA98" column is the **unexplained remainder: 18–26 % of the main thread in other render-graph
  nodes**. 365AA98 is the effect view recording (particles.md section 6), so the UI chain does not hang under it, the
  UI and 365AA98 columns do not overlap, and the remainder stands as computed.
  - What the self-time lists say about it (callers from the r5–r7 self tables, direct call graph from
    `callgraph.fsx`): the shader-constant / state commit family is used by ~140 render functions (post-process and
    other passes in 2F9xxxx, 309xxxx, 34Axxxx …), not only by 535188:
    - `1DF880 ShaderConstFlush`;
    - `1D5F20 / 1D6780 / 1D69CC / 1D6E64` → `1D7EB0 / 1D8220 / 1D8720 / 1D84A0` (≈5 % self together);
    - 1DAF6C (160 callers), 1E0B64 / 1E55D4 / 1E5854;
    - B94230 memcpy via the token-0x66 writers 242EB8 / 242FC4.
  - It is per-pass and per-draw dirty tracking (§4.3); nothing in it looked redundant.
- "not root, not 535188" (8–12 %) is other unwinds that hit big frames (4A529C 15.8 KB + deep callees, 365AA98 16.5 KB).

**Consequence for earlier notes:** mainthread-drawprep.md's tree "113204 (37 %) └ 535188 (35 %)" should read
"113204 = 535188 (35 %, invisible above itself) + ~37 % other render nodes and UI". The per-function self percentages
in those notes are unaffected; only inclusive parentage changes.

## 2. What [App]Game actually runs, and where the game logic is

Call chain: 1A74130 MainLoopFrame -> 3D1F20 FrameStep -> 112DD8 RenderTaskGraphRun(exec, ctx, firstNode, ...).
- **112DD8 RenderTaskGraphRun** walks the node list (next pointer at node+38h). For each node it calls the executor's
  vtbl+130h hook, then FrameRenderViews 113204, then vtbl+138h. After every 4th node that executed, it calls
  vtbl+70h. At the end it calls vtbl+118h and vtbl+120h, plus timing via 3E700 when profiler counters exist.
- **113204 FrameRenderViews** runs one render task-graph node: if the node flags are negative and ctx vtbl+F8h returns 0, it opens a
  debug marker (the node name, or `swprintf "Type:0x%08x,Name:0x%08x"` for unnamed nodes). It then calls the node
  task's vtbl[1]. When node stats exist, the call is bracketed by two QPC reads and the delta goes into a 10-entry
  ring with min/max under a spinlock. The marker is closed at the end.
- The node tasks are the KTGL render graphs (`.kidsrender`). 535188 is one node's task; the post-process, UI and
  365AA98 chains are others.
- The 33-node frame graph `0xbd13a52e.kidstask` (FrameSync, Behavior::Update ×2, EvaluateScript, UpdatePlaceable,
  RegisterLight / Primitive …; game-data.md §4.2) is **not** what runs here.
  - The only game-frame step in the profiles is 5ACF80 on KIDSTaskScheduler0 (61–66 % inclusive):
    5AC664 → 5AC9EC (51 callees) → 564F40 → 5651D0 PerFrameUpdater → 39E4E4 (68 callees, 35–40 %).
  - The object-update jobs 182000 / 204584 → 408FE0 → 16EF8C → 16CFF0 → 1A7BF0 (17–21 %) run on Scheduler0 and on
    workers 1–6 (~10 % each).
  - Script / mud strings: `mud::from_kidshost::EvaluateScript` is referenced from 13F910 / 2455900;
    `UpdateWithScript`, `UpdateObjectWithScript` and the `mud::debug::print_*` bindings are at 2461xxx.
  - None of these, and no interpreter-like dispatch loop, appear in the [App]Game self or inclusive tables of any
    capture. Confidence that logic is off the main thread: **high** for the profile evidence. Where exactly each
    kidstask node executes was not traced.
- **How the logic thread gates the render thread.** 3D1F20 returns 0 when the frame packet from the update side
  (`+138h / +1E1h`) is not new. 1A74130 then spins 1 ms on QPC and retries (D, 1A744E4 constant 0.001f; patch already in
  mainthread-drawprep.md). This was 5–10 % of [App]Game in r6/r7, so in those scenes the **update thread** was the
  limiter.

## 3. Patch built and verified

### 3.1 AABB: `ObbToAabb` 6EA50 → SIMD cave (bit-identical)

**What it computes.** `ObbToAabb(const Obb* o /*rcx*/, f32 out[8] /*rdx*/)`.
- Layout: `Obb{center@0, axis0@10h, axis1@20h, axis2@30h, halfExtents@40h}`; `out[0..2]` = max, `out[4..6]` = min,
  `out[3]` / `out[7]` untouched.
- For each direction d ∈ {+x, −x, +y, −y, +z, −z} the stock code does:
  ```
  P = C
  for i in 0..2:
      dot = Vec3Dot(d, A_i)                                    // 6F060: ((d.y*A.y)+(d.x*A.x))+(d.z*A.z)
      f   = dot > 0 (ordered, comiss/ja) ? 1.0 : -1.0
      T   = Vec4Scale(A_i, f*e_i)                              // 6F010, stored to stack
      Vec4AddInPlace(P, T)                                     // 6EFD0: P = T + P, through memory
  store P[k]
  ```
- That is 18 dots + 18 scales + 18 adds = **54 calls per box**, each add a 4-lane load-add-store on the same stack
  slot (store-forwarding chains).
- Hot RIPs: 6EFD4 / 6EFDB (≈60 % of 6EFD0) are the reload / store of P.

**Evidence of cost (Scheduler0 self, % of the thread; core % = self × thread CPU).**

| capture | 6EFD0 | 6EA50 | 6F010 | 6F060 | sum (core %) |
|---|---|---|---|---|---|
| r5 | 4.3 | 3.1 | 1.4 | 0.9 | 8.1 |
| r6-A1 | 3.3 | 2.2 | 1.3 | 1.2 | 6.5 |
| r6-B1 | 4.1 | 2.4 | 1.2 | 1.0 | 7.0 |
| r7-A1 | 4.3 | 2.4 | 1.2 | 1.0 | 7.1 |
| r7-B2 | 2.5 | 1.6 | 0.9 | 0.7 | 4.6 |

- KIDSTaskScheduler1–6 show 0.2–0.8 % each of the same family, ~0.02–0.04 core in total.
- Main caller: **71C60 BoundsFromPartObbs** (86 % of 6EA50's samples). Per part (half-float packed transforms or full
  matrices) it builds an OBB, calls ObbToAabb, and unions the results into one AABB. It is reached from the object
  update job 1A7BF0, so it runs per object per frame.
- The data changes every frame (animation), so caching would not help. The waste is purely the implementation.

**Patch.**
- `6EA50: 48 89 5C 24 08` (`mov [rsp+8],rbx`, exactly one instruction) → `E9 <rel32 cave+100h>`.
- Cave (`logic_live.fsx: buildCave`), 741 bytes of code, xmm0–5 / rax / flags only, leaf (no stack use):
  - `en` dword @+0 (bit0 = on, bit1 = count calls), counter @+4, constants @+40h.
  - If `en&1 == 0`, or `out` overlaps the OBB (`rdx < rcx+4Ch && rcx < rdx+1Ch`; the stock code re-reads the input
    after each store), it falls back to the stock path: `mov [rsp+8],rbx; jmp 6EA55`.
  - Otherwise, for the 3 output lanes at once:
    - `Z = 0*A`;
    - dots `D = ((V1)+(V2))+(V3)` with V1 = (Zy, ±Ay, Zy), V2 = (±Ax, Zx, Zx), V3 = (Zz, Zz, ±Az), i.e. the exact
      stock products and summation order per direction;
    - `f = (0 < D) ? 1 : −1` (cmpltps, ordered, like comiss/ja);
    - `S = f*e`, `T = S*A`, `P = T + P` (same operand order as the stock mulss / mulss / addss);
    - store with `movq` + `extractps`.
- Requires SSE4.1 (blendps / extractps). Every x64 CPU of the last ~15 years has it. The game's own minimum spec was
  not checked.

**Offline proof** (`dotnet fsi logic_live.fsx offline`; **ALL PASSED**):
- Site: bytes match, the first instruction is exactly 5 bytes, 6EA50 is a .pdata function start. Whole-.text sweep:
  58 direct branches / calls hit 6EA50 and **none** target 6EA51..6EA54.
- Native harness: the original 6EA50..6F0A0 (body, cold stubs, Vec4AddInPlace / Vec4Scale / Vec3Dot), Vec4Set3
  167B30 and the constants 3BD41C0..3BD4200 are copied from the dump to their RVAs inside one private region. The
  script also checks that every branch and RIP-relative operand of the copy stays inside the copied ranges.
- **300,000** random finite OBBs (unit rotations, wide-range floats, ±0, denormals, ±3e38, arbitrary bit patterns):
  **0 bitwise mismatches** over all 8 output dwords (including the untouched [3] / [7] sentinels).
- 20,000 inputs with NaN / ±inf injected: also 0 differences (reported, not required).
- 2,001 aliasing cases (out inside the OBB): fallback taken, 0 byte differences against the stock code.
- Switch off: 2,000 cases identical (the stock path).
- Timing (delegate overhead subtracted): stock **150–273 ns per box** (the range is between runs), cave **19–33 ns**,
  about **87 % less**.

**Expected saving.**
- ~87 % of 4.6–8.1 % ⇒ **4–7 % of KIDSTaskScheduler0 (≈0.04–0.07 core), ≈0.4–0.6 ms less update work per frame at
  ~113 fps**.
- Plus ~0.02–0.035 core on workers 1–6.
- Check: at ~273 ns per call, 8.1 % of a core ≈ 300 k boxes/s ≈ 2,600 boxes per frame, which fits per-bone bounds of
  the characters on screen.

**Risk: low.**
- Results are bit-identical (proved natively), the switch reverts behaviour without a code write, and the jmp sits on a
  single-instruction boundary at the function entry.
- In-flight calls of the old body are unaffected: the rest of the body is unchanged, so return addresses into it stay
  valid.
- `safeWrite` freezes the process and checks every thread's RIP.

## 4. Other findings (not patched)

### 4.1 Nothing script-VM / debug-string shaped on [App]Game
No Printf / formatting function, string-hash routine or interpreter dispatch appears in any [App]Game top-30, self or
inclusive. The only formatting on the main thread is 113204's `swprintf("Type:0x%08x,Name:0x%08x")` for **unnamed**
nodes when markers are enabled (`node+4 < 0`). Its cost is below the top-30 threshold in all captures.

### 4.2 RA: `RollingAvgListUpdate` 12B440 (Scheduler0 1.0–1.4 %)
- Per entry, every frame:
  - push a sample into the `std::list<float>` at entry+58h;
  - trim it to `ceil(fps × {1|60} / 60)` nodes (≈ fps nodes, i.e. a 1-second window, when the sample is non-zero);
  - then walk the **whole** list to sum it (`FLT_MAX` samples are counted as invalid) and store the average at +50h.
- The hot RIP 12B67D (100 % of the function's self samples) is the node-value load, so each node is a cache miss.
- Only caller: FE3AC0 (from 396674). What the average is used for was not identified.
- **Fix idea:** keep a running sum and count in a cave (add the new sample, subtract the popped ones). **Not built**,
  because the float summation order changes and the average would differ in the last bits from stock. That is harmless
  for an average, but not "provably identical". Expected ≤1 % of Scheduler0.

### 4.3 State-commit family on the main thread (1D69CC / 1D6E64 / 1D6780 / 1D5F20 → 1D8720 / 1D84A0 / 1D8220 / 1D7EB0)
- These run only when the shader program or binding table changed (`ctx+2578h` / `+2568h` shadows,
  `+13Bh` / `+13Ch` dirty bytes).
- They then refresh 5 + 5 per-stage slot tables (0x90-byte slots) and emit token 0x29 / 0x23.
- This is already dirty-tracked, so it is not redundant work. The ~5 % self is per-pass / per-draw state churn,
  spread over ~140 render callers.

### 4.4 Small items
- `3D859C` EffectGpuObjLatchSubsteps, the per-GPU-effect-object substep latch (main thread, under 694B38 in the
  365AA98 effect-recording chain, particles.md section 6): two `while (!CAS) Sleep(0)` spinlocks, then a per-entry
  virtual 0xE0 call. 3D8770 is ~0.8 % self in 4 captures. That is real work; the spinlocks only matter under
  contention.
- 113204 per-node `Stats`: 2× QPC + lock-cmpxchg spinlock + ring per node. It is cheap (<0.5 %). Turning it off needs
  `node+40h` to be NULL, which is a data change of unknown side effects. Not worth it.
- 3D1F20: two `lock cmpxchg` per frame on `+1F0h`. Negligible.

## 5. What remains unknown, and how to resolve it (not run here)

The 18–26 % "other render nodes" on [App]Game can only be broken down with a profile that unwinds through 535188 and
the other big frames. `cpuprof.fsx` already takes the stack size as its 6th argument:
```
dotnet fsi cpuprof.fsx 20 cpuprof-r10-deep.txt 1000 2 24 131072
```
Use 128 KB. The outer part is read after resume, so the hold time does not grow. In that capture, 113204's inclusive %
should rise to ~70 %, and the root to ~90 %.

## 6. How to test AABB (live; nothing here was run against the game)
```
dotnet fsi logic_live.fsx offline                       # re-verify against the dump (no game access)
dotnet fsi logic_live.fsx dry                           # read-only: site "original", body matches dump, cave preview
$env:FT_ALLOW_WRITE=1; dotnet fsi logic_live.fsx apply  # one cave per game process; jmp written with safeWrite
dotnet fsi logic_live.fsx status                        # switch / site state
$env:FT_ALLOW_WRITE=1; dotnet fsi logic_live.fsx count  # optional: switch=3, status then prints cave calls/s
$env:FT_ALLOW_WRITE=1; dotnet fsi logic_live.fsx off    # A/B: stock path, no code write
$env:FT_ALLOW_WRITE=1; dotnet fsi logic_live.fsx on
$env:FT_ALLOW_WRITE=1; dotnet fsi logic_live.fsx revert # original bytes back (cave stays allocated)
```

| signal (cpuprof at the r6/r7 spot, ON vs OFF) | expectation with ON |
|---|---|
| Scheduler0 self 6EFD0 + 6EA50 + 6F010 + 6F060 | 6.5–8 % → ~0 (cave samples appear as `<anon>`, ~1 %) |
| KIDSTaskScheduler0 CPU | −4 to −7 points |
| [App]Game RtlQueryPerformanceCounter ← 1A74BE0 (the D wait) | may drop where it was 5–10 % |
| `count` mode calls/s | ≈ 0.2–0.4 M/s in combat-heavy scenes (switch back to 1 afterwards; the counter is a shared non-atomic inc) |
| PresentMon | fps unchanged or up where the D wait was present; p99 unchanged |
| visuals | none possible (bit-identical). Watch for culling or bounds pop-in anyway |

## 7. Files (private dev repo)
- `logic_live.fsx`: AABB cave builder, offline verifier (site, sweep, native equivalence, alias,
  switch, non-finite, timing), and the live modes dry / status / apply / on / off / count / revert, with the
  FT_ALLOW_WRITE guard and `Mem.safeWrite`.
- `frames.fsx`: stack-frame size from unwind codes (`frames.fsx <rva>...`, `frames.fsx big <bytes>`).
- `callgraph.fsx`: direct call graph (`build`, `reach <root> [depth] <rva>...`, `path`, `callers`).
- Ghidra decompiles studied (112DD8, 3D1F20, 5ACF80, 5AC9EC, 5AC664, 5651D0, 564F60, 39E4E4,
  113204, 365AA98 chain, 1D69CC / 1D6E64 / 1D8720 / 1D84A0 / 1D8220 / 1D7EB0, 1DAF6C, 1DF880, 1E0B64, 1E5854, 242EB8,
  3D859C, 1DC0F0, 71C60, 6EA50, 182000, 204584, 408FE0, 16EF8C, 61FEA8, 66D00, 54360, 12B440, FE3AC0, E32D90, 284ED4,
  F33A0, 425B0); `bigframes.txt`; `dis_6EA50.txt`.
