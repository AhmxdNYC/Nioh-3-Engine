# Nioh3EngineFix on ROG Ally (Bazzite / Proton): field report, 2026-10-06 (summary of the user's report)

Device: ROG Ally Z1 Extreme (8C/16T Zen 4, 780M 12 CU), 16 GB LPDDR5; Bazzite (kernel 7.2.7), gamescope Game Mode;
Proton Experimental 11.0 (2026-10-01); Mesa 26.2.2 RADV + vkd3d-proton (DXVK threads also present); ntsync active
(+ WINEFSYNC=1); VKD3D_SWAPCHAIN_LATENCY_FRAMES=3; TDP PL1 25 W / PL2 26 W / PL3 27 W. Game v2.02 (0x6AA7C106).
Loader: Ultimate ASI Loader dinput8.dll + WINEDLLOVERRIDES="dinput8=n,b". Streamline (sl.*) and libxess(_fg) load.

## Mod status
Loads; code decrypted 200 ms after start; all sites OK; "running under Wine/Proton 11.0"; with L1OnWine=1 and
FGOnWine=1: 10 groups applied, 0 skipped (default ini: 8, L1/F1 skipped). Fast GHDM shader installed.

## Bottleneck: GPU/power-bound
GPU busy 87–98 %; socket power ~26 W flat at the PPT limit; GFX power 9–21 W (takes whatever the CPU leaves);
sclk 1.4–2.2 GHz (max ~2.7) = power-throttled; CPU ~34 % of 16 threads; main thread ~96 % (82 user / 14 kernel).
=> main-thread savings don't become fps; saving CPU watts gives the GPU more power.

## Where the CPU goes (10–12 s windows; vcsw = voluntary context switches/s)
| Group | Threads | CPU % | vcsw/s | state |
|---|---|---|---|---|
| KIDSTaskScheduler | 31 | 305 | 9,889 | ntsync wait 84 %, running 11 % |
| [App]Game (main + helpers) | 31 | 99 | 814 | main running 99 % |
| KIDSTS[s2:Stage…] | 11 | 94 | 2,787 | ntsync wait 88 % |
| KIDSTS[s9:Colli…] | 12 | 68 | 818 | ntsync wait 94 % |
| SynergisticDispatch | 1 | 32–38 | 343–475 | ioctl 60 %, running 40 % |
| KIDSTS[s66:TS_M…] | 14 | 35 | 1,687 | ntsync wait 96 % |
| Nioh3.exe (TID==PID, the W3 message thread) | 5 | 23 (8 user / 15 sys) | 17,000–24,000 | ntsync WAIT_ANY + readv(wineserver) |
| KIDSTS[s4:UpdSt…] | 11 | 19 | 1,005 | |
| vkd3d_queue | 40 | 15 | 4,143 | futex 96 % |
| [App]MissionThread | 1 | 12 | 169 | futex 87 % |
| [App]InputUpdate | 1 | 10–12 | 8,500 | futex 73 % |
| wineserver | – | 12 | | |

Findings: (1) W3 under Wine returns from MsgWaitForMultipleObjectsEx after ~40 µs instead of sleeping (~24k wakes/s,
wineserver round trip each) -> FIXED in the ASI 2026-10-06 23:5x: W3WineSleep=1 uses Sleep(W3_TimeoutMs) under Wine.
(2) Job-system wake storm (~140 KIDS threads, ~10k wakes/s, 3–5 cores of activity) = largest power sink; ideas:
measure spin-before-block, a WorkerCap for handhelds (opposite of P12), wake coalescing. (3) [App]InputUpdate 8.5k
wakes/s (likely XInput/HID polling). (4) Main thread 14 % kernel (likely ntsync per job dispatch + faults).

## MangoHud run (mod on, L1OnWine=1, FGOnWine=1; user believed FG was on)
263 s steady state: 32.6 fps avg (28–40), p99 75 ms, CPU 72 %, GPU 91 %, CPU 12.2 W / GPU 18.1 W, GPU ~1,570 MHz.
~32 presented fps is hard to reconcile with FG active; FG state/cap unconfirmed. No stock baseline recorded.

## Feature requests
1. Always-on top-left counter: "Real FPS" (engine frames) and "FG FPS" (presented incl. generated), "FG: off" when
   not running; optional frame time and VRR speed factor. [Overlay] FpsCounter=1, Position=TopLeft, Scale=1.0.
2. Startup notification (5–10 s): loaded + version, groups applied/skipped, whether FG is actually active (and type),
   VRR + FPS tier, warning if Dynamic FPS is on, Wine detected + L1/FG state. Must draw in-swapchain (gamescope).

## Missing data
Stock baseline, default-ini vs L1/FG-on-Wine, FG off vs on, 30 W TDP, Windows on the Ally; stock /proc thread sample.
Wanted: an in-mod sampling profiler (top RVAs of KIDS workers and the message thread under Wine).
