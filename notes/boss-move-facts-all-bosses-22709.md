# Hit counts + Unblockable for every boss (user #22709, 2026-10-21)

Offline. The game was not touched. The algorithm is the Yoritomo one (`tools/yoritomo_move_facts.py`, works on any 16-sub-table action file),
now fed with every boss whose action file could be bound.

## Binding a boss to its action file (not guessed)
character (`notes/boss-char-assets.csv`) -> setting DB -> `CharacterActionObjectNameHashArray` (`n3rdb.py get` + `n3kod_full.py`, the old
`n3kod.py` cuts arrays at 48) -> the archive_01 file whose action rows (+0xF0) hold those hashes. Done for all 247 characters with a setting DB:
**94 characters have an action file** (`notes/boss-action-files-22709.csv`: character, setting DB, hashes in setting, hashes found, rows, file).
Independent check for the 9 catalogue bosses: every action id the Training logs recorded is inside the bound file
(Shingen 22/22, Hino-enma 29/29, Masakage 26/26, Ippon-Datara 1/1, Otoroshi 13/13, Nuribotoke 12/12, Red Kappa 20/21 (only shared idle 0x2), Takasugi 19/19, Kamaitachi 31/31).

## What the menu now does
`tools/gen_boss_move_facts.py` BOSSES: Yoritomo + 9 more (Takeda Shingen, Hino-enma, Yamagata Masakage, Ippon-Datara, Otoroshi, Nuribotoke,
Red Kappa, Takasugi Shinsaku, Kamaitachi). `tools/add_static_boss_rows.py` adds each boss's whole game table to the move catalogue (+985 rows;
attack rows visible and grouped, everything else kind M = hidden unless Show twitches). Result: groups 1 hit / 2 hits ... / Unblockable / Other
attacks (hits unknown), same as Yoritomo. Yoritomo's output is byte-identical to before.
Facts per boss (rows / with hits / unblockable): Shingen 67/30/13, Hino-enma 44/28/11, Masakage 31/23/8, Ippon-Datara 35/34/7, Otoroshi 75/17/1,
Nuribotoke 26/12/1, Red Kappa 9/9/0, Takasugi 66/40/14, Kamaitachi 74/27/12.

## Rules / caveats
- The attack block (+0x13C) is rarely set on other bosses (Hino-enma: most real attacks have it 0), so an attack = attack block OR a timed hit window
  OR the GreatAttack flag OR owned hitbox rows.
- A spawn at frame 0 in >= 80% of a boss's actions (Nuribotoke `44@0`) is a standing object, not a projectile: it no longer makes hits "unknown".
  Assumption: that object's own hits are not counted.
- Spawned projectiles / untimed rows still give "hits unknown" (not guessed), as for Yoritomo.
- Hino-enma static hit window of 0x4E67 starts at frame 60 but the live meter reported startup 50: the offset between window frame and meter
  frame is unexplained (Yoritomo matched: 94). Hit COUNTS match (3 hits).
- Not bound: Ibaraki Doji (setting DB lists 0 action hashes; keeps old groups). The ~85 other characters with a bound file (Yoshitsune,
  Yoshinobu, Himiko, Munenori, Hanzo, Ryomen Sukuna, Shigurui, Enki, Oni, Yasha ...) have data in the CSV but no Training catalogue (no chr id /
  recorded name yet): when a boss is spawned there, add its chr id + file to BOSSES and run the two tools.
- Unblockable = action row +0x04 bit 0x2000 (code chain proven for Yoritomo, `notes/yoritomo-hit-source-22305.md`); same struct family, not
  separately proven in the game for the other bosses.
