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

## Live far-wall check (pc2/1, #102852, 2026-11-14 8:00 pm EST)

PC2, public 1.8.17 (EF d7cb9d69, BR 376532e9, TR 1cfd0921, CamKi e6142afc; Nioh3Fix aed178ea also present), Boss Match
`rm fight ID`, player xyz polled ~1/s (vpad state) from the fight stand while walking 8 directions x 12 s (plus 4 x 25 s for Kasha).
Radius = max horizontal distance from the stand; extents are min..max offsets from the stand in metres (x / z); walls were
confirmed by position plateaus (player stops moving while the stick is held) and an end screenshot against the arena edge.

| id | boss | box estimate m | measured max from stand m | x extent | z extent | within 35 m |
|---|---|---|---|---|---|---|
| R23E5 | Hino-enma | 39.5 | **28.1** | -12.8..23.1 | -27.7..7.4 | yes |
| R0034 | Enenra & Jakotsu-baba | 34.7 | **25.5** | -12.9..16.4 | -24.9..8.4 | yes |
| R7B35 | Minamoto no Yoritomo | 33.2 | **27.0** | -4.6..18.4 | -23.8..9.2 | yes |
| RCC53 | Kasha (outlier) | 58.7 | **33.8** | -5.0..33.1 | -11.6..26.1 | yes (wall at x=+33 hit 3 times) |

Verdict: PASS. The 35 m stop covers the far wall in all 3 normal arenas (max 28.1 m) and Kasha too (33.8 m); Kasha does
not need the slider max. The trigger-box radius over-estimates the walkable arena (stand sits off-centre, walls are closer).
Raw: pc2 C:/nioh3-re/ar102852 (pos_*.txt, *_end.png, rad.sh/radius.py/ext.py).
