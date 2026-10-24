# Half-rate activation telemetry (#28160)

Base: eccb0650, the source supplied for artifact1780. Version1.7.61-hrstats.
HRS/HRG/HRC stay default OFF; gate decisions, hooks, presets and INI are unchanged.
The existing worker writes three `HalfRate:` lines each second when installed and General.Log is enabled, including while all switches are OFF.

Example from the production-wrapper offline test:
```
HalfRate: HRS enabled=0 frames=5 reused_frames=2 recomputed_frames=4 mixed_frames=1 absent_frames=0 reused_calls=2 recomputed_calls=5 pending=0 clock=game_present totals=1
```

Counters are cumulative: subtract endpoints of each warmup/capture window. `reused_calls` counts actual skip decisions; `recomputed_calls` counts forwarded record calls, including OFF, near-shadow and VSM calls. Forwarding proves a record was allowed, not that GPU work completed. `reused_frames` and `recomputed_frames` count game-Present windows containing at least one respective decision; a mixed window increments both and `mixed_frames`. Thus reused + recomputed - mixed + absent = frames. Multiple cascades/calls cannot inflate a window count. `pending` is the unclosed window bitmask (1 reuse,2 recompute). No logged frame is synthesized by the once-per-second logger.

The frame boundary uses the existing NotePresentThread game/FSR-presenter classification, excluding classified generated-presenter calls. This is an observed game-Present window, not an independently verified engine record-frame ID. Multiple views/overlap can produce mixed windows. Missing Present instrumentation leaves calls growing with frames=0: treat frame totals as unavailable in that case. The original policy alternates per eligible CALL, not per distinct frame; this change deliberately does not repair or relabel that policy as true half-rate rendering.

Activation gates: identical artifact/config except one key; OFF reused_calls delta must be0; ON reused_calls delta must be positive and recomputed_calls must continue. Check enabled, pending, mixed/absent and frames too. Do not interpret OFF historical reused totals as new reuse. All-zero call deltas mean no activation proof. Camera-cut policy harness still passes. No game testing or performance claim.

Offline checks: production Gate/OnGamePresent/LogLine harness tests all three channels, OFF forwarding counts, ON alternation, multi-call dedup, mixed/absent windows, disable and not-installed logging. Existing skip/reuse/camera-cut harness also passes. Compile with /W4 /WX; build the full ASI separately using build.bat.
