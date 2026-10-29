# Nioh 3 frame timing — RE notes

Status: static analysis done + LIVE verdict in gameplay (§4b). Patch sets: v1 in §5-final (applied live, §8), v2 for all
tiers in §8, the >120 design in §9, the co-op auto-disarm in §10. Probe scripts: §7.
All addresses are **RVAs** in this build (dump image base was 0x7FF74F410000).
Scripts: `patcher\ft_x.fsx` (static disasm/xref over the dump: `dis`, `func`, `xref`, `xrefrange`,
`dispscan`, `gfield`, `ctx`, `imm`, `disp8`), `ft_pe.py`, `ft_strings.py`, read-only live probes
`ft_snap.fsx`, `ft_gobj.fsx`, `ft_rtti.fsx`, `ft_live.fsx`, `ft_findtask.fsx`, `ft_display.fsx`.
Raw disassembly dumps: `re\ft\*.txt`.

---------------------------------------------------------------------------------------------------
## 0. TL;DR

1. **The simulation step is not measured.** Each frame the game advances by `scale / fps`, where
   `fps = G+0x614`. That value is always the *target* tier (30 / 60 / 120) from `REN_FPS`, or the tier
   picked by the "FPS Dynamic Adjustment" controller. Real elapsed time is never used. [high]
2. **The engine limits frames with a QPC busy-wait.** It runs right after the frame's update
   (`0x5AD292`, slow path `0x19DF57E`). It spins on `QueryPerformanceCounter` until `QPF/fps` has
   passed since the last frame. While it waits, it steals scheduler jobs or calls `Sleep(0)`
   (`0x916EBC`). This is the "spin" CPU load the research mentioned. After the wait it stores the
   measured frame interval at `[r15+0x24]`. [high]
3. **Why there is judder between 60 and 120.** Each *update tick* advances 1/tier. The update
   thread (Scheduler0) paces itself with the QPC limiter and is decoupled from rendering by a
   single-slot packet hand-off (0x412748); the render thread draws only the newest packet. Game
   speed = ticks/s / tier: below 1.0 only when the update thread itself can't reach the tier
   (CPU-limited). When GPU-limited, ticks stay at the tier rate (PC2 stock: 118-120 ticks/s at
   37 fps), and the visible problem is a varying 0/1/2+ ticks per shown frame (judder), not slow
   motion. Locking exactly at the target gives one tick per shown frame, so it's smooth. [high]
4. **Dynamic FPS only switches between tiers.** The controller (`0x5AF4E4`) can only go
   120↔60↔30, and it decides from **CPU-only** frame time (`mon+0x74` = time from frame start to the
   end of the update, ~8 ms at 60 fps in the menu). It cannot see GPU-bound drops.
   - Lyall's Nioh3Fix "Dynamic FPS" hook (`0x5AF5D2`) sets `target = 1000 / mon+0x74`. That
     overestimates fps whenever the GPU is the bottleneck, which is probably why it's "experimental".
5. **Display finding (a setup issue on the test PC, not the engine and not a mod gain):** the test PC's Windows
   desktop was at **60 Hz**, though the monitor supports 48–240 Hz (`ft_display.fsx`: 2560x1440 @ 60 Hz; modes
   60/75/100/120/144/165/170/240). The game only offers 120 on a ≥120 Hz display, and VSync at 60 Hz caps everything.
   **Set the desktop to 240 Hz (or 120) before judging any fps fix.** The fps gained by changing this is a setup
   change you can make yourself, not something the mod does (docs/BENCHMARKS.md, section (b)).
6. **Recommended fix:** "variable-dt mode", a small code-cave patch (§5). When the target is 120, it
   replaces the fixed `fps` with `round(1 / measured_interval)`. It corrects the remaining rounding
   through the global time-scale, and it turns the 7 integer `120/fps` timers into a carry
   accumulator. Most consumers (~95%) use `fps` only as a float divisor, so they become correct with
   no further changes. Risk: medium; live checks are listed in §7.

---------------------------------------------------------------------------------------------------
## 1. Loop diagram (one rendered frame, game thread "[App]Game")

```
 ┌─ frame task vf (0x5AC664)  "frame begin"  ── G+0x7D8 := now_ns  (0x5AC6BE)
 │      dt60 = G[624]*60/G[614] -> 0x5AC9EC (early update)
 │
 ├─ frame task (0x5ACEE8) ──► 0x5AD5F8  "dt setup"
 │      G[614] := G[618]            (fps used for this frame's dt)   <── PATCH P1 (0x5AD677)
 │      G[61C] := old G[620]; G[618],G[620] := GetRenFpsCap() {30,60,120}
 │      G[624] := G[628] (time scale, 0 if paused)                   <── PATCH P2 (0x5AD6AA)
 │      G[62C] := frac(G[62C]) + G[624]*60/G[614]   (60 Hz tick accumulator)
 │      dispatch updates with dtSec=G[624]/G[614], dt60=G[624]*60/G[614]  (0x8C8E54, 0x5AB644)
 │      ctx[1E6] := max(1, G[614]/G[61C])   (render frames per logic step; 1 normally,
 │                                          2..4 when EVENT_FPS forces 30 in cutscenes)
 │      0x5AF410: CPU time = now - G[7D8] -> 10-sample ring -> avg mon+74
 │                -> 0x5AF670 hysteresis -> 0x5AF4E4 dynamic tier -> G[618]
 │
 └─ tail-jmp 0x5ACF80  "frame end"
        frameskip flag test 0x5AD0BA (profile+2C & 0x8000 -> ctx[200]|=2)  (Lyall's "Frameskip")
        per-scene updates (vtbl +40 lists)
        LIMITER 0x5AD229..0x5AD2BB (+cold 0x19DF57E):
            fps = profile fps ([r15+30]); period = QPF / fps
            while (QPC - last < period) { run a pending job or Sleep(0) }   // 0x916EBC
            [r15+0]  = now (QPC ticks);  [r15+24] = (now-last)/QPF  (MEASURED seconds)
            [r15+18] = profile[+44]  (or 1/fps, or measured — selectable by flags)
            game clock: [r13+3BFBC] + [r15+18] -> 0x5AB994
 render begin 0x4A529C: latency markers (Reflex/AntiLag/XeLL abstraction 0x6B88DC..), xellSleep,
 0x4A4F94 -> WaitForSingleObjectEx (32% of game-thread samples in cpuprof = back-pressure /
 job joins), Present via FSR3 FI proxy swapchain ("AMD FSR Presenter Thread", 0x37778B4).
```
`r15 = [[frameTask+0x60]+0x28]`; frameTask vtable = RVA **0x40358E8** (vf[1]=0x5ACEE8; verified live). (0x40EF418 is a sibling task class, not the owner.)
`r13 = [[frameTask+0x58]+0x28]`; the "frame profile" object is `r13+8` (vf+0x10 → profile, whose
`+0x2C & 0x8000` = frameskip flag, `+0x44` = dt, `+0x24 & 8` = dt-mode flag; vf+0x28 → &fps).

