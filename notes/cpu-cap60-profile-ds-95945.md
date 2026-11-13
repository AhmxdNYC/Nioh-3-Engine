# Main PC CPU at a 60 fps cap: profile + DS ABBA (#95945, 2026-11-13 7:39-8:04 am EST)

Device: Main PC (RX 9070). Public 1.8.13: EF ba089162, BR 8076cdd6, TR bcc4783e, CamKi a2f3e418.
Settings: 1440p, FG off, FSR 50 %, in-game cap REN_FPS 60 (the mod's [VRR] MaxFps only overrides the 120 tier), spawn move+RB loop.
Tools: patcher/cpuprof.fsx (pc_cpucut_arm.ps1 -Prof), pc_cpucut_arm.ps1 -RenFps 60 -Keys DS=0/1. Captures: captures/pc-cpuprof-95945, captures/pc-ds-95945.

## Profile at cap 60 (5.3-6.0 cores; uncapped 8.06 cores at 90-98 fps)
1. [App]Game 0.93 core: 32-35 % of its samples in RtlQueryPerformanceCounter from the main-loop 1 ms retry spin
   (Nioh3+1A74BD0..1A74C48, caller 1A74130) + 10 % loop self = ~0.4 core spinning. Existing fix: DS (default 0).
2. Nioh3CamKiProbe.asi thread 0.30-0.70 core for ~2-3 min after each launch: ScanCamSlice walks all private memory
   (16.5 GB) + mapped (286 MB) 128 MB per 12 ms tick, finds nothing (log: vtable-hits=0 tag-hits=0 obj=0), then stops.
   Camera hooks work without it. Fix = drop or gate the scan (CamKi owner).
3. KIDSTaskScheduler0 0.45: job work + SRW condvar waits, no spin.
4. SynergisticDispatcherTask 0.32: ~50 % blocking waits, rest D3D12/amdxc64 submit.
5. KIDSTaskScheduler1-6 ~0.15 each: mostly blocking waits.

## DS ABBA at cap 60 (A=DS 0, B=DS 1, same DLL, 2 x 60 s per launch; log: DS applied, ~730 timer sleeps/s avg 1041 us)
| Arm | cores w1 | cores w2 | mean cores | 1% low mean | fps |
|---|---|---|---|---|---|
| A OFF (launch 1, 4) | 5.79, 5.62 | 4.93, 4.73 | 5.27 | 44.6 | 60 |
| B ON (launch 2, 3)  | 5.31, 5.44 | 4.55, 4.61 | 4.98 | 46.6 | 60 |
DS ON: -0.29 core (-5.5 %), every ON window below both matching OFF windows; 1 % low +4.5 %; fps cap held. Same picture.
Verdict: KEEP. Passes the keep rule (CPU >= 3 % lower, p99 not worse). Candidate to ship DS = 1 by default (needs L1, on by default).
