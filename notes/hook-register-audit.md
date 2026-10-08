# Hook register-safety audit (ASI 1.7.23, 2026-10-08)

Why: the game is an LTCG build. A caller may keep a volatile register (rax/rcx/rdx/r8-r11, xmm0-5, flags) live
across a direct call when the compiler knows the callee never writes it. A hook that replaces such a call, or detours
the callee's entry into C++, then corrupts the caller (1.7.17 LoadIsLoadEnd r11 crash, fixed 1.7.19).

Method (static, `re\Nioh3.dump.exe`, no game, no Ghidra):
- `re\scripts_re\hook_liveness.py` - capstone liveness: first read-before-write of each register on all paths after
  a hook's return / resume address; direct callees transparent except rax/xmm0, import/indirect calls clobber all
  volatiles (ABI); `clob` = registers a function can write (CFG walk + E8 callees). A register is a real dependency
  only if it is live after the call AND not in the original callee's clobber set. `--selftest` re-finds the r11 of
  1.7.17 (LG6 -> 3B3503 `mov rcx,r11`).
- `re\scripts_re\hook_entry_audit.py` - entry detours: registers the thunk clobbers that the original never writes,
  then every direct caller's liveness for them; also xmm0-3 read at entry (float args).
- Inputs: `hook_sites_e8.txt` (44 call sites), `hook_sites_mid.txt` (15 mid-function caves), `hook_sites_cas.txt`.
- Known false positives (checked by hand): varargs home spill (`mov [rax+20h],r9` in 18CBDAC), partial writes
  (`xor r8b,r8b`), regs already clobbered by an earlier indirect call (LWs r9), arg chains ending at imports.

Rule of thumb for new hooks: a C++ hook reached from an E8 site or a function entry must save every volatile the
original does not write - simplest is rcx, rdx, r8-r11 and xmm0-5 (bosslog thunk below). FF 15 / vtable / COM sites
are plain Win64 ABI and need nothing beyond the ABI.

## Results

