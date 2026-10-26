# Perfect Ki Pulse (task 35702) — what is known

Human #35698: a distinct particle plays only on a perfect Ki Pulse (maximum Ki returned).

## Not in the program

The dumped exe has no effect name `kiPulse`, `ki_just`, `KiJust`, `perfect`, or a pulse effect. The only `pulse` string is a material parameter (`alpha_offset` / `pulse`). `Player::RecoverStamina` and `Player_RecoverStamina` are script names. They are not a native effect spawn.

## Full window (about 80%)

`KiTick` is RVA `0x2F8068`. `K` is `[actor+0x3A0]+0x98`. The caller is `0x2F744c`.

- `K+0x74` counts down while it is above 0. That is the blue bar (about 30 down to 0).
- While it counts, `K+0x58` fills toward the cap `K+0x5C` by `dt * K+0x6C` (`0x2F8800`).
- When the countdown crosses 0, `K+0x58` is set to `K+0x5C`, `K+0x6C` is cleared, and `K+0x80` is 1 for that tick. `K+0x80` means the window just ended. It is not a perfect-press flag.
- `K+0x7C` is the later Ki-break timer.

## Meter

The Training check mark looks for Ki going up inside that window. It does not separate a perfect press from any other return. No log of a perfect press is in the captures used here. No new ASI.

## End of the window (#35819)

Human #35819: look at the back end of the bar. Inside `KiTick` the only branch on `K+0x74` is "still above 0" versus "just hit 0". There is no second compare for the last few frames, and no flag that turns on before zero.

The stored amount `K+0x58` grows for the whole countdown and is capped at `K+0x5C`, so it is fullest in the last moment the bar is still up. `K+0x80` turns on only when the bar hits 0. That is the window ending.

No two timed R1 presses were measured for this note.

## Still open

The button handler that spends `K+0x58` and calls `Effect::Request` (string at RVA `0x3BF5470`). That is where a perfect grade and a second effect id would be.