The simulation and render do not share one cadence: N update ticks per rendered frame (0, 1 or 2+),
each sized 1/target, newest packet wins. No interpolation layer exists. Cutscenes are the exception: EVENT_FPS can give
`ctx[1E6]`=2..4, which makes the scene run its heavy step every Nth render frame (`0x412748`: counter
`ctx[1E7]`, flags `ctx[200]` bits 1/3).

---------------------------------------------------------------------------------------------------
## 2. Key objects / globals

| RVA / field | Meaning |
|---|---|
| `0x4759910` | pointer to `G` (`g_GameMainPtr`); the frame/time manager object itself is at `0x4759090` (vtable 0x3B1F308; vf[1]=0x5AC664) |
| `G+0x610` | update-tick counter (+1 per tick at 0x233B73) |
| `G+0x614` | **fps used for dt this frame** (int) |
| `G+0x618` | target fps after dynamic controller (`0x5AF5D2` writes it) |
| `G+0x61C` / `G+0x620` | previous/raw target; `G+0x620` is forced to 30 by event code (`0x10F7DAF`) and to max(30,target/2) by `0x98F3C6` (when `[0x4757480]!=0`, FG-related?) |
| `G+0x624` | **time scale used this frame** (float; from `G+0x628`; zeroed when `G+0x58B`/`G+0x58C` pause flags) |
| `G+0x62C` | 60 Hz tick accumulator (float, fractional carry) |
| `G+0x7D8` | FPS monitor: `+0` frame-start ns, `+8..+50` CPU-ms ring (µs), `+5C` tier state 0..4, `+60` hysteresis, `+64` cooldown, `+6C` configured target, `+70` dynamic target, `+74` avg CPU ms, `+78` valid |
| `0x45BAF54` | REN_FPS index → {30,60,120} |
| `0x45BAF58` | DYNAMIC_FPS on/off |
| `0x45BAFEC` | DYNAMIC_RESOLUTION index → table `0x3C06B90` {60,30,45,60,75,90,105,120} |
| `0x3BD4080` | 60.0f |
| `0x3BE2A20` | dynamic-fps tier thresholds {0.9,0.5},{1.0,0.8},{1.1,0.8},{1.1,0.8},{1.2,0.8} (CPU load fraction) |
| `0x45BA780` | engine clock (`+0x12` hasQPC, `+0x18` QPF) |

## 3. Functions

| RVA | Role |
|---|---|
| `0x5AF604` | GetRenFpsCap() = {30,60,120}[REN_FPS] |
| `0x46594` | GetFps() = `G[614]` — **129 call sites** (100 float `x/fps`, 7× int `120/fps`, 1× int `60/fps`, 4× `fps*1000/60`, 1× `fps/2` compare) |
| `0x46484` | GetDeltaTime60() = `G[624]*60/G[614]` (~40 callers) |
| 56 direct readers of `G[614]` via the global (the 14 listed here were the first found) | `0xE50215, 0xE5ED17, 0xE704B4, 0xF1CA19, 0xF826E1, 0xF85ED9, 0x10E0537, 0x115A3D8, 0x124D029, 0x124F601, 0x1251069, 0x1253951, 0x1253ED1, 0x1272C08`. Most are float `x/fps`; integer uses at 0xE6657B / 0xE66E66 (`% (fps*3)`). About 42 were not reviewed for VRR safety |
| `0x5AC664` | frame begin (stores frame-start ns) |
| `0x5AD5F8` | dt setup (§1) |
| `0x5ACF80` | frame end: frameskip flag, **QPC limiter**, measured interval, game clock |
| `0x916EBC` | limiter wait body: try to run one job, else `Sleep(0)` |
| `0x5AF410 / 0x5AF670 / 0x5AF4E4` | CPU-time monitor / tier hysteresis / dynamic tier (120↔60↔30) |
| `0x412748` | per-scene step gate: runs heavy step every `ctx[1E6]` frames |
| `0x5AF7F8` | dynamic-resolution controller (target fps × 0.5 when FG is on) |
| `0x4A529C` | render-frame begin: latency markers, `xellSleep`, frame ids `0x45BAA94` / `0x45C3F48` |
| `0x48AD4` | steady_clock::now (ns); `0x205530` raw QPC; `0xB60E98/0xB60EB4` QPC/QPF wrappers |

Integer `120/fps` sites (`8D 42 78 41 F7 F0` = `lea eax,[rdx+78h]; div r8d`, exactly 7 in .text):
`0x43593A, 0x437EE5, 0x438473, 0x55DC56, 0x5AF6C9 (dyn-fps), 0x29770C6, 0x29771B4`.
`60/fps`: `0x1975BAA` (`8D 42 3C 41 F7 F0`). The next instructions never read `edx` (remainder).

Nioh3Fix signature locations in this dump: Dynamic FPS setting `0x5AF50A` (`cmp [0x45BAF58],r11d`);
Dynamic target `0x5AF5C2` (hook at +0x10 = `0x5AF5D2 mov [rcx+618h],eax`); Frameskip `0x5AD0BA`
(hook +7 = the `jne` to `0x19DF552` which does `ctx[200] |= 2`); Menu input `0x433130`.

---------------------------------------------------------------------------------------------------
## 4. Live evidence so far (title screen, REN_FPS=60, DYNAMIC_FPS=ON, FSR FG cfg, 2026-10-06)
* `G+0x610` +1/frame at 59.8/s; `G+0x614..0x620` = 60; `G+0x624..0x62C` = 1.0.
* `G+0x7D8` deltas: 16.689 16.667 16.645 16.670 16.660 16.669 16.723 ms (paced).
* CPU-ms ring 8.2–8.8 ms ⇒ `mon+0x74` is **not** the frame interval.
* PresentMon baseline (in-game, FG): 92 presents/s, 59.4 displayed, CPU busy 10.7 ms.
  Real (non-generated) frame rate and `G+0x614` in-game are **not yet measured** → `ft_live.fsx`.
* cpuprof: game thread ~32% in `WaitForSingleObjectEx` under `0x4A529C→0x4A4F94`.

### 4b. LIVE VERDICT — gameplay, idle at shrine (REN_FPS=120, DYNAMIC_FPS=OFF, FG OFF, VSync ON, 240 Hz desktop, OptiScaler XeSS→FSR4, RGP dev mode)
`ft_live.fsx 5`:
```
counter advanced 540 in 4996.6 ms -> 108.07 update ticks/s (G+0x610 counts update ticks, not frames)
fps used for dt [614]: 120 (all 530 samples)   target [618]=120  [61C]=120  [620]=120  scale [624]=1.0
frame-start interval ms: mean 10.384  p1 8.307  p5 8.343  p50 9.342  p95 18.622  p99 20.045
histogram: 8.5–11.5 ms ≈ 400 frames, 16.5–22.5 ms ≈ 56 frames (double-length hitches)
implied game-time / wall-time = 0.901
```
→ **Confirmed:** update thread at 108/120 ticks/s (a CPU-bound run), so the game runs at **90% speed**;
every update tick advances a fixed 8.333 ms while real tick intervals jitter 8.3–20 ms. This slowdown is
the update thread falling behind; it is not what happens when only the GPU limits.

