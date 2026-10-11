# FO98 end-frame publication, drain and reset boundary - 2026-10-11

Bounded native/offline trace, main parent 57328f3; experimental 24494d5 unchanged.
FO remains NO-GO. No runtime, lifetime, safety or performance verdict.

Native 418C70 passes context+3C28 to 419028. That predicate compares
+8 against +18, then its cold tail 19A6724 compares +8 against +20;
it returns false only when both comparisons match. Relative to the
context, those are +3C30, +3C40 and +3C48. A nonempty range causes
418C70 to copy [context+3C48, context+3C40] to a two-pointer descriptor
and call 25426C with context+3BE0, flag 1 and statistics context+3C78.
25426C publishes a 32-byte ring record and increments ring+30 before
releasing its semaphore. Ring consumer field semantics remain open.

418C70 calls 418F40 unconditionally after optional publication and
before cursor reset. Native 418F40 repeatedly calls 418F90; the latter
returns busy when ring+24 differs from ring+28 or unsigned ring+30
is greater than zero. The loop's QPC and timer calls measure CPU wait;
this predicate has no fence call or resource-lock acquisition. At a
single predicate evaluation, idle therefore requires equal counters
and ring+30 == 0. This does not establish producer closure or guarantee
those values remain idle after the evaluation.

After that wait, 418C70 resets context+3C40, +3C48 and +3C68 to the base
at +3C30, retains max(old +3C50, old +3C40) at +3C50, increments +3C60
and clears +3C74. 418B94 then copies current statistics to the previous
statistics area and clears current statistics (existing decomp only).
The native final-kick body contains no direct resource mode helper call.
Whether a producer can append during/after this boundary is unresolved;
CPU replay idleness must not be promoted to GPU completion or a safe
FO flush point. Global busy-byte use is observed, not classified as a
race without surrounding thread ownership.

## Verification and checkpoint

`py -I re/scripts_renderer/fo98_endframe_boundary_provenance.py`:
five new static provenance windows PASS, including the cold predicate
tail. These hashes pin reviewed bytes; they are not behavioral safety
checks. Existing decomp covered 418C70/418F40/25426C; missing leaf
exports 419028/418F90 were resolved using Capstone on bounded windows.
A broad indirect-vtable decomp scan was cancelled without producing a
caller result; actual vtable+28 callers/modes remain unresolved. Prior
passed suites were not repeated.

No local game/test at entry/checkpoint; no lock needed, Ghidra, build,
deployment or installed ASI/INI/config/save/settings/hardware/default
visual changes. Prior restoration retained. Controller untouched;
no Claude call/wake. Phone inbox empty. Remote1767/1768/1781 pending
at last known bridge1785; no fresh bridge query. Unrelated captures
untouched. Serial ownership until exit; commit/push with Codex
attribution and Discord result through the existing notifier.

Next ready unit: trace dispatcher consumer writes to ring+28/+30 and
callback completion, then identify bounded resource vtable+28 caller
sites/modes. Producer closure, cache+20 identity, GPU completion,
retirement ring restart and combined queue/bounds/lifetime repair remain open.
