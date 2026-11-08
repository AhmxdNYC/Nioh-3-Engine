# Continuous camera spring (#83216)

Based on f93bff9c; separate from native-list/pack-pin work. PC live baseline: BR c7315ca3, CamKi 617d93cd, EF 9bfe7189 (public fleet latest remains v1.8.10 EF b7eeb478).

Read-only live evidence: latest camera session presses 2-10 report maximum applied offset steps 82.705-91.934 world units/frame. Press 10 enter peak is only 4.846, but whole-press peak is 91.156: the problem also happens AFTER entry. Exit blend peaks are 9.232-13.455. BR reports maximum absolute seal-player Y 361.85-433.46. Those dY fields are offsets, not consecutive-frame deltas. The logs are summaries, not a per-frame actor/target trace: they do not prove a particular seal snap, actor swap, fallback or filter reset. No extra enter/exit pairs appear within these completed presses. Evidence excerpts are adjacent.

Replaced persistent 20 ms exponential filter with exact critically damped integration, omega=1.67834699/0.25; a stationary error halves at 0.25 seconds from rest. Filter position and velocity survive target/session changes; steady-state gain remains 1. A sample >80 units from the accepted target is held for one frame, accepted if the next sample confirms it within 80 units, and eased through by the spring. Initial target goes through the same spring. No actor reads were added; producer identity guards are unchanged.

Enter/exit still use 0.7 second smootherstep from current applied position, with a decaying tangent to preserve applied velocity if interrupted. Exit works on stored numeric state. New per-press completion telemetry includes maximum raw target step and held-spike count alongside maximum camera offset step. Values describe the added camera offset, not total world camera movement.

Validation: 691 production-source C++ checks PASS, both Release builds PASS, CamKi probe_test PASS. At 62.5Hz, a 400-unit sustained mid-form target step peaks at 15.796 units/frame; an isolated 1000-unit spike is rejected. Stationary target half-response at 0.25s is verified; interrupted exit retains position and velocity. A 2000-unit initial height handover peaks at 89.950 units/frame (exit 89.151), so the dynamic-step result is not a universal speed bound. Producer lifetime/recycle/SEH and output-direction checks remain passing.

Candidate BR 7084366e92e350f929252dbb091a843ffe666ff64be13248e22b0c25c0ab11f5 (rebuilt companion, producer behavior unchanged).
Candidate CamKi a92e520b2f91922f9a98f1088b8b064ee06478321c9e845891817bfd2ee02afc.
No game interaction or installation. Next: Claude game owner checks the same forms live, compares max frame step and raw max step, and verifies camera feel/entry/exit. Native crash is not addressed by this camera change.
