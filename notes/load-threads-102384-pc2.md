PC2 load worker-thread attribution #102384 (performance-pc2/0), 11-14 4:35-4:50 pm EST
Device PC2 Ryzen 5 5600X 6c/12t, RX 6650 XT, NVMe Micron MTFDKBA1T0QFM 1 TB; user ini (VRR tier/cap 120: loads render ~100 fps, 10 ms frames)
Build: public BR 376532e9 TR 1cfd0921 CamKi e6142afc + LoadSampler EF built here from 1daec982 (sha 3f077474; b24d4bd3 = same source, other build)
Per launch: fightnow_test -Rows R1CBC (title Continue = load #1, Boss Match R1CBC = load #2); [Profiler] LoadStats=1 LoadSampler=1 ReadProbe=1
Per-thread: thrsamp.py (NtQuerySystemInformation thread state/wait reason ~60 Hz + thread CPU time), process ReadTransferCount
load          total s   proc CPU  read MB/s  FileLoad run  FsHdd in ReadFile  main run/ready  CamKi probe
Continue L1   39.6      8.37      115        73 %          48 %               55/37 %         0.47 cores (67 % run)
Continue L2   36.7      8.29      123        76 %          42 %               54/38 %         0.43
Continue L3   41.9      7.84      109        77 %          48 %               59/34 %         0.45
BossMatch L1  26.0      8.91       83        74 %          44 %               67/17 %         0.75
BossMatch L2  39.4      8.56       57        76 %          34 %               54/24 %         0.68
BossMatch L3  30.5      8.65       70        76 %          38 %               61/16 %         0.77
KIDSTaskScheduler pool (7 thr): ~1.8-2.1 cores, 18-22 % run, rest UserRequest waits. KIDSTS[s6:TrySync] 0.35-1.06 cores.
Main thread LoadSampler exe-frame top: 916EBC 8-23 %, CndWaitSrw B616A4 8-9 %, JobWorker 411C0 ~8 %; no FrameEnd (cap 120 not binding).
Reading: not IO-bound (57-123 MB/s on NVMe; FsHdd thread in ReadFile 34-48 %). CPU-bound/contended: ~8.5 of 12 logical busy, main thread ready-not-running 16-38 %.
Named >=30 %: KTGLGraph.Cache.FileLoad (1 thread, start Nioh3.exe+bdb740 trampoline) in Running state 73-77 % of every load window.
Side find: Nioh3CamKiProbe worker (probe.cpp ScanCamSlice, 128 MB per 12 ms tick) burns 0.4-1.0 core for ~2 min per launch spanning both loads (69 CPU-s in L2), scanning 15 GB and finding no camera (obj=0), 378 full scans in the log.
Caveat: thread CPU from thread times (15.6 ms tick granularity); FileLoad 'run 75 %' vs 0.55-0.74 cores = same thread, two meters.
Restored: public EF d7cb9d69, inis byte-equal, saves verified byte for byte each launch, lock released.
