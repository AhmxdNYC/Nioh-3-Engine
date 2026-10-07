# Nioh3EngineFix on ROG Ally (Bazzite / Proton): field report

**Summary:** The mod loads and every patch applies under Proton (10 of 10 groups). On Windows (7600X desktop) it gave large gains, but on the Ally it feels like no change. The data below shows why: the Ally is **GPU-bound at its 25 W package limit**, so main-thread savings don't become fps. There are also **Wine-specific costs**: W3's message thread wakes ~24,000 times/s, and the job-system workers wake ~10,000 times/s through ntsync. These eat into the shared power budget. No stock-vs-mod fps A/B has been recorded yet (see "Missing data").

All numbers were taken on 2026-10-06 during live gameplay with the mod active. The tools were non-invasive `/proc` sampling (no profiler or debugger was attached), `gpu_metrics` and hwmon.

---

## 1. Environment

| | |
|---|---|
| Device | ASUS ROG Ally, Ryzen Z1 Extreme (8C/16T Zen 4, RDNA3 780M 12 CU), 16 GB shared LPDDR5 |
| OS | Bazzite, kernel 7.2.7-ogc1.1.fc44, gamescope session (Game Mode) |
| Proton | Proton Experimental `experimental-11.0-20261001` (log: "Wine/Proton 11.0") |
| Graphics | Mesa 26.2.2 (RADV), DX12 via vkd3d-proton; DXVK threads also present (`dxvk-cs`, `dxvk-submit`, ...), so something creates a D3D11 device |
| Sync | **ntsync active** (`/dev/ntsync`, ~470 ntsync fds open in the game process); `WINEFSYNC=1` also set |
| Notable env | `VKD3D_SWAPCHAIN_LATENCY_FRAMES=3`, `ENABLE_GAMESCOPE_WSI=1` |
| TDP | asus-armoury `ppt_pl1_spl=25`, `ppt_pl2_sppt=26`, `ppt_pl3_fppt=27` W, GPU perf level `auto` |
| Game | Steam v2.02, PE TimeDateStamp 0x6AA7C106 (matches) |
| Loader | Ultimate ASI Loader `dinput8.dll` with `WINEDLLOVERRIDES="dinput8=n,b"` |
| Modules loaded | `libxess.dll`, `libxess_fg.dll`, `sl.interposer.dll`, `sl.common.dll`, `sl.pcl.dll`, `sl.reflex.dll` (Streamline is loaded on AMD under Proton) |

## 2. Mod status: works

Both `dinput8.dll` and `Nioh3EngineFix.asi` are mapped into the game process. Log from a run with `L1OnWine=1` and `FGOnWine=1`:

```
Nioh3.exe at 0000000140000000: TimeDateStamp 6AA7C106 SizeOfImage 52FB000 -> known build (v2.02)
code is decrypted (200 ms after start)
... all sites OK (W1, W2a-d, P3a-g, P6, P8a-d, P9, P2, P4, P1, W3, R1, R1j, L1, F1)
running under Wine/Proton 11.0
L1: applied     F1: applied
done: 10 group(s) applied, 0 skipped.
```

With the default ini, L1 and F1 are skipped under Wine as designed (8 groups applied). The fast shadow shader `shader_dx12/0x0104840c.file` was also installed.

## 3. The Ally is GPU / power-bound

Sampled at 1 Hz during gameplay (all patches on):

