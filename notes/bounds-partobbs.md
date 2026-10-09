# BoundsFromPartObbs 71C60: the next logic-thread target after AABB (2026-10-08, ASI 1.7.40, branch `bounds`)

**Verdict:** a bit-exact fix that clears the 0.1 ms bar is built as `[Patches] BPO` (default 0, live row "Faster part
bounds (BPO)"). Est. **0.12-0.25 ms/frame** of KIDSTaskScheduler0 on the PC (Q alone ~0.10-0.13). Not run in game.

## 1. Mechanism (decomp g4.txt / `patcher\dis.fsx 71C60:C40`)
`71C60(obj rcx, p2, OBB* out r8, parts r9, n [rsp+28h], dequant* p6)`, called once per object from the object update
1A7BF0 @1A8E0C (parts = [X+8], n = [X+2Ch], X = its 7th argument; p6 = its 9th). Once per object: vfunc +E8h = the
object's local bounds. Then **per part** (half path, 20-byte record `h x, h z, f y, h ax, h ay, h az, h angle, f scale`;
a 48-byte all-float path exists when the dequant flag byte is 0):
1. `ObjResTreeAllReady` 1A7AAC (**lock cmpxchg** 1A7AE2) and again **lock cmpxchg** 71DB2 on the same ready word [obj+3F8h]
   (both are atomic *reads*: eax = src = 0);
2. 6 half -> float unpacks (integer bit tricks, not F16C-compatible: exp 0 -> float denormal, exp 31 -> 2^16 range);
3. **quaternion from axis-angle**: SinCosApprox 54360(2*angle), q = (s*axis, c), |q| via sqrtss, 1/|q| via divss;
4. box centre = R(q)*(scaled local centre) + position (inline scalar matrix), half extents from the local bounds;
5. Vec4CopyW0 174610 x5 (centre, extents, the 3 rows of Mat44FromQuat 6D130) into the OBB, ObbToAabb 6EA50, min/max union.

Where the time goes (Scheduler0 self, median r5-r9, ms/frame; hot RIPs from cpuprof-*.txt):
| function | ms | hot RIPs / cause |
|---|---|---|
| 71C60 self | 0.27 | spread: 72027 / 7203F (after sqrtss / divss), 71DFB (first touch of the part), 71DB7 (after the lock), 71DD2 |
| SinCosApprox 54360 (74-90 % from 71FC1) | 0.10 of 0.14 | branchy range reduction + polynomial, recomputed for the same angle every frame |
| Vec4CopyW0 174610 | 0.12 | 174613 = 80-93 %: the 16-byte `movaps` load of a source just written with 4-byte stores -> store-to-load forwarding fails (5 per part) |
| 1A7AAC | 0.045 | 1A7AE6 = 84 %: right after its `lock cmpxchg` |
| ObbToAabb family 6EA50.. | ~0.63 (86 % of 0.73) | already handled by AABB (1.1.0, untested) |

Redundancy: the quaternion is a **pure function of the 8 bytes** at part+8 (constants 4.0 / 0.5 / 1.0, plus MXCSR), and
the part record looks like static placement data (x/z quantized against the object's dequant offset/scale, y float,
uniform scale), so the same quaternion is rebuilt every frame. The vfunc and 6D130 are not hot. Nothing per object is
redundant (the vfunc is called once).

## 2. Fix: group BPO, three parts (`[Tuning] BPO_Mask`, default 7), one live switch
- **Q (1)** quaternion memo. Hook A 71EFE (jmp, displaced `movzx eax,[rbx+6]; mov ecx,eax`): look the 8 bytes up in a
  seqlock table (2^`BPO_TableBits` x 32 B, default 16384 = 512 KB; entry `{seq, MXCSR&E040h, key, q[4]}`); hit -> xmm11 /
  xmm12 / xmm10 / xmm9, [rsp+40h], xmm1 = 1.0, rax = [r14+3F8h] exactly as the stock code leaves them, jump to 7206F.
  Miss / off -> the stock code. Hook B 72053 (jmp, displaced the 4 quaternion stores) inserts the stock result, keyed by the
  halves **re-packed from the 4 unpacked floats the stock code actually used** ([rbp-20h] [rbp-1Ch] [rbp-18h] [rsp+30h]),
  so a concurrent change of the part data can never pair a key with another key's result. Writer: `lock cmpxchg` seq
  even->odd, data, seq+2; reader: seq / key / MXCSR tag / q / seq again (x86 keeps loads in order). The MXCSR tag
  (FTZ, RC, DAZ) is required: half denormals unpack to float denormals, and under another MXCSR the stock result differs
  (1076 of 1600 denormal-heavy cross-mode calls in the harness) - an entry only serves the MXCSR it was made under.
- **L (2)** the two ready-word reads -> plain loads, as LK / MA (eax and memory identical; only the StoreLoad fence is
  lost; the word is a status flag written by others, no Dekker pattern). 71DB2 (r10d proven 0: only 71CDD / 72406 write
  it, 1A7AAC never does) and 1A7AE2 (ebx = 0 by 1A7AD9). **1A7AAC has 9 other callers**: they get the plain load too.
