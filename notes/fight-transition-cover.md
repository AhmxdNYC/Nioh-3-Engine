# Hosted Fight now transition cover (private candidate)

Engine 1.7.69 / addon 1.4.13 / menu bridge ABI2. The UI worker stages a unique cover token; it does not post Fight now. Engine draws an opaque full-screen quad, submits the frame, and acknowledges only an outermost non-TEST final original Present S_OK under the same token/session/context generation. This is accepted presentation, not physical scanout proof. The game-thread cover pump posts one Fight request only after that acknowledgment. It uses the monotonic native attempt counter, including after normal return resets the active record.

Release requires exact attempt, mission/start and expected placement; fresh same-attempt Character::Create and BossBattleMode entries; current readable solo/mission/loading/pause state; conservative native input restriction manager clear; expected actor materialized and algorithm active; conservative passive inverse of native DEAD; and current player status not dead. Reads are bounded and repeated, including actor/player identity and mission state. Unknown predicates remain closed. A continuous 500 ms readiness interval filters transient samples; it does not substitute for the native predicates. Pointer double reads do not establish allocation epochs or physical button response.

Input ownership uses actual pad and keyboard hooks, raw B/Escape cancellation, full keyboard/pad capture, and independent worker/presentation freshness checks. Pre-ACK capture has an explicit five-second lease. Absolute transition timeout is 90 seconds. Cancel/stale/unavailable/failure clears presentation and capture only; it preserves queued native requests, quarantine and spawn/map restoration ownership. No pause/loading-bit changes, automatic retries or exits. Terminal tokens are tombstoned per host session to prevent stale snapshots resurrecting capture.

## Pilot oracle

Pipe `cover` returns `token phase attempt mission start placement presented issued bossEntry placementEntry deadline control algo life playerAlive sample reason` key/value fields. Phase 1 awaits presentation, 2 awaits native entry, 3 observes load, 4 is ready, 5 failed, 6 cancelled. Logs include the same token with `event=begin`, `event=present_ack`, `event=native_post`, then terminal `event=phase`. Attempt is assigned at native_post. `sample` timestamps the last passive readiness sample, not a new read on status query. Phase4 retains its readiness sample.

Yamagata R1CBC: mission0x1CBC, start67064, placement65092; existing validated arena/spawn override and box66599 native entry pathway. This cover does not add new destinations or routes. Root must verify visible opaque transition, actual boss entry/control and native owned return on the exact built pair; no live acceptance is claimed by offline tests. Standalone Fight now, Shrine and Practice do not use the hosted cover.

## Offline checkpoint verification

Both builds pass. Engine `check_release.py --full` under project MSVC setup: 0 errors, 3 existing INI/provisional-profile warnings; all 255 Engine sites pass. Addon all 94 sites pass. Focused cover58, passive reader200, accepted-Present37, native-anchor20 and provider67 checks pass; actual menu/UI harnesses pass including repeated-attempt counter, failed ACKs, full input capture/cancel/stale behavior and retained queued request/recovery sentinels. Native admission96 + standalone return4 + unreadable exit gap1 + map ownership15 regressions pass. Existing Engine compiler C4456 shadow warning remains.

Frozen build SHA256: Engine `5efe5951bf69c170f36305bd1f0101b9d76f90658fa73f55e0993216dee62aa5`; addon `7c63fbc3411991161dbcdc4da549d390e23186bb69ef5cbe33085e66144cde98`. No deployment or game launch performed by this source checkpoint.
