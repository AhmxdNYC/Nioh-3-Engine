# Private spawn recovery candidate 1.4.8

Three production-function failure paths reproduced on parent ab4a5eb:
failed transform write, failed flags write and unreadable record discarded
recovery metadata. This change retains a pending record, restores each field
only when it still matches the original or our value, and verifies both
originals before clearing ownership. Apply publishes recovery before writing
and checks the written bytes. Replay menu, direct mission and scroll entry
all attempt restoration before any native request and refuse while unresolved.

Observed different lookup address/id or foreign field data latches a conflict.
It never writes that foreign data. Missing/unreadable lookup retains pending
ownership; a later explicit entry may retry. ToIdle can reset UI/session state,
but does not erase spawn recovery. Automatic reapply is suspended after an
ambiguous reload/rewrite. This deliberately narrows behavior: death/retry arena
spawn is not promised until native lifetime/epoch proof exists. There is no
unsafe UI override for the conflict latch; no runtime deployment in this work.
Unobserved same-address/same-id reuse remains an ABA limitation, not solved by
field comparisons. Native thread/session validity and live acceptance remain
separate gates. This is not a playable-training completion claim.

Validation: test_spawn_restore.py compiles actual SpawnApply/SpawnRestore with
injected reads/writes under MSVC /W4 /WX: 23 behavior checks PASS plus three
source admission-entry checks. Existing rematch 60, lifecycle 143/startup and
94 sites including shifted-image simulation PASS. Final release 1.4.8 builds
without emitted warnings; ASI SHA256
5729de354c7776e32f4350bd59db0f56e1cda99e4db951b661e6134049043bf5.
Installed game files, defaults and saves unchanged. Candidate remains private.
