# First soul-core use audit (#81537)

Base: 367033fc, equipment/menu owner cx-pc/0 on #81516. This branch adds diagnostics only, not a visual fix. No game was run or modified by cx-pc/1.

## Proven source difference

`CoverStartSmoke` asks `CoverAt` for the d-pad slot's smoke template. `g_slotTplOk[3]` starts false. Only `CoreTplBind`, using a recent native effect captured by `HkFxRec`, fills it. Consequently the first request uses the game's smoke; later requests can queue smoke immediately. Player fade and yokai reveal timers are armed before this failure return. Resolving the equipped core ID alone cannot initialize this independent effect cache.

`PickPoll` treats unknown core ID as transform ON; `PickLearn` can suppress a newly identified OFF core after press-time steps have already run. That explains a first-use OFF mismatch, but not by itself the human's report of poor first-use ON visuals. `HkMotionSpeed` has no first-use cache requirement for its configured FastThrow action multiplier. `HkPpF3D78` uses a fresh-spawn gate; missing model-slot capture has a separate spawn-hit fallback. Do not assert model loading is the cause without current logs.

## Historical evidence (not a current-build reproduction)

Local `captures/style-58307/log-ee905384-0400.log`: line 315 has no smoke template at t=53.986; line 445 has queued smoke on the next request at t=58.219. At the first request, spawn-hit fallback begins a form, the pack reports loaded and ready after one poll, and FastThrow later reports x8. Thus this historical first use was not a missing form or unloaded pack. Similar cold/warm smoke transitions appear at lines 331/501 in log-04e6c845-0347.log and in log-45fb8ec5-0354.log.

Current installed PC BR 8aeb68a2 log has no matching first-use lines. The human's current Ally session is not reproduced by these old captures.

## Diagnostic patch and next work

Adds `FormCore first-press scope=process dir=... core=... transform=... smokeReady=... source=...` at the first cover request for each direction (including an unknown-direction bucket). It reuses the existing CoverAt result, adds no scan, and changes no effect scheduling or visibility. It is process-first, not a save-load detector; OFF transformations or disabled smoke do not reach this log.

pc was asked for Opus assistance in #81544 to identify a safe per-core native effect initialization path. A persistent learned-template cache would not solve a never-used core on a fresh install. Do not substitute another core's smoke; existing per-core isolation prevents the wrong-size effect regression.

Claude test plan after the actual fix: fresh process and loaded save, first LEFT then first DOWN, repeat both; restart and reverse the order. Record first/later appearance, smoke placement/timing, one begin/end per accepted seal, and movement/input restoration. Exercise both per-core ON and OFF, plus a changed equipment loadout. Correlate first-press, SealCover, FastThrow, FormSlot0, spawn-hit and FormHide logs with video. Current diagnostic build alone does not satisfy the requested visual gate.