`ft_findtask.fsx 0x40358E8` (frame task, vtable RVA 0x40358E8, vf[1]=0x5ACEE8 → 0x5ACF80):
```
r15 timing block:  +18 dt = 0.008333 (fixed)   +24 measured = 0.0086..0.0099 s   +2C prev measured   +30 limiter fps = 120
```
`ft_prof.fsx` (frame profile = `r13+8`, r13=[[task+58]+28]; vtable RVA 0x4050898):
`flags[+3BC7C]=0xC`, limiter fps `[+416E0]=120`, profile@`+41668`: `+44 dt=1/120`, `+78 fps=120`, `+2C=0xFFFFFFFF`.

Who sets these (static, now resolved):
* `0x5AD860..0x5AD898` in the dt-setup function: `[rsp+20] = G[614]` → profile **vf+0x20 = 0x5AF400
  FrameProfile_SetLimiterFps** (`[+416E0]=fps`, flag 8). → **the limiter fps IS `G[614]`** ⇒ **P4 is required**
  (otherwise the measured rate self-locks).
* `call [vtbl+8]` = **0x5AF324 FrameProfile_SetProfile** with the struct at `rbp-59h`, whose `+0x44` = `G[624]/G[614]`
  (`0x5AD74B`). → the game clock step `[r15+18]` = `scale/fps`, which P1+P2 make **exactly the measured
  interval** ⇒ **P5 is NOT needed**.
* `0x39B55C` (called between) takes no args, so the `[rsp+20]` slot only feeds FrameProfile_SetLimiterFps.
* The profile `+2C = 0xFFFFFFFF` means Lyall's "frameskip" test (`&0x8000`) is taken every frame in this
  state (`ctx[200] |= 2`); that's stock behaviour and orthogonal to this patch.

---------------------------------------------------------------------------------------------------
## 5. Patch design — "variable-dt mode" (recommended)

**Minimal set = P1 + P2 + P3 + P4 (at 0x5AD860)**, implemented in `patcher\ft_vrr_patch.fsx`
(`dry` mode verified against the live process: all 10 original-byte checks match; cave 430 bytes,
disassembly printed in the dry run). The final site table is §5-final; P4 below is the first-draft site and P5 is not
needed (§4b).

Goal: when the player selects 120 (and no cutscene/event override is active), drive the game step
from the measured *update-tick* interval (the cave runs once per update tick, so it measures the
tick interval, not the frame interval). The limiter stays a cap at 120 (or a VRR-friendly cap such
as 116). When the update thread is CPU-limited this removes the slow-down. It does not change the
ticks-per-rendered-frame judder, and when only the GPU limits it is effectively stock. Caveat: below
the tier, dt can reach 2.5x the tier step (1/24 s at 60, 1/48 s at 120) while the engine's tier flags
stay at the tier, a combination stock never runs. At an exact 60/120 lock the result is
bit-identical to stock (stock path).

Allocate one RWX cave within ±2 GB (`Target.AllocNear`). Data block `D`:
```
D+00 i64 prevStart     D+08 u8 enable(1)     D+0C i32 armFps(120)
D+18 i64 minNs = 1e9/maxFps (8_333_333)      D+20 i64 maxNs = 1e9/minFps (e.g. 1e9/48 = 20_833_333)
D+28 f32 scaleCorr     D+2C i32 ticks120      D+30 f64 acc120
D+38 i64 lastInterval (diag)   D+40 i32 fpsOut (diag)   D+48 f64 1e9   D+50 f64 120.0
D+58 i32 limiterCap (120)
```

### P1 — measured fps into `G+0x614`  (site `0x5AD677`, 7 bytes)
Original `44 89 87 14 06 00 00` (`mov [rdi+614h],r8d`) → `E8 <rel32 caveFps> 66 90`.
Live registers at the site: `rdi`=G, `r8d`=tier fps (also passed on to `0x947250` through
`ecx` at `0x5AD694`), `eax` = old `G[620]` (stored to `G[61C]` next). xmm0–5 are dead (a call
follows).
```
caveFps: push rax; push rcx; push rdx; sub rsp,20h; movdqu [rsp],xmm0; movdqu [rsp+10h],xmm1
  mov  rcx,[rdi+7D8h]          ; this frame's start (ns)
  xchg rcx,[D+00]              ; rcx = previous start, store current
  cmp  byte [D+08],0 ; je stock
  cmp  r8d,[D+0C]    ; jne stock          ; only in the 120 tier
  cmp  eax,r8d       ; jne stock          ; event/cutscene override active (G[620]!=target) -> stock
  test rcx,rcx       ; je stock
  mov  rax,[rdi+7D8h] ; sub rax,rcx       ; interval ns
  cmp rax,[D+18] ; jge @1 ; mov rax,[D+18]
@1: cmp rax,[D+20] ; jle @2 ; mov rax,[D+20]
@2: mov [D+38],rax
  cvtsi2sd xmm1,rax
  movsd xmm0,[D+48] ; divsd xmm0,xmm1 ; cvtsd2si ecx,xmm0      ; fps_i = round(1e9/interval)
  mov r8d,ecx ; mov [D+40],ecx
  cvtsi2sd xmm0,ecx ; mulsd xmm0,xmm1 ; divsd xmm0,[D+48] ; cvtsd2ss xmm0,xmm0 ; movss [D+28],xmm0  ; fps_i*interval
  movsd xmm0,xmm1 ; mulsd xmm0,[D+50] ; divsd xmm0,[D+48] ; addsd xmm0,[D+30]
  cvttsd2si ecx,xmm0 ; cvtsi2sd xmm1,ecx ; subsd xmm0,xmm1 ; movsd [D+30],xmm0 ; mov [D+2C],ecx
  jmp done
stock:
  mov dword [D+28],3F800000h
  mov dword [D+40],0                     ; disarm P4/P5
  mov ecx,r8d ; test ecx,ecx ; je @z ; mov eax,120 ; xor edx,edx ; div ecx ; mov [D+2C],eax
@z:
done:
  mov [rdi+614h],r8d
  movdqu xmm1,[rsp+10h]; movdqu xmm0,[rsp]; add rsp,20h; pop rdx; pop rcx; pop rax
  ret
```
(Note: `eax` must be restored. It is popped from the stack, so the `cmp eax,r8d` before the pop
sees the original value.)

### P2 — exact time-scale correction  (site `0x5AD6AA`, 8 bytes)
Original `F3 0F 11 97 24 06 00 00` (`movss [rdi+624h],xmm2`) → `E8 <rel32 caveScale> 0F 1F 00`.
```
caveScale: mulss xmm2,[D+28] ; movss [rdi+624h],xmm2 ; ret
```
`xmm2` feeds `G[62C]`; the legacy 60 Hz callback (reader 0x5A535C via 0x5A540C -> 0x5A6BB0, once per
tick from 0x39E7D2) fires at most once per update tick, so P2 keeps it at 60/s only while ticks >= 60/s.
Below that it fires fps_i times per second (as in stock 30 mode).
The pause zeroing (`0x5AD6BB/0x5AD6CE`) still applies. The 100+ unscaled `60/fps` users get
`fps_i`, whose rounding error is ≤0.55% per frame, zero-mean, and causes no drift.

