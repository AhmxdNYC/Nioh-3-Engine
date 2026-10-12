# First-boss selectable move practice

User request2026-10-11: show a boss's move list, keep that boss idle, then let the player select an attack for the boss to perform. Scope is the first boss only for now. Working target is Yamagata Masakage, the existing Practice target R1CBC/mission0x1CBC/start67064/placement65092; a clarification about tutorial-boss terminology is pending. This does not authorize generalizing a discovered ID to other bosses.

Desired first interaction: enter the supported practice encounter, select a named move, request it once from verified idle, let the full attack and recovery complete, then return to waiting for the next selection. Natural animations, hit reactions and encounter cleanup must remain coherent. Random attacks must not continue between selections. Automatic loops, arbitrary summoning and arena changes are separate work.

Research is split between the actual per-boss action catalogue/native request path and the idle/AI scheduling/return-to-idle contract. An independent review checks actor identity, request ownership, phase restrictions and invalidation after death/loading/return. A sampled placement pointer or animation ID is not permission to call a native action through it.

Before implementing the first enabled move, bind an actual Yamagata actor variant to its action IDs and friendly labels; identify the native action request arguments and acceptance; suppress autonomous choices through a proved actor-local mechanism while retaining animation/reaction progression; prove completion/recovery and idle reentry. Unknown IDs, busy/reaction/dead/transition states must refuse without silently retrying. No global time freeze, guessed animation call or damage immunity is implied by the request.

The requested feature is under research. Existing Practice is native mission replay, not this move-drill mode. Findings belong in `training-yamagata-move-selection.md` and the complementary idle-contract note; do not expose a working move picker until its backend and one-move runtime cycle are verified.
