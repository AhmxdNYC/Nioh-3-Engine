# Repeat-last Fight-mode death (#69187)

Base pc/repeat-last-move 33770e97. Evidence: bridge file #5113 (not message #5113), SHA verified on download.

Observed: pc2 #247 Last 2 Fight substituted 4E6C then 4E7B. Last 3 Stand still completed leap chain 4E7B -> 4EA7 -> 4E7C several times. After switching to Fight, the leap ran, then at 08:04:11.358 the native pick 4E3B was replaced twice at the same timestamp by 4E55 then 4E64; HP became 0 by 08:04:11.498. Further native 4E3B picks kept cycling the capture list. Existing N3MoveUnsafe already identifies Yoritomo 4E3B/4E3C as boss-killing states.

Proven code fault: Repeat-last treated every catalogued ID >= 4E20 as a new attack, including the native death-action pick; it also treated low-ID landing/reaction states as cycle boundaries. The exact game-side cause of the leap's lethal transition is NOT proven from this excerpt. No HP write exists in the recorder/repeat implementation. BvHpHold only produces a positive target for a living boss with nonzero maximum; this patch does not change HP or revive a dead actor.

Fix: quarantine Yoritomo's captured leap/landing family 4E7B/4EA6/4EA7/4E7C from Repeat-last capture and validate EVERY history member when arming; log each skip. Preserve the order of safe members; if all are unsafe, stop the repeat. When a native 4E3B/4E3C pick/state appears, stop Repeat-last and let that native action through unchanged. Start the next captured member only from native idle 2, not recovery/reaction/unreadable frames. Do not add leap IDs to N3MoveUnsafe, which would suppress native landing continuations. Existing explicit move/sequence drill behavior is outside this automatic-capture fix.

Offline: actual move_toggle implementation harness PASS: 5061 catalogue entries, 4998 veto/toggle cases, captured unsafe non-head filtering, all-unsafe stop, safe order, other boss same numeric ID, native leap landing continuation, repeated same-timestamp death picks, and pump cannot re-force after stop. Existing Last 1/2/OFF and manual sequence regressions pass. Added Training build.bat matching existing build.sh flags so Windows build status is explicit.

Next: lead pc2 retest safe Last 1/2/3 in Fight plus Stand still, native leap capture logs a skip, stance switch does not force quarantined leap; confirm HP remains positive without attacking boss. No live test by cx-pc.