| Hook (group) | Site / entry | Kind | Verdict |
|---|---|---|---|
| BL1-30, RM1-29 (bosslog.cpp entry thunk) | 59 entries | C++ entry detour | **RISK -> fixed**: thunk saved rcx/rdx/r8-r11 only. MissionRecordGet 377B38 (131 callers, 17 flagged), MissionClearedGetSlot 702B64, MissionClearedGet 37AFC0, MissionUnlockedGet 909218 never write xmm; callers keep xmm0-5 live across them (915F0C: `divss xmm1,xmm2` 915F45, `divss xmm0,xmm2` 915F50; E23309, 1C0BD32, ...). C++ hooks may clobber xmm0-5. Thunk now also saves xmm0-5 (page 16 KB, 176 B/thunk). Only with BossLog=1 / RematchLog=1 (default 0). Other 55 entries write all of rax/xmm0-5/flags themselves; no float args. |
| LG1-9 (loadstats.cpp NewSaveThunk) | 3B3443..3B3559 | C++ call-site | fixed 1.7.19 (r11 at LG6/LG7 -> 3B3503). Re-checked: no other volatile (incl. xmm0-5) live+preserved at any gate; gates take 4 args, no stack/xmm args. |
| INF x8 (loadstats kInflate) | 258A5A 4201F5 420605 324BD61 324C409 A099DD 324C372 324C4A1 | C++ call-site (plain jmp thunk) | safe: nothing live+preserved; 5th arg read from the caller's slot and forwarded (5-arg signature). |
| PSO x7 (loadstats kPso) | 11970A 2EFE47C 119AE1 119D86 58BF92 58C07A 2EFE1F2 | C++ call-site | safe: nothing live+preserved; no stack or xmm args. |
| kMgrSleep, kRead (loadstats), RP1-17 (readprobe) | FF 15 Sleep/ReadFile/ReadFileEx/CreateEventA/CloseHandle/WFMO | import call | safe (ABI); ReadFile(Ex) 5th arg forwarded (readprobe thunk `[rsp+90h]` = entry+28h, checked). |
| RP18 cache (readprobe NewCacheThunk) | 198357C -> 2F77A6C | asm call-site | safe: calls the original, only rax/flags change, 5th arg forwarded. |
| AC1 (autocont) | 1CECAED -> 283950 | asm call-site | safe: callers keep rdx/r8/r9 (283950 preserves them); thunk writes al/flags only. |
| P8a-d (caveTier) | 20E77B 5AF526 5AF682 5AF69F | asm call-site | safe: eax only (P8b reads r8-r11 afterwards, P8a r9/xmm1/xmm2 - all kept). |
| P7 caveMenu | 43312B | asm call-site | safe: rcx live after (GetDelta60 keeps it); cave calls the original, adds xmm1/flags (dead). |
| L1 caveL1 | 19DF57E | asm call-site | safe: nothing live+preserved; tail path rebuilds rcx/rdx for 916EBC. |
| F1 fgWait | 3777A40 | C++ call-site | safe: nothing live+preserved; 1 arg. |
| PP, E1B, SPg/SPu, IK, IKc, TSw, TSb, EDk, EDj | 252677 160B83 11970A 62D7E7 5AB6CF 5ACAAA 4A52D4 4A535E 3D6F46 3D6F8B | asm call-site | safe: nothing live+preserved after any; callees' arg regs that the caves change (IKc rcx -> 86C018 only spills it; EDk rdx -> 2F6680 unused; E1B r9 is not an arg, 4th = xmm3) are unused. |
| DL caveDL | D83F4 -> D8740 | asm call-site | safe: caller relies on r11 (D840A) and rdx (2nd d8740 call D841F) being kept; stock path jumps to the original, skip path touches nothing. |
| P1 P2 P4 P9 WK TSl CP SPF M1 PT1b/c/d GL1a/b LWs W6W | mid-function | asm cave | safe: written regs dead at resume (tool, `hook_sites_mid.txt`). P2: xmm2 is read at 5AD702 but is meant to carry the scaled value (same as the stored [rdi+624h]) - design, not a leak. LWs: rsp 16-aligned at 1343D35 (push rbx; sub 20h). |
| MQ (88780), AABB (6EA50), B (3BC17 tail of 3BC0C), E1A (7E82B4) | entry / tail | asm cave | safe: cave clobbers are a subset of the original's (88780: rax r11 xmm0-5; 6EA50: rax rcx rdx r8 xmm0-5; 3BC0C: rax rcx rdx r8-r11; 7E82B4: rdx r9 + flags). |
| FPC (vptr 38FB9D8) | vtable slot | indirect | safe (ABI); fpcMiss frame aligned, MaterialFindParam takes rcx/rdx only. |
| W3 X1 W6 GF1/2 WL DS TSp WS0-5 WSs PT1a/r LWa-c | FF 15 / FF 90 | import / vtable | safe (ABI); W3 hookPeek has the 5-arg signature; asm caves keep rsp alignment. |
| RWb (loadonly) | 1A3146A | asm cave | safe: skip path drops `call 48B9C; Sleep(0)`, flags dead at 1A31486. |
| W1 W2 P3 P6 R1 P12 WC AS0 LC S1 M2 J1 LK | in place | byte patch | no hook; CAS->load rewrites checked for flags: dead or re-set (LKM5 `test`, LKJ4 next cmpxchg). |
| GIM (1.7.29 gimerge.cpp) | D3D12CreateDevice IAT, device vtbl CreateGfx/CreateCs/CreateCbv, Resource Map/Unmap, list calls via gpuopt thunks | IAT / COM vtable | safe (ABI). Thunk change: `test eax,eax; jnz -> ret` after the slow call - rax is volatile and not an argument; skip only for Dispatch (void); rsp at entry value. |
| GP, D3D12/DXGI vtables (gpuopt, gpuprof, osd, tilestats, psoswap), XInput IAT (menu) | COM / API | skipped (ABI). |

## For the loadstats.cpp owner (not edited here)
- No RISK left in loadstats.cpp. Optional hardening: make `NewSaveThunk` also save xmm0-5 (same layout as the bosslog
  thunk) so a future gate whose caller keeps an xmm live is covered; today none does.
