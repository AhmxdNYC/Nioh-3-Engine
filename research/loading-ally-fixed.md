# The Ally's title -> Continue load window (118.7 s): candidates, read fixes, Ally tests

**Current understanding (§7-§8.9):**
- No timer ends the window. In the 1.7.19 logs the load's work ends at about +93 s (manager A drains and the last
  LoadIsLoadEnd eval happens). The rest of the old 118.7 s window, about 25 s, is the tip page waiting for a button press.
  The fixed length of the old windows most likely came from the test harness pressing at a fixed delay.
- An LG probe run credits G9 LoadCharacterReady with ~26 s at 7.5 cores. That is the tip page: the probe credits
  post-eval time to the last failing gate. G8 StageAreasObjectsReady is the long gate of the real load.
- About 7.5 cores is the idle baseline with stock spins. The load adds 3-5 cores. The critical path is manager A
  request latency (reads ~30x slower per call than on PC), not CPU throughput.
- Measured (docs/BENCHMARKS.md, section (c)): title -> Load Game (Level 136), cold first launch, 96.0 s with the mod
  installed and EP off -> 47.3 s with EP (1.7.23). A paired SR test on 1.7.25 gave 47.25 -> 38.45 s. EP and SR switch on
  automatically under Proton. These numbers have not been re-measured on 1.7.61.
- §1-§6 are the static analysis of the first hypothesis (a 120 s online-op timeout). §7 rules that hypothesis out; the
  KTGL online, ws2_32 and WinINet facts in §5 still stand.

Date: 2026-10-08. Static work only; the game was not run. Inputs: every Ally mod log (L1-L3, v4-v9, v7.1), the Ghidra
decompile, research/loading.md, research/loading-bottleneck.md. A digest script (`ally_fixed_digest.py`, private dev
repo) reports window length, per-period frames, game ticks, GF, VRAM/RAM, plus frames and ticks integrated over each
58A window.

## 1. What the logs pin down (first reading; §7 supersedes the timer interpretation)

| run | window (58A set -> clear) | frames in window (OSD presented-frame counter) | game ticks (TG, interpolated) |
|---|---|---|---|
| L1 | 118.657 s | ~2007 | ~4950 |
| L2 | 118.688 s | **~2174 (+8 %)** | **~4310 (-13 %)** |
| L3 | 118.737 s | ~2007 | ~5060 |
| v7.1 (MAX settings) | 118.281 s | ~2012 | n/a |

1. **The end is paced by an outside clock, not frames and not game time** (§7: the outside clock is the button press). L2 renders 8 %
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
5. **The PC is work-bound.** The same route takes 25.8 +-0.5 s there, which varies the way work-bound loads do. The
   Ally's own work ends at ~+93 s (§7); the gap to the PC is per-request read latency, not a timeout.

## 2. Candidates, ranked

