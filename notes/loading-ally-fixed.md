# Why the Ally's title -> Continue load is a fixed 118.7 s (candidates, fixes, the Ally test)

> **Update (later 2026-10-08, Ally LG probe, reported by the coordinator):** most of the window is real multi-core CPU work:
> G8 StageAreasObjectsReady held 47 s at 10-11 cores, G9 LoadCharacterReady 26 s at 7.5 cores, and L2 ended at ~96 s, not
> 118.7 s. So the "fixed wall-clock deadline" reading of §1 is weakened. A 120 s online timeout can at most explain part of
> the window. §5 (static link) and §6 (LT/OJ spec) below are written with that in mind: LT has to show an online op
> overlapping the window before OJ is worth building, and the G8/G9 CPU work is the larger target either way.
>
> **§7 (1.7.19 log digest, SP + ticks) revises this again.**
> - No timeout ends the window. The load work ends at +93 s: manager A drains and the last LoadIsLoadEnd eval happens.
>   The rest of the window is the tip page waiting for the user's press.
> - The G9 "26 s at 7.5 cores" is that tip page; the probe credits post-eval time to the last failing gate.
> - About 7.5 cores is the idle baseline with stock spins. The load adds 3-5 cores. The critical path is manager A
>   request latency (reads ~30x slower per call than on PC), not CPU throughput.

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
| 1 | **an online/network operation that never completes under Proton and times out at 120 s** (KTGL online lib: P2P session join `OnlineSessionP2pSessionJoinParam` 1356AA0, timeout **120000 ms** at imm **1356BDC** (`C7 45 9F C0D40100`); *correction (§5): the clock()/QPC there only seeds a random pick, and 120000 ms is the default timeout of every KTGL online op param, not a join-only value*; the lib also has HOST / SEARCH / LOCK / AUTO_JOIN *_TIMEOUT error codes) | 120.0 - lead 1.3 s = 118.7 | wall clock (1.1), armed before the set (1.2), 120 s is the only ~2-minute wall-clock constant found in the export; the game runs its own UDP P2P sockets (ws2_32 sendto/recvfrom/bind/ioctlsocket), which behave differently under Wine/NAT; the PC never hits it | **most likely; not yet tied to the load-end path** (the join is called through a vtable, 2 DATA refs, so the static link to the gate is open) |
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

## 5. Static link: KTGL online ops vs the load end (2026-10-08, static only)

Tools: a capstone/numpy xref pass over `re\Nioh3.dump.exe` (E8/E9 call index, rip-relative and abs64 refs). Most of the
online lib is not in the decomp export. Names: `re\symbols\incoming\online-load-renames.tsv`.

**5.1 What 1356AA0 really is.** It is the *auto-join* continuation, not a generic join:
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

All of these are request objects (vtables 402E6E0..402EEFF) run by the online layer. **I found no static path from any of
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

### 6.1 LT (`[Profiler] LoadWaits = 1`, default 0, new file `loadwaits.cpp`, sonnet)
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

Honest scope: with the LG numbers, OJ can save at most the part of the window where only the online op is outstanding,
which LT will show. It cannot touch the 47 s of G8 or the 26 s of G9 CPU work. If the 1.7.19 log digest (another agent's
work in progress) confirms that no timeout ends the window, the online path is out and LT/OJ stay unbuilt; §5's finding
that no ws2_32 call can stall under Wine supports that.

## 7. Probe results: Ally ASI 1.7.19, online title -> Load Game -> Level 136, L1 + L2 (log digest, 2026-10-08)

