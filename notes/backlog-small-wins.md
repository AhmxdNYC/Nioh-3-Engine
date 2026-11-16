# Backlog: small wins (not prioritised)

User rule (2026-10-09 16:00): gains of about 1 fps or less can't be felt, so they are listed here and NOT prioritised.
Priority goes to anything with >= ~5 % fps potential, or a feelable smoothness / latency / loading improvement.
Small items run only when a machine would otherwise sit idle.

| item | where | measured | status |
|---|---|---|---|
| VCLOUD pfu/pfp cheaper cloud shader | Ally | +1.1 % fps (#803), picture PASS (#829) | keep candidate; pc2 neutral (async) |
| lodu MshLod#2 guarded fast path | Ally / pc2 | Ally 0.0 (#943), picture PASS | neutral, not shipped |
| mpc / mshlod culling shaders | pc2 | -0.01 ms (#906) | neutral, dropped |
| ESM1 ESM 2048->1024 | pc2 | 0.00 (#694) | dropped |
| POSTEFFECT_RESOLUTION STANDARD | pc2 | neutral (#736) | dropped |
| VSMA shadow-tile cap | PC/pc2/Ally | 0 tiles to cap (#853, #870) | parked (only big-camera-move scenes) |
| update-rate knobs LPVR/CLDR/SKYR/FOGT | PC/pc2 | neutral, FOGT visible | dropped |
| SkipDebugString | Ally | OutputDebugStringA 91/s -> 0.1/s (#667) | keep (cleanup, no fps) |
| TR timer resolution | PC | neutral (#704) | keep as safety, no perf |
| barrier batching retest (GPU-bound) | pc2 | <= 0.3 ms est. | backlog |
| MPCS meshlet tail skip | pc2 | est. 0.05-0.1 ms | backlog |
| PL boot pipeline library | PC | seconds at boot, est. | backlog |
| KL_Div (back-end segment hand-off every 16 draws, branch renderer-backend) | pc2 / Ally | pc2 +1.2..+1.3 % (SE 0.02-0.04, twice); Ally KLA1 #1381 inconclusive (scene got heavier mid-run, fps 29-37 swing; KL 4 -5.9 % +-1.5, KL 2 +1.0 % +-3.8) | backlog, default off; Ally no-harm needs a stable spot + more pairs before it could ship |
| FO frame overlap (branch frame-overlap, fo8) | pc2 / PC | pc2 +0.4 % (SE 0.02), PC +0.5 %; stable (17/17 self-test, 47k frames) | NO-GO as a feature (notes/renderer/frame-overlap.md 7.3), kept default off as infrastructure |
| GHDM | Ally / PC | fastab: +0.5 % Ally USER preset (#1178), +0.4 % PC no FG (#1063); old multi-launch +7.5 % Ally MAX / +11 % PC FG not re-verified | shipped; small win (re-check MAX + FG with fastab) |

Priority list (feelable): PCIe/no-ReBAR fix (CbvMirror v2), display latency (VrrPresent), weak-CPU gains,
stutter (PJH, needs a boss-fight route), cost-vs-looks only for items >= 1 ms.

- 2026-11-16 performance-pc/0 | CANDIDATE (CPU, same picture): ECL submit thread does one ExecuteCommandLists(1) + one fence Signal per closed list (EclSubmitAndSignal 0x4F90DC, deepseek-flash-re-552 #108478). If lists/frame is high, batching N lists into one ECL + one Signal cuts driver CPU. First step: runtime count ECL calls/frame on Main PC (hook vtable+0x30) when the game is free; skip if under ~10/frame. Ring full = recorder stall at EclRingPushItem 0x86F828 (INFINITE wait).
  - add-on (#108516, deepseek-flash-re-553): descriptor ring exhaustion cuts the command list and bumps ctx+0x2494/+0x2498/+0x249c (CBV/SRV/UAV). Log those per frame in the same run as the ECL count: cuts = extra lists = extra submits.
  - lever (#108538, re-556): ring exhaustion = render thread Sleep(0) spin until GPU fence (0x945610). UAV ring is smallest (768 tables/wrap) -> log ctx+0x249C first. If it bumps per frame/hitch: raise the UAV region size (params+0x74, floor 16; heap total 0xAD000 far under the D3D12 1M cap, re-550). Same picture, pure stall removal.
  - caveat (#108615, deepseek): +0x2494/+0x2498/+0x249C also count cheap FALSE stalls (completed-fence cache ctx+0x2510 is refreshed only on wait paths, so it under-reads). A bump alone is not proof of GPU backpressure; pair each counter read with time spent in the stall handler 0x945610 / FenceValueReachedOrPoll 0x4920A0. Sampler ring has its own counter +0x24A0 (bumped in 0x1188AC): log it too.
  - add-on (#108659, deepseek): immediate-draw upload rings (ctx+0x2390 verts / ctx+0x23D8 indices, max(2, GfxDesc+0x10C) segments) block the render thread in SynUploadRingAcquireBlocking 0x45B470 (flush, submit, Sleep(0) fence spin). Same run: count/time 0x45B470 entries and read ring byte size at runtime (ring+0x08; lost in decompile). If it blocks per frame: raise segment count GfxDesc+0x10C or ring size. Same picture.
