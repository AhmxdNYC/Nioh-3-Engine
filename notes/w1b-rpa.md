# W1B + RPA: two small same-result Ally main-thread savings (2026-10-09, branch w1b, ASI 1.7.42)

From `ally-drawprep-2.md` s.3-4 (whats-left #3 bundle). Built, proven offline, not run in the game. Both live switches in
drawprep.cpp (same installer / trampoline page as DQ, P1E7); `[Patches] W1B`, `[Patches] RPA`: 0 = off, 1 = on,
2 = installed + off (menu rows "Glyph clear plain stores (W1B)", "Range copy render params (RPA)"). v2.02 only.
Version note: main took 1.7.41 (boss code split) while this was built, so this branch says 1.7.42.

## W1B: glyph padding clear 61B1C (site 61BAC, 22 B, resume 61BC2)
- Design: ui-mainthread.md rank 1 (2026-10-06). Per glyph row the function clears pad (= 1, atlas `+7C`) dwords in the
  write-combined atlas staging. The game did `and dword [rbx+rax*4],0` (a WC read); W1 replaced it in place with
  `rep stosd` (count 1), whose start-up is where 97-100 % of 61B1C's samples sit (61BBE).
- Cave: switch on = the game's own loop with `and [m],0` -> `mov [m],0`: same addresses, same order, same final eax / ecx /
  flags as the original, rdi untouched (W1 clobbered it). Switch off = what the site held at install: W1's bytes
  (`dp_w1b_w1 = 1`, W1 = 1 is the default and is applied first) or the original loop. Site -> E9 + int3 x17 (never run).
- Deps: 61B1C..61BAB and 61BC2..61C96 FNV-1a (patches.json deps W1B). Branch scan: only the loop's own jb (61BC0, inside
  the site) targets it; 61B39 is a byte inside `mov edi,[rcx+7Ch]`. Site supersedes W1 (test_patterns allows exactly that).
- Offline (this PC, WC memory, per row incl. harness overhead): original 265 ns, W1 120 ns, **W1B 25 ns**.
  Ally target 61B1C 0.23 (0.32) ms/frame -> **est. 0.15-0.25 ms/frame**.

## RPA: RenderParamsAssign 247950 (site 247950, 5 B `mov [rsp+8],rbx`, resume 247955)
- 0x3D38-byte operator=: 4785 movs (29.6 KB) + 2477D4 (header block) + 2478A0 x2 (0x780-byte blocks, movaps on src).
- `--derive` runs the dump's 2477D4..24ECE9 natively on random src/dst (6 trials) and classifies every dst byte as copied
  from the same src offset or untouched (anything else fails): **21 ranges, 15551 bytes** (`tests\rpa_ranges.txt`).
  Gaps (0..F, padding after bools) are never read or written. `tools\gen_rpa_copy.py` emits the cave copy: 64-byte loops
  for long ranges, 16-byte moves with an overlapping last move inside the range, 8/4/2/1-byte pairs for short ones.
  Uses rax, r8, xmm0, xmm1 (volatile), returns rax = dst.
- Deps: 2477D4..24ECE9 FNV-1a 0xFAC0A5A1 (the code the ranges came from); install refuses on any change (another mod,
  an FnProbe hook). Assumption: dst and src never partially overlap (separate objects; self-assign is tested and exact).
- Offline: 428-463 -> 217-272 ns per call with the code hot; in the game the 29.6 KB also evicts L1i per call.
  Ally target 0.11-0.15 ms/frame -> **est. 0.07-0.1 ms/frame**.

## S2F (3C4B74 memo): not built
The two hash scans read list entries and a float through a pointer each call; a memo keyed on list ptr + count is only
equal to the original if the list content does not change within a frame. That is a runtime invariant the offline
harness cannot prove, so it does not meet "provably identical" (~0.05 ms anyway).

## Proof and checks
- `py -I mod\Nioh3EngineFix\tests\run_w1b_rpa_harness.py` (dump bytes vs the real drawprep.asm caves):
  W1B 200k rows x (original, W1, on, off/W1, off/original), pad 1..64: row bytes + every GPR / xmm0-15 identical.
  RPA 300k calls x (original, off, on), dst alignment 0..15, self-assign, fills: all bytes in +-0x400 guard margins,
  rax, non-volatile GPRs + xmm6-15 identical. **ALL PASSED**; a mutated cave fails (checked).
- test_patterns **ALL 285 SITES OK** (+ drawprep.cpp W1B / RPA byte, FNV, resume, branch checks); drawprep harness ALL
  PASSED; menu_map selftest OK; build OK.

## Test line (Ally, with the #3 bundle)
`[Patches] W1B = 2, RPA = 2`, toggle the two rows live (ABBA). Expect log `W1B: glyph padding clear 61BAC -> plain stores
(switch ...; off = W1 rep stosd)` and `RPA: RenderParamsAssign 247950 -> 21 range copies`. Main-thread ms (SP 61B1C,
247950 rows) is the metric; fps only moves when the main thread, not the GPU, is the limit.
