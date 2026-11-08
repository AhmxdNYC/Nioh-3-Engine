# Collider identity and open clusters (#82179)

Base 35fb8235: eight-second cold admission, combined first-press fixes, independent post-ghost learner.

Supplied pc census: 60B86 raw one at bind, second object at +.084 (repeat +.100), same objects across .533 action change, new objects around 1.276..1.342, lingering object until 3.126. Previous exclusion was pruned when the object temporarily stopped qualifying by owner/body fields, and its 100-ms window missed later appearance objects. Aggregate nonzero state also merged attacks that overlapped in lifetime.

Appearance registration now covers owned objects through bind+200 ms BEFORE body filtering. Exclusion remains keyed by object address while that address exists anywhere in the pool, even when owner/attack fields change. Only an observed pool absence retires it. No action change or timer promotes it to an attack.

Eligible new attack objects form a cluster: a gap of at least 100 ms since the preceding new eligible opening starts another burst. Opens closer together join the preceding cluster. This does not require all older attack colliders to close. Active and post-ghost trackers share the same rule, and the ghost observer copies the current identities and last-new time, so it cannot count the exit-triggering cluster again.

Learned sections advance to SealHitsA3 / SealHits3; A2 and earlier counts are untouched and ignored. Every core has one learning use. Existing exit offset, ghost appearance, reveal, tail speed and deadline behavior are unchanged.

Validation: 139 focused checks, including reported 60B86 count sequence with second appearance at .084/.100/.117 and metadata changes while present: learns one cluster, repeat exit at1.276, next object1.310 does not add another. Doji two clusters still exit at1.517/1.668; overlapping old attack objects no longer block a new cluster. Tail clustering, versioned read, max-only and full earlier suites pass. Plus52 first-use +112 picker +23 smoke +29 equipment +357 rematch =712 unique checks. sc22 available replay passes; Release built. Replays use supplied times and synthetic object addresses, not a live new-build measurement. Next pc first/repeat 60B86 and Doji; no game touched.
