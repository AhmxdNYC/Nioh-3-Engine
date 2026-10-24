# Return time: ranked offline plan (#10221 / backlog #9608)

Verdict: first shorten the cover after successful put-back; next replace the global cell-completion wait with verified destination-cell readiness. Do not start by bypassing the world loader. A captured 61.579 s Return includes **25.000 s waiting after home-world readiness** and **8.845 s after put-back**. Neither is evidence that native WarpOtherWorld itself costs 61 s.

Scope: static source/log study on cx-pc, 2026-10-17. No game, installation, benchmark, or binary change. Proposed settings below do not exist yet. Implementation source reviewed: `../nioh3-hp0/mod/Nioh3Training/src/training.c`, branch pc/hp0-stagger at 87dcfe7. The main workspace has an older source layout; port to the lead's current tested build, preserving subsequent fixes.

## Evidence and limits

All rows below are historical PC logs from `captures/pc/run_0354_Training.log`; repeated copies in other capture folders are not independent runs. Return is B1 -> E0, start 2058080, mission 0, journey/mode 1. Graphics settings and full per-run build provenance are not established by this aggregate log. These are logged events, NOT measured screen visibility or input response; no matched OFF/ON Return experiment exists here.

| Log lines | Request-to-done | Native request-to-put-back | Put-back-to-cover-release request |
|---|---:|---:|---:|
| 62466 / 62555-62556 | 58.250 s | 47.047 s | 10.954 s |
| 68855 / 68944-68945 | 61.578 s | 51.328 s | 10.001 s |
| 72653 / 72742-72743 | 57.375 s | 48.360 s | 8.747 s |
| 84804 / 84894-84895 | 62.766 s | 47.469 s | 15.051 s (settle cap) |
| 91716 / 91722 / 91769 / 91807-91810 | 61.579 s | 52.485 s | 8.845 s |

Last row breakdown: click-to-native-request ~0.255 s; native-request-to-home-ready 27.485 s; home-ready-to-put-back 25.000 s; put-back-to-done 8.845 s. Rounding/tick vs log clocks differ by a few milliseconds. Home-ready means CurWorld==home, PlayerChar present, StageState==5, GAMESTATE[2]==0, not proof of collision at the stored spot.

Exclude lines 88085-88095 from timing comparisons: F9 freeze prevented progress and the old 60 s timeout falsely logged success. The subsequent source fix releases F9 and preserves the position watch. Do not regress that fix.

## Why the waits occur

- `RhTick` (2309-2365) fades out, ends training/boss mode, sends native WarpOtherWorld (22399B8), waits for XrTick, then waits on FadeSettled before RhDone releases the Return overlay and fade.
- `XrTick` (1939-1959) requires home-world/stage/player/load readiness, then 700 ms minimum. It waits as long as **25 s** if CellStats reports any non-ready cell. That is exactly the latest observed plateau. At 25 s it warps anyway; the timeout is not a positive collision proof.
- `CellStats` (1533-1543) uses the whole stage cell count, iterates at most 256 entries, and counts byte +E8 as ready. It is not a home-position query. Log n as well as ready: n>256 cannot reach equality with this implementation (not proven to occur in this capture).
- Phase 4 waits for FadeSettled (2115-2126), whose AreaObjPending check is global and whose PlaceableWaitMs default is -1, with a 15 s outer cap. It does not identify the destination's visible/required objects.
- Return has its own EngineFix loading overlay (`RhLoadScreen`, 2079) and own fade (`FadeWant`). Native widget close alone cannot remove those. `notes/loadscreen-state-study.md` also identifies the separate native black fader, released in sequencer 0x12.
- Forward lsfade is an early presentation change around a native cross-world transition. It is not proof that an unloaded world can be restored by an in-place actor swap. Same-world Return already uses direct Warp; cross-world Return needs the home world's cells and actors.

## Ranked experiments

