# Return object: native 'End mission?' box (#13796, #9664 onward)

Branch pc/gate-native-suppress (base pc/stale-boss-mode 1f9a49b). Offline only: nothing installed, the game was not launched.

## Symptom
In Training / Practice / Boss Match arenas the Return object (Nioh3Training 'GATE spawned', ReturnGateAlways) opened the game's own
'This will end the mission. Proceed? Yes / No' box first. Our 'Return gate - Practice' page came only after another press, sometimes
both at once (shot-13796.png). Then the native box could not be answered.

## Evidence (live logs copied to captures/gate13796/, user session 22:37-23:36)
- Training 23:25:17.192 / 23:25:20.995: 'GATE: player at the gate with the boss alive (Practice session) ... hidden'. That line logs
  each entry into the 110 u circle, so the player left the circle and came back in between.
- 23:25:25.534 to 23:26:30.094: 11 times 'pad A at the gate (boss alive, Practice) -> our gate page (2)'. EngineFix shows 'opened
  (Boss Rematch request)' then 'closed (B / Esc)' 1-2 s later each time. This is the loop: A to answer the native box, we hide it and
  reopen our page, B closes ours, repeat.
- In every captured log (this session, freeze9655, selftests) none of the game-side blockers ever logged once:
  EndMissionConfirmOpen 12A1F80, PauseMenuDecide 1DAE4E0 / 1DB58B0, DialogOpen 348274 (which logs EVERY box while the gate is up),
  MissionRequestReturn 22396D8. So the box does not go through any of them. The only thing that ever kept it away was hiding the pad
  buttons from the game (selftest 10106: idle gate, 400 u mask, 5/5 tap-A showed only our page).
- EngineFix 'game menu signal' (5A37D4 / D63C0) did not change while the box was open at 23:25. The pause-menu signal does not
  cover this box.

## Root cause
1. Wrong reach: with the boss alive (Practice / Stand still, ReturnGateAlways #13255) the face-button mask and our tap-A opener
   only covered ReturnGateRadius = 110 u (|dy| < 250), from #13548. The clear gadget's native interact reaches farther. An A
   between 110 u and that reach went to the game: native box first. The idle path used ReturnGateMaskRadius 400 u and worked.
2. Stuck: with the native box up, the next A (meant to answer it) was inside 110 u. We hid it from the game, and the raw-XInput A
   read (GatePadA, independent of the mask) opened our page on top. The menu then blanks the whole pad, so the box could never get
   input. B on our page went back to the mod menu's first page (another screen still masking) instead of closing.
3. The four game-side hooks target the wrong functions (static RE guesses). The real open path for the gadget is still being traced
   (see the RE section).
4. Side note: in this session the gate again floated 95-111 u above the floor ('GATE ground: y 15111 -> 15111 lowest trail sample',
   player y 15000). Its only trail samples near it were the frozen warp pose. This was not changed here.

## Fix (commits on pc/gate-native-suppress)
- Training gate_zone.h (unit test tests/gate_zone_test.c, in run_move_toggle_tests.py): ONE zone for every mode (idle, Practice,
  Stand still, Boss Match), radius ReturnGateMaskRadius (400 u, |dy| < 400):
  - Inside the zone, while our page is open, and for a short time after leaving (2000 ms idle / 300 ms with a boss), the face
    buttons are hidden from the game. The native interact never sees a press.
  - A TAP of A (ReturnGateOpenButtons 4096, released within ReturnGateTapMs 350) pressed inside the zone opens our page on the
    release. A longer hold does nothing ('cancelled'), and the game never sees it (EngineFix keeps a masked held button hidden
    until it is released).
  - A press made while our page is open never reopens it (that was the loop).
  - Safety net: while a native modal is detected (EngineFix Nioh3EngineFix_NativeModal, or our dialog hooks letting a box through),
    nothing is masked, nothing opens, and our menu is closed (Nioh3EngineFix_MenuClose). The player can always answer the game's box.
  - [Return] ReturnGateZone 0 = the old two blocks.
- EngineFix: B on a page the add-on opened (the Return object page) closes the whole menu. B at the base of a shortcut mode page
  (Practice / Boss Matches) goes straight back to the first page, not after a dead second B. New export Nioh3EngineFix_MenuClose(why).
- Mode page on the shortcut (Start x3 / Esc x3 / menu key -> Practice or Boss Matches page) was already in the base (a9c66f6,
  OpenModePage; live log 23:20:05 'opened on the Practice page (current mode, #13529)'). Reused, not duplicated.

## Trade-off
In a Boss Match where the boss is fighting, the face buttons are hidden within 400 u of the Return object (about 2.2 m behind the
round-1 start), and a tap of A there opens our page. Before, fight inputs were untouched and the native box could open. The user
asked that the native box never opens. To change this: ReturnGateMaskRadius smaller, or ReturnGateZone 0.

## Needs an in-game check
- Walk up to the Return object in Practice (Stand still and Fight normally) and in a Boss Match. Tap A: only our page, no native box.
  Hold A: nothing opens.
- B on the Return page closes the menu. Start x3 opens the mode page, and B goes back to the first page.
- Log lines 'GATE zone: player at the Return object', 'GATE zone: tap ... -> our gate page', and 'GATE zone: a native modal is open'
  (the last one should never appear).
