# Native default compute root signature: version1.0 recovered

2026-10-11. Bounded read-only decomp/native audit plus isolated offline RGA;
no game, staging, root-signature replacement or installed-file modification.

**The native default compute signature uses Root Signature1.0.** The earlier
Root Signature1.1 static-descriptor concern does not apply to this recovered
default path. It is not necessary to invent bounds flags for that path.

Evidence from existing `D:/nioh3-re/decomp/0x2efea14.c`:

- Second serializer call builds4 descriptor-table root parameters, ALL
  shader visibility, one range each, root flags0, no static samplers.
- Ranges: CBV b0 count14; SRV t0 count48; UAV u0 count16; sampler s0 count16.
  Register spaces0, offsetsAPPEND. Thus t21 lies within the SRV range.
- `D3D12SerializeRootSignature(local_6a0,1,...)` serializes that layout.
  Subsequent Device.CreateRootSignature output is stored at SynCtx+6E0.
  `notes/renderer/backend.md` identifies SynBeginList49349C binding that field.

`root_signature.py` independently resolves the actual native import and
checks bounded instructions from the flat private dump:

| Path | Version-register definition | Argument assignment | Serializer call |
|---|---|---|---|
| graphics initializer |2EFEA47 mov edi,1|2EFEF50 mov edx,edi|2EFEF63|
| default compute initializer |2EFEFFD mov edi,1|2EFF0F6 mov edx,edi|2EFF151|
| async compute initializer |2EF873D mov esi,1|2EF8875 mov edx,esi|2EF8880|

The script checks the last explicit native writes to the nonvolatile source
register and argument register before each call; import table names identify
D3D12SerializeRootSignature. Decomp corroborates value1 on all three paths.
Receipt includes exact window hashes. This is bounded native evidence, not
an exhaustive census of every runtime signature producer/override.

Version1 corresponds to D3D_ROOT_SIGNATURE_VERSION_1/1_0. Version1.0 retains
the volatile-descriptor behavior; safe buffer bounds behavior cannot be
removed via the1.1 static descriptor promises on this signature.
[Microsoft Root Signature1.1 behavior](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-version-1-1).

## Fresh codegen using this recovered layout

`native_layout_isa.py` reconstructs the described default layout and explicitly
compiles it with DXC `rootsig_1_0`, then passes the resulting binary to RGA
for BOTH original and bound current3-copy MIC. This replaces the earlier
generic-layout assumption for this comparison. It is a reconstructed matching
description, not an extracted original serialized byte blob.

| Fresh gfx1103 metric | Original | Current |
|---|---:|---:|
| instructions |2813|2755|
| full waits |33|20|
| branches |101|92|
| VGPR / SGPR |72 /62|64 /62|
| VMEM loads |77|77|
| wave / scratch |32 /0|32 /0|

The apparent codegen benefit survives the native-derived version/layout.
Static waits/registers are not measured FPS or hardware occupancy. This is
still offline Windows AMD compiler output, not installed RADV.

## What remains open

Default signature identity is recovered; no live MIC PSO/signature binding
was observed. A complete claim must show MIC uses this signature rather than
an alternate/override and that t21's descriptor/resource remains valid for
the dispatch. Register-range coverage is not buffer element-count proof.
The OOB corner witness is permitted only under the applicable valid-resource
bounds contract. Root signatures do not rescue invalid descriptor lifetimes.

Full actual-CS sample/UAV/atomic/final-output proof is still absent. The6000
corner comparisons remain bounded and must not be promoted into that claim.
No native queue yet; continue t21 producer/lifetime and full-body proof.

Reproduce:

    py -I -B gpu-shaders/micsat-proof/root_signature.py
    py -I -B gpu-shaders/micsat-proof/native_layout_isa.py

Receipts: ignored `out/root-signature.json`, `out/native-layout-metrics.json`;
root signature binary/HLSL and ISA under `out/native-layout/`.
