# Boss arena sizes for the draw-distance "arena size" stop (pc #102806, human #102651)

Offline, no game. Script: `tools/arena/arena_size.py <bosses.tsv> <out.tsv> [arenas.tsv]`. Inputs: bosses TSV 3f42f027
(origin/main, CRLF), PC2 Nioh3BossRematch_arenas.tsv (in-game BossBattleMode-start captures, used as the stand point when
the catalogue has no arena_x). Per-row table: `notes/arena-size-102651.tsv`.

Per catalogue row, XZ points in the plane: player stand (arena_x/z, else the arenas.tsv capture), boss_pos, and the 4 corners of
arena_boxgeo (centre, yaw, full size). radius = minimum enclosing circle (m). span = largest point-to-point distance (m), i.e.
what a player at one edge must still draw. "Confirmed" = the catalogue `confirmed` date column (63 rows).

| set | n | radius median | p75 | p90 | max | span median | p90 | max |
|---|---|---|---|---|---|---|---|---|
| confirmed, stand+boss+box | 14 | 19.3 | 25.7 | **34.7** | 39.5 | 38.2 | 69.4 | 79.0 |
| confirmed, any data | 41 | 10.0 | 19.3 | 25.7 | 58.7 | 19.2 | 51.4 | 117.5 |
| all rows with data | 48 | 10.0 | 19.3 | 31.2 | 58.7 | 19.6 | 62.4 | 117.5 |

**Recommended default arena stop: radius 35 m around the arena (span about 70 m).** Universal rule, no per-boss numbers (#87625):
the slider stop should keep everything within ~70 m of the camera, which covers 90 % of measured confirmed arenas edge to edge.

Limits:
- arena_boxgeo is the script trigger box (the "BOSSAREABOX" the player walks into), not the arena walls, so these are lower
  bounds on the real arena; a live check of the far wall in the largest arenas (Hino-enma R23E5, Enenra & Jakotsu-baba R0034,
  Yoritomo R7B35) is the proof.
- Kasha RCC53 (117 m stand->boss) is an outlier: the capture and boss_pos are probably not the same fight point.
- Missing data: 105 rows lack a piece (mostly the trigger box): 49 confirmed rows (27 lack only the box, 17 lack the stand + box,
  5 lack all three: the dojo Novice rows RF5E5, RD759, R26F3, RA0A1); all 20 Picture Scrolls lack a stand point; 30 X rows lack everything.
  Full list: run the script (stdout).
