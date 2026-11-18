# Streaming "lock" 0x491B04 = lazy archive-file open with close-on-last-release (pc/2, human #117226, 2026-11-18)

Claude RE + one live counter run on Main PC (Ryzen 5 7600X 6c/12t, RX 9070), EF 1c8e1245, FnProbe only (no game change).

## What the code is (static, image base 0x7FF74F410000)

- `LazyGuardAcquire 0x491B04(w)` calls vtable+0x10 of a per-file object = `0x491B30(file, 1)` (vtable at 0x38F4E38:
  +0x10 0x491B30 acquire, +0x20 0x491D50 AddRef (+0x2C users), +0x28 0x493050 release).
- `0x491B30`: spinlock at file+0x34 (cmpxchg, `Sleep(0)` every 256 spins). If no handle (file+0x10 == 0): check the owner's
  open-handle cap (owner+0x84, live value **64**) against owner+0x88 (open count), then open the file via
  `0x315064 -> 0x314A80` (MultiByteToWideChar + open of `.\archive\...`), then AddRef. Over the cap -> returns 0
  (caller defers: StreamPageLookupOrInsert `[ctx+0x18]++`).
- `0x493050` release: spinlock, users-- ; when users hits 0 -> `0x751784` closes the handle (`0x314098`), and if the
  file status is 2..7 (I/O in flight) first waits in `0x9AB0E4`; owner+0x88--.
- So every streaming burst that touches a file with no other user re-opens it, and closes it again when done.
  DeepSeek's "batch the guard" idea is already in the game: 0x116A24 memoises the guard across its page loop.

## Live counts (FnProbe, 15 s windows; D:/nioh3-re/sg117226/ef-log-full.txt)

| phase | file opens 0x314A80 | close-on-last 0x751784 | acquires 0x491B30 | page lookups 0x116A24 |
|---|---|---|---|---|
| boot + title + load (run 1) | peak **1,510/s**, ~29,400 in 48 s | peak 608/s | peak 12,696/s (release) | - |
| roaming, 12 LP | 67-130/s (peak 677) | 50-112/s | 300-1,070/s | 254k-443k/s |
| roaming, affinity 8 LP (4c/8t like GH #113) | 26-103/s (peak 424) | 16-56/s | 106-733/s | 301k-370k/s |

- owner open count (+0x88) read live: 0 most of the time, 19 once, cap 64. Files almost never stay open.
- Cost on this PC: open+close of an archive file = **26.7 us** (warm NVMe, `Temp/lock117226/ocbench.py`).
  Roaming ~100/s = ~2.7 ms/s (~0.3% of one thread); load peaks 1,500/s = ~40 ms/s (~4% of one thread).
  On a 4-core HDD/Defender PC each open is several times dearer, and a close can wait for in-flight I/O.
- 0x116A24 page lookups run 250k-440k times/s even while walking; 99% from 0x49EFB. That is the steady
  meshlet streaming cost (the "7%", notes/meshlet-mainthread.md), separate from the file churn.

## Fix idea (not built yet)

Keep released archive handles open (linger) while owner+0x88 stays below the cap minus a margin; evict the oldest
lingering file (take its +0x34 spinlock, users == 0, call the stock 0x751784) when an open would hit the cap.
Same picture, fewer CreateFile/CloseHandle calls. Measure with this same FnProbe list (opens/s must fall) plus
`[ctx+0x18]` denials (must not rise).

## A/B: FileLinger v1 (EF b60ac52a, branch pc2/streamguard-117226 aa18779a), Main PC, 90 s roam at 12 LP + 90 s at 8 LP

| arm | file opens 0x314A80 (whole session) | closes 0x751784 | field opens/s median |
|---|---|---|---|
| FileLinger 0 | 45,480 | 22,952 | 17.7 |
| FileLinger 1 (max 32, free 24) | 45,865 | 22,740 | 53.5 |

- **No gain.** Kept 13,387 handles, evicted 11,388 of them again, closed 11,351 at once (open count over cap - 24).
  Only ~2,000 of ~22,700 closes were saved (~9%), and the total open count did not fall (half the opens come from another
  opener, close site 0x3178C1). Streaming walks through far more than 32-40 distinct files; the game's cap of 64 leaves no
  room for a useful cache. No crash, no errors, install restored to EF 1c8e1245. **Not shipped**, branch kept as a record.
- Cost bound: even removing every open+close saves ~27 us x 20-130/s = 0.05-0.35% of one thread walking here.
  The file churn is not where the streaming CPU goes.
- The large number is **0x116A24 page lookups: 170k-440k calls/s** in the field (99% from 0x49EFB). That is the next target.
