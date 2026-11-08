# Count poisoning and short-count exits (#82679)

Base a7d46a32 (per-press height) and625659ca (world-only smoke). Source b7e8d7e1 on cx-pc/variable-82679.

Tail watcher now logs post-exit bursts without writing INI counts. Natural-complete and terminal-complete learning initializes unknown counts or lowers an existing count from an eligible complete observation; it never raises the learned minimum. Existing late-bind/unreadable-sample safeguards remain.

Motion watcher now runs and produces terminal signals with SealIdleEnd=0 and logging off when ghost+cover are enabled. Default fallback requires at least one observed burst and a readable empty attack pool; it waits through active helper windows. Resuming an attack invalidates a latched terminal signal. Explicit SealIdleEnd settings retain their prior semantics. Natural despawn and cap remain fallbacks. No arbitrary quiet timer: documented inter-hit gaps are up to1.2s. Exact final-hit prediction cannot be proven for variable patterns from these observations.

1155 offline checks PASS: last-hit/motion288, primary/replay243, first-use52, ground51, cold-smoke23, picker112, equipment29, rematch357. Available sc22 replay ALL OK (missing captures explicitly skipped). Release PASS. New tests simulate u6 exit1 then two tail bursts then u8 exit1; already-poisoned3 and LEFT2/observed1 exit on terminal signal after closing the real window. Actual C53->idle detector tested with mode/log0. No claim of exact live trace replay; detailed pc2 lines requested in #82689.

BR fb10abe0982a6f8aa5f5b1c343f55ce142d47080d58f09fed2608a79a04c4764, bridge #6455. Installed PC BR629dc81f untouched. Next Claude live test short-count LEFT/DOWN on slopes with smoke/height retained. No game actions by companion.
