# Ki press probe plan (#35849)

gxl-pc #35843: on a miss the blue bar is about 23 frames (swing frame 18 to 41). Ki climbs about 55, about 3.4 every 10 ms, while the pool drains from 53.9 to 0. The check stays 0. 1153 is the idle Ki cap with the bar off, not a payout. No timed R1 was logged.

## Patch

One new log in `FmFeed` (`mod/Nioh3Training/src/frame_meter_game.h`), inside the existing Ki-window block. No meter colors change. Ini `[Debug] KiPressProbe=1`, default 0.

On each pump tick while the player is valid, read:

- window `K+0x74`, pool `K+0x5C`, stored `K+0x58`, rate `K+0x6C`, end byte `K+0x80`
- Ki at `status+0xAC` (`status = [actor+0x3A0]`, `K = status+0x98`)
- R1 as XInput bit `0x0200` (the same bit VirtualPad uses for RB)

`barFrame` counts pump ticks from the blue edge (`K+0x74` in `(0.05, 31]`). It resets each time the bar opens.

Log `KIPRESS` on three events, cap 80 lines:

1. Bar open: `KIPRESS open act %u`
2. R1 rising edge while the bar is up: `KIPRESS r1 frame %d t74 %.2f pool %.1f stored %.1f Ki %.1f`
3. Six ticks later, and again when the bar closes: `KIPRESS after frame %d dKi %.1f pool %.1f end80 %u`

A miss with no R1 stays the old shape: Ki up about 55, check 0, no `r1` line. A real press is the `r1` line plus the `dKi` on the after line.

Effect spawn stays out of this patch. `Effect::Request` has no native address yet. The Ki fields above are the comparison. A second patch hooks a spawn only if a late press changes a field the miss never changes.

## How pc drives it

VirtualPad on for that run only, then back to 0. Three swings. One-frame RB, not a hold.

- Early: RB on bar frame 2
- Mid: RB on bar frame 11
- Late: RB on bar frame 21

The bar opens about swing frame 18, so the waits after the swing are about 20, 29, and 39 frames. One swing each. Send the log. No new check-mark rule until those three `dKi` numbers exist.

## Measured (#35963)