### P3 — integer 120 Hz timers  (7 sites, 6 bytes each)
`8D 42 78 41 F7 F0` → `8B 05 <rel32 to D+2C>` (`mov eax,[D+2C]`) at
`0x43593A 0x437EE5 0x438473 0x55DC56 0x5AF6C9 0x29770C6 0x29771B4`.
Stock path: `D+2C` = 120/fps, identical to the original. Variable path: `floor` with fractional
carry, which is exact on average. The `60/fps` site `0x1975BAA` stays unpatched (fps_i>60 gives
0, the same as stock at 120).

### P4 — keep the limiter at the cap  (draft site `0x5AD229`, 8 bytes; final site `0x5AD860`, §5-final)
Needed: §4b shows the limiter fps `[r15+30]` follows `G[614]`, so without P4 the measured rate would self-lock
(limiter at 90 → measured 90 → …).
Original `41 8B 4F 30 4D 8D 4F 10` → `E8 <rel32 caveCap> 0F 1F 00`.
```
caveCap: mov ecx,[r15+30h] ; cmp byte [D+08],0 ; je @x ; cmp dword [D+40],0 ; je @x
         cmp ecx,[D+58] ; jae @x ; mov ecx,[D+58]
@x:      lea r9,[r15+10h] ; ret
```
(`D+40` should be cleared on the stock path so P4 only acts while armed.)

### P5 — game clock  (site `0x5AD30B`, 6 bytes `F3 41 0F 11 4F 18`; not needed, §4b)
Only for the case where `[r15+18]` (absolute game-time increment: wind, shader time, …) turns out to be the fixed 1/target
(`profile+0x44`), use the measured `[r15+24]` while armed:
`E8 <rel32> 90` → `cave: cmp dword [D+40],0 ; je @s ; movss xmm1,[r15+24h] ; @s: movss [r15+18h],xmm1 ; ret`.

### Settings for use
REN_FPS=120, **DYNAMIC_FPS=OFF** (otherwise the tier drops to 60 and the patch falls back to stock),
desktop at 120/240 Hz with VRR. VSync: ON with VRR is fine. For a 240 Hz panel, set the cap
(`D+58`) or the in-game target to ≤ refresh−3 to stay inside the VRR window. FG: with FSR3-FI the
base rate may float, which FSR handles. With FG check `G[620]` first (`0x98F368` may halve it,
which would keep the patch disarmed).

### Rollback
Restore the original bytes listed above (P1 `44 89 87 14 06 00 00`, P2 `F3 0F 11 97 24 06 00 00`,
P3 `8D 42 78 41 F7 F0` ×7, P4 `41 8B 4F 30 4D 8D 4F 10`, P5 `F3 41 0F 11 4F 18`). The cave can be
left allocated. Runtime kill-switch: `D+08 = 0` → every cave takes the stock path, with output
identical to the original code.

### Risk assessment
| Risk | Level | Mitigation |
|---|---|---|
| Unknown consumers that assume fps ∈ {30,60,120} (e.g. `Shot::Is30Fps`, 30-fps gameplay branches) | low-med | arming is only at the 120 tier; fps_i stays in [maxFps..minFps]=[120..48]. Audit found no `cmp fps,1Eh/3Ch/78h` right after any GetFps call |
| `fps*1000/60` (`0x20E77B`, `0x5FB360`) and `fps/2` (`0x1271FB`) integer users | low | values change smoothly (1500..2000); review the two call trees if anything looks off |
| Integer `120/fps` sites missed in other forms (inlined with other registers) | med | the byte scan only covers the r8d form. Run a broader `div` scan around GetFps/`G[614]` readers |
| Physics/animation subsystems with their own fixed step (Havok-like) that read target via other paths (`0x947250 → 0x1334980` receives fps) | med | `ecx` passed there = fps_i (consistent). Watch ragdolls/cloth |
| Limiter self-lock | med | P4 |
| Online co-op desync (variable dt per peer) | **high** | **offline only** (project rule) |
| Cutscenes (EVENT_FPS) | low | the patch disarms when `G[620] != target` |
| Rounding/rate error | negligible | P2 makes scaled time exact; unscaled ≤0.55%, zero-mean |

### Alternatives considered
* **(b1) Fixed tick + render interpolation** of camera/transforms: the "proper" design. It would need
  a duplicated transform state and a render-side lerp in the scene/camera pipeline: thousands of
  sites and no clean hook. Not feasible as a byte patch. Rejected.
* **(b2) Better automatic tier choice (Lyall-style, fixed).** Hook `0x5AF5D2` with
  `target = floor(1e9 / avg(G+7D8 deltas))` (the full interval, not CPU time), with headroom probing.
  This keeps the stock fixed-step semantics, and the limiter paces evenly at an integer like 97.
  It has the lowest patch surface (one hook + the 7 integer sites, since 120/97 truncates). But it
  needs a probing heuristic to climb back up, and it cannot react within one frame. It's a fallback
  if variable-dt shows side-effects.
* **(b3) No code (do this first):** desktop at 120/240 Hz, REN_FPS=120 if the PC holds 120,
  otherwise REN_FPS=60 + FSR FG → 120 output. Both are "exact tier" setups and smooth.

---------------------------------------------------------------------------------------------------
## 6. Open items / unknowns
* Resolved (§4b): the limiter fps `[r15+30]` is set from `G[614]` through FrameProfile_SetLimiterFps 0x5AF400, and
  `profile+0x44` = `G[624]/G[614]`; so P4 is needed and P5 is not.
* Resolved (§8.3): `0x98F368` (sets `G[620]=max(30,target/2)` when `[0x4757480]`∈1..4) is a gameplay time-effect
  override, not frame generation; the P1 arm condition (`old G[620] == tier`) keeps it stock.
* Meaning of `ctx[200]` bit 1 ("frameskip" per Lyall; read at `0x412748` and `0x32ABEF5`, where it
  skips one scene sub-step submission). The stock 1-step-per-frame model already explains the
  judder; no separate "catch-up loop" (extra sim steps per frame) was found in the frame task. The
  research's catch-up theory isn't confirmed for Nioh 3; the "110 fps looks like 55" reports would
  instead come from the tier controller toggling 120↔60.
* The 60 Hz tick accumulator `G[62C]` has a reader: leaf 0x5A535C (`cvttss2si rax,[rax+0x62C]`),
  called from 0x5A540C, which calls 0x5A6BB0 (passes QPC to a callback `[rbx+0x28]`) when the
  result is non-zero; 0x5A540C runs once per tick via 0x5A5374 (called at 0x39E7D2). The
  accumulator at 0x5AD6D8 drops the integer part each tick, so the legacy tick fires at most once
  per update tick and the excess is lost. (An earlier version said "2 ticks per frame at 30 fps";
  that was wrong.)
* In-game real frame rate vs `G[614]` with FG on (baseline shows ~92 presents/s for 59.4 displayed
  at REN_FPS=60).

## 7. Live verification plan (read-only; run in game)
1. `dotnet fsi patcher\ft_live.fsx 5`: real frames/s, histogram of `G[614]/[618]/[61C]/[620]`,
   `G[624]`, dyn target, frame-start interval distribution, and the **implied game-time/wall-time
   ratio**. A ratio <1 at off-target fps proves the slow-motion mechanism directly.