| # | candidate | timeout / count vs 118.7 s | evidence | verdict |
|---|---|---|---|---|
| 1 | **an online/network operation that never completes under Proton and times out at 120 s** (KTGL online lib: P2P session join `OnlineSessionP2pSessionJoinParam` 1356AA0, timeout **120000 ms** at imm **1356BDC** (`C7 45 9F C0D40100`); the clock()/QPC there only seeds a random pick, and 120000 ms is the default timeout of every KTGL online op param, not a join-only value (§5); the lib also has HOST / SEARCH / LOCK / AUTO_JOIN *_TIMEOUT error codes) | 120.0 - lead 1.3 s = 118.7 | wall clock (1.1), armed before the set (1.2), 120 s is the only ~2-minute wall-clock constant found in the export; the game runs its own UDP P2P sockets (ws2_32 sendto/recvfrom/bind/ioctlsocket), which behave differently under Wine/NAT; the PC never hits it | **ruled out (§7): no timer ends the window.** Statically the auto-join is not tied to the load-end path either (§5.1) |
| 2 | **G5/G8 object/area readiness held by #1 or by another async request** (ObjectListAnyNotReady 13335C: map at [45B9E60], node[5]->+2C8h == 0; StageAreasObjectsReady 3B3A28: area +30h obj +1F0h / +1ECh, entry +1C0h) | whatever the request's timeout is | the PC's ~15 s post-drain gap is in G1-G8 or a sequencer state, and G5/G8 is the prior (loading-bottleneck §3.2); an online-fed object (graves, visitors) would hold G5 until its request answers or times out | G8 is the long gate, but it waits on manager A's resource requests, not on an online op (§8.9) |
| 3 | WinINet HTTP request (only user: 322F908, which calls `HttpSendRequest(Ex)A`/`InternetReadFile`; `InternetSetOption` is **not imported**, so the requests use Wine's default timeouts, not the Windows ones) | Wine default connect/receive timeouts (to be measured) | wall clock; one call site | ruled out with #1 (§7) |
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
| #1: the online join/host times out inside the window (offline run is PC-like) | **OJ** | first find which call fails under Proton (the LT probe below logs the KTGL error code and the socket call), and fix that call if it is a Wine-semantics difference (game-side, e.g. how the game uses a socket option). Fallback: while 58A is set, lower the 120000 ms at 1356BDC to a cap that is above the PC's measured join time (LT logs it) | a join that would time out ends in the same offline state, only sooner. Joins that succeed finish below the cap, so they are unchanged | none: the timeout case does not occur (§7) |
| #2: G5/G8 holds until about the clear and the not-ready object is network-fed | OJ (same) | as above | as above | as above |
| #2: G5/G8 holds and the object waits on streaming | LS | streaming in-flight caps while 58A is set (loading-bottleneck §4) | same requests, more in flight | unknown |
| #3: a WinINet call blocks for most of the window | **HT** | after the game's InternetOpenA/HttpOpenRequestA, set the Windows default connect/send/receive timeouts with InternetSetOptionA (GetProcAddress) | same request and same reply; only the failure timing changes, to what Windows does | the timeout difference |
| #4: a scene or movie, or a route mismatch | - | write-off (it is the look) | - | 0 |

None of these is built, and §7 rules out the online-timeout cases. OJ would change when a failing online join gives up
(not the look); it would need the same behaviour-change flag in the menu text as TG.

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
3. Only if steps 1-2 point at #1-#3, build **LT** (new file, `[Profiler] LoadWaits = 1`, default 0). It is not
   built; it would log per 58A window:
   - an entry hook at 1356AA0 (issue time, timeout argument);
   - the KTGL online error codes raised;
   - an IAT hook on the 11 WININET imports (per call: duration and result);
   - an IAT hook on WaitForSingleObject(Ex)/WaitForMultipleObjects, counting the WAIT_TIMEOUT returns with a timeout of
     at least 1 s, by caller RVA;
   - at each 1 s tick while G5/G8 fail: the first not-ready object's vtable RVA ([45B9E60] map node[5]) and the area
     index (*entry >> 4) with its +1F0h/+1ECh/+1C0h.

   This yields "the X call blocked from +a to +b, then timed out". That names the OJ/HT target.

A PC LG run uses the same probe. If the PC's ~15 s post-drain gap is the same online wait
completing normally, the PC shows it as the same holder.

## 5. Static link: KTGL online ops vs the load end (2026-10-08, static only)

Tools: a capstone/numpy xref pass over the exe dump (E8/E9 call index, rip-relative and abs64 refs). Most of the
online lib is not in the decomp export. The names are in symbols/symbols.tsv.

**5.1 What 1356AA0 (P2pAutoJoin_PickAndJoinCont) really is.** It is the *auto-join* continuation, not a generic join:
`Network::SessionP2p::join(JoinAutoParameter)` 287CD1C -> 2996CF0 -> 1352410 -> 1352BB0 (builds a functor, vtable 3B89468)
-> invoke slot 1357C90 -> **1356AA0**. It forwards an error from the previous step (the search). Otherwise it picks a
**random** entry of the search results (online mgr 31FA9EC: +480h search id, +488h list, +494h count) and issues
`OnlineSessionP2pSessionJoinParam`. The clock()/QPC read ([45BA780]+12h selects which) only seeds the RNG
(`(t*0x10DCD+1)>>16 % count`); it is not the timeout clock. The only game caller of the auto-join is 21BF38C, slot [1] of
the online-request vtable 402E840 (ctor 21B9720). That object is created by the online-flow state machine 21D8EE0..21E027C
(state handler set by 21DFE4C, vtable 402EDD8). This is a matchmaking flow (random co-op/visitor join). Nothing ties it to
LoadIsLoadEnd, the FT/WarpB sequencers or the title Continue path. **A solo title -> Continue should never run an
auto-join.**

**5.2 120000 ms is the default of every KTGL online op.** The param base ctor 2992370 (`OnlineSessionP2pActivateParam`,
also the base of Search/Create/Join/AutoJoin/CreateOrJoin) sets +18h = 120000 (imm 29923B6). A setter 29949D4 and an
inline init 2997D28 do the same. A second family, 32EE510 (vtable 40D4A78) and its caller 228CD84, also passes 120000.
So the 120 s is not specific to joins: any KTGL online op (activate, search, create/open, lock, CloudStorage, HTTP) that
never completes ends at 120 s. Patching only 1356BDC would cap the auto-join and nothing else.

**5.3 Game-side issuers of online ops.** All go through g_NetMgr [45C84F8] thunks (+F0h SessionP2p, +328h CloudStorage,
+3F0h Leaderboard, +480h Statistics, +20h/+78h friend/block lists, +D0h UserRestriction):
- SessionP2p: open 21BF4F8, search 21C2340, join(search) 21C1FEC, join(auto) 21BF38C;
- CloudStorage.open 21BF568;
- +F0h calls with int arguments from A62110, 1C9BA90, 1C9F194..1C9F734, 21BDB80 and 21E10FC (thunks 287FD10, 287FF64;
  likely session data or lock updates);
- close 2879B98, called from 10C3BA8, 1C30528 and 21DD8D4..21DE8F0.

All of these are request objects (vtables 402E6E0..402EEFF) run by the online layer. **There is no static path from any of
them into LoadIsLoadEnd G1-G9, FastTravelSeqTick or WarpSeqBTick.** Those functions reference no g_NetMgr or other online
state. An online op can hold the load end only indirectly: through an object or area that waits on online data (G5/G8),
or through a script wait (the natives Mission::StartSessionLock* 23A604C / 23A5FE4 exist, and KTGL has
LOCK_SESSION_TIMEOUT). LT has to measure this; static analysis cannot close it.

**5.4 ws2_32 surface (complete, 14 imports) and Wine-vs-Windows semantics.**

| site | call(s) | Windows vs Wine | can it hang? |
|---|---|---|---|
| P2pUdp_Open 322C124 (Steam P2P platform, ctor 322BFFC, vtable 40C8778) | socket(AF_INET, DGRAM), ioctlsocket(FIONBIO=1), bind(INADDR_ANY, 12000+rand 8000), 10 retries | same. No SO_REUSEADDR / SO_EXCLUSIVEADDRUSE (setsockopt is not imported), so reuse semantics cannot differ | no (non-blocking, bounded retries) |
| P2pUdp_DrainRecv 322C418 | recvfrom loop while ret > 0 | Windows can return WSAECONNRESET on UDP after an ICMP port-unreachable; Linux/Wine does not on unconnected sockets. Either way the loop stops for this tick (WSAGetLastError is not imported) | no |
| P2pUdp_SendTo 322C4DC | sendto, result ignored | same | no |
| Net_GetLocalIPv4_Loopback 323F6C4 | UDP connect(127.0.0.1:53) + getsockname | returns 127.0.0.1 on both | no |
| WSAStartup 134F550 / 1351B10, WSACleanup 134FC30 | | same | no |

Not imported at all: select/WSAPoll, TCP connect, setsockopt, WSAGetLastError, getaddrinfo/gethostbyname, IPv6 APIs and
broadcast options. **No ws2_32 call in this binary can block, or behave differently enough under Wine to stall an op.**
The real network transports are the Steam API (lsteamclient -> native Steam client; SteamMatchmaking009, ping data:
`KTGL_ONLINE_E_SESSIONP2P_STEAM_GET_PING_DATA_FAILED`) and **WinINet**:

| KtglHttp_RequestSync 322F908 | semantics | Wine risk |
|---|---|---|
| InternetOpenA(agent, INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, 0) | synchronous handles, no status callback | the PRECONFIG proxy comes from Wine's registry/env, not WinHTTP autoproxy |
| HttpOpenRequestA flags | RELOAD + NO_CACHE_WRITE (+ NO_AUTO_REDIRECT per a request bit); https adds SECURE (+ IGNORE_CERT_CN/DATE per request bits) | TLS goes through Wine schannel (GnuTLS) |
| HttpSendRequestExA + InternetWriteFile loop + HttpEndRequestA (bodies), or HttpSendRequestA | each call blocks the worker thread | a stalled call is only noticed between calls: the cancel token ([rbp+1140h]->+30h bit0, 320AA94) is polled after each WinINet call, never during one |
| HttpQueryInfoA(RAW_HEADERS_CRLF), InternetReadFile loop | | |
| InternetSetOption | **not imported**: the process runs with the implementation's default connect/send/receive timeouts | the Windows and Wine defaults may differ; check the Proton build's wininet source rather than assume |

So the only Wine-sensitive paths that can stall an online op until its 120 s op timeout are **a blocking WinINet call or a
Steam call result/callback that never fires**. A socket cannot do it.

## 6. LT probe and OJ fix (spec; default-off; nothing built)

### 6.1 LT (`[Profiler] LoadWaits = 1`, default 0, new file `loadwaits.cpp`)
Log per 58A window, plus the 30 s before it (ring buffer, flushed at the clear). All times are seconds relative to the 58A
set.
1. **KTGL op issue.** An entry hook on the param base ctors (2992370, 2997D28) is not enough, because they carry no op
   identity. Hook the game-side issuers instead: the request exec slots 21BF4F8 (open), 21C2340 (search), 21C1FEC (join
   search), 21BF38C (join auto) and 21BF568 (Cloud.open), and the g_NetMgr thunks 287FD10 / 287FF64 / 287F258 / 2879B98
   (call-site or entry hooks on the verified-bytes thunk path). Log `op=<name> t=<s> caller=<RVA>`, plus the param's
   timeout (+18h) where the param pointer is an argument.
2. **Op completion or error.** Log every non-zero first dword of the 16-byte OnlineErr passed into continuations (invoke
   slots like 1357C90), with its code, and log any `KTGL_ONLINE_E_*TIMEOUT` explicitly (the strings are at 3973A50..3976358;
   their table gives the code -> name map).
3. **WinINet.** IAT hooks on all 11 imports. Per call log start, duration, return value and GetLastError on failure; log
   the host from InternetConnectA and the verb/path from HttpOpenRequestA. Do not log query strings or headers, because
   they may carry tokens.
4. **Steam.** Count SteamAPI_RunCallbacks calls (the 5A612C caller) per second. Log RegisterCallResult/UnregisterCallResult
   pairs with the time between them, and flag any pair longer than 5 s.
5. **ws2_32** (cheap, for completeness): sendto/recvfrom counts per second, and recvfrom returns <= 0.
6. **Waits.** WaitForSingleObject(Ex)/WaitForMultipleObjects calls that return WAIT_TIMEOUT with a timeout of at least 1 s,
   by caller RVA.
7. The LG gate holder per 1 s tick (LG already logs it), so a long op can be lined up with G5/G8/G9.

Decide: the OJ target is an op or WinINet call that is issued before the set and still open when G8/G9 stop failing, or
that ends with a TIMEOUT code at about +118 s. If every op finishes in under 5 s, the online path is out. The window is
then the G8/G9 CPU work (loading-bottleneck §4, rows LB/LS).

### 6.2 OJ (`[Load] OnlineJoinCap`, default 0; build only after LT names an op)
- **Variant A (preferred: fix the API usage).** If LT shows a WinINet call blocking for most of the window, call
  `InternetSetOptionA(hRequest, INTERNET_OPTION_CONNECT_TIMEOUT / SEND_TIMEOUT / RECEIVE_TIMEOUT, ...)` with the Windows
  defaults right after the game's `HttpOpenRequestA` returns (GetProcAddress on wininet; no exe edit). The request and the
  reply are the same; a stalled call just fails when Windows would fail it. If LT shows the proxy lookup is what blocks,
  change the access type the game passes (`INTERNET_OPEN_TYPE_DIRECT` instead of PRECONFIG), and only then.
- **Variant B (fallback: cap the op timeout).** While 58A is set, and only then, make the param base write a smaller
  timeout for the op type LT named. Either hook 2992370 / 2997D28 / 29949D4, or patch their imm32 120000 to the cap at
  29923B6, 2997D3F and 29949D7 (and the join site 1356BDC) and restore them at the clear. **Cap = max(3 x the PC's measured
  time for the same op, 15 s)**; LT's PC run supplies the PC time. Ops that succeed finish under the cap and are
  unchanged. A failing op ends in the same offline/failed state, only sooner.
- This is a behaviour change, not a look change. The menu text must say it ("online: gives up a stuck online request
  sooner while loading"), the same treatment as TG. Never enable it for an op that also times out on the PC, because that
  would change PC behaviour too.

Scope: OJ could only save the part of the window where an online op alone is outstanding; it cannot touch the G8
resource work. §7 (1.7.19 log digest) shows that no timer ends the window at all (the load work ends at +93.6 s; the rest
is the tip page), so #1 is ruled out. The static result agrees: no ws2_32 call can stall under Wine, and nothing links an online op to the load-end gates. **LT and OJ stay
unbuilt**; this spec is kept only in case a later route (e.g. a co-op or visitor load) shows a KTGL `*_TIMEOUT` inside a
window.

## 7. Probe results: Ally ASI 1.7.19, online title -> Load Game -> Level 136, L1 + L2 (log digest, 2026-10-08)

Logs: two Ally mod logs from ASI 1.7.19 (L1, L2). The ini had
LoadStats=3, LoadGates=1, MemStats=1 and SP=1. **GF, W6 and RWb were off**, so the game's own spins were running.

New digest: `ally_load_sp_phases.py <log> [--phases name:a:b,...]` (private dev repo). It buckets the SP rows and the
1 s ticks by gate phase.

### 7.1 Timeline

Offsets are from the 58A set. L2 matches L1 within 0.5 s until the tail.

| phase | L1 s | CPU cores | fps | MB/s read | manager A | what holds |
|---|---|---|---|---|---|---|
| pre-gate | +0..+30.5 | 12.4 (13.8 in the last 7 s) | 36 (25 late) | 10 | at cap 10 | LoadIsLoadEnd not called yet; in L2, G2 StageAreaListBusy at +25.5..+33.4 |
| **G8** StageAreasObjectsReady | +30.5..+77.4 | 10.5 | 43 | 23 | at cap 10 | area objects wait for their resources |
| G9 first-failing | +77.4..+81.6 | 10.7 | 39 | 24 | at cap | - |
| **G5** ObjectListAnyNotReady | +81.6..+92.1 | 11.5 | 29 | 17 | at cap; drains at **+93.5** | - |
| last LoadIsLoadEnd eval | **+93.6** (L2: +93.1) | - | - | - | 0 | **the load's work ends here** |
| tail (tip page) | +93.6..+115.8 (L2: ..+298.5) | **7.7** | 42 | 1-8 | 0 | waits for the A press |

Reading the timeline:

- **G9 is not a holder.** The probe credits the time after the last evaluation to the last gate that failed.
  - LoadIsLoadEnd stops being called at +93.6 s.
  - G9 also fails all the time outside loads (the title and gameplay `tick out` lines).
  - So the G9 time (26.5 s in L1, 208 s in L2) is the tip page, and so is the "7.5 cores, ~0 reads" period.
  - **Probe fix:** report "work end = last eval" and stop adding held time after the last eval.
- **The 120 s online-timeout hypothesis (§2 #1, OJ/HT) does not hold.** No timer ends either window; each one ends on the
  button press (L1 at 115.8 s, L2 at 298.5 s). The old fixed 118.7 s windows were most likely the Ally harness pressing at
  a fixed delay.
- What the player waits for on this route: Continue -> 58A set (not measured) + **~93 s** + their own press. The PC does the
  same route's work in +10..13 s (its managers drain then).

### 7.2 What the busy cores do

`CPU cores` comes from GetProcessTimes. Under Proton that counts every thread in the process: spin waits, and threads
that are not [App] threads. The SP RIP share gives the threads that are running game code (round-robin share x thread
count).

| phase | CPU | above the tip-page baseline | KIDSTS threads in game code | main thread |
|---|---|---|---|---|
| tail (no load work) | 7.7 | 0 | 2.0 | 57-65 % in FenceWait Sleep(0) (IsFenceComplete 1180A4) |
| pre-gate | 12.4 | +4.7 | 4.5 (8.6 during G2 in L2) | 32 % [unix] + 16 % [ntdll] in MainLoopFrame 1A74130: QPC/Sleep(0) spin while FrameStep is not ready |
| G8 | 10.5 | **+2.8** | 2.7 | 37 % GfxFencePollWorker 117D6C Sleep(0) + 13 % FenceWait: waiting for the GPU/upload ring |
| G5 | 11.5 | +3.7 | 3.7 | 28 % [ntdll] <- MeshletExtraDispatch 5DA48 + 21 % FenceWait |

1. **About 7.5 cores is the idle baseline with stock spins.** The scene keeps rendering behind the cover at 42 fps, and
   Sleep(0) spins burn whole cores under Wine. The load adds only 3-5 cores on top. It is not 10-11 cores of load work.
2. **The extra worker CPU is resource-dependency polling.** The top game rows on KIDSTS in every load phase:
   - ResourceIsReady 1A6030 (CAS busy bit, stage step);
   - ResourceDependenciesReady 1A6820;
   - ResHandleListAllReady 1A19E0;
   - ResResolveDependencyArray 1A009C;
   - ResEntryListAllReady 904B58 / ResourceIsReadyTmp 1A5EC4;
   - ResSetupStageStep 1A5820;
   - the job pollers 40800 / 411C0.

   The rest is collision (CollisionObjectUpdate D8152) and AABB/visibility work (16728D, 16C140, 2D9B84) for the scene
   that keeps rendering.
3. **Ruled out by the logs:**
   - **PSO/pipeline creation:** 20-25 graphics + 8 compute runtime PSOs in the whole window, 0.01 CPU-s.
     winevulkan/d3d12core appear only on the dispatcher, in under 10 % of its few samples. There is no vkd3d compile storm.
   - **Shader/material compile:** not visible.
   - **zlib inflate:** 5.05 CPU-s in 93 s, about 0.05 core.
   - **Physics/navmesh build:** G1 StagePathInitStep never failed.
   - **Memory:** RAM available 4.9-5.8 GB, no swap jump; VRAM 4.2 -> 6.0 GB of a 10.7 GB budget.
   - **Duplicate work between G8 and G9:** none; G9 is not a work phase.
4. **The critical path is how long each manager A request takes, not CPU throughput.**
   - Manager A sits at its in-flight cap of 10 for the whole +0..+93.5 s, with 957 requests pending on average. The PC
     has 277-354 pending and drains at +10 s.
   - It logs **34,330 no-progress Sleep(1)s** (PC: 1.5k).
   - Request-seconds: Ally 90.7 s x 10.5 = ~950, PC ~86. That is **~11x longer per request**.
   - The clearest sign is I/O: **640k ReadFile calls with 68 s spent inside the calls** (PC: 3.2 s).
     - 'other' reads: 578k calls at **105 us/call** (PC: 3.8 us), 1.1 KB on average.
     - Archive reads: 63k calls at 114 us/call (PC: 14 us).
     - Average read rate: 20 MB/s (PC: ~220 MB/s while its managers are busy).
   - Why they serialize under Wine: an overlapped ReadFile on a regular file completes inside the call.
     FileReadAsyncSubmit 312F7C and FileReadBatchLocked 7401F0 release their SRW lock before ReadFile (§8.1), so reads
     that are async on Windows become blocking reads on the calling (step) threads, and their parallelism is the number
     of calling threads.
   - Other multipliers:
     - The Ally frame is 23-34 ms vs 9 ms on PC, and steps and uploads are noticed once per frame.
     - The main thread spends ~50 % on upload/frame fences, with the GPU at 99 %.
5. **The 'other' read sites, statically:**
   - 740392 in FileReadOverlappedBatch 740300 (one overlapped read per entry, called from 7401F0);
   - 1FD9CA3 in FileReadWholeByPath 1FD9C58 (the SAVEDATA.BIN whole-file read, §8.1);
   - 32562BE in FileReadSyncAtOffsetB 325626C.

   Names are in symbols/symbols.tsv. Which site makes the 578k calls is not measured yet.

### 7.3 Fixes per holder (game-side, default-off ASI flags; the ones built later are in §8.6-§8.9)

| # | flag | targets | design | same look because | expected (of the ~93 s) | gate before building |
|---|---|---|---|---|---|---|
| 1 | **GF+W6+RWb during loads** (already in the ASI) | the 7.5-core idle baseline; main-thread fence spins (50-65 %) | turn them on for this route; no new code (note: W6 never engages in a stock Proton setup, docs/BENCHMARKS.md) | already proven same-result | **3-10 s** (estimate): on the shared 25 W budget, spinning cores take power from the step threads and the GPU (loading.md §3 #2) | A/B only (§7.4 step 1) |
| 2 | **RC** read-coalescing cache | 578k small 'other' reads (60.8 s in the calls) | IAT-hook the 3 'other' sites and 312DD2. Per read-only handle: on a miss, read the aligned 256 KB block; serve later reads inside a cached block from memory. LRU, 32-64 MB cap; only handles opened GENERIC_READ without write sharing | identical bytes from a file that does not change during the run; same call order and results | if >= 80 % of reads fall within 256 KB of an earlier read: 60.8 -> ~5 s in the calls, **~15-40 s wall** if those reads sit on the TrySync step threads | RP probe: locality and calling thread per site |
| 3 | **AQ** true async archive reads under Wine | 63k archive reads (7.2 s in the calls), blocking on the calling thread under Wine | in FileReadAsyncSubmit 312F7C, hand the ReadFile to a 4-thread ASI I/O pool and return ERROR_IO_PENDING; complete the game's OVERLAPPED/event (the Windows contract the game already handles) | same reads, same completion path | 2-6 s (estimate) | RP: in-call time at 312F7C |
| 4 | **TS** TrySync step threads 3 -> 6 (next launch; pool table 4B722D0, entry for pool 6) | step threads blocked in reads; ~11x request latency | raise the thread count before the pools are created. Steps already run 3-wide, so they are thread-safe; the order within a request is unchanged | same steps; only how many run at once changes | up to ~2x on the I/O-blocked share; 0 on PC (LC showed the PC is CPU-bound) | RP: TrySync threads spend > 50 % of their time in ReadFile |
| 5 | completion event instead of Sleep(1) (loading.md §3 #5) | 34k no-progress Sleep(1)s | signal from ResourceStepJob_Run 1343D00, keep a 1 ms timeout | same steps, noticed sooner | 0.5-3 s | none |
| - | write-offs | PSO/pipeline dedupe or earlier pipeline-library warmup (28 PSOs, 0.01 CPU-s); inflate; memory; OJ/HT (no timeout ends the window); the tip page (user input) | - | - | 0 | - |

Dependency polling (the ResourceIsReady CAS loops) costs ~1-2 worker cores. It is a symptom of slow requests, so revisit
it only after #2-#4.

### 7.4 Next measurement

Same save and route on the Ally. Time the "work end" (last LoadGates eval, manager A last busy), not the window.

1. **No build needed:** compare the 1.7.19 ini plus `GF=1 W6=1 RWb=1` against the L1 ini, title -> Load Game x2 each.
   - Metrics: the last LoadGates eval and manager A `last +x s` (L1: 93.6 / 93.5 s), plus the tail CPU cores.
   - Press A as soon as the tip page shows.
2. **RP probe** (new file). Per 58A window, for each of the 7 ReadFile sites, log:
   - calls, bytes, and a histogram of time spent in the call;
   - the calling thread's pool (TrySync / RequestLoad / KIDSTS / other) and the number of distinct handles;
   - the gap to the previous read on the same handle: same block, < 256 KB, or far.

   Also fix the LoadGates tail accounting (§7.1). RP decides between RC, AQ and TS.

## 8. Read fix design (2026-10-08, static only; nothing built, game not run)

Inputs: §7, `mod/Nioh3EngineFix/src/loadstats.cpp` (kRead sites), 4 read-only Ghidra queries, a byte scan of the exe dump. The
names are in symbols/symbols.tsv. The SRW lock is not held across the read (8.1), so RP does not time it.

### 8.1 What the file layer does (static facts the design rests on)

| fact | where | consequence |
|---|---|---|
| Archives open with `GENERIC_READ`, share **`FILE_SHARE_READ` only**, flags `0x80` + `FILE_FLAG_OVERLAPPED` when open bit 0x200 is set. No NO_BUFFERING / SEQUENTIAL_SCAN / RANDOM_ACCESS | FileOpenWin32 314144 (CreateFileW 314199; `and eax,200h` at 314181) | the flags are not pathological. Without write sharing no writer can exist while the handle is open, so cached bytes cannot go stale |
| The SRW lock f+A0h is **released before** ReadFile | FileReadAsyncSubmit 312F7C, FileReadBatchLocked 7401F0 | no lock serializes reads; read parallelism = number of calling threads |
| **One kernel event per read, created and closed every time**: FileReqAlloc 311E50 calls CreateEventA (311EE1) per OVERLAPPED; FileReqFree 311F58 closes them (311F89) | all archive reads (312F7C: n = 1; batch 740144: n = entries) | under Wine both are wineserver calls, and LoadStats does not time them: ~640k creates + closes per load |
| Completion = `WaitForMultipleObjects(events, bWaitAll, INFINITE)` | FileReqWaitAll 312754 (3127A1). No GetOverlappedResult and no `Internal` poll in the layer | anyone who fills `Internal/InternalHigh` and **signals hEvent** completes a read: AQ is feasible |
| Every hooked site passes an OVERLAPPED with an explicit offset | 311D17, 312DD2, 31305A, 3130D6, 740392, 32562BE | the file pointer is never used; a cache can serve without touching it |
| The game has its own read cache: only on FileStreamReadStep 311BFC, only for files with flag 0x40, caching exactly what was read (no read-ahead) | FsReadCacheLookup 2F77A6C (call 198357C), FsReadCacheInsert 2F77474 | not a fix by itself; RP logs whether it ever hits |
| FileStreamBounceRead 3256334 reads in chunks of the stream's bounce size [s+40h] | 32562BE | if this is the hot 'other' site, calls = bytes / bounce size |
| 1FD9CA3 is the SAVEDATA.BIN whole-file read (open, read, close) | FileReadWholeByPath 1FD9C58 | never touched by a fix |
| 13 ReadFile + 1 ReadFileEx call sites in .text; LoadStats hooks 7 | others: AF5656 (device read with timeout), BDCC46, BF9485, BF9647, BF98E5, BF9FC7, ReadFileEx 631FD5 | RP hooks all 14 so nothing hot is missed |
| TrySync = pool table entry 9: count u32 at **4B723B4** (= 3), weight i32 at **4B723B8** (= 4); count = trunc(12/13 x weight) | SizeWorkerPools 1FE2FD4 (PoolBaseForIndex = 12 for pool 9) | weight 7 -> 6 threads on every CPU (Zen1 divisor 16: 5) |

### 8.2 RP read probe (`[Profiler] ReadProbe = 1|2`, default 0, needs LoadStats >= 1; new file `readprobe.cpp`)

**Hooks** (the verified-bytes FF 15 -> E8 thunk path LoadStats uses for kRead; extend `ls_sites.py` -> `READPROBE SITES OK`):
- R1: the 13 ReadFile sites + ReadFileEx 631FD5, one site id each. With RP on, RP's per-site hook replaces HkRead<C> at
  the 7 shared sites and still feeds LoadStats' counters (no double hook, no IAT patch).
- R2: event churn: CreateEventA 311EE1, CloseHandle 311F89, WaitForMultipleObjects 3127A1 (count + in-call time).
- R3: game cache: the E8 at 198357C -> 2F77A6C (calls; hits = return > 0).

**Per call** (two QPCs, a few relaxed atomics, one 4k-slot open-addressed handle table): site, handle slot, offset (from
the OVERLAPPED), size, ok/pending/error, duration, thread slot, in-flight count at entry (global and per handle).
- First sighting of a handle: `GetFinalPathNameByHandleW`, `GetFileInformationByHandleEx(FileStandardInfo)` and
  `NtQueryInformationFile(FileModeInformation)` (synchronous vs overlapped handle), once, on the calling thread. That is
  ~3 calls per file, a few hundred per run.
- Thread -> pool from `GetThreadDescription` (the game's `KIDSTS[s%d:%s]` names), cached per thread.

**Locality per handle**: the last end offset plus a ring of the last 8 64-KiB block ids. Each read is classed:
- `seq`: offset = last end;
- `near`: < 64 KiB away;
- `inblk`: its 64-KiB block is in the ring;
- `reread`: the same offset + size is in the ring;
- `far`: none of these.

**Calibration** (once, at the first title frame, never inside a 58A window, ~50 ms, on the sampler thread). Open
`package\root.rdb` twice: synchronous and FILE_FLAG_OVERLAPPED, GENERIC_READ, share READ (compatible with the game's
handles). Then:
1. 1000 x 4 KiB and 1000 x 1 B reads of one just-read offset on each handle -> **per-call cost of a cached read**
   (Wine + syscall overhead only);
2. 32 random uncached offsets x {4, 64, 256} KiB -> **device cost by size** (is a 64 KiB read about as cheap as 4 KiB?);
3. 200 x CreateEventA + CloseHandle -> the event churn cost.

**Log per 58A window** (also split at the last LoadGates eval). Shape (numbers illustrative):
```
ReadProbe #n site 740392: 412k calls, 450 MB, in-call 43.1 s | thr TrySync 81 % RequestLoad 12 % other 7 % | handles 3 |
  ovl-api 100 % pending 0 % | size <=1K 61 % 1-4K 30 % 4-64K 8 % >64K 1 % | us <8 4 % 8-32 9 % 32-128 70 % 128-512 15 % >512 2 % |
  seq 55 % near 10 % inblk 12 % reread 3 % far 20 % | inflight global avg 2.9 max 3, same-handle avg 2.7
ReadProbe #n files (top 8 by in-call s): root.rdb ovl 38.0 s 510k calls ... | 0001.fdata ...
ReadProbe #n events: CreateEventA 413k (avg 18 us), CloseHandle 413k (14 us), WFMO 400k (avg 95 us) | game cache calls 0 hits 0
ReadProbe #n threads: TrySync x3 in ReadFile 62 % of their window, in WFMO 9 % ...
ReadProbe calib: cached 4K sync 9 us ovl 14 us 1B 8 us | uncached 4K 95 us 64K 130 us 256K 330 us | event pair 25 us
```
- `ReadProbe = 2` also writes a binary trace: 32 B per call, a 32 MB RAM ring, flushed after the window to
  `Nioh3EngineFix_reads_<n>.bin`.
- A new `rp_sim.py` (private dev repo) replays the trace. It predicts the RC hit rate per block size and cache MB, and the AQ
  in-call time per queue depth, using the calibrated device costs.
- Cost of RP=1: < 1 us per ~100 us call.

**Verdict rules** (the probe decides; the numbers are the thresholds):

| RP shows | means | build first |
|---|---|---|
| calib cached 4K >= 40 us, or hot-site calls mostly in 32-128 us with `seq+near+inblk` >= 60 % | per-call Wine overhead | **RC** (hits never enter Wine) |
| cached 4K <= 15 us, uncached 64K <= 1.5 x uncached 4K, `seq+near+inblk` >= 60 % | device latency, good locality | **RC** (fewer device trips) |
| as above but `far` >= 50 % | device latency, random | **AQ** (queue depth), then **TSY** |
| event pair >= 15 us and >= 300k events per load | event churn | **EP** (cheap, alongside any other) |
| TrySync threads in ReadFile + WFMO >= 50 % and global in-flight ~= 3 | read-bound step threads | **AQ** or **TSY** |
| overlapped cached 4K >= 1.5 x synchronous cached 4K | overlapped handles cost extra under Wine | **SH** (Ally only) |

### 8.3 Fixes, ranked (Ally seconds of the ~93 s work; zero effect on output bytes)

| # | flag (default 0) | hook points | design | memory | expected Ally | PC | risks |
|---|---|---|---|---|---|---|---|
| 1 | **GF=2, W6=2, RWb=2** (loads-only, §8.5) | existing caves; runtime words g_gfEnable / g_w6Enable; RWb needs a switchable cave | on while G+58Ah is set | 0 | **3-10 s** (power back to the step threads and the GPU) | 0 | nothing new except the RWb live switch (§8.5) |
| 2 | **RC** read cache: `[Patches] RC=1` (1 = 58A windows, 2 = always), `RCMB=64`, `RCKB=64`, `RCMaxReadKB=64` | register at CreateFileW 314199 (GENERIC_READ, no FILE_SHARE_WRITE); unregister at CloseHandle (IAT 38E2390, hash-set check); serve at 311D17, 312DD2, 31305A, 3130D6, 740392, 32562BE (never 1FD9CA3) | reads <= RCMaxReadKB on a registered handle are copied from cached aligned RCKB blocks. A miss reads the whole aligned block(s) with the real ReadFile (own OVERLAPPED + per-thread event + GetOverlappedResult wait, so it is also correct on Windows). Other threads that want a loading block wait on it (WaitOnAddress). A hit returns exactly like a synchronous completion: TRUE, `*got`, `Internal = 0`, `InternalHigh = n`, `SetEvent(hEvent)`. A short block at EOF or a failed block read passes the original call through (same errors). CLOCK eviction, 64 shards. RC=1 frees the blocks 10 s after the clear | RCMB (64 MB default, max 256; 4.9-5.8 GB available during the Ally load) | **10-30 s** if 60-80 % of reads hit (68 thread-s in calls -> ~10-15, mostly on the 3 step threads); ~0 or slightly negative if `far` dominates (a 64 KiB miss costs more than 4 KiB) | 0 (3.2 s in calls in total; managers off the critical path) | staleness impossible while the share mode holds; handle reuse after a close -> the CloseHandle unregister; an extra copy per hit (~1 us per KB-sized read) |
| 3 | **AQ** async archive reads: `AQ=1`, `AQThreads=4` | 31305A, 3130D6, 740392 (the overlapped sites; their callers accept ERROR_IO_PENDING) | set `Internal = STATUS_PENDING (0x103)`, queue {handle, buf, n, OVERLAPPED*}, return FALSE with `SetLastError(ERROR_IO_PENDING)`. An ASI worker reads into the game's buffer (through RC when it is on), fills `Internal/InternalHigh` and signals `hEvent`. The game already waits in FileReqWaitAll (WFMO on hEvent): the Windows overlapped contract | 4 threads | **5-20 s** if device-latency bound and `far`-heavy: step threads stop blocking in reads and the device sees queue depth 4+ instead of <= 3 | 0 (Windows is already async) | the game must not reuse the buffer or OVERLAPPED before its wait (it waits in 312754 before FileReqFree 311F58); only events made by FileReqAlloc are signalled (hEvent != 0 checked) |
| 4 | **EP** event pool: `EP=1` | CreateEventA 311EE1, CloseHandle 311F89 | hand out manual-reset, non-signalled events from a pool (ResetEvent when returned); create only when the pool is empty; cap 4096 | ~0 | **0-8 s** (2 wineserver calls per read: 640k x 2 x 10-25 us = 13-30 thread-s) | ~0 | an event still referenced by a pending read is impossible: FileReqFree waits for all first; ResetEvent on return keeps "non-signalled at creation" |
| 5 | **TSY** TrySync threads: `TSY=6` (3..8) | data 4B723B8: weight 4 -> the smallest weight giving N (7 for 6), written before SizeWorkerPools (the boot race P12/WC already win); assert count 3 at 4B723B4 | more step threads run read-blocked steps at once | +3 threads (blocked when idle) | **5-15 s** if step threads are read-bound and the device has queue headroom; 0 if CPU-bound | 0 (LC showed the PC is not step-bound) | boot-time only (next launch); steps already run 3-wide, so they are thread-safe; more CPU contention on the APU in CPU-heavy phases. "TS" is already an exp-menu key, hence TSY |
| 6 | **SH** synchronous archive handles: `SH=1`, Ally only | 314181 `25 00 02 00 00` -> `25 00 00 00 00` (before the first open; boot) | handles open without FILE_FLAG_OVERLAPPED; the game's reads become positioned synchronous reads that return TRUE, the path Windows takes on cache hits | 0 | **0-6 s**, only if calib shows overlapped cached reads >= 1.5 x synchronous | **negative** (Windows serializes I/O per synchronous file object): never on PC | excludes AQ; write/rw opens should keep the flag (restricting to mode&3 == 1 needs a small cave instead of the imm; builder decides) |
| 7 | **MM** mapped reads: `MM=1`, `MMViewMB=64` | the RC serve sites | serve from 64 MiB read-only views (CreateFileMapping + MapViewOfFile, LRU of 32 views) by memcpy inside `__try`; EXCEPTION_IN_PAGE_ERROR -> pass through | 0 private; page cache only; 2 GB of VA | like RC hits, plus kernel read-around on faults | ~0 | if Wine falls back to reading a whole view (a map taking > 5 ms), disable MM for that file; fault-time blocking is invisible to the game. Only if RC's memory or copy cost is the problem |
| - | write-offs | - | FILE_FLAG_SEQUENTIAL_SCAN / RANDOM_ACCESS (cache-manager hints on Windows; no known Wine equivalent); NO_BUFFERING (bypasses the page cache, needs sector-aligned buffers, hurts both devices); the game's FsReadCache (exact-range, no read-ahead, one path); 1FD9CA3 (save data) | - | 0 | - | - |

Together (RP picks 2-3 of rows 2-7): **~15-40 s** off the ~93 s, plus row 1.
- Build order: RP + LO (row 1) in one build, then the RP-chosen fix(es) in the next (one build per session).
- **Proofs before shipping** (offline, no game):
  - `mod/Nioh3EngineFix/tests/run_readfix_harness.py` (new) runs 8 threads of random reads on a 1 GiB test file: 1 B - 128 KiB,
    unaligned, block-crossing, at and past EOF, synchronous and overlapped handles, completion via WFMO on per-read
    events like 312754. It goes through RC, AQ, EP and RC+AQ, and compares against plain ReadFile: bytes, return value,
    `*got`, `GetLastError`, `Internal/InternalHigh` and event state -> `READFIX HARNESS: ALL OK`.
  - Site bytes: `READPROBE SITES OK`, plus `mod/Nioh3EngineFix/tests/test_patterns.py` -> `ALL N SITES OK`.

### 8.4 Ally A/B (exact)

Route and metric as in §7: same save, online title -> Load Game -> Level 136; press A as soon as the tip page shows.
- **Metric = work end**: the last LoadGates eval and manager A last busy (L1: 93.6 / 93.5 s). Also RP in-call s, CPU
  cores, RAM available min, VRAM 60 s after the press.
- **Base ini** = the 1.7.19 L1 ini (LoadStats=3, LoadGates=1, MemStats=1, SP=1) + `ReadProbe=1` in **every** arm, so
  its overhead cancels.
- **Each arm = one launch, two loads**: title -> Load Game (#1, colder page cache), press A, 60 s in game, quit to
  title, Load Game (#2, warmer). Compare #1 with #1 and #2 with #2; RP's duration histogram shows the cache warmth.
- **Order ABBA** (4 launches). A fix counts if both A-B pairs improve the work end by more than the B-B spread (expect
  <= 1 s).
- **Same look and content**: a screenshot 5 s after the press in each arm; LoadStats `MB read` (game-requested bytes,
  counted before RC) within 1 %; VRAM at +60 s within 2 %.

| session | build | arms (A / B) |
|---|---|---|
| R1 | RP + LO (row 1) | B = base; A = `GF=2 W6=2 RWb=2`. Launch 1 also runs `ReadProbe=2` (trace for rp_sim.py) |
| R2 | the RP-chosen fix | B = base (+ LO if R1 kept it); A = base + one of `RC=1` / `AQ=1` / `EP=1` / `TSY=6` / `SH=1`. A second fix gets its own ABBA (or a 2x2 in R3) |

PC: one ABBA of the kept fixes on the PC LG route (`loadstats_ab.ps1`) to confirm "0 or better". SH never runs on PC.

### 8.5 Loads-only gating via G+58Ah (LO)

- ini: `GF`, `W6`, `RWb` accept **2 = loads only** (1 = always, as now). The caves are installed as for 1, with the
  runtime word starting at 0.
- Gate thread: LoadStats' 10 ms sampler if LoadStats >= 1, else a small 10 ms timer thread. It reads byte
  `[g_GameMainPtr]+58Ah` (the same read LoadStats does).
  - Rising edge: set `g_gfEnable = 1`, `g_w6Enable = 1`, `g_rwbEnable = 1`.
  - Falling edge: set them to 0 at once. The tip page is inside the window, so it is helped too (no spin).
  - Log `LO: on at 58A set #n, off at clear (+x s)`.
- GF and W6 already switch live: the mod menu flips the same words, and a waiter falls back to the stock Sleep(0) when
  its word is 0.
- RWb is a 22-byte reorder at 1A3146A with no runtime word. For mode 2:
  - the builder replaces it once with `jmp caveRWb` + nops, installed with LoadStats' EB FE two-step write;
  - the cave holds both orders (stock and RWb) and branches on `g_rwbEnable`;
  - the cave calls JobWakeOneInPool 48B9C by absolute address, because the original E8 rel32 cannot move;
  - the patterns test proves both cave paths match the stock and RWb sequences.
- Expected: row 1 of §8.3 on the Ally, 0 on PC. In gameplay the words are 0, so the game runs stock code there.

### 8.6 Built in ASI 1.7.23 (2026-10-08)

- **RP** = `[Profiler] ReadProbe = 1|2` (needs LoadStats >= 1), `mod/Nioh3EngineFix/src/readprobe.cpp`. All of 8.2 except `rp_sim.py` (the RP=2
  trace format is in the `.txt` sidecar; the simulator is the next small job). Deviations: reread is checked before seq;
  calibration uses the biggest archive seen (root.rdb only as fallback) for all reads; first-touch reads are skipped for
  files < 64 MB; handle paths are re-read once per window (handle reuse inside one window would mix two files).
  LoadStats leaves its 7 ReadFile sites to RP (`readsByProbe`) and gets the same counts through `loadstats::NoteRead`.
  Thunks keep rcx/rdx/r8/r9/r10/r11 and forward the 5th stack argument; the R3 cache counter is an asm thunk that only
  changes rax/flags. Sites: patches.json RP1-RP18 + 3 deps; `ls_sites.py` -> READPROBE SITES OK (13 ReadFile + 1 ReadFileEx
  = every such call in .text).
- **LO** = `[Patches] LoadOnlyCpuFixes = 1` (one key instead of GF/W6/RWb = 2), `mod/Nioh3EngineFix/src/loadonly.cpp`. GF/W6 installed with
  their words at the ini value; RWb via a switchable cave that rewrites only the first instruction (6 bytes, one CAS);
  the RWb order's wrapped case runs `dec ebx` and re-enters the stock tail, so the wake call stays in game code (no
  relocated call). `test_patterns.py` "LO" interprets the real bytes: cave word 0 == stock, word 1 == RWb, all 256 bl.
- **EP** = `[Patches] EP = 1` (8.3 row 4, built early: exact by construction). A closed event is pooled only if
  NtResetEvent reports it was signalled (=> no read pending on it); otherwise it is really closed. Harness
  `mod/Nioh3EngineFix/tests/run_readprobe_harness.py` -> READPROBE HARNESS: ALL OK (save-thunk registers, cache thunk, EP reuse/close paths).
- Ally session R1 (8.4) can run with this build: base = 1.7.19 L1 ini + `ReadProbe=1`; arm A adds `LoadOnlyCpuFixes=1`;
  launch 1 of the first B arm uses `ReadProbe=2`. EP gets its own ABBA (A = base + `EP=1`) in the same or the next session.

## 8.7 Ally 1.7.23 R1 data -> the next read fix: SR (built in ASI 1.7.25)

Logs: Ally 1.7.23 session R1 (a = base, b/c = EP=1). EP cut the load 96.0 -> 47.3 s (docs/BENCHMARKS.md, section (c);
event pair 84 us -> < 2 us per read).
What is left with EP on (b / c, 47 s window):

| site | calls | in-call | per call | shape | thread |
|---|---|---|---|---|---|
| 740392 FileReadOverlappedBatch | 536k | 24.9 s | 46 us (79 % 8-32 us) | <= 1 KiB 99 %, seq 91 %, near 8 %, pending 100 %, in flight 0.2 | one unnamed loader thread |
| 631FD5 ReadFileEx | 12.6k | 9.3-10.2 s | 740-810 us | 2.7 GB, far 86 %, in flight 3 | ResourceLoad |
| 31305A FileReadAsyncSubmit | 18k | 3.2-3.6 s | 170-200 us | 4-64 KiB, far 82 % | unnamed |
| 311D17 | 44k | 0.3-0.4 s | 6-10 us | synchronous handles | KtglFsThread_Hdd |

- Calibration: cached 4K **synchronous 2.0 us, overlapped 52-58 us** (1 B: 2 vs 70 us). "pending 100 %" with in-call ~=
  device time means Wine does the read inside the call and then pays ~50 us of async completion bookkeeping.
- LoadGates holder: G8 StageAreasObjectsReady 17.7-19.1 s of 47 s (first fail +18-20 s, last +37-38 s). The 740392
  loader thread spends 24.9 s in calls over the same window, serially (in flight 0.2): it is the likely G8 critical path.
- Verdict (8.2 rules): overlapped >= 1.5 x synchronous -> SH-class fix; seq >= 60 % -> RC. Both attack the 46 us x 536k.

**SR (synchronous redirect, `[Patches] SR = 1`, Wine only).** At the archive sites, a read on an OVERLAPPED game handle
of an eligible package file is done as a positioned synchronous ReadFile on a private read-only handle to the same file
(opened once at classification with GENERIC_READ + FILE_SHARE_READ, compatible with the game's own share mode; closed
when the game's handle is closed or its value reopened), then completed like a synchronous success (TRUE, Internal 0,
InternalHigh n, hEvent signalled, last error unchanged). Any failure, a read past EOF or n = 0 passes the original call
through (same error semantics). Nothing asynchronous becomes blocking: Wine already did these reads inside the call.
Unlike SH it leaves the game's handles alone (ReadFileEx 631FD5 and the other subsystems are untouched).
- Expected: 740392 46 -> ~3-5 us per call (sync read + SetEvent) = **~20 thread-s off the loader thread**; if that thread
  is G8's holder, the 47 s load drops toward ~30 s (estimate; measured in 8.9: 47.25 -> 38.45 s). 31305A ~1 s more.
  PC: refused (Windows).
- RC on top (`RC = 1`): the <= 1 KiB sequential reads become memcpy from 64 KiB blocks (~98 % hits); block loads use the
  private handle. Worth a second arm only if SR leaves > ~3 s on 740392.
- Not addressed: ReadFileEx 631FD5 (9-10 s on ResourceLoad, device-bound large far reads, 3 in flight) - SR would save only
  ~0.6 s there; G9 LoadCharacterReady 3-4 s and G5 5.8 s are later gates.
- Also built (default off): AQ (async queue; for a device-bound profile, not this one), TSY (TrySync threads, boot),
  SH (sync archive handles, boot, Wine only). TSY/SH run in dllmain's decrypt-wait loop like P12/WC.

Proofs: `mod/Nioh3EngineFix/tests/run_readfix_harness.py` -> READFIX HARNESS: ALL OK (8 threads x 3-4k random reads per config: off, RC, AQ,
RC+AQ, SR, RC+SR, RC+AQ with private handles; each read compared with the real ReadFile on a twin handle of the same kind:
bytes, result, count, error, event, file pointer; handle reuse; bad buffer; TSY float formula; SH refused off Wine).
`test_patterns.py` ALL 212 SITES OK (RP19 CreateFileW 314199, RP20 CloseHandle 313AE6, RP21 SH imm, TSY dep 4B723B4).

**Ally session R2 (ASI 1.7.25).** Base = R1 base (1.7.19 L1 ini + `ReadProbe=1`) with EP auto (on). ABBA x 4 launches,
2 loads each, route/metric as 8.4: B = base, A = base + `SR=1`. Check: "ReadFix: SR 1 ..." start line, "ReadFix #n SR:"
served count ~= 740392 calls and "private handles n opened, 0 failed", site 740392 avg us in the ReadProbe line, G8 held
s, work end, MB read within 1 %, screenshot 5 s after the press. Keep if both A-B pairs beat the B-B spread. Then (same or
next session) A = base + `SR=1 RC=1` vs B = base + `SR=1`. Send back: grep 'LoadStats|LoadGates|ReadProbe|ReadFix|EP:' of
each log.

## 8.8 The two read sites left after EP / SR, and PF trace prefetch (built in ASI 1.7.26, default off, not run in game)

Static only (capstone over the exe dump plus the earlier decompile of 429E4; game not run). Data: 1.7.23 R1
logs b/c (EP on).

### 8.8.1 What 631FD5 (ReadFileEx) reads, and who waits

| step | where | fact |
|---|---|---|
| owner | RIS resource job **429E4** (ResourceFileOpenAndLoadTask; string "Failed to read RIS Header ... ResFileID"), ResourceLoad pool (7 threads) | ResFileID -> archive path + entry offset (vt+80h of the resource DB), then one read of the whole IDRK entry ('IDRK0000', 38h header + body) into memory; the header is parsed from memory afterwards (memory stream vtable 38EE630) |
| open | stream vtable **38EE660** slot +38h = **631EC0** | `CreateFileA` 631F20 (GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_OVERLAPPED \| NORMAL) **per resource**; pos [s+20h] = entry offset. Close = slot +08h 631C84 (CloseHandle 631CBC / 631CFF) |
| read | slot +40h **631C48** -> **631F70**(req, h, buf, off, n) | OVERLAPPED = req itself; `ReadFileEx` 631FD5 with completion **631C04**; then `GetLastError() == 0` (or 3E4h) is required. 3256310 is a second caller (offset 0) |
| completion | 631C04 | adds the byte count at req+90h (timing build only) and `xchg [req+20h] -> req+34h = 1` |
| wait | slot +48h **716280** (inlined in 429E4 at 42A8E) | `SleepEx(0, TRUE)`, then `lock cmpxchg [req+34h] 1 -> 0`. Not done -> job+34h = 1, flags \|= 1, return 0 (the job is re-run later). The APC runs only in the **issuing** ResourceLoad thread's own SleepEx |

- Under Wine the read happens inside ReadFileEx (inline pread + APC queued), so the first poll always succeeds: the
  ResourceLoad thread sits out the whole device read. On Windows the call returns at once and the job is re-run.
- Sizes (b): 67 % <= 1 KiB, 10 % 1-4 KiB, 15 % 4-64 KiB, 8 % > 64 KiB (~1000 reads of ~2.7 MB = almost all of the 2.7 GB;
  probably the raw StreamingTexContext / StreamingMeshletModelData entries, loading.md §2). Durations: 48 % 128-512 us,
  44 % > 512 us = cold device reads (calib: first-touch 4K 526 us, cached overlapped 58 us).
- RP's "handles 118, far 86 %" for this site is mostly handle reuse: every resource opens and closes its own handle, so
  RP's per-handle-value locality mixes files. The real per-file pattern is unknown until a ReadProbe=2 trace.
- Not thread-bound: the [App] pools are parked most of the window (SP: 57 % in CndWaitSrw B616A4), ReadFileEx is 3 % of
  the ResourceLoad thread-time. The reads are **latency-bound on resource dependency chains**: resources complete ->
  ResourceIsReady 1A6030 / ResourceDependenciesReady 1A6820 -> G8 StageAreasObjectsReady (area objects wait for their
  resources) and G9 LoadCharacterReady; manager A (10 in flight, ~1100 pending) waits on the same requests.

### 8.8.2 31305A

FileReadAsyncSubmit 312F7C (archive layer): 64 KiB chunk reads of the zlib IDRK entries (100 % 4-64 KiB, 1.1 GB), on the
unnamed step threads (x3), completed by FileReqWaitAll 312754 (WFMO on the per-read event, EP pools it). Long-lived
handles, so its far 82 % is real. SR (1.7.25) removes the ~50 us Wine bookkeeping; ~150 us of device time per read stays.

### 8.8.3 Options

| option | verdict |
|---|---|
| async ReadFileEx (worker + QueueUserAPC to the issuer) | restores the Windows timing, but the pool is idle (no backlog to overlap), and the APC is delivered only when the issuing thread polls again (a later job on that thread): adds latency and a stall risk. Not built |
| SR-style sync redirect for ReadFileEx | saves the ~60 us bookkeeping x 12.6k = ~0.8 s; device time unchanged; needs the completion run outside an alertable wait (contract change). Not built |
| bigger merged reads | each call already reads a whole entry; merging needs to know the next entries |
| skip redundant reads | RP: reread 0 % |
| **prefetch into the page cache** | the only option that removes device latency, and exact by construction (the game's I/O is untouched) -> **PF** |

### 8.8.4 PF = `[Patches] PF` (mod/Nioh3EngineFix/src/prefetch.cpp / prefetch.h; keys PF, PFThreads 4, PFLeadMB 768, PFTraces 4, PFMatchPct 50)

- **Observe** (no new hook site): readprobe's existing R1 hooks at 631FD5 (RP14), 31305A (RP3), 3130D6 (RP4) call
  `prefetch::Note` before the real call. Note keeps the last error, takes the file id from NtQueryInformationFile
  (FileInternalInformation, immune to the handle reuse), the path once per file (package\ only), the offset from the
  OVERLAPPED. Inside a 58A window every such read is appended to a RAM list (24 B, cap 262k).
- **Record**: at the clear the list goes to `Nioh3EngineFix_pf<k>.bin` next to the ini (package-relative paths + reads).
  Slot = the trace the load matched (>= 50 % of its reads), else a free slot, else the oldest (PFTraces = 4).
- **Replay** (PF = 1): the first 24 package reads of a load pick the stored trace holding >= PFMatchPct % of them. The trace
  becomes runs (same file, gap <= 256 KiB, <= 8 MiB). PFThreads workers read the runs in recorded order on their own
  synchronous read-only handles (share READ|WRITE|DELETE: no game open can fail) into a 4 MiB scratch buffer, at most
  PFLeadMB ahead of the furthest run the game has asked for; runs the game already read are skipped. The replay stops if
  < 25 % of the game's reads are in the trace. PF = 2 records only (the B arm). Windows is allowed (~0 there).
- **Register safety** (hook-register-audit.md): nothing new; Note runs inside HkRead<2/3> / HkReadEx behind the existing
  save thunks (rcx/rdx/r8-r11 kept, 5th arg forwarded). The sites are FF 15 imports, so the callers already treat every
  volatile as clobbered (xmm0-5 included); 631FD5's caller reads only eax and the last error after the call.
- **Cost**: per observed read one NtQif (fd-cached under Wine, ~1-2 us), a hash lookup, 24 B. Device: the same ~3.9 GB is
  read earlier at queue depth 4. RAM: page cache only, <= PFLeadMB ahead (768 MB of 4.9-5.8 GB available).
- **Expected (Ally)**: upper bound with every read cached: 631FD5 9.3-10.2 -> ~1 thread-s, 31305A 3.2-3.6 -> ~1 (~0.1 with
  SR): ~11 thread-s of blocked reads off ResourceLoad and the step threads. Wall clock **~3-8 s** of the load if those
  chains still gate G8/G9 after SR; 0 if the holder is then CPU work. The first load after turning PF on only records.
  PC ~0 (pc-vs-ally-load.md).
- **Proofs**: `mod/Nioh3EngineFix/tests/run_prefetch_harness.py` -> PREFETCH HARNESS: ALL OK (the game's reads through the real HkReadEx /
  HkRead<2>: bytes / results / counts / opens exact while recording and prefetching; last error kept; trace round trip;
  pick / no-match / record-only / corrupt trace / stop rule; run merging; synthetic W2: 96 % of reads prefetched before the
  game asked, lead <= limit). READPROBE / READFIX harnesses ALL OK, test_patterns ALL 212 SITES OK, READPROBE SITES OK.
  `rp_sim.py` now prints a "PF sim" upper bound per site (selftest OK); no ReadProbe=2 trace of the Ally exists yet.

### 8.8.5 Ally test protocol (PF; ASI 1.7.26 = 1.7.25 + PF, so R2 can run on it unchanged)

Route and metric as 8.4 (online title -> Load Game -> Level 136, press A at the tip page; metric = work end = last
LoadGates eval / manager A last busy). Base = the R2 base (1.7.19 L1 ini + `ReadProbe=1`, EP auto) plus SR as R2 decided
(the same SR value in every arm). Copy the whole `bin\` (asi + ini); PF writes `Nioh3EngineFix_pf0..3.bin` next to the ini.

1. **Prime** (1 launch, not counted): base + `PF=2`, one load (`ReadProbe=2` here too gives rp_sim a trace). Check:
   "Prefetch: PF 2 (record only)" at start and "Prefetch #1: recorded N package reads (M MB) -> Nioh3EngineFix_pf0.bin
   saved" (expect N ~30k, M ~3.9k).
2. **ABBA x 4 launches, 2 loads each**: A = base + `PF=1`, B = base + `PF=2` (records, no prefetch: same hook cost, keeps
   pf0 fresh). Compare #1 with #1, #2 with #2.
3. **Check per A load**: "Prefetch #n: trace pf0 picked (>= 12 of the first 24 ...)"; at the clear "in trace pf0 ... (>= 80
   %): already prefetched >= 60 %", "stop: window end"; ReadProbe site 631FD5 avg us (B 740-810 -> A target < 200) and
   31305A; LoadGates work end, G8 / G9 held; MemStats RAM available min (must stay > 1.5 GB); LoadStats MB read within 1 %
   of B; screenshot 5 s after the press.
4. **Keep** if both A-B pairs beat the B-B spread on the work end. "no trace matches" / "stop: match rate < 25 %" = the
   route differs from the prime: re-prime and say so.
5. **Send back**: grep 'LoadStats|LoadGates|ReadProbe|ReadFix|Prefetch|EP:' of each log, plus the size of each pf*.bin.
   Undo: `PF = 0` (the pf*.bin files can be deleted).

## 8.9 Ally 1.7.25 R2 digest: what G8 waits on, and LC = N (built in ASI 1.7.30, default off, not run in game)

Logs: Ally 1.7.25 session R2 (L1/L4 SR=0, L2/L3 SR=1, EP auto in all). GF=1, W6=1, RWb=0,
LoadOnlyCpuFixes=0, WorkerCap per the Ally profile. Digest: `ally_load_sp_phases.py` + the `tick #1 ... A act` column.
Static: capstone over the exe dump (no Ghidra).

**Where SR's 8.8 s came from** (47.25 -> 38.45 s, docs/BENCHMARKS.md section (c); offsets from the 58A set):

| | L1 / L4 (SR 0) | L2 / L3 (SR 1) |
|---|---|---|
| pre-gate (no LoadIsLoadEnd eval yet) | +0..+14.5-16.7 | +0..+13.5-13.8 |
| G2 StageAreaListBusy | 4.1 / 5.5 s | 0.3 / 0.5 s |
| G8 StageAreasObjectsReady | 16.8 / 17.5 s (+20..+37.6) | 15.9 / 15.5 s (+13.8..+29.8) |
| G9 + G5 (second manager-A batch) | ~8 s | ~7 s |
| work end (last eval) / clear | +45.3-45.6 / 47.1-47.4 | +36.6-36.7 / 38.4-38.5 |

SR removed the G2 time and ~1.5 s of G8; the pre-gate and G8 are now the two blocks (~14 + ~15.5 s).

**What G8 polls** (3B3A28, 47 bytes of loop): for each entry i < [stage+560h] of the area list stage+540h (D3B9C =
element i), skip if the area id filter does not match (entry dword >> 4); not ready if `[entry+30h]` (area object) has
byte +1F0h == 0, dword +1ECh != 0, or `[entry+1C0h]` != 0. No waits, no work: a pure flag scan, called once per
LoadIsLoadEnd eval (~40 evals/s). The flags are set by the area objects' own setup when their resources are in.

**What it waits on: the first manager-A batch draining at the in-flight cap of 10.**
- A act (in-flight) is 10 in every 1 s tick from +3 s to the end of G8 (all 4 logs); G8 passes at +29.7 / +29.8 and
  the next ticks show A act 0 / 1 (L2 / L3). With SR=0 the next batch refills at once (A act 8 at +37 in L1).
  Pending averages ~1000 (L2 1020, L3 993). The second batch (+32..+36, A act 10 again) is the G5 / G9 time.
- Nothing else is saturated during G8 (L2/L3): CPU 8.3-11 cores of 16 (pre-gate peaks at 14 at +8..+13), inflate 0 ms,
  PSO 0, reads 33-97 MB/s with 631FD5 in flight 2.4 and the rest 0.2 (the device does ~450 MB/s at QD1 for 256 KiB,
  calib); KIDSTS (108 threads) 95 % parked in JobWorkerThreadMain, ~3 threads in game code (ResourceIsReady /
  ResourceDependenciesReady / job pollers / collision); ResourceLoad x7 in ReadFileEx 3-4 % of their time; the 3
  unnamed archive-read threads (740392 / 31305A; very likely TrySync, which SP cannot see: it samples named threads
  only) in ReadFile 10 % (20 % with SR=0).
- Main thread: 49 % in GfxFencePollWorker 117D6C (an event wait with GF on) + 12 % FenceWait at ~48 fps. 117D6C's
  callers are the synchronous copy-and-wait helpers (5C0D4, 5C228, 41FD9C, 62EBB0 at device+43E8, TextureCopyRegion
  69A1B4): record one copy, submit, wait for that fence. The GPU is at 99 % with the scene behind the cover.
- Manager A (ResourceLoadIdlePoll 1FEE28) per pass: takes min(pending, cap - active) (cap = `sbb r15,r15; and r15,-9;
  add r15,10` at 1FEE85..1FEEA0), then pushes one ResourceStepJob per active request that wants a step to TrySync
  (GetPoolId_TrySync 5CF368, JobPushToPool 31A514); 12.4k no-progress Sleep(1)s (17.1k with SR=0).
- Throughput: if each request is one RIS resource read (12.6k reads at 631FD5) over ~33 s of manager-A time at 10 in
  flight, that is ~380 requests/s, i.e.
  **~26 ms per request (about one frame)** while every resource it uses is mostly idle. That is latency per request,
  not throughput: the in-flight cap is what turns it into 15.5 s.

**Fix: LC = N.** LC (1.7.14, default off) already raises both managers' cap 10 -> 16 with byte-only immediates
(LCa 1FEE98, LCb 1FF3BA; no cave, no hook, no register change). 1.7.30: `LC = 11..64` sets that cap
(`and r15,-(N-1); add r15,N`, boot flag 1 still gives 1; `mov eax,N` for manager B); `LC = 1` stays 16. test_patterns
proves the arithmetic and that only the immediates change for every N in 11..64. LoadStats reports the cap.
- Expected (Ally): the cap-bound non-CPU-saturated time is G8 ~15.5 s + the second batch ~5 s. If per-request latency
  stays ~26 ms: 16 -> **-5 to -8 s**, 32 -> **-8 to -14 s** (upper bounds; the +8..+13 s CPU-saturated part of the
  pre-gate does not gain). Estimated 3-8 s. **0 or worse** if the 3 TrySync threads are the limit (the PC case: LC=16
  raised in-flight 9.6 -> 15 but stretched the busy time 8.7 -> 11.3 s, loading-bottleneck.md); then add `TSY = 6`
  (1.7.25, boot-time). Also watch fps in G8: if the per-request cost is the main thread's synchronous copy waits, more
  in flight means more waits per frame.
- Risks: LC also applies while playing (streaming): more requests in flight, same requests and order. PC ran LC=16
  without a crash. No new code path; undo `LC = 0`.
- Not chosen: LW (manager Sleep(1) -> event; 12.4k sleeps x ~1.1 ms are on manager A's own thread, ~0.5 ms notice
  latency per step at 10 in flight = ~1-2 s at most; still worth one arm), RC on top of SR (740392 is 94 % < 8 us now),
  more PF sites (G8 reads are not device-bound).

**Ally session R4 (ASI 1.7.30; SR auto and EP auto by default).** Route / metric as 8.4 (work end = last LoadGates
eval). Base = R2 base (LoadStats=3, LoadGates=1, MemStats=1, SP=1, ReadProbe=1). ABBA, 4 launches x 2 loads:
B = base, A = base + `LC = 32`. If A gains: a second ABBA `LC = 16` vs `LC = 32`; if A is flat or worse: A = base +
`LC = 32` + `TSY = 6` vs B = base. Optional arm: `LW = 1`. Check per load: "LC: ... cap 10 -> 32" at start, LoadStats
"in-flight avg A" (expect > 10) and "pending avg A", the A act ticks, G8 held s and its last fail, work end, fps per
phase, MB read within 1 %, screenshot 5 s after the press. PF (8.8.5, R3) can run on the same build; keep LC and PF in
separate ABBAs. Send back: grep 'LoadStats|LoadGates|ReadProbe|ReadFix|LC:|TSY|EP:|SR:' of each log.