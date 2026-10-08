# PC matrix, ASI 1.7.16, 2026-10-08 (stand fps mean of 2x20 s windows; launch items vs nearest BASE launch, live items vs BASE-live in one launch)

| id | ref | fps | ref fps | d fps % | p99 ms | ref p99 | d p99 % |
|---|---|---|---|---|---|---|---|
| WL | BASE | 152.0 | 144.7 | +5.0 | 14.36 | 14.69 | -2.2 |
| AS0 | BASE#2 | 145.4 | 144.6 | +0.6 | 14.71 | 14.71 | +0.0 |
| DLCP | BASE#2 | 148.0 | 144.6 | +2.4 | 14.55 | 14.71 | -1.1 |
| GL1 | BASE#3 | 147.2 | 152.5 | -3.5 | 14.55 | 14.31 | +1.6 |
| LC | BASE#3 | 151.8 | 152.5 | -0.5 | 14.34 | 14.31 | +0.2 |
| LW | BASE#3 | 152.0 | 152.5 | -0.3 | 14.33 | 14.31 | +0.1 |
| W6 | BASE-live | 180.8 | 179.8 | +0.6 | 13.34 | 13.44 | -0.7 |
| B | BASE-live | 207.8 | 179.8 | +15.6 | 12.45 | 13.44 | -7.4 |
| PP | BASE-live | 189.3 | 179.8 | +5.3 | 13.06 | 13.44 | -2.8 |
| GF | BASE-live | 187.2 | 179.8 | +4.1 | 11.86 | 13.44 | -11.7 |
| Barriers | BASE-live | 167.0 | 179.8 | -7.1 | 13.76 | 13.44 | +2.4 |
| MQ | BASE-live | 181.0 | 179.8 | +0.7 | 11.90 | 13.44 | -11.5 |
| PT1 | BASE-live | 180.3 | 179.8 | +0.3 | 13.50 | 13.44 | +0.4 |
| DL | BASE-live | 180.5 | 179.8 | +0.4 | 13.53 | 13.44 | +0.7 |

BASE launches (stand fps): BASE 144.7, BASE#2 144.5, BASE#3 152.5 => launch-to-launch spread ~5 %, so launch-item deltas under +-5 % are noise. BASE-live (in-launch) 179.8.
LoadStats (58A window, s): BASE 25.3 / 26.2, GL1 25.8, LC 25.4, LW 26.4 / 25.7 => no load gain from any (LC raises in-flight 9.6 -> 15, loader busy up, wall time unchanged).

