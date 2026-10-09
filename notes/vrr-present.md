# VrrPresent + present mode log (ASI 1.7.44, branch `vrrpresent`)

Status 2026-10-09: built, offline-verified (test_patterns, build). **Not run in the game yet.** Root cause and numbers:
`notes\mod-vs-stock-2026-10-09.md`. Tags: **M** measured / byte-verified, **E** estimate.

## Plain summary

- On some launches (with or without the mod or OptiScaler) the game hands every frame to Windows as "wait for the next
  refresh" (SyncInterval 1). With FreeSync that turns the fast path (independent flip) into the slow one (composed flip):
  about **+7 ms display latency** and **8-20 % of the frames the PC makes never reach the screen**. The stock run that felt
  better simply did not hit that state.
- **`[Latency] VrrPresent = 1`** makes the mod hand every frame over the way the good stock run did: SyncInterval 0 with
  tearing allowed, so FreeSync shows each frame when it is ready. Only the two arguments of the game's own Present call
  change. No timing change, no image change, FG keeps its own pacing. Default **0** (off) until the PC test says KEEP.
- **`-1` = auto**: on only while the game's own VSync setting is OFF (then you already chose tearing-allowed frames and a
  stuck 1 is a bug). With VSync ON in the game, only `1` acts, because then it is your choice to rely on FreeSync.
- **`[Debug] PresentModeLog = 1`** (default, measurement only) writes one log line each time the present mode changes and
  says which part of the game set it. That answers "who flips it" on the next launch that shows the bug.
- Windows only. Under Proton (Ally) the setting is ignored (vkd3d-proton/gamescope treat it differently; see below).
- Tearing: none while the fps stays inside the display's VRR range (user PC: 120 cap, FG output ~150-175 on a 48-240 Hz
  panel). Above the panel's refresh frames would tear; the mod logs a warning when it sees that.

## 1. The engine's SyncInterval (static RE, v2.02, byte-verified) [M]

`GfxDevice` = `[Nioh3+45BA588]`. `GfxDevicePresent 3D62E4` uses `SyncInterval = (params bit 0) ? 0 : [d+424Ch]`
(3D64ED..3D6501), then either presents inline (`[d+3F80h] == 0`) or hands `r13d` to the present thread at `[d+7738h]`
(3D6545). The only stores to `[reg+424Ch]` in the code:

| writer | site | what | hooked |
|---|---|---|---|
| GfxDevice ctor | `2EDFBBD` `mov dword [rdi+424Ch],1` | default 1 (the ~20 s of SI 1 at every boot) | no: inferred (first look / new device) |
| DisplaySettingsWatcher 1350210 | `13503A3` `mov [rcx+424Ch],eax` | VSync slot `45BAF3C`, only when the slot differs from its cache `w+1C0h`. **It updates the cache before its guard** (`d+2538h != 0 && d+2544h >= 0` skips the store) **and never retries**: a VSync change seen while the guard is set is lost and the old value stays | PMw |
| setter 2EEBB50 | `2EEBB68` `mov [rcx+424Ch],edx` | `max(edx,0)` unless the same guard (1A7C6EC) is set. **No static reference** (no call, jmp, lea, pointer or RVA in the image except .pdata): reached through a computed pointer | PMs + caller |

A fourth source is per frame, not a store: GfxDevicePresent's params bit 0 forces SyncInterval 0 for that call. The FSR FG
presenter (`37772FC`) presents `(vsync byte, tearing && !vsync && !exclusive ? ALLOW_TEARING : 0)`, i.e. it forwards what
the engine asked. Which source produced the SI 1 seen by PresentMon is what fix A logs.

## 2. What 1.7.44 does

**A - `[Debug] PresentModeLog` (0/1/2, default 1).** In osd.cpp's Present / Present1 hook (vtable of the real dxgi.dll
swapchain class: below Streamline's interposer, OptiScaler's dxgi.dll and the game's static FFX presenter, so it sees what
PresentMon sees). Per Present caller (return address, module+RVA, thread role FSR presenter / game): one line at first
sight and on each change of (SyncInterval, flags, what was sent). Per engine change of `[d+424Ch]`: one line naming the
writer: watcher (VSync slot), setter + its caller (module+RVA), ctor default (first look / new GfxDevice), or "not probed".
Also: VSync setting changes, and "engine SyncInterval differs from the VSync setting for > 2 s" (the lost-write case).
Lines are capped at 200 per session. 2 = plus a 30 s summary per caller (presents/s, % SI > 0, % rewritten) and the count
of writes that did not change the value. Cost: one SRW lock + three memory reads per Present; the caves run only when the
VSync setting changes. Group **PM** (2 call sites, 6 deps): each cave does the original store first, then records value /
device / sequence (and the setter's caller at `[rsp+38h]`), flags untouched (mov/lea only), all registers preserved.
0 = no log and no PM sites.

**B - `[Latency] VrrPresent` (-1/0/1, default 0).** In the same hook, per Present that is not `DXGI_PRESENT_TEST`:

- swapchain check every 2 s: `GetDesc().Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` (game chain 0x842 has it) and
  `GetFullscreenState() == FALSE`; display refresh from `GetContainingOutput` + `EnumDisplaySettings` (log only);
- flags may only contain ALLOW_TEARING / RESTART / DO_NOT_WAIT, otherwise untouched;
- when wanted (1, or -1 and VSync slot == OFF) and `SyncInterval != 0 || !(flags & ALLOW_TEARING)`: send
  `(0, flags | DXGI_PRESENT_ALLOW_TEARING)`;
- if DXGI answers `DXGI_ERROR_INVALID_CALL` (nothing was presented), the game's original call is made at once and that
  swapchain is excluded for the session (logged);
- logs `VrrPresent: swapchain ... flags, windowed, display N Hz: allowed / left alone` and a one-time warning when a
  rewritten chain presents above the display refresh.

Not touched: the engine's +424Ch value, the VSync slot (so the game's own VSync-dependent logic, e.g. the dynamic
resolution target halving at 5AF7F8, behaves as before), the FG presenter's timers / waits, MaxFrameLatency.

