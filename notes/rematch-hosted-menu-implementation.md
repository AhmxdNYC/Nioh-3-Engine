# Hosted boss/practice menu candidate

Engine 1.7.68 and addon 1.4.10 add **Boss Rematch / Practice** to the existing
EngineFix in-game menu root (also available at the end of the legacy flat list).
Use the existing EngineFix opening gesture/key, then select this entry. Once
hosted, the addon's separate F7/View+Up window and input ownership are suppressed;
they are not a second way to open the hosted page. Existing EngineFix navigation
and neutral-release handling apply. No controller configuration is changed.

The addon must have `[Menu] Overlay=1` to start its UI worker. Actual rematches
still require their existing startup configuration; the native practice action
requires Rematch Enable/FightNow and a cleared current-journey Yamagata mission.
The practice page says normal native rewards, drops and progression apply. It
starts exactly R1CBC/0x1CBC/65092 at shrine 67064, without warp, spawn override or
gate injection, and provides a return restricted to its owned native attempt.
Walking, combat and death/retry remain ordinary player-controlled gameplay.

The host resolves a versioned in-process ABI on a background worker, after an
actual EngineFix Present has occurred. The addon UI worker remains the sole
owner of its Ui* model. The render thread only copies a bounded POD snapshot and
posts normalized actions through try-lock callbacks; it performs no pipe, file
or native mission operation. The addon worker consumes those actions and the
existing game pump executes native requests through the established gates.

Attach is exclusive and session-bound. A worker acknowledgement follows hiding
the standalone window and making the addon's chained XInput hook a transparent
passthrough; no hook is removed from beneath another module. Queued actions are
checked against the displayed model generation; model changes invalidate stale
actions. Close has a separate priority mailbox and clears queued actions. A
worker heartbeat and independent pending-action timeout close a stale hosted
page and release EngineFix input through its normal neutral-release latches.

Offline coverage includes actual provider callbacks/worker, actual addon XInput
passthrough on four slots, both real menu models, and native admission/ownership
failure paths. These do not prove that the page renders or its gesture works on
the Ally. A live hosted-menu/controller check and the native Yamagata
entry/death-retry/return scenario remain required before a ready-to-play claim.
The Ally activation failure that motivated hosting is not yet causally diagnosed.

The prior Engine 1.7.67/addon 1.4.9 ASIs were archived at
`D:\nioh3-re\candidates\menu-9eeb174` with both original SHA256 pins verified
before these candidate binaries were rebuilt. Nothing was installed by this work.
