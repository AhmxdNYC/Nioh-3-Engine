# MW integrated with the current rematch engine

2026-10-13. Base: `34296f3c4c2f86505387dab054b9aa15a7525ebc`, the installed
1.7.73 engine and current hosted rematch/menu source. Isolated worktree:
`D:/nioh3-wt/mw-hosted-20261013`, branch `codex-mw-hosted-20261013`.

## Why this build exists

The earlier MW candidate was built from a 1.7.61 source branch. It must not
replace the user's 1.7.73 engine because that would discard newer menu and
rematch integration. This port applies only the MW engine changes from
`364c5e2`, installer diagnostics from `94b92ba`, and native thread enumeration
from `b6f793f` onto the current baseline.

The port preserves `MOD_VERSION=1.7.73`, `mh.tuning=&LiveTuning`, and
`ai.coverPermission=&menu::CoverTipAllowed`. Menu, OSD, AutoContinue, and all
BossRematch addon source remain byte-identical to the baseline. The installed
addon is not part of this build or replaced by it.

## Behavior and status

MW remains startup-only and explicitly opt-in (`[Patches] MW=1`). It replaces
the main thread's meshlet completion Sleep(0) round with a one-byte address
wait, and wakes that address after the original request job clears its busy
byte. It does not skip job work or change asset feedback age.

Both sites are verified and published together. The initial retained thread
handles and final frozen census now use native enumeration consistently;
unknown threads or incomplete inspection still reject installation. Installer
diagnostics are logged after threads resume. Requested wait timeout is 1 ms,
not a guaranteed wall-time scheduling bound.

Main-PC earlier installation failed before publication: 199 native census
checks saw a thread outside the initial Toolhelp set. A later read-only sample
had equal 242-thread sets, so a permanently hidden thread was not established.
The corrected enumeration keeps exact returned handles instead of reopening
thread IDs. The separate PC2 older candidate reportedly installed; that report
does not prove this combined artifact's gameplay behavior or performance.

## Artifact and verification

Built artifact: `mod/Nioh3EngineFix/bin/Nioh3EngineFix.asi`, 1,826,816 bytes.
SHA256: `4cec2ae50f0f6d50d81e480c615cf46ea0d3bc6e0fccbfbab64ba94901104207`.

- Release build passed; existing local-name shadow warning retained.
- Exact MW native site/ABI binding test passed against the retained game dump.
- Native wait/publication harness passed, including 50 concurrent wake/clear
  cycles, enrollment races, timeout rechecks and publication fault cases.
- Both real settings-menu and Boss UI harnesses passed.
- Independent source review confirmed preservation of the newer baseline
  integration and unchanged menu/addon sources.

No game launch or installation was performed by this port task. The coordinator
owns deployment and capture. Actual MW ON/activity and CPU/frame-time results
remain required before claiming a gameplay or optimization success.

## Follow-up: positively terminated thread handles

The first combined startup reported `suspend=200`, error5, TID2264, with zero
publication writes. Later read-only OpenThread returned87 and native
enumeration no longer included2264; this does not prove its exact state at the
failed suspension. The revised installer checks the retained thread handle
with `WaitForSingleObject(handle,0)` using SYNCHRONIZE rights. Only
`WAIT_OBJECT_0` proves a terminated thread can no longer execute and excludes
it. This check covers initial enumeration, pre-suspension, the exit race after
a failed suspension, and the final census. Alive or unqueryable threads still
block publication. Confirmed terminated handles are neither context-queried
nor resumed. Their observations are logged after the freeze ends.

The exact production suspension/context block passed a standalone harness
with a live worker and a retained terminated worker deliberately returning
exit code259. The signaled handle, not the ambiguous exit code, caused the
exclusion. The live worker was suspended, context-queried and resumed.
Harness: `D:/nioh3-re/reviews/mw-signaled-thread-harness.cpp`.

Rebuilt ASI SHA256:
`41553b09a131903f959afee5bd6ad7b5e1b92a217072c507b29a4fe36948984e`.
Build passed; current menus and addon remain unchanged. Actual next-startup
activation and gameplay validation remain coordinator-owned and pending.