pc, Training `543a68c8` (file #2408, no effect id), empty field, act 25160, 120 pump ticks per second. A later R1 returns more Ki, smoothly, with no step.

dKi by press frame: f2 2.7, f10 10.0, f21 19.8, f29 27.9, f42 48.1, f48 67.6, f52 78.7 (t74 3.98), f56 90.0 (t74 1.95). No press (miss): about 95.8.

Frame numbers are pump ticks from the bar opening, not 60 fps game frames. A press closes the bar at once, so end80 stays 0. The pool stays 53.9 until about f20, then drains.

Ki amount is not a perfect mark. The miss paid more than the latest press, and the curve has no jump, so do not paint t74<=4 from this log. The particle was not logged. File #2421 (sha 9ae298bd, commit 614bfa34) logs `KIPRESS fx` while the bar is up. Answer #35991.

## Caller (#35918)

The effect line is `KIPRESS fx <id> from <rva> frame <n>`. `from` is the return address minus the image base (a direct call is 5 bytes earlier). Effects still log while the bar is up with no R1, which is the miss control: fx lines and a close, and no r1 line. The hook still stays off if the live bytes at 3EAB1C differ.

## Measured effects (#36027)

pc, Training #2421 sha 9ae298bd, KiPressProbe=1, act 25160. File #2433. Hook bytes matched. Game restored to 8cfd9dc1, VirtualPad 0. This is not the caller build.

The file has 8 open/close pairs and no r1 line and no fx spawn line. The id below is the one on the close line.

Miss, end80 1: frame 49 dKi 92.3 fx 204, frame 54 dKi 95.7 fx 204, frame 54 dKi 97.2 fx 204, frame 60 dKi 96.4 fx 200, frame 57 dKi 95.8 fx 200.
Bar closed early, end80 0: frame 32 dKi 10.3 fx 202, frame 43 dKi 9.3 fx 202, frame 14 dKi -25.0 fx 200.

No press near the end of the bar. The close-line id is `g_kp_fx.last` from commit 614bfa34: every call at 3EAB1C updates it, and it is not reset per bar and not limited to the player. With no spawn line, 202 is not proven to be the pulse on the two short closes, and it is not ruled out as a perfect effect. The eight bars stay inconclusive. cxl-pc #36047. cxl-pc #36049 agrees, commit 0c245342 on cxl-pc/ki-effect-35871: no effect can be assigned to a perfect pulse from this file. Training 96b640ca was not this run.

## Grade flag (#36095)

pc, Training cc892f00, grade lines from the 96b640ca code. Not run by gx-pc.

t74 3.45, 2.90, 2.31 at frames 53-55: b19=1, x1060=07004000, grade perfect, dKi 78.8-84.6.
t74 15.47 at frame 29: b19=0, x1060=05004000, grade full, dKi 27.9.
Same effect both times: fx 800650 from 3EA5A5.

On these four presses bit 19 matches t74<=4. The effect id did not change. The earlier miss returned about 95.8, which is more than these perfect presses, so the flag is not the largest Ki number. One refill at 1153 had the window off and b19 0.

## Object flag (#36101)

pc, Training cc892f00 file #2437, KiPressProbe=1, empty field, act 25160. Log captures/kipress/kipress-merged.log. Game restored to 8cfd9dc1, VirtualPad 0.

The r1 line itself has b19 0. The next lines carry the grade. Perfect presses, bar frames 53, 54, 53, 55 (t74 3.45, 2.90, 3.45, 2.31): bits include 19, x1060 07004000, object slot 46 f13c 0x3080 (0x2000), close b19 1, dKi 78.8, 84.4, 78.8, 84.6.
Normal: frame 29 t74 15.47 dKi 27.9, and frame 44 t74 7.96 dKi 56.3. Bits omit 19, x1060 05004000, f13c 0x1080 (0x1000), close b19 0.
Same def 135 at +148. Effect id 800650 from 3EA5A5 on five of the six presses; the first perfect close is fx 0. The id does not split perfect from normal.
No press in this log has t74 between 4 and 8. The code rule is <=4. The meter slice uses that rule.

## Meter slice (#36098)

The player Ki bar stays blue for the open window. Cells where K+0x74 is in (0.05, 4] are a second color, cyan. The mark is 2 in `kiwin`. It is the timer, not a fixed 4-cell width. Not installed. The game stays closed.

## No boss (#36186)

#2450 did not draw in an empty field because the overlay required a Practice session. Scope 2 draws the player row when KiWindow or PlayerFrameMeter is on and there is no boss. The boss row still needs a session. An idle refill to 1153 with the window closed and no after-time is not a press (`FmKiPulseGrade` returns 0). The game stays closed.


## Buff grant log (#36261)

Hook 0x3EFB90 when the live bytes match. The log line is caller RVA, buff id in DX, RCX, R8, actor+0x89C, both +0x13C words, and the Ki timer. Grade words are perfect, normal, closed, or unread. They come from the timer on the next pump tick, not from buff id 900. Missing 900 does not reject a perfect press. Seeing 900 does not name the swirl. Effect creates already log as KIPRESS fx with the timer. Grants are logged while the bar is open and for 20 ticks after it closes. The game stays closed. Not installed over Training af8b26c4.


## Flag 0x2000 (#36498)

One reader in the dump: 0x2362ED7 tests the pulse object +0x13C bit 0x2000. The taken path grants buffs 900, 822, 496, or 2. It does not call the effect spawn. It also adds skill record +0x68C into a number (0x1ABB86C to 0x623AB4). No model, material, or mesh scale. Script effect 511 is only a Tokoyo-break candidate, not a read of this bit. The probe line is KIFLAG with bit19. Not installed. Training stays 8cfd9dc1.


## R1 buff and pick (#36541)

Same probe, two more lines while the Ki bar is open and for 20 ticks after it closes. KIR1 call marks entry to 0x23644F0. KIR1 buff marks the grants whose caller is 0x23645EA, 0x2364652, or 0x8448D0 (ids 0x15E, 0x1B2, 0x114, 0x248, 0x140). KIPICK is helper 0x845148: status+2DC8 bit 0x80 clear logs skip and id 0; set logs 0xA605B when the grade byte is nonzero and 0x73E5A when it is zero. bit19 on the line is the press grade. Not installed.


## Buff table dump (#36559)

Once KiPressProbe is on and the table pointer is filled, seven lines: KIBUFF id, idx, kind, raw 44 bytes. Ids are 900, 822, 496, 2, 0x15E, 0x1B2, 0x114. The map object is the pointer at T+0x20. Kind is the u16 at record +0x2A. Not installed.
