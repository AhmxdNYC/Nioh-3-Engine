# Early spawn hits and press-height restore (#82344)

Base f0a134ab (primary actor + all-actor ghost) includes b659838f multi-source c3e0207f. This is the combined line, not a fork omitting the Blood Edge follow-up.

Evidence: pc2 #82341 / #6400: A2ACB .274s early collider, real hit 1.665s; learned 2 caused repeat to miss last-hit exit. Restore used min(current,start), preserving 21784 despite saved21792 and contaminating the next start.

Change: exclude appearance objects observed through 400ms after the first noted spawn of the current press, retaining 200ms bind safety and lifetime identity exclusion. Later helper spawns do not restart this grace period. A4 learned-count sections discard stale A3 counts. No core-specific timing table.

Press snapshot now saves player/Y independently of actor-table readability. Form follow start uses that matching recent snapshot; restore sets Y exactly to saved height (both upward/downward correction), retaining current X/Z and existing warp/write fallback. During follow the ceiling policy remains; non-follow forms remain untouched.

871 offline checks PASS: last-hit 238; ground restore29; first-use52; picker112; smoke23; primary/all-actor ghost24; equipment36; rematch357. Replays include .274 false hit +1.665 real, next-use count1, delayed spawn, existing Doji/60B86/helper timing, 21784->21792 restoration, next-use height, pre-bind snapshot and stale/wrong-player snapshot rejection. Release build checked separately before delivery.

Limit: saved player Y is not a terrain query; horizontal movement onto a slope/new elevation still needs live checking. Fixed400ms appearance grace can exclude a legitimate unusually early attack; other-core live acceptance remains required. No game operations. Next pc2 first/repeat DOWN plus Doji/60B86/Blood Edge and Y logs.
