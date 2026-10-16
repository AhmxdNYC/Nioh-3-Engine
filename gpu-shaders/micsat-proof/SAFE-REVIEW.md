# Safe-index and header-guard mutation checks

`safe_review.py` is an explicit author self-check, not independent peer review.
It freshly validates the signed candidate through `safe_check.validate()` and
executes actual mutated instruction bodies. Candidate SHA256 remains
`9a64454f4f89d6571c01cbaf38229aca08ec66e2f0a206fc969a77a9e4409c66`.

**15 negative cases pass:** each of the nine safe-index selections is individually
bypassed, and each of the six guarded T[2]/T[1] header loads is individually
hoisted before its original guard. Every mutant introduces a t21 address absent
from the original thread's read set, while final UAV state and observable
UAV/sample/atomic event traces remain unchanged. Therefore output comparison
alone would miss these regressions; the address gate rejects all of them.

The unmodified safe candidate passes output comparison, address containment and
other-resource read equality on all42 targeted scene variants used to obtain
these witnesses. Header cases use width0 with one-word storage, or a header
with zero views. These are bounded model cases under stable-resource semantics,
not a claim about observed native bindings or universal equivalence.

The initial old-fixture attempt did not detect the first index bypass: its
viewport scale64 often clamps both projected SAT corners to the same edge.
The targeted fixtures use unit viewport scales and zero offsets, exercising
skipped corner predicates. This illustrates why all-block coverage did not
establish coverage of every safe-index selection. No shader output mismatch
was found; the adversarial fixture was strengthened.

Per-copy bypass first detections:

| Copy / block | Corner0 | Corner1 | Corner2 |
|---|---:|---:|---:|
| b393 | seed1 | seed1 | seed1 |
| b1573 | seed3 | seed3 | seed3 |
| b1826 | seed21 | seed21 | seed29 |

T[2] and T[1] hoists in the three copies are detected at seeds0,3,5 respectively.
Some unwanted corner reads are within the allocation; the gate deliberately
checks the stronger original-path address contract, not only OOB behavior.

    py -I -B gpu-shaders/micsat-proof/safe_review.py

Local execution took1.96 seconds. Receipt: `out/safe/address-review.json`.
Only the new checker and this note were added; no candidate rewrite, compile,
staging or installation occurred.
