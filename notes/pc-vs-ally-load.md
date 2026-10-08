# Do the Ally load wins (EP, SR, ReadFileEx) help the PC? (2026-10-08)

PC: native Windows 11, RX 9070. Game folder is on C: (Steam library under `C:\Program Files (x86)\Steam`), C: is the
Samsung 980 NVMe (D: is the HDD). The game was not launched or touched. Bench: `tools\bench\pcio` (C#, P/Invoke,
`dotnet run -c Release`), 64 MB file fully cached, 3 runs each, median.

## 1. Native microbenchmarks (us per operation)

| test | PC | Ally (Proton) |
|---|---|---|
| CreateEventA + CloseHandle pair (x540k) | **0.57** | 84 |
| 1 KB random read, sync handle | **2.7** | 2.0 |
| 1 KB overlapped, per-read event + GetOverlappedResult | 4.0 | ~55 (46 in-call w/ EP) |
| 1 KB overlapped, pooled event (EP-style) | 3.6 | - |
| 4 KB ReadFileEx + SleepEx(alertable) | 5.0 (sync 2.9, overlapped+pool 3.9) | ReadFileEx site 740-810 us (device-bound, 2.7 GB far) |
| 256 KB ReadFileEx + SleepEx | 28.4 (sync 24.7, overlapped+pool 26.0) | same |

Windows completes a cached overlapped read inline, so overlapped costs only ~1 us more than sync. The Proton penalty
(~50 us async bookkeeping, ~84 us event pair) does not exist natively.

## 2. What the PC logs say (captures\pc\loadstats BASE_a/b, LW; 3 runs)

- Load window **25.3 / 26.2 / 26.4 s**, 8.3 CPU cores busy, ~2900 frames at 9 ms.
- Reads: 1554 MB archive in 68k calls = **0.97 s in-call total**, plus 653 MB "other" 584k calls = **2.24 s in-call**
  (3.8 us avg, consistent with the bench) summed over all threads, not wall.
- Inflate 5.5 CPU-s, PSO creation 0.00 CPU-s (22-31 gfx runtime, max 25 ms), event scene 0 s.
- Managers A/B busy 8.7 / 4.5 s, last busy +9.9 s: they drain ~10-12 s in, then ~13-15 s remain in gates/sequencer.
  No LoadGates holder line exists for the PC yet (G9 never armed; G5/G8 suspected), so that tail is unattributed.

## 3. Predictions (wall-clock seconds on PC)

| fix | arithmetic | PC gain |
|---|---|---|
| EP | 0.57 us x ~540k = 0.3 s of one thread (+ tiny for other sites) | **<= 0.3 s thread, ~0 s wall** (auto = off on Windows is right) |
| SR | refused on Windows; would save (3.6-4.0) - 2.7 = ~1 us x 536k = 0.5-0.7 s of the loader thread | **~0.5 s thread, 0-0.5 s wall** (not worth enabling) |
| ReadFileEx fix (631FD5) | 12.6k calls x (5 - 3 us) = 0.03 s; the 256 KB reads are memcpy/device bound | **~0** |

All three together: < 1 s thread time of a 25 s window. The Ally gains came from Wine/Proton overhead of 20-80 us per
call; on Windows that overhead is 0.5-1 us.

## 4. What bounds the PC load

Not I/O (3.2 s in-call across all threads, NVMe, 0 stalls), not PSO (0.00 CPU-s), not inflate (5.5 CPU-s over 4 threads,
~1.4 s wall). It is multi-core CPU work in the game's own streaming/object setup (8.3 cores average = near saturation
of the machine's cores) for the first ~10-12 s, then ~13-15 s in gates/sequencer states whose holder is unknown. To get
a PC load win, run the PC LG probe (LoadGates=1, STATUS queue) and attack the G5/G8 holder, not I/O.
