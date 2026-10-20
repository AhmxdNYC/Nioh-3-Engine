# Yoritomo search, paused 7:18 pm EST (#21589)

Boss: Minamoto no Yoritomo, character 16102. The menu was not changed. The search is paused until the user says resume.

## What was asked

One rule that can sort any boss, including bosses added later. Real hit counts and real attack strings, from the game's own data. Not a hand-made list. Frame marks were also requested for a future Street Fighter style bar. Unknown stays unknown.

## What the move list contains

173 moves. 88 are marked attacks. Those 88 use 20 animation clips. Every attack points at 1 clip. 0 attacks have two clips. An extra number on 40 of the 88 is not a second clip, so it is not a sequence. Sequence count from that rule: 0.

A 7-step list is in the file the game loads. It is 6 choices. Each choice is 1 out of 11 or 12. If it is not taken, the move ends at 4E7A, then idle. It is not a forced string.

## What the hitbox rows are

The hit shape table has 182 rows, 192 bytes each. Move 4E68 owns rows 92, 93, and 94 (3 rows). 4E6C owns 5. 4E74 owns 5. 4EA5 owns 0. The code loads one row as its number times 192. One check uses 1 row, not all of them. A caller loop runs 0, 1, or 2 times, not 3. The 3 is a bounds check, not a hit count.

The byte that looked like a saved slot is 0 on all 88 attacks. The slot lives on a side record. This move has 0 file rows for that record. The table has 51 rows and the code asks for row 72, so the pointer stays empty.

## What the damage code says

One damage call applies one amount. Nothing in that call is a hit count. On these rows the amounts are 30, 25, and 40. The scale in this dump is 1. The switch that would change the scale is off. None of that is 221.

A distance check used by his attack choice is where he is standing. It is not a hit count.

## What the live log says

Around 6:32 pm EST the bar was full at 6:32:49, value 2658. At 6:32:50 the refill starts from 2437. The line that mentions 221 is health put back, 2437 to 2658, tagged as the training refill. There are 0 hit lines in that second. The drop itself is not in the log. The damage code still says 30, 25, or 40.

## Frame marks

A frame mark is on 64 of 88 attacks, usually near frame 14. Another mark on the three checked moves sits at frame 51, 83, and 77. These are times inside the clip. They are not proven startup, active, or recovery.

## Where the notes are

Longer code notes: `C:\Users\Ahmad Hamza\nioh3-sort-20999\notes\yoritomo-attack-values.md` on branch `gxl-pc/move-sort-20999`.

Sorter already built, not wired into the menu: 14 bosses, 1784 moves, 280 attacks by the file flag. Four bosses have that flag on 0 rows. Motion-number guesses were not used.
