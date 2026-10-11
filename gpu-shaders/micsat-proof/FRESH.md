# Fresh MIC identity/codegen and bounds contract

2026-10-11. All compile work finished before root's local game pilot notice;
no long/full proof is running. No staging, installation or live test.

## Fresh bound chain PASS

`bind.py` read original game file0x17ad25bb.file, SHA256
1c89c8e765015265b57781f913e9e7eab881790fdc82a3301844837b41337fdf,
and selected the sole indexed record with digest
e31c389f6d4a38de7b1b784b87c710d2 (unpacked offset588,size24976).
Original record SHA2563566a0efaaeac7724d0354c6a79bd0007ff15db33629f23b0555a324a80aeffc.
Fresh DXC dump matches the retained body used by the corner verifier.

Current micsat.py changes all3 copies. Candidate assembled, signed/validated,
freshly dumped body equals rewritten body; original signature/PSV/SFI parts
are preserved. Candidate digest5570428b0cf8a46e12299e50c0963ad0,
SHA256bc5acd4195f7fe2be492aeb3048b13c8957c2a4d33c0fa01981aee17e4e2bc86.
Original game file equality rechecked after work. No repacked file created.
`out/binding.json` also binds source-body/candidate-body and rewriter hashes.
`check.py` now checks its source and current rewrite against this bound chain
when the receipt exists. Corner proof scope remains as README, not full CS.

## Current offline gfx1103 comparison

| Metric | Fresh original | Fresh current3-copy |
|---|---:|---:|
| instructions |2810|2752|
| VMEM loads |77|77|
| samples (ISA analyser metric) |32|32|
| full memory waits |33|20|
| partial waits |25|30|
| branches |101|92|
| VGPR / SGPR |72 /62|64 /62|
| wave / scratch |32 /0|32 /0|

These supersede stale retained estimates for THIS setup. RGA requires a root
signature; the first attempt without one failed explicitly, then both arms
used the same generated register-compatible signature from rga_ps.root_sig.
It is a synthetic table layout, **not the game's extracted compute signature**.
Its HLSL has no explicit descriptor-range bounds flags; don't claim its
optimized codegen or OOB behavior matches the live root signature. Offline
Windows AMD compiler output is not installed RADV. Metrics are static, not
GPU duration or FPS, and register count alone does not prove occupancy gain.

## Authoritative OOB rule: important exception

The blanket statement that every D3D12 buffer read out of range safely returns
zero is insufficient. Root Signature1.1 static buffer descriptors can make
out-of-bounds accesses undefined, potentially including device reset.
DESCRIPTORS_VOLATILE retains safe bounds behavior; the later
DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS flag preserves it while allowing
static descriptors. This is a descriptor contract, distinct from DATA flags.
Sources: [Microsoft Root Signature1.1](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-version-1-1),
[D3D12 Resource Binding spec](https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html).

Vulkan robustness2 can provide zero-valued out-of-range uniform/storage texel
buffer reads, with format component substitution. The applicable pipeline
robustness setting and enabled device features determine that guarantee.
[Khronos shader robustness specification](https://github.khronos.org/Vulkan-Site/spec/latest/chapters/shaders.html).

Pinned local vkd3d source HEAD2230755878b01993b4b82d0ac0c0624f3ab4d333
requires robustness2 at device.c:2639; this does not identify the currently
installed backend/version or prove how this buffer descriptor is translated.
Root-signature flags, t21 descriptor type/range/lifetime and actual backend
remain unbound. The existing strict-bounds witness is relevant precisely
because the rewrite relies on discarded OOB reads.

## Next precise gate

Existing native notes locate compute signature at SynCtx+6E0, creation in
2EFEA14, bound by SynBeginList49349C. Next read-only task is the serializer/
version and t21 range flags along that creation path, plus actual t21 producer
descriptor bounds. Do not treat the synthetic RGA signature as game evidence.

Full actual-CS sample/UAV/atomic proof remains unfinished. The6000 corner
checks alone cannot establish end-to-end equivalence. Keep native queue
closed until the resource/OOB contract and full proof are adequate. No heavy
proof while root's local game pilot runs.

Reproduce fresh binding/codegen only when lead clears heavy work:
`py -I -B gpu-shaders/micsat-proof/bind.py`.
