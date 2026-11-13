# Main PC uncapped CPU profile (#96172, 2026-11-13 8:32-8:41 am EST)

Device Main PC RX 9070, public 1.8.13 (EF ba089162, BR 8076cdd6), 1440p FSR 50 %, FG off, REN_FPS 120 + [VRR] MaxFps 240, DS 0.
Spawn move+RB loop; pc_cpucut_arm.ps1 -Prof (patcher/cpuprof.fsx), 2 x 45 s. 87.9 / 85.4 fps, 7.93 / 7.68 cores, GPU 5.6-5.7 ms.
Captures: captures/pc-cpuprof-96172/uncap/20261113-083237-P (P-1/P-2-prof.txt + .tsv).

## Main thread [App]Game, ms per frame (frame 11.4 ms)
| # | function | share | ms/frame | what |
|---|---|---|---|---|
| 1 | 3FD10 KtglSignalUntilAwake2 (from 5DC18) | 19-22 % | 2.2-2.5 | Sleep(0) poll on M+5F8 = waits for MeshletRequestJob 4A2D0 (critical path) |
| 2 | 1DC2F0 DrawEmitSubPasses / 1E80B0 DrawEmit | 15.5-16.6 % incl | 1.8-1.9 | per-draw state diff + token write (self 0.64) |
| 3 | 5D214 MeshletFrameBegin | 13.3-13.6 % incl | 1.5 | incl 4AA34 MeshletCheckRequestLoad 0.31 self |
| 4 | 4BBEC MeshletUpdateSceneBuffers | 9.3-9.8 % incl | 1.1 | incl 1E7050 TextureStreamUpdate 0.7 |
| 5 | B94250/B94230 memcpy | 4.5-4.8 % | 0.5 | also 7D9E90 SpinWaitFlagSleep0 0.45, 418F40 drain spin 0.23 |

## Why #1 is long
TS_MESHLET_REQ9 is inside the root job 4A2D0 for 72 % of wall (~8 ms/frame). Join 403A0 55 %: children MeshletRequestLoad 48E90 31 %,
SleepEx 18 %, readbacks 5B3AC 8 %. In all six REQ threads 37-47 % of 48E90 samples sit in the 12-byte loop 49150..4915A: per request item it
re-sums dword node+14h over three linked lists (next node+30h) at [[rsi+1C8]+67A8/67B0/67B8] (67A8 skipped when byte +67C0 == r13b), times
[rsi+134h] << 16, as the capacity bound for [rsi+488h] + size (bail 4935B). Candidate: compute once per job (same picture).
Blocked on the writer map (deepseek #96240; Claude RE paused #92352). Already measured, no gain: LK, MA, MW, J1, WG (bundle #95941).
