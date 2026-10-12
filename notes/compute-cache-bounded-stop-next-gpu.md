# Compute cache owner search: bounded stop and next GPU unit

2026-10-11. Final bounded outer-owner attempt after the preload admission and
pool audits. No game, ASI, runtime observer or deployment changes.

## Narrowed destruction evidence

The409A020 vtable is written by base constructor**2EE0144**. Its sole retained
direct constructor caller is **2EDFB40**, which calls it at2EDFB52 and then
**overwrites the object's vtable with3B99288 at2EDFB67**. 2EDFB40 is reached
by CreateD3D12Device2EE300C. That device creation path publishes the resulting
device at global45BA588 only after native initialization succeeds and retains
it through256868. Initialization failure invokes the object's current virtual
destructor before allocator release.

This qualifies the prior note:409A020 ->2EE0D90 is a real base virtual-destructor
route, but **a published, independently used base object with that vtable has
not been demonstrated**. Do not describe it as an observed second runtime
destruction channel. Common2F00BC4 retirement is still necessary; this finding
does not prove the upstream owner closes all RequestLoad work first.

The bounded search examined retained constructor/caller exports, native vtable
references, the derived overwrite and creation publication/failure paths.
It did not establish a shutdown call sequence that closes manager admission,
waits all in-flight producers and joins the separate RequestLoad pool before
graphics-context destruction. Direct-edge absence on cancellation wrapper
2948B2C and producer11022FC does not establish runtime unreachability.

**Stop this serving-cache lane at NO-GO.** Repeating more generic byte scans
or shadow-cache tests would not resolve the missing owner identity. Resume
only with a concrete outer caller/registration or retained runtime ownership
evidence; do not add a serving hook or invent a new native synchronization
policy. No game-race claim is made.

The existing audit now passes26 pinned windows,26 altered-byte negatives,
36 instruction assertions. Newly pinned constructor windows are2EE0144/67
and2EDFB40/2A; no game bytes are committed.

## Ranked same-picture GPU work

1. **Already queued PB2/MIC exact native acceptance** remains the nearest
   implementation gate. Preserve its Windows/Proton/root/pack/output rules;
   no new shader variant should displace it just because remote ACK is late.
   This is not a claim those candidates improve frame time.
2. **Next independent offline unit: PB2 pixel-material repeated resource work.**
   Start exact PS `7dcfe3de3f3fcbc199be405eae1596b4` paired withVS1559714c,
   then compare PS `05ad17151de4f513fb82c7428874a7f9` only if needed. Source
   index `gpu-shaders/dxil_index.csv` maps the first to
   `0xb4751b0f.file`/record1 (also46F41041/DF8C9879) and the second to
   `0xbc09ad4a.file`/record1. Fresh-bind container -> actual DXC body -> original
   native ISA, identify repeated *same-lane* sample/descriptor/address work
   with identical resource, sampler, coordinates, LOD/derivatives and guards.
   Only propose a narrow rewrite if generated code still repeats it; reject
   compiler-eliminated expressions. Preserve exact float evaluation order,
   discard, derivatives and every render-target output. No pixel broadcast,
   reduced sampling or quality change is authorized by this suggestion.

   Evidence: historical USER profile `notes/ally-gpu-budget.md`, S1 gameplay
   windows6–9, reports1.57ms for the first whole PSO and1.10ms for the second;
   the G-buffer family totals7.54ms. These are **not isolated pixel-shader
   costs or savings budgets**. The exact existing VS proof does not establish
   equivalence of a PS rewrite. This is a concrete next analysis target, not
   an already found redundancy or performance candidate.
3. **Bounded fallback: triangle-index temporary selection in the same VS.**
   `notes/material-pb2-next-opportunities.md` identifies integer-only3-element
   selection, but original ISA has no scratch traffic. Try only an offline
   codegen comparison and reject unchanged/increased instructions/resources.
   Its expected value is lower than the material lane.

Do not revive fog as a measured win: latest native result is neutral. Do not
rank GI stencil compaction using the speculative1–2ms renderer estimate:
`notes/invisible-gpu.md` resolves existing early-outs and only15–30us saved
in its retained r5 trace; `gpu-savings-round2.md` already deprioritizes it.
No optional quality reductions are mixed into this same-picture ranking.
