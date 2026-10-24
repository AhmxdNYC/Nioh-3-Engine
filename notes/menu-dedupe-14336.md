# Menu audit #14336 (read from boss-hpki merge, rematch.cpp Items() ~2556-2725)
Rows that duplicate or make no sense; fix in the single cleanup build (slot-1 / pc/boss-hpki):
1. Training Bosses page (uTools): "Boss moves" + "Boss A moves" + "Boss B moves" = same thing -> keep only per-boss rows, shown only for spawned bosses (A, B, C... unlimited).
2. Boss moves page (uMoves) repeats Stand-still, Gap between repeats, Aggro, Reset position, Repeat selected/Repeating move that already live on the Boss page / tools page. Keep each setting in ONE place: per-boss page (stance, aggro, reset, gap, repeat, HP/Ki, moves groups).
3. Gap between repeats: in uTools, uMoves and uGate -> only inside the Boss X page.
4. "Repeat selected move" + "Repeating move" (+ same row repeated inside every move group): collapse to one "Repeat this move" ON/OFF per move row (#13623); drop RPT wording and the Right-key trick text in footers.
5. Boss Match rules shown in 4 places (uTools, uSummon, uGate, hub "Boss Matches"): keep Boss Matches page only (+ Gate in a Boss Match).
6. "Remove all summoned bosses" shows when no boss is summoned (uTools, uGate), label hardcodes A/B -> show only when >=1 spawned, list the actual bosses.
7. Summon page "Summon into: Boss A/B" slot picker: replace with "Summon another boss" (auto next letter A,B,C...); "Wait before attacking" slider contradicts new idle-by-default (#14304) -> replace by the CPU/idle timer slider (#14313).
8. Hub: "Train here" and "Train in a boss arena" make no sense while already in an arena in Training (#14333) -> hide both (Leave arena stays). Settings page also has a second "Training mods" door -> remove (hub is the only door); rename Training mods -> Training Bosses (#14301).
9. "Leave arena" appears on root, hub and Boss Matches -> hub/gate only.
10. Boss Matches page legacy rows: "Practice Yamagata", "Start at boss shrine", "Return from practice" vs "Return to where I was"; "Open Battle Scroll replay menu", "Use Picture Scroll" are dev rows -> move to Settings > Advanced or hide unless enabled.
11. Gate page (Return object): Back AND Close AND pinned Back -> one Back + Close only; Gate in Training repeats the whole Training Bosses list -> keep short: Fight again, Player, Training Bosses, Return.
12. Player page has only one row (infinite health): fine for now, plus "Infinite health" row duplicated as Player(...) label on hub: ok, one entry only.
13. Footers: strings still say "two bosses at once: A and B", "RPT" -> update for N bosses.