2. `dotnet fsi patcher\ft_findtask.fsx 0x40358E8`: finds the frame task (vtable RVA 0x40358E8; DONE, see §4b) and prints
   `[r15+18]` (dt used for the game clock), `[r15+24]` (measured interval) and `[r15+30]` (limiter
   fps) over several frames.
3. Repeat in a moving-camera gameplay scene with REN_FPS=120
   (desktop must be ≥120 Hz).

---------------------------------------------------------------------------------------------------
## 5-final. Patch set ready to apply (supersedes the P4/P5 drafts above)

Tool: `patcher\ft_vrr_patch.fsx` — modes `dry` (read-only), `apply`, `status`, `on`/`off`, `revert`
(write modes need `FT_ALLOW_WRITE=1`; every write first re-checks the original bytes).
Config constants at the top: `ARM_FPS=120`, `MAX_FPS=120`, `MIN_FPS=48`, `CAP_FPS=120`.

| Patch | RVA | Original | New | Purpose |
|---|---|---|---|---|
| P1 | `0x5AD677` | `44 89 87 14 06 00 00` | `E8 rel32 66 90` → caveFps | `G[614] = round(1e9/interval)` from `G+7D8` frame-start deltas, clamped to 48–120 fps; armed only if tier==120 and `G[620]`==tier (no event override); computes scaleCorr and 120 Hz ticks with carry; otherwise stock (scale 1.0, ticks=120/fps) |
| P2 | `0x5AD6AA` | `F3 0F 11 97 24 06 00 00` | `E8 rel32 0F 1F 00` → caveScale | `G[624] = G[628] * scaleCorr` (exact time; also feeds the `G[62C]` 60 Hz accumulator, profile dt `+44` → game clock `[r15+18]`) |
| P3 ×7 | `0x43593A 0x437EE5 0x438473 0x55DC56 0x5AF6C9 0x29770C6 0x29771B4` | `8D 42 78 41 F7 F0` | `8B 05 rel32` (mov eax,[ticks120]) | integer 120 Hz timers use the carried tick count instead of `120/fps` (identical when stock) |
| P4 | `0x5AD860` | `8B 87 14 06 00 00` | `E8 rel32 90` → caveLim | FrameProfile_SetLimiterFps receives `CAP_FPS` instead of the measured fps while armed (prevents self-lock); stock otherwise |

Apply order (each step is consistent on its own): cave → P3 (ticks preset 1) → P2 (scale preset 1.0)
→ P4 (armed flag 0) → **P1 last** (activates). Revert order: P1, P4, P2, P3. Kill switch: `off` mode
(writes the enable flag; all caves fall back to exact stock behaviour next frame).

Write-safety note: P1/P2/P4 are 6–8-byte writes into a function the game thread runs ~100×/s and
they straddle a qword boundary, so `WriteProcessMemory` is not atomic. The chance of a torn fetch
is tiny (µs window), but the clean options are a brief suspend of thread "[App]Game" during the
4 site writes, or applying on the loading screen. The cave and P3 sites are safe at any time.

Expected result at the measured state (≈108 fps, 120 target): game-time/wall-time 0.90 → 1.00, and
the per-frame step follows the real interval (8.3–20 ms) instead of a fixed 8.33 ms. The camera and
everything driven by `GetFps`/`GetDeltaTime60`/the game clock move at real speed. Verify after applying
with `ft_live.fsx 5` (expect `[614]` to vary between ~50 and 120, implied ratio ≈ 1.00) and
`ft_vrr_patch.fsx status`.

