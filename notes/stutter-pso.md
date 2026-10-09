# Stutter: pc2 hitch regression + PSO-creation path, and the PJH hand-over fix (2026-10-09)

Inputs: pc2 play sessions `captures\pc2\raw\20261009-0020-session` (stock) and `20261009-0036-session` (mod rc 1.7.44 +
GHDM/fogpar loose shaders), bridge #441/#460 (file #124), PC session `captures\playsession\20261008-2059-session`, the
run8b PJ tour log `captures\tm\abba2\run8b_P\J\...\logs\run01.log`, Ghidra (`re\decomp.cmd`), `re\scripts_stutter\devcalls.fsx`.
Earlier work: `notes\stutter.md` 3.1 / 6 / 6.1 (SPR probe, PJ journal + prewarm).

## 1. What the pc2 "94 vs 39 frames > 100 ms" really is

PresentMon CSV times are UTC (+7 h to the log). Phases from the mod log's stop flags (58A = level-load window). Class:
GPU-bound when GPUBusy >= 0.6 x FrameTime.

| frames > 100 ms | mod (94) | stock (39, no log: loads not separable) |
|---|---|---|
| boot / title, first 40 s | 10 | 10 |
| inside level loads (58A), GPU-bound end-of-load burst | **56** | (load-end frames 55-80 ms, 10 > 100 ms) |
| inside level loads, CPU stall at load start | 24 | (in the 19 CPU-side below) |
| gameplay, CPU-side | **4** | 19 (incl. the boss-kill cluster 00:31:10-31:48) |

- **80 of the mod's 94 are loading-screen frames.** Each long load ends with ~3-6 s of 100-130 ms frames at 100 % GPU
  (gpuSum ~1000 ms/s, 8-17 fps); stock's matching bursts are 1-2 s of 55-80 ms frames at 17-23 fps (also 100 % GPU). Same
  GPU saturation, about twice the GPU work per presented frame in the mod. Cause not identified (Ally phase C,
  `stutter.md` 3.2: candidate = stage-entry GPU work; next step GPUProf across the load end, mod vs stock). The player is on
  the loading screen: not a gameplay stutter, but it drags the 0.1 % low.
- The compare report's "longest sustained segment" (mod 23 frames > 100 ms vs stock 0) is a segmentation artifact: frames
  < 250 ms are not stalls, so the mod's 459 s segment starts inside the mission load's end burst (00:40:31-35).
- **Gameplay hitches > 100 ms in the mod run: 4**, all first-use PSO compiles: 3 at the boss kill (282 / 102 / 448 ms at
  00:47:53-58; SPR window: +36 runtime gfx creates, 24 >= 1 ms, worst 206 ms) and 1 after the last load (216 ms; +7 creates,
  avg 22 ms). Stock has the same boss-kill cluster (frames of 105-235 ms at 00:31:10-31:25).