- **V (4)** the 6 `call 174610` in 71C60 -> a copy with 4-byte loads (`movss` + `insertps`, forward from the 4-byte
  stores) and **one** 16-byte store with w = 0 (forwards to ObbToAabb's / the AABB cave's 16-byte loads; the stock 16 + 4
  byte store pair cannot). Same memory, rax, xmm0 (w lane re-inserted after the store; src and dst never overlap here).
SSE4.1 (insertps; AABB needs it too). Caves `caveBpoQA/QB/L1/L2/V` in caves.asm = 1:1 port of the Iced caves (175
instructions compared, 0 differences); 10 sites + 4 deps (FNV of 71C60, its cold stubs 1907C32.., 1A7AAC; 174610 bytes).

## 3. Proof (`re\scripts_logic\bounds_offline.fsx`, output `bounds_offline_out.txt`: OFFLINE CHECKS: ALL PASSED)
- Code walk from 71C60 over direct branches / calls (1325 instructions, 13 ranges, 24 rodata refs, 5 never-taken stubs);
  every walked rip-relative operand is a read. Site bytes, instruction boundaries, no direct branch into any patched
  range (whole-.text sweep), the 6 calls target 174610.
- **Liveness** (path-sensitive, through the loop back edge and cold stubs): everything a cave leaves different from the
  stock run is written before it is read - QA hit: rcx rdx r8 xmm0 xmm2 xmm3 xmm4 xmm7 flags and stack slots [rbp-1Ch]
  [rbp-18h] [rsp+30h] [rbp-30h] [rbp-2Ch]; QA miss: r8 xmm0 xmm9-12 flags [rsp+30h]; QB: rcx rdx r8 r9 r11 flags
  [rbp-30h]; L1 / 6 V return sites: flags.
- **Native equivalence**: the whole 71C60 and all callees copied from the dump to their RVAs in two regions (stock /
  patched), fake object + vtable stub, output OBB compared bitwise (0x60 bytes incl. sentinels): 200k calls over mask 0 /
  1 / 2 / 4 / 7 (miss and hit runs, random + realistic objects, both part layouts, p6 null / given, offset present /
  absent); **4 x 65536** single-part calls sweeping every half code in each key lane (and the inserted key = the part's
  8 bytes, i.e. the re-pack inverts the unpack); poisoned entries (odd / zero seq); 7 MXCSR modes; cross-mode (table
  filled under mode A, read under B); 4 threads x 300k calls on a 4-entry table (constant evictions). **0 differences.**
- Timing in the harness (warm caches, uncontended ready word, measured before the game test started; the last run's
  timing block in the out file was taken while the game ran and is not valid): stock 99.2 ns/part -> Q 88.1-88.4, L
  100.5-101.4, V 97.4-98.4, all 83.1-84.4 (-15-16 %). Caves installed but off: +3-4 %.

## 4. Estimated saving (numbers of pc-cpu-next.md §2)
- Q: SinCos from 71FC1 (0.10 ms) disappears on hits, plus the unpack / sqrt / div share of 71C60 self (72027 / 7203F,
  ~0.03-0.04), minus the lookup (~40 instructions incl. stmxcsr): **~0.10-0.13 ms**. The harness agrees (-11 % of the
  ~1.2 ms/frame 71C60 subtree). Assumes high hit rate: 2^14 entries vs an estimated ~2,000-2,600 parts / frame (direct
  mapped: ~12 % collide if all keys differ; raise `BPO_TableBits` if a profile still shows 54360 under 71FC1). If the part
  data changed every frame, every part would also pay the insert's locked op (no counter: judge by the A/B and SP).
- L: the in-game lock RIPs (1A7AE6 ~0.04 + 71DB7 ~0.02) are larger than in the uncontended harness (~0): 0-0.06 ms.
- V: 174613 is ~0.1 ms in game; harness -1-2 %: 0.02-0.08 ms.
- **Total 0.12-0.25 ms/frame of Scheduler0.** Frame time only moves where [App]Game waits for the logic packet (abba2
  scene: ~0.6-0.7 ms/frame) - and AABB, if KEEP, takes 0.5-0.6 ms of that first, so test BPO on top of AABB. Ally: 0
  (GPU-bound).

## 5. Test (PC, ASI 1.7.40 from branch `bounds`; nothing else changes)
`abba2_pc.ps1 -Live AABB,BPO -LaunchFlags none -Quads 3 -MaxFps 240 -Input vpad`
(items absent from the base ini are installed switched off = 2; AABB's block runs first and is left ON, so BPO's quads
measure its gain on top of AABB). Read-backs: boot line `BPO: BoundsFromPartObbs caves installed (... 16384 entries ...)`,
`MENU: Faster part bounds (BPO) -> ON/OFF`. Mechanism (optional, `[Profiler] SP = 1`, one ON and one OFF window): 54360 under
71FC1 -> ~0, 174613 and 1A7AE6 shrink. Parts separately if wanted: `[Tuning] BPO_Mask = 1` (Q only) / `6` (L+V).
Re-run the offline proof / timing (no game needed, ~3 min CPU): `dotnet fsi re\scripts_logic\bounds_offline.fsx`.