Remaining caveats (watch in testing): (1) the 1-frame latency of the dt (it uses the previous
frame's interval, like most engines). A 20 ms hitch frame is followed by one long step, which is
physically correct but visible. (2) Subsystems with their own fixed-rate logic that don't read
`G[614]` (Nioh1Fix needed ~10 subsystem fixes: grass wind, ocean, clouds, UI anims, input cadence).
Check camera rotation speed, lock-on, menu input repeat (Lyall's menu-input fix at `0x433130` hints
that the menu repeat is fps-relative), cloth/hair, and particles. (3) Offline only.
(4) The frame-gen path (`0x98F368` halving `G[620]`) was not exercised; with FG ON the patch may stay
disarmed (by design of the `G[620]`==tier check).

---------------------------------------------------------------------------------------------------
## 8. v2: all tiers (handhelds 20–60 fps), FG, events, subsystem audit  (2026-10-06, dry-run only)

Status of v1: applied live. `ft_vrr_patch.fsx verify 5` on the live v1 install:
`real fps 103.9 | G[614] 70..119 (100% armed) | implied game-time/wall-time = 1.000 | interval p50 9.6 ms p99 19.8 ms`.

### 8.1 Bug found in v1 (fixed in v2): fn 0x947250 gets the measured fps
`0x5AD694 mov ecx,r8d` → `0x947250` → **`0x1334980(obj=[0x475A148], fps)`**:
```c
if (fps == 30) *(u16*)(obj+0x1D) = 0x0100;      // +1D=0, +1E=1
else if (fps == 60) *(u16*)(obj+0x1D) = 0;
else if (fps == 120) *(u16*)(obj+0x1D) = 1;       // any other value: unchanged
```
`obj+0x1E` is **`Is30Fps()` = `0x20E8F8`**. It has 107 call sites, including the script functions
`Shot::Is30Fps` 0x23F0D48, `Shot::IsSecondHalfUpdate30Fps` 0x23F21E0 and
`Groupthink::GetNumberToBeAddedToTimerByFPS` 0x23D8178 (returns 1 or 2). `obj+0x1D` is the 120-fps
mode flag.

In v1 the cave overwrote r8d, so this switch saw fps_i. Non-tier values leave the flags unchanged,
which is harmless. But a frame with fps_i == 60 exactly (a 16.4–16.8 ms hitch) flips the engine into
"60 mode" until the next fps_i == 120 frame. In the 30/60 tiers it would be worse.
**v2 keeps r8d = tier** and writes fps_i straight to `G[614]`, so the mode switch always gets the
tier, exactly as stock does.

### 8.2 Generalisation to the 30/60/120 tiers
* Each tier has its own arm flag and clamp in `ft_vrr.cfg`. Defaults: 30-tier 20..30, 60-tier 24..60,
  120-tier 48..120. The max can't exceed the tier, because both the limiter cap (P4 = FrameProfile_SetLimiterFps)
  and the ratio `ctx[1E6] = max(1, G[614]/G[61C])` assume fps ≤ tier. With fps_i ≤ tier the ratio
  stays 1.
* Integer 120 Hz timers (P3 ×7): the carry accumulator gives exactly 120 ticks per real second at
  any fps. Stock gives 120/tier per frame (4 at 30, 2 at 60), which is 120/s only when locked.
* **New P6 `0x1975BAA`** (fn 0x281470 returns `(float)(60/fps)` as an int-div): stock gives 2 @30,
  1 @60 and 0 @120. v2 uses a 60 Hz carry accumulator in the 30/60 tiers (60 per real second) and
  forces 0 in the 120 tier, matching stock.
* **New P8: these keep tier semantics** (the `GetFps` call is redirected to `caveTier`, which returns
  the tier):
  - `0x20E77B` in fn 0x20E540: `x += (fps/60 int) * k * dt60`. Its per-second rate is `tier*k` only
    when fps = tier. This function is a per-object speed/time-scale integrator (called from the
    character/motion update). **v1 has this bug at the 120 tier and v2 fixes it:** with fps_i at
    60–119 it runs at **half speed** (109/60 = 1 vs 120/60 = 2).
  - `0x5AF526`, `0x5AF682`, `0x5AF69F`: the dynamic-FPS controller (load fraction and ms budget
    relative to the tier). This only matters with DYNAMIC_FPS=ON.
* These stay on the measured fps, because their frame-count semantics are correct at the real rate:
  `0x5FB360` (history window = N*fps/60 frames), `0x1271FB` (frames vs fps/2), the ~100 float `x/fps`
  sites, and the 14 inlined readers. The ~45 direct REN_FPS readers are only the null-G fallback of
  inlined GetFps.

### 8.3 Frame generation
* `0x98F368` is **not** frame generation. `[0x4757480]` is a gameplay **time-effect state machine**
  (states 1..4, fns 0x22378C0 / 0x22379B4 / 0x22376F4 / 0x2237840; reset by 0x2235490).
  - It ramps a time factor `[0x4757490]` from the update params (`scale/fps`), then resets a group's
    speed to 30.0.
  - It runs every frame from the per-frame updater `0x5651D0`.
  - While active it sets `G[620] = max(30, tier/2)`. The scene then steps every 2nd frame
    (`ctx[1E6]=2`), giving a half-rate slow-mo effect.
  - v2 stays stock during it (the arm check is `old G[620] == tier`). The exception is the 30 tier:
    tier/2 → max 30 = tier, and stock doesn't run at half rate there either.
* The FG setting is `FRAME_GENERATION` = `[0x45BB008]` (written by `0x1FDDFCC`; 1/2/3 =
  DLSS/FSR/XeSS).
  - 21 references: 3 writes in the settings code (0x1FDDxxx), 18 reads in 14 functions, including
    the render setup (`0x39A39C`: swapchain/FG params) and dynamic resolution (`0x5AF7F8`:
    resolution target ×0.5 with FG). None writes G[614/618/620], REN_FPS or DYNAMIC_FPS.
  - **There is no engine-side change to the sim fps.** REN_FPS sets the base rate and FG doubles the
    output, so the patch acts on the base frame rate and works unchanged with FG on.
  - Handheld example: REN 60 + FSR FG, base 35–55 fps → the 60 tier is armed → real time.
  - The 2026-10-06 baseline (60 Hz desktop, REN 60, FG, ~46 base fps): game speed was not measured
    there (46 is a render rate, not an update-tick rate), so no speed figure follows from it.

### 8.4 Events and cutscenes
* Five places write `G[620]`: 0x39E418, 0x98F3C6, 0x10F7DAF (= 30), 0x2705228, 0x28A9276 (= 60).
  - `0x10F7DAF`: event code sets 30 when the event-fps flag is set (EVENT_FPS).
  - `0x98F3C6`: the time effect from §8.3.
  - `0x39E418` (EVENT_FPS, state 10-14) also writes `G[618]`, so VRR arms at the event rate there
    (30-tier clamp).
* P1 arms only when the previous `G[620]` == tier, so events and effects that write `G[620]` alone
  (0x10F7DAF, 0x98F3C6) in the 60/120 tiers run **stock**. That keeps the event logic step (`ctx[1E6]` = tier/30 frames per event step) intact.
* In the 30 tier an event sets 30 = tier, so the patch stays armed there. That is correct, since the
  ratio is 1 anyway.

### 8.5 Subsystems that may still assume a fixed fps (audit)
| Item | Finding | Risk with v2 |
|---|---|---|
| Is30Fps / 120-mode gameplay branches (107 callers of 0x20E8F8) | mode flag set from the tier (v2) | none (stock mode per tier) |
| `GetNumberToBeAddedToTimerByFPS` (script) | returns 1 or 2 by mode, per frame | per-frame script timers run slower when fps < tier (stock-like). Watch boss/script timers in the 30/60 tiers |
| Integer 120/60 Hz timers | P3/P6 carry | none |
| `0x20E540` speed integrator | P8 tier | none |
| Round-robin schedulers: `i % (fps*3)` in fn 0xE66E20 (0xE66520 similar) | modulus now varies per frame | low: some entities may update one 3 s cycle late or early |
| UI fade counters in fn 0x5A9C70 (`tierFps*15`, `*6` frames, set once at mode change) | frame-count | cosmetic: fades take longer when fps < tier |
| Menu input hold/repeat (`0x433114`, GetDeltaTime60 accumulators; Lyall quantises dt to 0.25 for dynamic fps) | variable dt | repeats may drop. Optional **P7** `0x43312B` (`menuQuantize=1`): quantise to 0.25 with carry |
| Camera / lock-on | patch 1.05 made lock-on fps-correct; camera reads dt (`GetDeltaTime60`/`G[614]`) | test |
| Cloth/hair/particles/physics | read dt from update params (`scale/fps`), which P1+P2 make real; no fixed-rate path found in the frame task | test, especially at 20–30 fps (large steps) |
| Game clock `[r15+18]` (wind/shader time) | = profile dt = scale/fps → real | none |
| Limiter | P4 caps at the tier while armed | none |

### 8.6 v2 patch set (`patcher\ft_vrr_patch.fsx`, config `patcher\ft_vrr.cfg`, cave 768 bytes, dry run OK)
| Patch | RVA | Original | v2 |
|---|---|---|---|
| P1 | 0x5AD677 | `44 89 87 14 06 00 00` | call caveFps (tier table, clamps, fps_i → G[614], r8d untouched, ticks120/60, scaleCorr, capCur, tier) |
| P2 | 0x5AD6AA | `F3 0F 11 97 24 06 00 00` | call caveScale |
| P4 | 0x5AD860 | `8B 87 14 06 00 00` | call caveLim (limiter = tier cap while armed) |
| P3 ×7 | 0x43593A 0x437EE5 0x438473 0x55DC56 0x5AF6C9 0x29770C6 0x29771B4 | `8D 42 78 41 F7 F0` | `mov eax,[ticks120]` |
| P6 | 0x1975BAA | `8D 42 3C 41 F7 F0` | `mov eax,[ticks60]` |
| P8 | 0x20E77B / 0x5AF526 / 0x5AF682 / 0x5AF69F | `E8 14 7E E3 FF` / `E8 69 70 A9 FF` / `E8 0D 6F A9 FF` / `E8 F0 6E A9 FF` | call caveTier |
| P7 (opt) | 0x43312B | `E8 54 33 C1 FF` | call caveMenu (GetDeltaTime60 quantised to 0.25 + carry while armed) |

Script modes:
* `dry` (read-only): prints the cave disassembly and the bytes for each site.
* `verify [s]` (read-only): real fps, tier, G[614] histogram, armed %, implied game/wall ratio,
  interval percentiles, cave state.
* `apply`: writes with the process frozen, in a safe order: P3, P6, P8, P7, P2, P4, then P1 last.
* `status`, `on`/`off`.
* `revert`: restores every v1+v2 site, so it also works on a v1 install.

**To upgrade the live v1 install: `revert`, then `apply`.** A dry run against the live v1 process
reports "differs" at the v1 sites, as expected.

Benchmark plan:
1. For each tier (REN 30/60/120) with FG off and on, run `verify 10`. Expect an implied ratio ≈ 1.00,
   G[614] spread inside the clamp, and armed ≈ 100% outside events.
2. Gameplay checks:
   - lock-on rotation speed
   - menu repeat (turn on P7 if repeats drop)
   - Living-Weapon / time-effect sequences (expect stock)
   - cutscenes (stock unless in the 30 tier)
   - boss script timers in the 30 tier

---------------------------------------------------------------------------------------------------
## 9. Above 120: 240 Hz cap / unlocked  (2026-10-06, design + dry run only)

### 9.1 Recommendation: keep the internal tier at 120 and only raise the measured-fps ceiling (design A)
The REN setting stays 120 and `G[618]` / r8d stay 120. So every piece of "tier" logic keeps seeing 120
exactly as in stock: the Is30Fps / 120-mode flags (`0x1334980`), P8 sites, the event step ratio, and
dynamic FPS. Only the measured fps written to `G[614]` (and therefore every dt) may go above 120,
and the limiter cap (P4) is raised to `maxFps`.

Changes vs v2 (all in `ft_vrr_patch.fsx`, cfg key **`maxFps=120|144|165|240|0`**, 0 = unlocked, cap 1000):
* The 120-tier clamp max and limiter cap become `maxFps` (cave tables `lMinNs[2]` / `lCap[2]`).
  There is no new site for this; the cave is just rebuilt with the new constants.
* **New P9 `0x5AD89B`**: `8B 87 14 06 00 00` (`mov eax,[rdi+614h]`) becomes `E8 rel32 90` (call
  caveTier).
  - This feeds `ctx[1E6] = max(1, fps / G[61C])`, the "render frames per scene logic step" ratio
    (gate in `0x412748`). With measured fps ≥ 240 (= 2×120) the ratio becomes 2, and the scene would
    step only every 2nd frame. Near a 240 cap the measured value flickers between 239 and 241, so the
    ratio would alternate 1/2: judder.
  - With P9 the ratio uses the tier, which is identical to stock in every case. Armed frames give
    tier/tier = 1. Stock frames give G[614] = tier. In events, 120/30 = 4 as in stock. So P9 is safe
    to keep on permanently.
* **No patch needed** for any of these:
  - `0x5AF604` (target table).
  - The refresh gate `0x1FE402C`: idx 1 needs refresh ≥ 58 Hz, idx 2 needs ≥ 118 Hz. `0xFBA838` lowers
    REN_FPS when the gate fails. The refresh rate comes from `0x9AFE00`, cached in `[0x45BBCC4]`.
  - The Is30Fps / 120-mode flags.
  - P3/P6/P8.
  - The dynamic-FPS controller (keep it OFF).

Per-site behaviour above 120 with design A:
| Item | At 240 fps | OK? |
|---|---|---|
| 100 float `x/fps` sites, GetDeltaTime60, profile dt, game clock `[r15+18]`, `G[62C]` 60 Hz accumulator | dt = 1/240 | yes, real time |
| P3 carried 120 Hz ticks | 0/1 per frame, 120 per second. Stock never produced 0, but all 7 consumers only add/subtract it | yes |
| P6 60/fps (120 tier) | 0 (stock-120 value) | yes |
| `0x20E77B` integrator (P8 → tier 120 → fps/60 = 2) | rate = 2·k·dt60 per frame = 120·k per second, the same as stock-120. *A true 240 tier would give 4 → 240·k/s → 2× fast.* | yes (A) |
| Is30Fps / 120-mode flags | fed 120 | yes |
| Event / cutscene ratio `ctx[1E6]` | P9 → tier | yes |
| Limiter (P4) | cap = maxFps | yes |
| `GetNumberToBeAddedToTimerByFPS` (script) | returns 1 per call (120 mode) | **open**, see 9.3 |
| Round-robin `i % (fps*3)` (0xE66E20) | 3 s cycle in frames, still ~3 s | yes |

### 9.2 Why not a true 240 tier (design B)
Returning 240 from `0x5AF604` (plus a new gate index) breaks stock integer assumptions:
* `120/fps` = 0 at all 7 P3 sites if P3 isn't active.
* `60/fps` = 0.
* `0x1334980` ignores 240, so the mode flags go stale.
* `0x20E77B` gets fps/60 = 4, i.e. 2× speed.
* The event ratio becomes 240/30 = 8.
* The dynamic-FPS cooldown `120/fps` = 0, so it never decrements.

Every one of these would need its own patch, for no benefit over A. **Not recommended.**

### 9.3 Open risks for >120 (verify these)
1. **Present / VSync.** I haven't located the Present call yet; it goes through KTGL (the engine's
   graphics library) and the FSR/XeSS proxy swapchain. If the engine derives SyncInterval =
   refresh/target (2 at 240 Hz with a 120 target), fps stays capped at 120 whatever the limiter does.
   The baseline PresentMon showed SyncInterval 0 (proxy swapchain).
   - Test: maxFps=240, VSync ON at 240 Hz. If `verify` shows fps pinned at 120 while GPU busy is
     <8 ms, switch in-game VSync OFF and rely on VRR/driver.
