# Training mode progress - 2026-10-11

Three isolated agents completed the first spawn, stage and integration research
pass. Root reviewed the notes and independently checked eleven native stage/pump
instruction windows plus the ResetReceivedDead binding. No training source,
build, live test, save or installed-file change has been made.

## What exists and what is still missing

Existing BossRematch can request a cleared mission on the current journey and
enter its native boss arena. The private rematch-instant branch has experimental
1.4.0 arena-first player placement, with a prior reported Yamagata run. Main still
has add-on 1.3.3. That experiment does not establish arbitrary arenas, enemy
creation, repeatable resets or a reward-free sandbox.

| Requested feature | Current finding | Next gate |
|---|---|---|
| Choose a known boss arena | Reuse native mission loading and a verified start point | Observe mission, actual geometry, target boss and exit |
| Change stage live | Current route loads another mission | Streaming, assets, scripts and navigation prevent a seamless claim |
| Summon a boss/enemy | Activation needs a registered wrapper; acceptance can precede construction | Prove assets, ownership, readiness and disposal |
| Reset/revive a boss | ResetReceivedDead is a VM wrapper clearing one flag | Full HP/AI/script/gate reset is not proved; use native reload first |
| Despawn | A named despawn address is a dependent cold block | Establish a genuine single-actor disposal contract |
| Practice without progression | Native replay retains normal rewards/save behavior | Separate story, inventory, achievement and autosave policy/proof |

The first implementation is an **off-by-default read-only observer**, inside the
add-on's existing game-thread pump. It records valid-or-unknown session state,
mission/journey/retry IDs, request and load transitions, placement activation
entries, fight events and restore outcomes. It issues no game calls and changes
no mission, actor, health, position, reward or save state. Entry counts are never
reported as completed spawns or live actor counts.

The first later practice candidate is **R1CBC / Yamagata Masakage in its native
arena**: mission 0x1CBC, placement 65092, start 67064. Require strict request
admission, observed load/target readiness, native death/retry behavior and verified
exit/restoration. Before mutation support, audit request overwrite, failed session
reads, accepted requests that outlive timeout/abort, and v4 restore ownership.
Queue acceptance differs from completion; unknown session state must fail closed.

Evidence and concrete acceptance tests: [spawn](training-spawn-contract.md),
[stages](training-stage-contract.md), [integration design](training-mode-design.md).
All remain private research notes. There is no operational arbitrary Spawn button
or new runtime compatibility/performance claim.
