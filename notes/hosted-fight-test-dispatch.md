# Private hosted Fight now test dispatcher

Separate successor of7aedb5c. Addon1.4.20, unchanged Engine1.7.71/ABI2. Not installed; native acceptance pending.

Explicit local settings: `[Remote] Pipe=1` and `HostedFightTest=1` (default0). The ordinary hosted Engine menu and input hooks must work. This is a private test command, not a new gameplay option.

Submit `hostfight R1DEA`: `queued serial=<u64> session=<u64> row=R1DEA`. This only queues one UI-worker operation. Another pending request is refused without replacement. Poll `hostfight`: `serial=<u64> session=<u64> state=<int> row=<id> token=<u64> reason=<text>`. State0=none,1=queued,2=cover begun,3=refused. State2 is NOT native acceptance or readiness. Errors start `error `. Never retry an ambiguous submission automatically.

Worker consumption rechecks exact catalogue ID, ordinary Gates/CanArena/current cleared result, session, deadline and interference. Manual confirmation uses the same BeginHostedFight helper. Unknown or active cover cannot be queued; close/detach,5s queue deadline and1500ms worker freshness invalidate pending work. Invalidated work cannot revive. Native request remains behind real successful covered Present; no pipe fight fallback or synthetic ACK exists.

Cover telemetry retains canonical `placement` and appends `row` plus `observedActor`. Bind submission serial/session/row, consumed token, then same token ordered begin/Present ACK/native post/ready and native attempt. Phase4 and its sample timestamp are historical after completion: require separate current player/load/mission checks and explicit root visual/control approval. Native return and exact original-entry restoration remain the owning pilot's responsibility.

Offline: actual shared frontend UI harness covers absent row, uncleared/config refusal and CoverBegin without native Post; provider harness covers default-off, malformed ID, duplicate pending request, once-only worker consume, actual ACK absence, close/detach/stale/deadline invalidation and normal-gate refusal. These are not native gameplay results.