2. **Per-frame script timers.** `GetNumberToBeAddedToTimerByFPS` returns 2 in 30-mode and 1 otherwise.
   - If scripts add it once per render frame, stock script timers already run at a different
     real-time rate at 60 vs 120. That suggests scripts run on a fixed logic cadence instead:
     `IsSecondHalfUpdate30Fps` implies two 60 Hz half-steps per frame at 30 fps.
   - The `G[62C]` 60 Hz accumulator is read by leaf 0x5A535C (via 0x5A540C -> 0x5A6BB0, once per
     tick from 0x39E7D2); the legacy callback fires at most once per update tick.
   - If some script/AI update is instead gated by frame parity (every 2nd frame at 120), it runs 2×
     fast at 240, and 13% slow at 104 under v2 already.
   - Test: time a script-driven timer (status effect / buff duration, enemy grab or respawn timer,
     Ki-pulse window) with a stopwatch at locked 120 vs 240.
3. **Physics/cloth/particles at dt 4 ms.** Smaller steps are normally safer; watch cloth/hair
   jitter and collision tunnelling (no change expected).
4. **CPU cost scales with fps.** Per-frame work runs 2× as often, and the limiter spin (`0x916EBC`)
   disappears.
5. **Menu repeat.** dt60 = 0.25 at 240, which is exactly the 0.25 quantum. If repeats drop, enable P7.

