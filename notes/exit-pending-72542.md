# Pending exit after hub re-entry (#72558 / Ally #72542)

Base origin/pc/final-72394 d5b23cb9; branch cxl-pc/exit-pending-72542. Offline only. No install/game/save/lock access.

## Proven sequence
Bridge5498, ret_fast.log:12:02:13 am EST (+38.2s after Return) hub mission0/retry4019512, loading0, state2. At12:02:16.759 another load begins before the5s cleanup timer. At12:02:18 mission0xCC53/retry4061333 (ordinary start, not arena4066690). At12:02:48 loaded but state2 persists.12:03:26 Fight now refuses 'native replay/return still pending';12:03:28 Return refuses 'return already requested'; still stuck12:03:55. Controls finish about5s after hub load-down. The exact Battle Scroll click is not logged; hub-to-new-load/same-mission/different-retry is proven (46 A presses in script).

Cause: TrackReplay considers any mission==g_fightMission to still be the owned Boss Match. It clears g_exitSince on that equality or loading. The new real mission has the SAME ID, so its arrival permanently erases the short hub departure. AttemptObserve previously observed returning state only for standalone returns, leaving aReturning locked until the old replay tracker eventually exits.

## Fix
Every successfully called explicit return now records the mission/retry it is leaving, resets its own load witness, and watches for that return's load. After load-down, a readable changed mission OR retry latches departure. The first ready/player-present/nonpaused landing completes the exit, including mission0 hub or a non-hub destination. If the hub remains UI-paused and another load begins, latched departure survives; even the same mission/retry landing can finish the old exit. No5s window is needed for explicit returns.

Normal ToIdle performs ownership-aware spawn/map restoration; conflicts still quarantine. Unknown/online sessions, unreadable state, no return load, no departure, and failed return calls never unlock an attempt. Natural non-explicit mission exits and same-mission death/retry retain their previous logic. Training needs no change: its existing MissionRequestReturn hook already cleans native ownership before forwarding the return (confirmed in Ally log). Clear guard/story code unchanged. No hub ID or per-mission/boss lists.

## Offline validation
Production AttemptObserve/AttemptReturning/TrackReplay/Tick/ToIdle and ExecDirect are compiled into the existing test_native_attempt.py fault fixture:
-8 new recovery checks: observed hub->same mission, actual subsequent Fight now admission (request count1->2), paused hub->same ID+retry, direct same mission/different retry non-hub landing, unchanged landing held, restoration conflict quarantine.
-5 standalone landing checks: hub0 valid; unknown game state does not fake a load edge; no player does not settle.
-Existing5 gate-return/retry,4 admission, unreadable gap,15 map ownership and96 transaction checks pass.
Fixture refreshed for upstream ClearedNow signature and generated EntryGateChoice helper; no game calls mocked differently to obtain recovery.
BR and TR build exit0. Actual output SHA256:
BR90ee19f79ff5a78aa81b33f947bf457d8863f2a79a688c2cd9af288834155584
TR2221eecef359fe926f849ef18b80cd3f7d37ff329b461ca430ec09ff99edf8d2 (unchanged Training source, rebuilt matching base)

## One live gate (pc/6 decides integration)
Return from one Boss Match, immediately start that same mission normally from hub Battle Scroll. After landing expect state0 and no pending-return refusal; request another Fight now and confirm admission. Include one normal Return after it. No long repeats or all-device gate needed. Live behavior remains unproven; offline only as requested.
