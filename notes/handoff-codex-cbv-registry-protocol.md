# CBV registry protocol: VS registration and handle-slot seam

2026-10-11 14:44, OpenAI Codex. Offline inert-image audit only.

Native `3032CFC` reads the stage count at input-object +A8. Zero count
returns without a registry call. For nonzero count, accessor `3034528`
first advances the input pointer by +40, requires original-object +13C
nonzero and a signed-positive offset at original-object +140, then returns
original-object +40 + offset; otherwise it returns null. These fields are
blob location/admission fields, not renderer layout +38/+40/+44 fields.

The registration loop preserves incoming RCX as registry, RDX as an opaque
caller argument, R9 as the output handle-array pointer and the accessor
result as blob cursor. Registry vtable slot +0 receives registry in RCX,
the opaque argument in RDX, cursor in R8, the caller's fifth DWORD in R9D,
zero as its fifth argument (kind zero), and a local output-pointer address
as its sixth argument. The local output pointer is cleared before each call.
Both a nonnull output pointer and nonzero AL are required before publishing
the handle. On success it stores the pointer, advances the cursor by the
caller-supplied DWORD and the handle-array pointer by eight bytes, then loops
to the declared count. Either failed condition sets the caller's sixth
argument's pointed-to byte to one and exits the loop. The audit does not
resolve that flag's outer cleanup or the opaque argument's meaning.

The helper has no additional local null-cursor rejection or stride-positive
test before slot +0. This does not establish a reachable defect: input
contracts and the concrete virtual implementation remain unknown. In
particular, this caller-supplied cursor stride cannot prove positive declared
CBV entries or first-serving invalidation in the renderer's bound layout.

Companion `3032FF4` reads the same count, loads global registry `4BD0A38`,
and invokes its vtable +18 with the **handle-slot address** in RDX and kind
zero in R8D, advancing the slot address by eight per iteration. This is a
candidate release/retirement seam; without the virtual body it does not prove
what is freed, whether slots are cleared, or how renderer references retire.
The prior getter seam used vtable +10, so the concrete registry now has
three useful protocol targets: +0, +10 and +18. Names describe inferred roles.

Validation: `py -I -B tools/bench/audit_cbv_registry_protocol.py` PASS:
three SHA256-pinned native windows/302 bytes,53 instruction checks, and
truncated/corrupted memory-only rejection for each window (six controls).
The accessor provenance window contains padding/neighboring getters; its
instruction assertions and conclusions stop at the first accessor body.
`git diff --check` PASS. No existing passed suites repeated.

Next independent unit: resolve a concrete registry vtable through targeted
static references/constructor ownership; trace +0 outputs through +10 into
bound-layout +38/+40/+44 and inspect +18 retirement/generation semantics.
Positive-size first-layout guarantee remains open. No native defect, safety,
performance or visual-equivalence claim. Stable1.7.61, private guard PR1
unmerged, FO NO-GO; external default-stack/player gates remain pending.
No local game/capture/Ghidra or restoration needed; no build/deployment,
save/settings/hardware/controller/hold/Claude changes. Other writers,
worktrees and device-pinned inputs untouched. Initial phone check empty; final item1917 (sender=ally) reports save/Cloud gate BLOCKED with no launch. No sender=user steering; no device-launch decision made.