### 9.4 Site list (v2 + P9), and how to upgrade the live v2 install
P1 `0x5AD677`, P2 `0x5AD6AA`, P4 `0x5AD860`, P3×7, P6 `0x1975BAA`, P8×4 (`0x20E77B`, `0x5AF526`,
`0x5AF682`, `0x5AF69F`), **P9 `0x5AD89B`** (new), and optional P7 `0x43312B`.
Dry run with `FT_VRR_CFG=<cfg with maxFps=240>`: P9 original bytes OK, cave 768 bytes, patch bytes
printed. The other sites show "differs" because v2 is live.

To upgrade: set `maxFps` in `ft_vrr.cfg`, then `revert`, then `apply`. The cave constants (clamp and
cap) are baked in, so changing `maxFps` always needs a re-apply. Each re-apply leaks one 4 KB cave,
which is harmless. `FT_VRR_CFG` can point at an alternate cfg for dry runs.

### 9.5 Verify plan
1. Lower the settings / render scale until GPU busy is <4 ms. `maxFps=240`, REN 120, DYNAMIC OFF,
   FG OFF, 240 Hz desktop.
2. Run `verify 10` and expect:
   - real fps > 120
   - G[614] histogram reaching ~240
   - implied game/wall ratio ≈ 1.00
   - the frame-interval percentiles show no alternating ~4/8 ms pattern (that pattern is what a
     missing P9 or a SyncInterval=2 cap would cause)
3. Repeat with VSync OFF if fps stays pinned at 120 (risk 9.3.1).
4. Run once with `maxFps=0` (unlocked).
5. Gameplay checks:
   - camera pan smoothness at ~200 fps
   - lock-on speed
   - script timers 120 vs 240 (risk 9.3.2)
   - projectiles / Shot behaviour
   - cloth
   - menu repeat

---------------------------------------------------------------------------------------------------
## 10. Online co-op safety  (2026-10-06, design + dry run; live read-only check in solo)

### 10.1 Session state (found)
* **`Online::IsOnlinePlay`** is the script function. Its binding table entry is at `0x397D6F0`; the
  native function `0xA5E83C` calls **`0x85834`**:
  ```c
  bool Net_IsOnlinePlay() {                   // 0x85834, 107 callers
    obj = [0x4709B50];                        // net session object
    return (obj && obj->mode(+0x34) == 1) || Net_IsTokoyoCoopMode();   // 0x85948: obj && obj->mode == 2
  }
  ```
  Many online code paths branch on `[[0x4709B50]+0x34]`: `cmp ...,2` in 0xE37940, 0xE4221D,
  0xE45A34, 0xE597C0, …
* `Online::IsPlayerGuest` = `0x2853D4`: `[0x45C84F8]+0x1DC && !( [0x45C84F8]+0x2C8 )` (0x5A9028).
* The network layer is KTGL `COnlineSessionP2p*` / `CSteamOnlineSessionP2p` (Steam P2P sessions). It
  has host/guest/member events (`OnlineEvent_SessionP2p_MemberAdd/ChangeHost/GhostMember`), and the
  mission scripts query `Online::IsOnlinePlay`, `IsPlayerGuest`, `IsPlayerCoop`, `IsOnlinePlayVersus`,
  `IsTokoyoCoopMode`.
* **Live, solo (test run):** `[[0x4709B50]+0x34] = 0`, guest flags `0/0`
  (`patcher\ft_session.fsx`, and in `ft_vrr_patch.fsx verify`/`status`).

### 10.2 Lockstep or state-sync? → **state-sync with per-object authority** (confidence: medium-high)
* 236 `Online::*` protocol messages. All of them carry state or requests, not inputs or frame numbers:
  - `PlayerSyncData`, `CoopPlayerState`, `CoopGaugeSync`, `KatanadukaStateAndPosition`,
    `ForceGadgetPosition`
  - `SynchroObjectNpcControlPlayerUpdate(/Tiny)`, `SynchroObjectNpcControlPlayerRequest`,
    `…ControlForceRequest` (one peer "controls" each NPC/gadget and broadcasts its state; the others
    are non-controllers)
  - `MissionSyncRandomValue`, `SynchroTimerSetting`, `MissionRequestGuestWarpCloserToHost`
* No input/lockstep messages, and no frame-counter or tick fields in the message names.
* **No fps equalization exists for co-op.**
  - Every frame-pacing function (`0x5AF604` target, `0x5AF4E4`/`0x5AF670` dynamic tier, `0x5AD5F8`
    dt setup, `0x10F7D00`/`0x98F368` overrides) has no online condition.
  - The only `IsOnlinePlay` users in the frame-manager area are an autosave-style 6 s timer
    (`0x5A6E68`) and a UI/online state toggle (`0x5A3C7C`).
  - So stock co-op already runs peers at different tiers (30/60/120), dynamic-FPS switching, and
    off-target slow motion. State-sync with authority handoff tolerates per-peer step-rate
    differences by design.
  - VRR's real-time stepping reduces divergence compared with stock slow-mo, so variable dt is
    **very likely safe** online.
* It is still unproven without a two-player test. So **`coopSafe=1` is the default.**

### 10.3 Auto-disarm (implemented in `ft_vrr_patch.fsx`, cfg `coopSafe=1`)
* In the P1 cave, right after the enable check:
  `if (coopSafe && [base+0x4709B50] && [[base+0x4709B50]+0x34] != 0) → stock path`.
  - Any non-zero mode is treated as online, which is conservative: it also covers lobby/
    matchmaking states that may set the mode before peers connect.
  - The value read is stored in the cave (`lNetMode`) for diagnostics.
* On the stock path every patched site is bit-identical to unmodded behaviour:
  - G[614] = tier
  - scale 1.0
  - ticks = 120/tier, 60/tier
  - limiter = G[614]
  - caveTier = G[614]
  - the P9 ratio is stock
  So **in a session the game is frame-timing-identical to stock** (the CPU/spin patches remain; they
  don't touch logic).
* Re-arming is automatic on the first frame after the session ends.
* Dry run: cave 830 bytes, original bytes match (no patch installed in the current process). The
  disassembly shows
  `mov rdx,<base+4709B50>; mov rdx,[rdx]; test; mov edx,[rdx+34h]; mov [lNetMode],edx; test edx,edx; jne stock`.
* `status` now prints `session mode live=… cave=… (solo | ONLINE -> stock path)`. `verify` prints the
  live session mode and whether P1 is installed. Both tolerate a stale `ft_vrr_state.txt` left by a
  previous process.

### 10.4 How to verify later (needs a second player or a co-op summon)
1. Apply with `coopSafe=1`. In solo, `status` should show `session mode live=0 … solo` and `armed
   fps_i` varying.
2. Open an online session: host a co-op room or join one (an expedition/visitor-style summon via
   the shrine).
   - `status` should show mode 1 or 2, `ONLINE -> stock path`, `fps_i=0` (not armed).
   - `verify 5` should show G[614] == tier at 100%.
3. Check whether the mode is already non-zero while only searching/matchmaking (expected
   harmless, since it's still stock).
4. Leave the session; `status` should return to solo and armed within one frame.
5. Optional, after (2) proves stable: test `coopSafe=0` online with both peers patched and then one
   unpatched peer at off-target fps. Watch enemy positions/HP sync, warp-to-host, gauge sync,
   synced timers (`SynchroTimerSetting`) and rescue/revival timing. Turn it off at the first desync
   symptom.
