# GPU meshlet culling chain: same-result kernel rewrites (Topic B of shadows-culling.md)

Status 2026-10-07: built + proven offline, staged in `gpu-shaders\replacement\` (gitignored, rebuilt by
`gpu-shaders\sweep\build_sweep.py mpc mshlod`), **not installed**. Game file v2.02 only (digests checked at build).

## 1. What was rewritten (dispatch signatures unchanged: same records, numthreads, bindings, cbuffers, dispatch dims)

| Item / file | Record | Shipped | Replacement |
|---|---|---|---|
| `mpc` 0x29ee0902.file (one combined file; supersedes the sweep's mpc23-only build 26417c0f…) | MPC#23 | per thread up to 63 dependent load round trips (block-local prefix sums), 11–16 µs × 4 | every element loaded once; for cnt ≤ 1024 (all captured chains: Dispatch(1,1,1)) group 0 computes the **final** state of the whole #23→#31 chain in LDS (O, A, L1 scratch); cnt > 1024: the shipped block-local result via an LDS scan (the sweep's MPC23_fast algorithm) |
| | MPC#28 | per thread up to 16 dependent loads | `return` when cnt ≤ 1024 (results already final); else shipped code 1:1 |
| | MPC#29 | `numthreads(1,1,1)` serial superblock loop | `return` when cnt ≤ 1024; else shipped loop |
| | MPC#30, #31 | block/element fix-ups | `return` when cnt ≤ 1024; else shipped code |
| `mshlod` 0x8d9282e6.file | MshLod#3 | every active thread runs the same ≤ 31-iteration pad loop (9 × per frame) | u3[0] read by all lanes → group barrier → same store; pad element c+j written by lane j |

Sources: `gpu-shaders\culling\MPC23_chain.hlsl`, `MPC28/29/30/31_chain.hlsl`, `MshLod3_fast.hlsl`.
MPC#23 also writes u9[1] = max(1, ceil(u9[0]/2048)), the value MPC#26 writes later in the same chain (nothing in
between reads or writes u9[0..1]: #24 writes u9[4..6] and u9[9i+7..9i+15] only). That changes nothing for the
file-only install and lets #26 be skipped engine-side (§3).

## 2. Proof (DXIL reference interpreter `culling\vendor\sim.py`, shipped DXBC vs compiled DXIL)
- `py -I culling\verify_culling.py` → **95 scenes, 0 failures** (`culling\results\verify_full.txt`). Chain-level, dispatched
  as 5301FC records them (#23 ceil(cnt/1024) | #28 ((ceil(cnt/64))+511)>>9 | #29 1 | #30 | #31 | #26 1), UAV barrier between.
  cnt around every 64/1024 boundary up to 33000, random full-range u32 (wrapping sums), offsets, u9[0] across the 2048
  rounding. Checks: (A) file-only install == shipped chain, all UAV contents; (B) engine skip (#23 only, no #26 when
  cnt ≤ 1024) == shipped chain; (W) no write outside the shipped write set; (R) cnt > 1024: each record alone ==
  shipped record (contents and write log). `selftest` → PASS (catches 3 broken chains). Order: every element keeps its
  index; nothing order-dependent changed.
- `py -I culling\verify_culling.py mshlod` → **2926 scenes, 0 failures** (532 also vs the shipped DXIL), incl. the
  c ≥ 2^32−31 wrap, negative/huge payload offsets, 0/1/2/31/32/33/2^32−1 active threads, 1–2 groups. The shipped
  code's cross-lane u3[0] read/write is modelled with wave semantics (all lanes load before any lane stores) by a
  transcription that is checked against the interpreter for ≤ 1 active thread. `mshlod selftest` → PASS (`culling\results\verify_mshlod.txt`).
- Interfaces (parts, SFI0/ISG1/OSG1/PSV0, bindings, cbuffer layouts) identical: checked by build_sweep.py.
- RGA (`culling\results\rga_stats.md`): no scratch, VGPRs 8–36, 16 waves/SIMD everywhere.

## 3. Optional engine-side skip (for the ASI owner; NOT needed for the file-only gain)
With `mpc` installed, the chain is final after #23 when cnt ≤ 1024, so MeshletPrecullingRecord 5301FC could skip 5
dispatches + their UAV barriers per chain (20 per frame). `cnt` = `[rsp+0A8h]` (ebx at #23..#31; it is the value of
the dispatch dims; confirm it equals the cbuffer's nVSDrawPrm.y before relying on it).
- #23 block 530E8E (bind table idx 8, inline Dispatch((cnt+1023)>>10,1,1) at 530F07, barrier 530F5D) – keep.
- #28 530FE4 (idx 0Eh, Dispatch 531052) / #29 5310E1 (idx 0Fh, 531142) / #30 5311D1 (idx 10h, 53123F) /
  #31 5312CE (idx 11h, 531336): one detour at 530FE4: `if cnt <= 1024 goto 5313C5` (start of the #24 section).
- #26 531517 block (idx 0Bh, Dispatch(1,1,1) at 531570, je at 531533 → 5315C5): skip when cnt ≤ 1024.
- **Guard:** only when the installed 0x29ee0902.file has sha256 3947f3d7… (with the shipped #23 the skip is wrong).
- Each block is already skipped by the engine when its PSO bind ([rax+0A8h]) fails, so skipping is a known-safe path.

## 4. Expected gain (RGP r5 segment times; floor ≈ 2.2–2.5 µs per dispatch incl. drain)
| | PC (RX 9070) | Ally 780M (latency ×2–4) |
|---|---|---|
| MPC#23 51.8 → ~12 µs, #28 20.4 → ~9.5, #29–#31 ~−3, MshLod#3 42.6 → ~39 | **≈ 0.055–0.06 ms** | ≈ 0.11–0.24 ms |
| + engine skip (§3, 20 dispatches + barriers) | + ≈ 0.05 ms | + ≈ 0.1–0.2 ms |

## 5. Not rewritten (and why)
- MshLod#2 (150 µs), MPC#27 (130), MPC#20 (75), MIC#0 (47), MPC#33 (14): dependent pointer chases (instance → G1M
  root → meshlet → material) ending in atomic slot allocation; no serial loop or redundant chain to cut without changing
  which loads feed which (sweep note agrees). The persistent-thread MshLod traversal / single-pass scan of the whole
  #22→#27 chain need new kernels and a new recorded sequence (shadows-culling.md B1) – not same-signature.
- MPC#22/#0/#24/#26, MshLod#0/#1/#4/#5/#6: no loop or ≤ 1 load per thread, already at the launch floor (2.2–3.9 µs).
  MshLod#4 has a 1-thread pad loop but runs once (2.2 µs): not worth a file.

## 6. Install / revert (not done)
Windows: `gpu-shaders\apply_sweep.ps1 -Item mpc|mshlod [-Revert|-Status]`; Ally: `ally_shaders.sh apply|revert|status
mpc|mshlod`. sha256: 0x29ee0902.file 42f045c3… → **3947f3d70264920e8cf0514139b715ce6552c6389b0e364ca9fb443cca4f0d73**;
0x8d9282e6.file ec0f5283… → **214ac371d68ff4e258526bd6f1d7cd044347bc8c417410966ae6400a02986c24**. Symptom of a bad swap:
missing/flickering meshlet geometry or LOD popping → revert. RGP check: MPC#23 digest c7e0a06b…, MshLod#3 794b5de5….
