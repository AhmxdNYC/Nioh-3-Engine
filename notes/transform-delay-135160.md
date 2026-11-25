# Transformation delay: hold fix / character clock check (TASK #135160, human #135156, 2026-11-25 1:20 pm EST)

Offline, Opus subagent, game pid 54928 untouched. Live logs 11-25 12:09-12:12 (15 presses), BR 90dcf051, TR e7792313, EF 0c05109e.

| Suspect | Verdict | Proof |
|---|---|---|
| Hold fix | NO | "chr38 0 hold40 0" = two zero fields of the soul-core record (checkpoints 33-48), not a fix; GH #154 = keyboard hold input; field_hold.h = warp hold; EF `HOLDKEYGAP: off` (EF log 12:08:05.025) |
| Character clock (GH #159) | NO | Training log: seal throw 7 frames in 113-125 ms, next 14 frames in 234-240 ms (1.0x); PlayerLog pad 12:12:06.452 -> throw .473, 96-98 fps; no EF HITCH 12:09-12:12; OSD speed 0.98-1.02x. 15109 `rate=0.400` is the game's own opening speed, BR speeds it x1.40 |
| BR SealShowMs 400 / fade 250 / FormHideDelayMs 400 | PARTLY | only matters when the yokai spawns before +650 ms (15109 spawns +203 ms, visible ~+650 ms) |
| Game's own yokai spawn | the universal part | `FormKey seal spawn hit` after press: 80D5F +482..1168 ms (9 presses), 5216E +907/1071/1391, 5129B +505. Idle -> spawn varies 49-917 ms, so not a fixed mod timer. Cause (cold load / spawn path) not proven |

Last press: 12:12:06.449 press, .881 form start, .922 player idle, 07.179 yokai spawn (+0.70 s), bind same frame; 0 ms of SealShow hold left.

Fix candidates: [Spirit] SealShowMs=150 (yokai fade-in cross-fades with the 250 ms player fade-out inside the smoke; 15109 visible ~+400 ms instead of +650). Universal part needs a code look at why the spawn comes 50-900 ms after the throw ends.
Camera bounce (plausible, unproven): form start warps the player to the yokai (`FormFollow at ... warp=1`) 69-76 cm.
