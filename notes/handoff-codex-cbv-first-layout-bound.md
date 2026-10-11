# CBV first active layout: native admission bound

2026-10-11, OpenAI Codex. Offline inert-image audit; no mod/default/build,
game, Ghidra, deployment, or performance change.

**Result:** the consumer/binder code does not enforce a positive-size first
CBV. The remaining obligation belongs to the layout producer. This is a
conditional lifetime-proof gap, not evidence of a reachable native defect.

`114B00` marks bit31 iff layout+44 table count exceeds C+B90[stage]. It
independently iterates layout+40 entries from layout+38 and marks a slot only
when its unsigned declared size exceeds C+F30[stage,slot]. Native branches
114B2A and 114B8C are `ja`, not inclusive comparisons. Fresh native sizes are
zero, as previously established in `handoff-codex-cbv-native-reset-identity.md`.
Consequently a positive-size entry forces stock upload, but table count alone
does not. Entry count zero or all declared sizes zero gives sentinel-only
admission, absent another write. These are conditional inputs, not observed
game layouts; slot validity remains the producer's contract.

`115484` (stage0 binder) has the same independent tests. Its extra constant
write is conditional on layout+64 != 0 (115528..11552F); that branch cannot
supply a universal guarantee. Stage5 binder `97CA14` reaches out-of-line body
1A49ADE: after setting the shader pointers it calls 114B00 at 1A49B15 and jumps
straight back to the return at 97CA31. There is no additional slot write there.
The exported size106 includes discontiguous code; hashing that many contiguous
bytes at 97CA14 would not validate the actual binder body. The auditor pins
the out-of-line body explicitly.

Decompilation reads also cover 45ACD8/45AE24/45AEB0/97CA3C, the five exported
114B00 callers. Their optional constant writes do not establish a universal
positive-size invariant. Caller listings are discovery aids, not an exhaustive
indirect-call proof. The prior native118DA8 caller and production CBT tests
still admit bit31 alone when n != 0 and retained cache keys match.

Verification: `py -I -B tools/bench/audit_cbv_first_layout.py` PASS: three new
native provenance windows and12 instruction checks; a memory-only truncated
image fixture correctly fails provenance before disassembly. The script reads only
0x101+0x12B+0x42 bytes and prints hashes/predicates, never native bytes.
The helper's0x101 window differs from the prior note's0x10B window by extent;
this is not a code change. No prior passed suites were repeated. An attempted
export search for simultaneous +38/+40/+44 writes was stopped by exact owned
PID at the bounded checkpoint without a result; it supplies no coverage claim.

Next independent unit: locate the shader layout metadata constructor/parser
and trace writes to layout+38/+40/+44. Prove or bound `n>0 => at least one
positive declared entry`, or find generation invalidation before first serving.
Use targeted references/vtable ownership instead of rescanning all exports.
Normal reset's changed-tag proof remains closed; fresh-address reuse, actual
repeated resource addresses, and same-open-list replacement remain separate.
Stable1.7.61, private guard PR1 and FO NO-GO unchanged. No local game/capture
at entry/checkpoint; no restoration was needed. Phone inbox empty; controller
holds, other writers, pinned builds and device queues untouched.