- The mod adds no runtime compiles: 218 runtime gfx creates per session (PC 247), and the 7-minute mission had **0 creates
  >= 1 ms** (only 0-8 per 30 s at ~0.2 ms). The loose shaders (GHDM 0x0104840c, fogpar) are fixed bytes: the driver
  compiles their PSOs once (most likely in the boot list's "Compiling shaders N/N" screen on the first launch with the files)
  and caches them; nothing recompiles per launch. pc2's stock run was also the first launch at that config.xml, so both runs
  had cold-ish caches.
- Also in the mod run only: FnProbe (14 hooked functions, up to 4.7 M hits / 30 s), PadLatency, CpuStats, SPR. #442's
  minimal-key warm rerun is still worth doing.

Verdict: the "mod made stutter worse" reading is wrong for gameplay. The gameplay hitch class that players notice on
both builds is first-sight PSO compilation (boss kill, new effects), and the loading-screen end burst is a separate, open
item.

## 2. RE: how Nioh 3 creates PSOs

- **Synchronous, on first use, on the render (SynergisticDispatcher) thread, under the PSO-cache lock.** Per draw,
  SynFlushGfxStateForDraw 1180D0 calls the functor SynFlushPsoAndBindings 118604 (vtable 3C332A8); when the PSO is dirty
  (`c+B24h & 20h`) it calls PsoGetOrCreateGfx 119670: hash the 0x80-byte key, `EnterCriticalSection([c+818h]+10h)`, look up
  `c+708h`, on a miss **CreateGfxPso 9290D8 inside the lock**, insert (9B05BC). The main thread spins at frame end until the
  dispatcher drains (418F40): a create of X ms = a frame X ms longer. Decompiled again 2026-10-09.
- **No pipeline library, no CachedPSO, no own disk cache.** devcalls.fsx finds no `CreatePipelineLibrary` (vtbl+160h) call on
  the device pointers ([g+4268h/4270h/4278h]); CreateGfxPso zero-fills its 0x288-byte desc and never sets CachedPSO; the
  stream path (mesh shaders) uses ID3D12Device2::CreatePipelineState (vtbl+178h). The game relies on the driver's shader cache.
- **The game precompiles its whole PSO list at every boot** (PreloadPSOMode, resource 0x17A4; pc2 showed
  "Compiling shaders 17,639/17,639"; the PC run8b log counted 17,145 creates in the first ~15 s). Stage loads create 0
  preload PSOs (LoadStats). Every gameplay create is therefore a **runtime miss outside the shipped lists**.
- **A NULL PSO already skips the draw.** 119670 returns NULL when its cache is full; 118604/118758 then return 0 with the dirty
  bit kept, and every draw caller (1167D4 DrawInstanced, 6EB908 indirect, 2EFD0A0 DispatchMesh, ...) skips its draw. And
  PsoPreloadGfxCreate 2EFE440(ctx, key, force) is a thread-safe "create outside the lock + insert or release".

Options:
| option | feasible? | verdict |
|---|---|---|
| (a) persist an ID3D12PipelineLibrary in our device hook | yes (Device1 vtbl, keyed by the PJ desc key) | not built: on Windows it duplicates the AMD cache; the hitch also hits warm-cache machines (PC user: 25 creates >= 1 ms, worst 153 ms in a replayed mission), so the cost is not only "cold disk cache" |
| (b) precompile all PSOs at boot from the game's own lists | the game already does it | the hitching PSOs are by definition not in the lists; the only source is a journal of what the game built before -> PJ |
| (c) async create + skip the draw | yes: hook 119670's miss, queue 2EFE440 on a worker, return NULL | not built: changes the picture (objects / whole full-screen passes missing while compiling); keep as a fallback for first-ever sights |
| **PJH: hand over prewarmed PSOs** | **built (1.7.45)** | zero-cost create for every PSO the game built in an earlier session, independent of the driver cache state, same picture |

## 3. Bug found in PJ (1.7.31): the journal never held the PSOs that hitch

run8b J log: `PJ: journal full (63.9 MB, 14399 PSOs)` 13 s after launch, `engine creates 17145`, `dropped 1358 -> 1454` during
the tour. The boot list (made again in every launch, so useless to prewarm) filled the 64 MB / 16,384-record cap before the
title; every runtime first-use PSO after it was dropped. PJ as built could not have removed any gameplay hitch.

Fix (journal v2): the boot gate that already started the prewarm (no engine create for PJ_QuietMs, title idle) now also
starts journaling. Creates before it are only marked "created this session"; creates after it are journaled. A v1 file is
replaced automatically (header version 2).

## 4. PJH - PSO hand-over (ASI 1.7.45, `[Patches] PJH = 0`, live menu row "Pipelines ready before first use (PJH)")

- PJH = 1 on, 2 installed + switched off (A/B live from the menu), 0 off (default). PJH != 0 implies PJ = 1.
  `PJ_KeepMax = 4096` caps the held PSOs.
- The engine's PSO creates are serialized **before** the call (same canonical key as the journal). After the boot gate, if
  the prewarm holds a PSO with that key, the hook returns it (QueryInterface for the caller's riid, our reference dropped)
  and skips the driver call. Otherwise the engine's own call runs unchanged and the desc is journaled.
- The prewarm (title screen, one lowest-priority thread, pauses in level loads) keeps each PSO it creates instead of
  releasing it, built on the **engine's own root-signature objects** (PJH holds one reference per root-signature blob, 17 in
  the game). A PSO built on a prewarm-created root signature, or one the engine already created, is released.
- Same picture: same device method (through the hooks below PJ, e.g. psoswap), same desc bytes, same shader bytes, same root
  signature pointer the engine would have passed. Only the creation time moves.
- Log: `PJ: boot PSO list done: N engine creates`, `prewarm done ...`, and in `stats/s PJ:` `| PJH on: handed N, runtime
  creates without one M, waiting K, not kept R`. SPR's runtime-create count stays the same (the engine still calls
  CreateGfxPso); its `>= 1 ms` count and max should collapse.
- Limits: helps from the 2nd launch on (the journal must have seen the PSO once); a first-ever sight still compiles.
  Memory = the held PSOs not yet requested (a few MB for ~250 per session).
- Offline: `py -I mod\Nioh3EngineFix\tests\run_pj_harness.py` -> ALL PJ CHECKS OK (real WARP device; new launches `hand`:
  3 kept, 3 handed, no create reaches a spy below PJ, each handed object passes GetDevice + SetPipelineState + Close;
  `handoff`: kept, not handed; v2: the boot-list PSO is never journaled). `tests\test_patterns.py`: ALL 231 SITES OK.

## 5. pc2 A/B (queued on the bridge)

ASI 1.7.45 = bridge file #127. Four hands-free tours (`tools\tour\pc2_run_tour.ps1 -Input vpad -List tools\tour\tourlist_pj.txt
-NoProfile`), same config.xml as #441, empty `Nioh3EngineFix_pj` before J:
1. N  `Patches.PJ=0;Patches.SPR=1` (baseline, also warms the driver cache for the route)
2. J  `Patches.PJH=1;Patches.SPR=1` (cold: empty journal, records the route)
3. W  `Patches.PJH=1;Patches.SPR=1` with `-WaitPrewarm` (warm: prewarm at the title, hand-over on the route)
4. N2 `Patches.PJ=0;Patches.SPR=1` (bracket)

Read: `py -I tools\pj_hitch.py --out pjh.txt J=<dir> N=<dir> W=<dir> N2=<dir>` now prints per launch frames > 50 / > 100 ms and
the 0.1 % low next to SPR creates >= 1 ms / max and the PSO-attributed hitches. Keep PJH = 1 when W beats N and N2 on
creates >= 1 ms and route hitches with 0 refused, `handed` > 0, and a same-spot screenshot identical (shotdiff).

## 6. Open

- The end-of-load GPU burst (mod ~2x GPU ms per frame on pc2): GPUProf over the last 10 s of a load, mod vs stock.
- If first-ever sights still matter after PJH: option (c) as an opt-in, or ship a key-only seed list (no game bytes) so the
  prewarm can build PSOs from the game's own shader groups on a fresh install.