Logs: `D:\nioh3-re\ally-logs\nioh3_ally_modlog_1719-L1_0852.log` and `-L2_0859.log` (bridge #80/#81). The ini had
LoadStats=3, LoadGates=1, MemStats=1 and SP=1. **GF, W6 and RWb were off**, so the game's own spins were running.

New digest: `py -I re\scripts_load\ally_load_sp_phases.py <log> [--phases name:a:b,...]`. It buckets the SP rows and the
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

Corrections to the bridge #216 summary:

- **G9 is not a holder.** The probe credits the time after the last evaluation to the last gate that failed.
  - LoadIsLoadEnd stops being called at +93.6 s.
  - G9 also fails all the time outside loads (the title and gameplay `tick out` lines).
  - So the G9 time (26.5 s in L1, 208 s in L2) is the tip page, and so is the "7.5 cores, ~0 reads" period.
  - **Probe fix (for the src owner):** report "work end = last eval" and stop adding held time after the last eval.
- **The 120 s online-timeout hypothesis (§2 #1, OJ/HT) is retracted.** No timer ends either window; each one ends on the
  button press (L1 at 115.8 s, L2 at 298.5 s). The old fixed 118.7 s windows were most likely the Ally harness pressing at
  a fixed delay.
- What the user waits for on this route: Continue -> 58A set (not measured) + **~93 s** + their own press. The PC does the
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
     FileReadAsyncSubmit 312F7C and FileReadBatchLocked 7401F0 hold an SRW lock across that call. So reads that are async
     on Windows become serial, device-latency-bound reads on the 3 TrySync step threads.
   - Other multipliers:
     - The Ally frame is 23-34 ms vs 9 ms on PC, and steps and uploads are noticed once per frame.
     - The main thread spends ~50 % on upload/frame fences, with the GPU at 99 %.
5. **The 'other' read sites, statically:**
   - 740392 in FileReadOverlappedBatch 740300 (one overlapped read per entry, under the lock in 7401F0);
   - 1FD9CA3 in CrtReadFd 1FD9C58 (CRT _read);
   - 32562BE in FileReadSyncAtOffsetB 325626C.

   Names are in `re\symbols\incoming\ally-load-renames.tsv`. Which site makes the 578k calls is not measured yet.

### 7.3 Fixes per holder (game-side, default-off ASI flags; none built)

| # | flag | targets | design | same look because | expected (of the ~93 s) | gate before building |
|---|---|---|---|---|---|---|
| 1 | **GF+W6+RWb during loads** (already in the ASI) | the 7.5-core idle baseline; main-thread fence spins (50-65 %) | turn them on for this route; no new code | already proven same-result | **3-10 s**: on the shared 25 W budget, spinning cores take power from the step threads and the GPU (loading.md §3 #2) | A/B only (§7.4 step 1) |
| 2 | **RC** read-coalescing cache | 578k small 'other' reads (60.8 s in the calls) | IAT-hook the 3 'other' sites and 312DD2. Per read-only handle: on a miss, read the aligned 256 KB block; serve later reads inside a cached block from memory. LRU, 32-64 MB cap; only handles opened GENERIC_READ without write sharing | identical bytes from a file that does not change during the run; same call order and results | if >= 80 % of reads fall within 256 KB of an earlier read: 60.8 -> ~5 s in the calls, **~15-40 s wall** if those reads sit on the TrySync step threads | RP probe: locality and calling thread per site |
| 3 | **AQ** true async archive reads under Wine | 63k archive reads (7.2 s in the calls), serialized per file by SRW lock f+A0h | in FileReadAsyncSubmit 312F7C, hand the ReadFile to a 4-thread ASI I/O pool and return ERROR_IO_PENDING; complete the game's OVERLAPPED/event (the Windows contract the game already handles) | same reads, same completion path | 2-6 s (more if RP shows lock waits) | RP: lock-hold time at 312F7C |
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
2. **RP probe** (new file, sonnet). Per 58A window, for each of the 7 ReadFile sites, log:
   - calls, bytes, and a histogram of time spent in the call;
   - the calling thread's pool (TrySync / RequestLoad / KIDSTS / other) and the number of distinct handles;
   - the gap to the previous read on the same handle: same block, < 256 KB, or far;
   - the SRW lock-hold time at 312F7C and 7401F0.

   Also fix the LoadGates tail accounting (§7.1). RP decides between RC, AQ and TS.
