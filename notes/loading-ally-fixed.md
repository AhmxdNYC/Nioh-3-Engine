# Why the Ally's title -> Continue load is a fixed 118.7 s (candidates, fixes, the Ally test)

Date: 2026-10-08. Static work only; the game was not run. Inputs: every Ally mod log in `captures\ally\` (L1-L3, v4-v9,
v7.1), the decompile (`D:\nioh3-re\decomp` + decomp_ro), `notes\loading.md`, `notes\loading-bottleneck.md`.
New digest: `py -I re\scripts_load\ally_fixed_digest.py captures\ally\*modlog*.log` (window length, per-period frames,
game ticks, GF, VRAM/RAM, plus frames and ticks integrated over each 58A window).

## 1. What the logs pin down

| run | window (58A set -> clear) | frames in window (OSD presented-frame counter) | game ticks (TG, interpolated) |
|---|---|---|---|
| L1 | 118.657 s | ~2007 | ~4950 |
| L2 | 118.688 s | **~2174 (+8 %)** | **~4310 (-13 %)** |
| L3 | 118.737 s | ~2007 | ~5060 |
| v7.1 (MAX settings) | 118.281 s | ~2012 | n/a |

1. **The end is a wall-clock timer (or something paced by an outside clock), not frames and not game time.** L2 renders 8 %
   more frames and runs 13 % fewer game ticks than L1/L3, but ends within 0.03 s of them. The OSD's game-speed samples in the
   window range from 0.58x to 1.09x, so a dt60 (game-time) timer would drift by seconds between runs. The spread between
   runs (0.08 s) is about one frame at 15-20 fps, which is how much a polled wall-clock deadline should jitter.
2. **The timer starts before 58A is set.** v7.1 (MAX settings, slower frames) is 0.4 s *shorter*. That fits a deadline
   armed at an earlier event E, with the 58A set coming a frame-dependent delay after E: window = T - lead. With
   T = 120.0 s, the lead is 1.26-1.34 s (L1-L3) and 1.72 s (v7.1). On the PC the loading UI opens 2.10 s before the set,
   so E falls inside that fade.
3. **The game is not done at the deadline.** VRAM keeps rising 20-30 s after the clear. In the bucket before the clear,
   UTM falls from 11-25 to 2.5 tiles/frame, so streaming is still running.
4. **The window is not throughput-bound.** VRAM growth differs between runs (second bucket: L1 19.9 MB/s, L3 11.7 MB/s),
   yet the runs end together. So it is not a byte-rate cap or a "disk speed" estimate.
5. **The PC never reaches the timer.** The same route takes 25.8 +-0.5 s there, which varies the way work-bound loads do.
   Something that finishes on Windows never completes under Proton and runs into its timeout instead.

## 2. Candidates, ranked

| # | candidate | timeout / count vs 118.7 s | evidence | verdict |
|---|---|---|---|---|
| 1 | **an online/network operation that never completes under Proton and times out at 120 s** (KTGL online lib: P2P session join `OnlineSessionP2pSessionJoinParam` 1356AA0, timeout **120000 ms** at imm **1356BDC** (`C7 45 9F C0D40100`), timed with clock()/QPC depending on [45BA780]+12h; the lib also has HOST / SEARCH / LOCK / AUTO_JOIN *_TIMEOUT error codes) | 120.0 - lead 1.3 s = 118.7 | wall clock (1.1), armed before the set (1.2), 120 s is the only ~2-minute wall-clock constant found in the export; the game runs its own UDP P2P sockets (ws2_32 sendto/recvfrom/bind/ioctlsocket), which behave differently under Wine/NAT; the PC never hits it | **most likely; not yet tied to the load-end path** (the join is called through a vtable, 2 DATA refs, so the static link to the gate is open) |
| 2 | **G5/G8 object/area readiness held by #1 or by another async request** (ObjectListAnyNotReady 13335C: map at [45B9E60], node[5]->+2C8h == 0; StageAreasObjectsReady 3B3A28: area +30h obj +1F0h / +1ECh, entry +1C0h) | whatever the request's timeout is | the PC's ~15 s post-drain gap is in G1-G8 or a sequencer state, and G5/G8 is the prior (loading-bottleneck §3.2); an online-fed object (graves, visitors) would hold G5 until its request answers or times out | the way #1 would reach the load end; LG decides |
| 3 | WinINet HTTP request (only user: 322F908, which calls `HttpSendRequest(Ex)A`/`InternetReadFile`; `InternetSetOption` is **not imported**, so the requests use Wine's default timeouts, not the Windows ones) | Wine default connect/receive timeouts (to be measured) | wall clock; one call site | medium-low until a timing shows it inside the window |
| 4 | a fixed-length scene, movie or sound (Movie::IsEndOfPlay, RTM) behind the cover | the clip length | external-clock exact | low: the PC shows 0 event-scene seconds on the "same" route. Rule out a route/save mismatch first (LoadSeq owner, MB read) |
| 5 | GPU fence/readback wait that times out under vkd3d | - | GF counts 80-340 fence waits/s at an average of 3-10 ms, 0 fallbacks; nothing waits for seconds | ruled out |
| 6 | frame-count settle (N frames x Ally frame time) | ~2000 frames | L2 +8 % frames, same end | ruled out |
| 7 | dt60 timers: LoadIsLoadEnd ready-wait (acc 45C84A4 += GetDeltaTime60 = G[624]*60/G[614], > 120 units), FastTravelSeq/WarpSeqB 2-25 units | 120 units = 2-5 s at 24-60 ticks/s | game ticks differ by 13 % between runs | ruled out as the 118 s source (still a few seconds of the window) |
| 8 | XAudio2 voice drain (3224564: GetState until BuffersQueued == 0, 50 ms waits) | the clip length | blocking mode only from the audio shutdown 3224B7C; the per-frame path 5A65D4 calls it non-blocking | ruled out |
| 9 | explicit WaitForSingleObject timeouts | 5000 ms (AEF3A0), <= 120 ms elsewhere | no 118-120 s constant among the 4.9k exported functions | low (the export is partial) |
| 10 | byte-rate / disk-speed budget | - | VRAM slope varies between runs, end does not (1.4) | ruled out |

## 3. Fix designs (default-off ASI flags; build only after §4 names the holder)

| if §4 shows | flag | design | same look because | expected |
|---|---|---|---|---|
| #1: the online join/host times out inside the window (offline run is PC-like) | **OJ** | first find which call fails under Proton (the LT probe below logs the KTGL error code and the socket call), and fix that call if it is a Wine-semantics difference (game-side, e.g. how the game uses a socket option). Fallback: while 58A is set, lower the 120000 ms at 1356BDC to a cap that is above the PC's measured join time (LT logs it) | a join that would time out ends in the same offline state, only sooner. Joins that succeed finish below the cap, so they are unchanged | up to ~100 s on this route (118.7 -> PC-like 26-40 s) |
| #2: G5/G8 holds until about the clear and the not-ready object is network-fed | OJ (same) | as above | as above | as above |
| #2: G5/G8 holds and the object waits on streaming | LS | streaming in-flight caps while 58A is set (loading-bottleneck §4) | same requests, more in flight | unknown |
| #3: a WinINet call blocks for most of the window | **HT** | after the game's InternetOpenA/HttpOpenRequestA, set the Windows default connect/send/receive timeouts with InternetSetOptionA (GetProcAddress) | same request and same reply; only the failure timing changes, to what Windows does | the timeout difference |
| #4: a scene or movie, or a route mismatch | - | write-off (it is the look) | - | 0 |

None of these is built yet. OJ changes when a failing online join gives up (not the look). Flag it as a behaviour change in
the menu text, the same way TG is flagged.

## 4. Ally measurement (one session, ASI 1.7.17, no new build needed for steps 1-2)

ini: `[Profiler] LoadStats = 3`, `LoadGates = 1`, `MemStats = 1`, `SP = 1`, `ReportSec = 5`. Everything else as in L1.

1. **Title -> Continue x2, online as usual.** Read `grep "LoadStats\|LoadGates\|LoadSeq"`:
   - `LoadSeq #n owner at set` (FT / B / script) and the `LoadStats tick out` lines before the set (they show the lead).
   - `LoadGates #n holder:` with the held seconds per gate, `last failing eval` and `first full-pass eval`.
   - Decide: (a) G5 or G8 fails until about +118 s -> #2, then step 3. (b) a full pass well before the clear while the owner
     waits -> a script/online wait (#1/#3/#4). (c) the evaluation count stops rising -> the owner waits on something other
     than LoadIsLoadEnd.
2. **Title -> Continue x1 with the game in offline mode** (the game's own title/network option; this is a measurement, not
   a fix). If the window drops to ~26-40 s, the online path is confirmed (#1). If it stays at 118.7 s, #1 and #3 are out.
   Then compare `io` MB read with the PC's 1554 + 653 MB to rule out a route/save mismatch (#4).
3. Only if steps 1-2 point at #1-#3, build **LT** (sonnet, new file, `[Profiler] LoadWaits = 1`, default 0). It is not
   built yet; it logs per 58A window:
   - an entry hook at 1356AA0 (issue time, timeout argument);
   - the KTGL online error codes raised;
   - an IAT hook on the 11 WININET imports (per call: duration and result);
   - an IAT hook on WaitForSingleObject(Ex)/WaitForMultipleObjects, counting the WAIT_TIMEOUT returns with a timeout of
     at least 1 s, by caller RVA;
   - at each 1 s tick while G5/G8 fail: the first not-ready object's vtable RVA ([45B9E60] map node[5]) and the area
     index (*entry >> 4) with its +1F0h/+1ECh/+1C0h.

   This yields "the X call blocked from +a to +b, then timed out". That names the OJ/HT target.

The PC LG turn (STATUS queue item -2) runs the same probe. If the PC's ~15 s post-drain gap is the same online wait
completing normally, the PC shows it as the same holder.
