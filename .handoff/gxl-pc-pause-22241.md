# Grok PC pause, 10:49 am EST, October 21, 2026 (#22241)

The human asked for a write-up of this Grok lead's optimization check, then a pause. The game was not opened for this pause. No switch was changed. No injector was built. The Yoritomo training search stays stopped.

## What was asked

- Look through the picture-safe optimizations already in the notes.
- Could they become one injector for the next Katana game without reversing that game?
- The question means every optimization, not the shader warmup (#21920).
- The question is whether that is likely, because this game's problems are now known (#21928).

## Answer

Unlikely without looking at the next game. The lessons shorten the search. They do not skip it.

## Counts

- 10 hooks sit on a graphics API entry. 0 of those 10 are patches inside a Nioh function. Names: PJ, PJH, PL, VrrPresent, Barriers, R2a, TileBatch, GIMerge, CbvMirror, MaxFrameLatency. Result #21939. List file #1271.
- 54 hooks sit on a Nioh 3 function address. They would have to be found again in the next game. Result #21925.
- 73 picture-safe speedups are in the current build. 20 are on in the live settings file. Result #21931, restated #22098, file #1277.
- Of those 20, 1 is a graphics call (PJ), 17 patch this game, and 2 are on neither list (VRR and J1). Result #21941.
- 13 of 64 have a measured frame-rate or CPU number. Only 1 gains more than 2 percent. Result #21945.

The 20 that are on: MW, W1, W2, VRR, W3, R1, L1, WK, FG, W6, B, J1, GF, WG, LK, CP, MQ, PT1, DL, PJ. RD and M2S are on in the settings file and are not in this build.

## The one measured gain over 2 percent

CBN on pc2 (Ryzen 5 5600X, RX 6650 XT, ReBAR on). OFF: CPU busy 25.84 ms. ON: 22.29 ms. Frames +15.6 percent. The printed pair, CBN plus CBT, is OFF 38.3 fps and ON 44.3 fps. CBT's own add is about 0.3 points.

The same pair on this PC is 94.3 to 94.7 fps (+0.49 percent). On the Ally it is 40.62 to 40.56 fps (-0.16 percent). The big gain is one device. It is not a keep for this PC.

Barriers on the RX 9070 PC went from 5.52 ms to 5.89 ms per frame, about 7 percent fewer frames. The other measured hooks stay under 2 percent or have no number.

## Already told, and left as found

- Shader warmup (PJ) is in the October 19 build. The default is off. The live settings file has it on. A 64 MB journal from 6:56 pm EST on October 11 is on disk. The hitch to beat is 167 ms. All 6 slow gameplay frames in that old session were shader compiles. The on-versus-off pair was never run. The notes are for this PC only.
- The 30 Hz logic cut (TG) is in the build, and this PC's settings already have it on. A tap has to be held about 33 ms. The measured frame-rate gain was about 3 percent, not 26. This check did not turn it on.
- An older page-pool note says about 3 percent on this PC. That sweep was left out of the 73, so it is not the same count as the 13 measured hooks.
- Yoritomo training search is stopped. 173 moves, 88 attacks, 6 sequences. The practice fight was not launched.

## Not done

- No injector file was built.
- No new in-game test was run for this check.
- The 10 graphics hooks might copy only if the next game makes the same graphics call. There is no next game here to try that on.
- An Ally on-versus-off test of R2a and R1 was named and not run.

## Pause

10:49 am EST, October 21, 2026. Workers were told to stop (#22247). This Grok lead will not pick up new optimization work until the human says resume. The shared pause file was not created, so the other fleets were not stopped. The game was not closed.
