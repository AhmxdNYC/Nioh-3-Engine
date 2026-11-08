# Camera handover smoothing #83125

Full seal-player Y tracking retained. Shared Spring now separates a persistent 20 ms follow filter from a 0.4 s smoothstep handover. Enter begins at current applied offset and approaches the moving filtered target. Exit holds the last valid yokai filter target and blends the current applied offset to zero. First transition frame has exactly zero displacement. Interrupted exits and new session IDs while active begin from current output. No filter reset on any handover.

CamKi logs per-press enter/exit start and end offsets, blend maximum per-frame step, and whole-press maximum step. Previous peak target/applied logs retained. Native camera hook and actor positions unchanged.

Validation: 416 camera checks + 51 ground checks PASS; both Release builds PASS; CamKi probe_test PASS; diff check PASS. 2000-unit stationary target at 60Hz: enter maximum step 124.731, exit 124.711 (<130 bound); first frames exactly continuous. Full steady follow within 5%; grounded zero; re-entry/active session replacement covered. Per-frame bound is fixture-specific: rapid target motion and low frame rates can produce larger steps. Live visual smoothness remains for Claude pc.

BR c357195026a0ca8d5663c126daae0ecaa95e69659d119578e42f7b84f2a38e1c
CamKi 5938a5ad47e2c182125b8f303bc7674620f9e830e579866f3a6e2596727ce2db
