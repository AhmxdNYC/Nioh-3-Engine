# Ki Pulse success signal (offline, 2026-10-25, branch pc/ki-pulse-success)

Offline only: logs in `captures/ki2281/training-sweep.log` (bridge #2301), `captures/camki2/probe-1332.log`,
`captures/camki4/run.out`, decompile `D:\nioh3-re\decomp` (KiTick 0x2f8068, XInputFindActivePad 0x435b8c), game text.

## 1. 0x6249 / 25161 is not a landed pulse
`probe-1332.log` t+290906: act 0x6248 -> 0x6249, motion 4100 -> 4110, **Ki 1019.5 -> 921.1** (98 spent), +5C 53.94 -> 133.43,
K+0x74 still -1. Ki goes down: this is the second hit of the light combo (next motion of the chain), which also refills the
pool +5C. A pulse returns Ki. The Training check (e8e0c63a..66f02c44, `FmKiPulseLanded`) was built from this line, so it
only lit on a second X press. The vpad tests sent X then RB, so they could never match it.

## 2. What the fields do (sweep #2301 + KiTick decompile)
- Swing 0x6248: Ki 1153 -> 1019.5 (cost 133.5) over ~0.2 s; pool K+0x5C = 53.9 (about 40 % of the cost).
- Window K+0x74 opens about 0.41 s after the action starts (30, counts down by dt), lasts about 0.38-0.43 s. This is
  the "blue lights gather" phase (game text: "press STANCE_CHANGE as blue lights gather ... just as the lights coalesce").
- Miss: the pool drains into Ki at most ~3.5 Ki per ~10 ms sample near the end of the window, K+0x74 runs down to 0
  (last open sample ~2), then the action goes 0x6248 -> 0x6224 (stance idle). Ki never jumps.
- KiTick (v2.02 export): while K+0x74 > 0 it counts down; when it ends, K+0x58 = K+0x5C, +0x74/+0x6C = 0, +0x80 = 1.

## 3. New check (Training, `FmKiPulseStep2`)
Landed = previous window K+0x74 above 3 (time left) and either Ki jumps by >= 15 and >= 60 % of the pool in one sample,
or the window ends early (K+0x74 <= 0) with Ki rising. Ki going down is never a land. Sample lines now also log
`window` (K+0x74). Unit test replays the #2301 miss (no land) and the 0x6249 combo hit (no land).
Unproven: no real pulse has been logged yet, so the exact Ki jump of a pulse is inferred, not measured.

## 4. Input path
The vpad answers the game's own `XInputGetState` IAT slot (exclusive mode hides real XInput pads). `run.out` shows
`press 0200 80 ms (20 pad reads rounds)`: the game read RB. XInputFindActivePad 0x435b8c lists 0x200 among its
activity buttons, so RB is a normal pad bit on the same XInput path as a physical Xbox RB. A DualSense used natively goes
through the game's HID reader instead (different device path, same logical STANCE_CHANGE). No mod masks RB (EngineFix
masks only the menu button, B for the cover and 0xF000 near the return gate).
In all 15 sweep tries the player action never left 0x6248 except to 0x6224 at the natural end (and 0x1874/6260 three
times, Ki unchanged and pool cleared; probably a hit reaction: one came 22 ms after the boss started 0x4E2C). A pulse at any time in the window plays its own motion, so RB did
not trigger STANCE_CHANGE at all - this is not a timing miss.
Main timing error in the sweep: every command was a separate `py vpad.py` process (~0.1-0.3 s startup), so the sweep
times in `sweep.txt` are about 0.45-0.5 s earlier than the swing in the log.

## 5. Next vpad test (one pipe session, server-side waits)
Check first: Samurai style (not Ninja: there RB after an attack is Mist), in-game Controls has Ki Pulse/Stance on RB.
```
py -I tools/testmatrix/vpad.py run -   (one session; repeat per N = 300 350 400 450 500 550 600 650)
press X 100
wait N
press RB 150
wait 2500
```
RB edge = X edge + 100 + N ms. Window ~410-830 ms after the swing starts, so N 300-650 covers it in 50 ms steps.
Then try `hold RB` / `wait 300` / `release RB` at N = 400 (hold vs edge). Read `KI PULSE sample` lines: a pulse = an action
other than 25160/25124 while `window` > 3, and `KI PULSE land`. If no action ever changes, RB is not reaching
STANCE_CHANGE: compare with one human R1 press (VirtualPad 0) and the in-game key config.
