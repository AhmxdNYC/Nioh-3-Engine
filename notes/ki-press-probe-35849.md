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
