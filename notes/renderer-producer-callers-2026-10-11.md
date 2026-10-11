# Renderer resource update callers

Private native analysis against the retained flat dump, parent 971946c.
No game launch, installed change, new FPS result or FO safety acceptance.

Four reviewed indirect vtable+28 calls pass mode 1: 129841 in
TextureStreamRequestTiles (1290B8), 490817 in 4906E8, 2F087CD in
2F08094, and 2F11D19 in 2F11C9C. Argument windows contain no intervening
call/branch. For the previously identified 38EB460 resource vtable,
mode 1 means shared resource+198 locking. Receiver type still needs
individual provenance; an indirect slot alone does not identify a class.

4906E8 acquires context+6EA8 exclusively at 490743 and releases it at
490856, enclosing 490817. The receiver comes from tile-record+38;
the tile argument is record+18. Its direct caller 3D1E4C separately
holds an atomic lock at its own object+A0. This establishes a bounded
outer-lock path, not global exclusion of stream producers.

2F11C9C is reached through 2F11514 <- 2F0ED78 <- KtglCacheShutdown
(1896AC8), based on retained decomp caller records and reviewed native
calls. It therefore adds a shutdown path to the closure investigation.
The leaf and the two intermediate wrappers contain no direct SRW acquire;
upstream ownership remains unresolved. Do not infer concurrent execution
or the absence of all synchronization from this observation.

The previously reviewed end-frame routine 418C70 publishes, waits for CPU
replay and resets stream cursors. These caller findings do not prove it
excludes every producer, nor that GPU use of resources has completed.
FO remains NO-GO/default OFF. Next proof needed: concrete receiver identity
and upstream thread/lock ownership for request, update and shutdown paths,
then their relationship to end-frame publication/reset and retirement.

Validation: `py tools/bench/audit_renderer_cache_locks.py` PASS for four
straight-line argument windows plus resolved acquire/release imports and
context-lock offset. `find_fo98_resource_calls.py` produced 7,502 candidates
from 5,864 candidate functions; this is a search index, not caller closure.
It excludes non-unwind functions, functions over 64 KiB and some indirect
encodings. Full candidate output remains private under scratch.