| Metric | Value |
|---|---|
| GPU busy (`gpu_busy_percent`) | **87-98 %** |
| Socket power (`gpu_metrics` v2.1 `average_socket_power`) | **~26.0 W, flat**: pinned at the PPT limit |
| GFX power (`average_gfx_power`) | 9-21 W (it swings and takes whatever the CPU leaves) |
| GPU sclk | 1.4-2.2 GHz, mostly ~2.0 GHz (max ~2.7): **power-throttled** |
| CPU total | ~34 % of 16 threads (~5.5 threads' worth) |
| Main thread `[App]Game` | ~96 % of one core (82 % user, **14 % kernel**) |

**Interpretation:**
- On the 7600X the README says the main thread is the limit, so W1 and W2 (11-22 % of the main thread) turn directly into fps.
- On the Ally the GPU is saturated and clock-limited by the shared 25 W budget. A faster main thread doesn't raise fps.
- On this device, every CPU watt saved only helps by moving power to the GPU. W3, R1 and L1 do that, but the remaining CPU spend (section 4) is far larger than what they recover.

## 4. Where the CPU (and therefore GPU power) goes

10-12 s windows. Notes on the columns:
- `cpu%`: 100 = one core.
- `vcsw/s`: voluntary context switches per second, roughly one per wake.
- State: share of samples from `/proc/<tid>/syscall`. `ioctl` here is the ntsync wait (`0xc0284e82` = `NTSYNC_IOC_WAIT_ANY`).

### Grouped by thread name

| Group | Threads | CPU % | user / sys | vcsw/s | Sampled state |
|---|---|---|---|---|---|
| `KIDSTaskScheduler` | 31 | **305** | 294 / 11 | **9,889** | ntsync wait 84 %, running 11 % |
| `[App]Game` (main + helpers) | 31 | 99 | 84 / 15 | 814 | main thread: running 99 % |
| `KIDSTS[s2:Stage...]` | 11 | 94 | 90 / 4 | 2,787 | ntsync wait 88 % |
| `KIDSTS[s9:Colli...]` | 12 | 68 | 68 / 1 | 818 | ntsync wait 94 % |
| `SynergisticDispatch` | 1 | 32-38 | 37 / 1 | 343-475 | ioctl 60 %, running 40 % |
| `KIDSTS[s66:TS_M...]` | 14 | 35 | 31 / 5 | 1,687 | ntsync wait 96 % |
| `Nioh3.exe` (incl. TID==PID thread) | 5 | 23 | **8 / 15** | **17,000-24,000** | see 4.1 |
| `KIDSTS[s4:UpdSt...]` | 11 | 19 | 18 / 1 | 1,005 | |
| `vkd3d_queue` | 40 | 15 | 4 / 11 | 4,143 | futex 96 % |
| `[App]MissionThread` | 1 | 12 | 12 / 0 | 169 | futex 87 % |
| `[App]InputUpdate` | 1 | 10-12 | 6 / 6 | **8,500** | futex 73 % |
| wineserver (separate process) | | **12** | | | |

Proton's own translation cost is small: vkd3d is about 0.2 core in total. Most of the CPU is the game's job system.

### 4.1 W3's message thread spins under Wine (highest-value finding)

TID == PID thread (`Nioh3.exe`, 79016):
- **~23,800-25,000 voluntary context switches/s** (measured over 2 s and 10 s windows; 12.6 M total since launch).
- CPU 22-26 % of a core, of which ~65 % is kernel time.
- Sampled syscalls: `ioctl NTSYNC_IOC_WAIT_ANY` (319/400), running (46/400), `readv(fd 11)` (35/400). The `readv` is the wineserver reply pipe.
- wineserver itself runs at ~12 % CPU.

This matches the W3 hook's wait (MsgWaitForMultipleObjects-style, 1 ms timeout) **returning after ~40 µs instead of sleeping**. Each iteration also does a wineserver round trip, because Wine's message-queue status/mask calls are server requests. Possible causes:
- (a) Under Wine the wait is satisfied immediately because the queue's wake bits stay set for the mask used. For example, `MWMO_INPUTAVAILABLE` / `QS_ALLINPUT` when the peek doesn't remove the message, or posted/timer bits that never clear.
- (b) Wine's queue-status update re-arms the wait every call.

**Not yet measured:** the same thread on the stock game under Proton. It may be just as bad or worse with the stock busy `PeekMessage` loop, so W3 may still be a net win here. But it clearly doesn't reach the intended ≤1 kHz wake rate.

Suggestions:
- Under Wine, check `GetQueueStatus(QS_ALLINPUT)` first and fall back to `Sleep(1)` / `NtDelayExecution`, or to `WaitMessage()` with a timer, when nothing is pending.
- Or count consecutive immediate returns and back off.
- Log the wait return code distribution (`WAIT_TIMEOUT` vs `WAIT_OBJECT_0`) once per second when running under Wine.

### 4.2 Job-system wake storm (`KIDSTaskScheduler` / `KIDSTS[...]`)

- About 31 + ~110 worker threads. Each one wakes 100-1,450 times/s and runs 10-20 % of a core.
- They sit 80-95 % of the time in ntsync waits. The ~10-20 % running share suggests a spin-then-block pattern or many tiny jobs.
- Under Wine with ntsync, every SetEvent/Wait is a real syscall (`ioctl` on `/dev/ntsync`).
- On a 25 W APU this costs **roughly 3-5 CPU cores of activity**, which is power the GPU can't use. **This is the largest power sink, and no current patch touches it.**

Ideas, in order of safety:
- (a) Measure the spin count before block in the KIDS scheduler wait. If it spins on `SwitchToThread`/`Sleep(0)`/`pause`, add a shorter spin or `WaitOnAddress`-style block for low-power profiles.
- (b) A P12-like change in the *opposite* direction for handhelds: cap the pool sizes. There are 16 hardware threads but ~140 KIDS threads, and on a GPU-bound APU fewer workers may cost no fps while saving watts.
- (c) Coalesce wakeups (wake one worker per batch instead of per job).

### 4.3 Main thread

`[App]Game` runs at ~96 % of a core, with **14 % kernel time** and ~500 voluntary + ~320 involuntary switches/s. The kernel share is probably ntsync ioctls for job dispatch (SetEvent per job) plus page faults. This may be more expensive than on Windows. W1 and W2 are already in place here.

### 4.4 Other

- `[App]InputUpdate`: 8,500 wakes/s with ~50 % kernel time (futex). That looks like a high-rate poll; it may be the XInput/HID path under Wine (`wine_xinput_hid` thread present). It could be a candidate for a W3-style wait.
- `SynergisticDispatch`: 32-38 % CPU, 40 % running. Its role is unknown and worth identifying.
- `VKD3D_SWAPCHAIN_LATENCY_FRAMES=3` is set by the session. R1 waits on the frame-latency waitable, which vkd3d emulates, so its behaviour may differ from DXGI on Windows.

## 5. Frame generation: unconfirmed

- `libxess_fg.dll` is loaded, but that may happen regardless of the setting.
- F1 hooked successfully (`F1: applied`), but that only means the site was patched, not that frame gen was running.
- No thread clearly identifiable as an FSR frame-gen presenter was found. The 5 unnamed `Nioh3.exe` threads are candidates.
- Thread wake rates are ambiguous: several game threads run at ~60-75/s and `vkd3d-swapchain` at ~133/s. That fits either ~66 fps with frame gen or ~133 fps without it.
- **Next run:** a MangoHud CSV log (presented fps + frametimes) will settle this.

### 5.1 MangoHud run (mod on, `L1OnWine=1`, `FGOnWine=1`, user believed frame gen was on)

Log: `Nioh3_2026-10-06_23-45-02.csv`, steady state after the first 30 s (263 s of gameplay):

| Metric | Value |
|---|---|
| Presented fps (avg) | **32.6** (swung 28-40 in 15 s buckets) |
| p99 frametime | 75 ms |
| CPU load | 72 % (55-93 %) |
| GPU load | 91 % |
| CPU power / GPU power (MangoHud) | **12.2 W / 18.1 W** |
| GPU clock | ~1,570 MHz (vs ~2,000 MHz in the earlier sample) |

- ~32 presented fps is hard to reconcile with frame generation being active. The user could not confirm the in-game cap or frame-gen state, which is why the feature request below matters.
- CPU load was much higher than in the earlier `/proc` sample (~34 %). The CPU took ~12 W of a ~26-30 W budget, and the GPU clock dropped accordingly. This could be a heavier area or post-restart shader compilation; it isn't isolated yet.
- **No stock baseline was recorded.** The user does not plan to run stock A/B tests.

## 6. Feature requests from the user

### 6.1 Always-on FPS counter: real frames vs frame-gen frames (top-left)

Please add a small, always-on counter in the **top-left corner** that shows two numbers:

```
Real FPS:    40      <- frames the game actually rendered (base frame rate)
FG FPS:      80      <- frames presented including generated ones (FG: on)
```

- **Real FPS** is the engine's own rendered/simulated frame rate. The mod already reads it via the GetFps / VRR timing sites (P1/P2/P4).
- **FG FPS** is the total presented frames (real + generated), counted at the frame-gen presenter (the F1 site) or at Present.
- When frame gen is off, show `FG: off` instead of a second number, so the counter also answers "is frame gen working?" at a glance.
- Optional extras on the same line: frame time (ms) of real frames, and the VRR speed factor (should read 1.00x).
- External overlays (MangoHud, the Steam performance overlay) only see presented frames. They can't separate real from generated frames, so this is something only the mod can show.
- Ini switches: `[Overlay] FpsCounter = 1`, `Position = TopLeft`, `Scale = 1.0`. Default on would suit handheld users; default off is fine if desktop users prefer it.

### 6.2 Startup status pop-up

Please show a **short on-screen notification when the game starts** (e.g. 5-10 s, top corner) that reports:
- That Nioh3EngineFix loaded, plus the version and how many patch groups were applied/skipped (same as the log's `done:` line).
- **Whether frame generation is actually active.** Check the game's real runtime state (e.g. the FG presenter running / the F1 site being hit), not just "F1 hooked". If possible, say which frame gen it is (FSR / XeSS FG).
- Key state that changes behaviour: VRR on/off and the current FPS tier, "FPS dynamic adjustment" on/off (warn if on, since VRR needs it off), and Wine/Proton detected (with L1/FG status).

Right now the only feedback is `Nioh3EngineFix.log`. On a handheld in Game Mode there's no easy way to read it, and the user couldn't tell whether frame gen was on.

Implementation ideas:
- Draw via the existing present path (a small D3D12 text overlay).
- Or simplest, a borderless topmost window or `MessageBox`-free toast. Note that gamescope may not show separate top-level windows, so an in-swapchain overlay is safer on Steam Deck/Ally.
- Add an ini switch `[General] StartupPopup = 1`.

## 7. Missing data

FPS logging is now built into the user's launcher (`nioh3-mod bench on`). MangoHud writes CSVs to `~/Nioh3EngineFix/bench/`. The user does not plan to run the stock comparison, so these remain open. Runs that would answer the open questions (same scene, ~60 s each):
1. Stock (normal Steam tile).
2. Mod, default ini (L1/FG off under Wine).
3. Mod, `L1OnWine=1` + `FGOnWine=1`.
4. Mod with frame gen off vs on.
5. Same pairs at a 30 W TDP, to test whether the gap opens up when power is less tight.
6. Ideally, the same A/B on Windows on the Ally (the device can dual-boot). That would separate "Proton cost" from "Ally hardware".

Also worth repeating on stock: the `/proc` thread sample. It gives the baseline for the W3 thread's wake rate and the job-system wake rate.

## 8. How this data was collected (reproducible)

- Per-thread CPU, context switches and syscall state: a Python sampler reading
  - `/proc/<pid>/task/<tid>/{stat,status,syscall}`
  - ~80 sweeps/s for 12 s
  - utime/stime deltas and voluntary/nonvoluntary switch deltas
- Power and clocks:
  - `/sys/class/drm/card1/device/gpu_metrics` (v2.1: socket/gfx/core power, mW)
  - `gpu_busy_percent`, `pp_dpm_sclk`
  - hwmon `amdgpu` PPT
- Syscall return PCs all land in host `libc.so.6` (Wine unix side), so they don't identify game code. Attributing user-mode hot spots to game RVAs needs `perf` (not installed on this image) or an in-process sampler. **An in-mod sampling profiler** that logs the top RVAs of KIDS workers and the message thread under Wine would be the most useful next step.

---

## 9. v2 field results (2026-10-07, Optimized launch, Proton Experimental 11.0)

All 11 groups applied, 0 skipped (WK and WorkerCap included; `WC: worker-pool base capped at 10 (of 16 logical CPUs)`).

### 9.1 BUG: the on-screen overlay never appears under Proton, so the menu, FPS counter and startup panel are all invisible

```
OSD: engine is rendering (5906 ms after start) - installing the Present hook
OSD: system d3d12 C:\windows\system32\d3d12.dll+0, dxgi C:\windows\system32\dxgi.dll+0
OSD: dummy swapchain 0000000031BD06F0, vtable S:\steamapps\common\Nioh3\sl.interposer.dll+66328
OSD: hooked Present: vtable slot 00006FFFFC756368, original S:\steamapps\common\Nioh3\sl.interposer.dll+29090
OSD: hooked Present1: vtable slot 00006FFFFC7563D8, original S:\steamapps\common\Nioh3\sl.interposer.dll+2A980
OSD: hooked SetColorSpace1: ...
MENU: hooked the game's XInputGetState: IAT slot Nioh3.exe+38E2C50, chained to XINPUT1_4.dll+2080
MENU: hooked DirectInput GetDeviceState (W)/(A) ...
MENU: ready - open with F10 / Hold Start 5s
OSD: IDXGISwapChain::GetDevice(ID3D12CommandQueue) (not a D3D12 swapchain?) failed (hr 80004002) - overlay off for this swapchain
```

- The Present hook works. The failure is `GetDevice(__uuidof(ID3D12CommandQueue))` returning `E_NOINTERFACE` on the real swapchain.
- That swapchain is Streamline's (`sl.interposer.dll`) proxy, wrapping DXVK's dxgi with a vkd3d-proton D3D12 backend.
- Because of this, the overlay is off for the only swapchain, and the user sees **nothing**: no startup panel, no FPS counter, and no menu, even though the menu's input hooks are live. Holding Start for 5 s may be opening an invisible menu.

Suggested fixes, most robust first:
1. **Capture the queue without `GetDevice`:** hook `ID3D12CommandQueue::ExecuteCommandLists` (vtable from a dummy queue) and remember the `D3D12_COMMAND_LIST_TYPE_DIRECT` queue(s). At Present, match the queue to the swapchain, e.g. the queue that submitted last before Present, or the one whose device matches `GetBuffer(0)->GetDevice`. This is the standard approach used by ReShade/imgui hooks, and it works with Streamline and vkd3d-proton.
2. Get the device via `swapchain->GetBuffer(0, IID_ID3D12Resource)` → `resource->GetDevice(IID_ID3D12Device)`. That always works on vkd3d-proton. It only gives the device, not the queue, so combine it with (1).
3. Try `GetDevice` on the *unwrapped* swapchain. Streamline exposes the native object via `slGetNativeInterface` / `QueryInterface` on its proxy.
4. If no overlay is possible, still give feedback: when the menu opens or closes, write a log line and play a short sound (`MessageBeep`), or flash the window title. Then the user knows the hold-Start gesture registered.

### 9.2 Stats from this run (stats/s, 30 s windows)

| | |
|---|---|
| W3 waits (Wine `Sleep(1)` path) | **715-902/s**, down from ~24,000 wakes/s in v1: **fixed** |
| L1 timer sleeps | 5-51/s |
| **F1 timer sleeps** | **0/s for the whole session**: the FSR frame-gen presenter never ran, so **frame generation is not active** under Proton even if the menu says it's on (or it was off; the user can't see the overlay to check) |
| WK wakes kept / skipped | 6,383-22,198 kept vs **27-264 skipped** (<1-4 %): WK barely triggers on this workload; the wake storm is mostly "real" wakes |

So on the Ally, WK isn't reducing the job-system wake load (section 4.2) in a meaningful way. WorkerCap=10 is in effect; its effect on CPU power hasn't been measured yet.