**Why auto = "VSync OFF" only.** The only signal that tearing-allowed presents are wanted is the user's VSync OFF. DXGI
cannot tell whether VRR is actually on (ALLOW_TEARING support only says the OS could do it), so with VSync ON forcing
SI 0 could tear on a panel without FreeSync. With VSync OFF the user already accepted that, and the stuck 1 is the bug.

**OptiScaler.** Present or not makes no difference: its dxgi.dll proxy calls down into the real swapchain we hook. With
`VrrPresent = 1` its `[V-Sync] ForceVsync` would be overridden below it (the user's OptiScaler.ini has `OverrideVsync=false`).
Its own FG (`FGOutput != nofg`) would be treated like the game's presenter (arguments only).

**Proton / vkd3d-proton (Ally): not applied.** There the hooked dxgi is vkd3d-proton's: SyncInterval 0 selects a
MAILBOX/IMMEDIATE Vulkan present mode instead of FIFO (which one depends on ALLOW_TEARING and the driver [E]), and
gamescope then composites and paces anyway; the Ally's problem is GPU time, not the flip model, and gamescope's tearing option is a
system setting (out of scope). Unknown effect = no change: `VrrPresent` != 0 under Wine logs "not applied". The log (A)
still runs there.

## 3. PC test (launch A/B, user settings, FG on)

Prereqs: run8 done (`%TEMP%\nioh3_measuring.lock` gone), the Ally does not hold the game. User settings unchanged:
2560x1440, REN_RAT 75, cap 120, **VSync ON**, XeSS->FSR4 (OptiScaler as installed), **FG = FSR**, HDR; FreeSync on.

1. Build `D:\nioh3-wt\vrrpresent\mod\Nioh3EngineFix\build.bat`; copy its `bin\Nioh3EngineFix.asi` (1.7.44) into
   `C:\Users\Ahmad Hamza\nioh3-engine\mod\Nioh3EngineFix\bin\` (the session kit installs from there; or merge the branch).
2. Double-click **Nioh3 Session START** (backs up saves + ini/asi, starts PresentMon `--v2_metrics --track_pc_latency`,
   follows relaunches). In the game folder `Nioh3EngineFix.ini` set `[Debug] PresentModeLog = 2` and
   `[Latency] VrrPresent = 0`.
3. Four launches in the order **A B B A** (A = `VrrPresent = 0`, B = `VrrPresent = 1`; edit the ini only while the game is
   closed). Each launch: continue the same save, play the same area ~5 min (camera pans, combat), quit to desktop.
   During B watch a fast camera pan for tear lines.
4. **Nioh3 Session STOP**. Then:
   `py -I tools\testmatrix\vrrp_report.py captures\playsession\<session>\presentmon.csv --arms A,B,B,A --skip 120 --sec 240`
   Per launch: presented / displayed fps, PresentMode, SI 0 %, tearing %, DisplayLatency p50/p95, AllInputToPhotonLatency p50.
5. Per launch log (`captures\playsession\<session>\` mod logs): `grep "PresentMode\|VrrPresent"`. B must show
   `-> sent SI 0 flags 0x200 (VrrPresent)`; A shows which writer left the engine at 1 (or that it ran SI 0).

**Decision:** KEEP (then default `-1` or `1` on Windows) if DisplayLatency p50 drops **>= 3 ms** and displayed fps rises,
with no tearing seen inside the VRR range. The stuck SI 1 does not happen on every launch: the report excludes A launches
that already ran SI 0 and says INCONCLUSIVE when no A launch had SI 1 (rerun A). Expected from the measured runs: B =
`Hardware Composed: Independent Flip` >= 95 %, DisplayLatency p50 ~7.6 vs ~15 ms, displayed ~98-100 % vs ~80-92 %. [E]

Matrix: `tools\testmatrix\matrix.json` item `VrrPresent` (pc, launch, grp 2, metric latency); alloff gets `VrrPresent = 0`.

## 4. Offline checks [M]

- `tests\test_patterns.py`: PMw / PMs unique patterns, RVAs, original bytes, 6 deps; caves repeat the exact store (same
  base and source register); no branch into either site; watcher bytes after the site are mov/lea/mov/cmp (flags rewritten
  before use); setter frame `sub rsp,28h ... add rsp,28h; ret`; exactly three `[reg+424Ch]` stores in the code (13503A3,
  2EDFBBD, 2EEBB68); pmode.cpp offsets; GfxDevicePresent hand-off bytes (`mov [rdi+7738h],r13d`, `cmp [rdi+3F80h],sil`).
- `tools\testmatrix\vrrp_report.py --selftest`: SELFTEST OK (KEEP / DROP / arm-count mismatch).