| Rank | Candidate | Expected scope of benefit / risk | Gate before promotion |
|---|---|---|---|
| 1 | Release Return cover shortly after successful put-back | Targets observed 8.7-15.1 s tail; leaves native load and 25 s position gate intact. Props may still stream; visual and input checks required. | Same correct home position, collision, and no added visible loading failure; measure visibility separately from control. |
| 2 | Destination-ready put-back, replacing global CellStats equality | Targets up to the observed 25 s plateau. Prefer positive destination-cell/collision readiness over changing timeout from 25 s to 0. | Identify stored-position cell and required neighbors; prove local collision/geometry ready even when unrelated cells remain pending. |
| 3 | Return-only early native fade/widget close before put-back | Could reveal the home world earlier; cannot claim faster completed Return. May expose native start point followed by teleport, frozen input, or streaming holes. | Log destination readiness first; release all three cover layers explicitly; keep input gated until safe arrival. |
| 4 | Near-home selective loading | Could reduce the native-load component; no measured Return gain. Forward NearArenaOnly was neutral/worse on PC (17.8/21.9 s OFF vs 23.9/22.3 s ON; different boss rows in cross-world note). | Return-specific allowlist includes native start AND stored destination, collision and dependencies; prove deferred loads resume on every exit. |
| 5 | Retain/restore home world in place | Highest engineering risk; may require retaining cell resources, actors, scripts, mission context, and teardown ownership across the outbound warp. | Demonstrate retained valid resources first. Never reuse old entity pointers or write CurWorld to simulate a switch. |

Rank 2 offers the larger credible opportunity but requires a new readiness predicate. Rank 1 is the smallest useful behavioral probe. Pure early cover at native home-ready would expose ~34 s of remaining return processing in the latest row; do not label that an instant Return.

## Smallest testable build: Return-only cover tail probe

Training-only patch on the lead's latest working branch; no EngineFix change expected. Add `[Return] CoverAfterPutBackMs=-1` (legacy default), with test ON=500. Keep existing forward Fade/LoadingScreen settings identical in both arms; do not globally change PlaceableWaitMs.

1. Add a per-return result latch set by XrTick's actual Warp return value; same-world Warp populates it too. Reset on every request. Preserve g_xr watch, F9 release, abort and timeout handling. A late-watch/failed warp must never qualify as successful put-back.
2. In phase 4, ON may release cover after 500 ms only if successful put-back, no pending g_xr, target world still matches, valid player, stage 5, GAMESTATE[2]==0, and FadeBaseReady. Retain legacy FadeSettled completion and 15 s cap as fallback, but log timeout/fallback distinctly from verified success. Do not clear GAMESTATE[2], bypass sequencer/actor gates, or call RhDone early from phase 3.
3. On the ON completion path call the existing RhDone: it clears RhLoadScreen and FadeWant together. Native fader should already be naturally released this late; if visibility remains black, record native sequencer/fader state before adding a BlackFadeIn call. Never assume a release log proves a bright frame.
4. Add per-return event timestamps: click, native request, first home-world/stage ready (independent of load flag), load flag clear, Xr ready, CellStats n/ready/loading/relq, actual put-back result and position, phase-4 entry, AreaObjPending counts, cover request, completion reason. Sample changing gates/once per second while waiting, not every frame. Log effective settings and build hash with the run manifest.
5. Offline validation: compile on the actual integration branch; review OFF equivalence, ON latch reset and watch/failure paths. This deliverable specifies the build; it does not implement or claim to compile it.

If the latest row repeated, the cover-request endpoint would move from 61.579 s to roughly 53.234 s (8.345 s earlier). That is a conditional estimate, not a measured improvement or guaranteed control time.

## Lead-owned game validation and next step

PC only first; same save/home coordinates, boss/world, graphics settings and installed hashes. AABB: OFF=-1 twice, ON=500 twice, keeping all other settings fixed. Lead owns backups, lock, installation, launches, capture and restoration; cx-pc does none of those.

Record (a) click-to-first usable visible frame by luminance/video, (b) click-to-correct-home-position, (c) click-to-actual player control, (d) native sequencer/load duration and (e) cover tail. Check movement/collision and that no training boss/NoDeath/virtual-pad override survives Return. A later hands-on visual check belongs to the lead's needs-player workflow, with real controller restored.

Accept only a reproducible shorter visible wait without slower native load, wrong location, falling, hung control, or missing required geometry. No gain: keep OFF and use the gate trace to determine whether the 25 s global wait is irrelevant distant cells. Next build for rank 2 should log candidate destination-cell identity/readiness before changing its admission rule. Reuse existing cell enumeration metadata, not the forward boss whitelist blindly. Existing comments at training.c:1545 document that bypassing G9 only moved time into actor state 6 and bypassing that actor wait hung at state 0xF: do not repeat that as a Return optimization.

References: `notes/cross-world-load-2026-10-16.md`, `notes/loadscreen-state-study.md`, source anchors above, and the aggregate capture log. Historical logs lack Return luminance/control synchronization and an OFF/ON pair; performance remains unproven.
