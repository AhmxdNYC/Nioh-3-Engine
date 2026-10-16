# PB2 recorded native pipeline admission

2026-10-11. Separate offline tooling; no ASI edits, installed shader changes,
game launch, command queue or draw. This replaces the synthetic sink-PS input
for the next Windows compiler gate with actual recorded material pipelines.

## Exact recorded state found

Read-only local `Nioh3EngineFix_pj/pj_win_dgpu_sm68.bin`, SHA256
`f1845f5f3d61f7f7d59538cb187fef57df433f6e3f3fced9c852b75cec97c40d`,
contains eight graphics PSO records pairing full VS digest
`1559714ca496fffe6ddd3c827e1d962e` with material PS
`7dcfe3de3f3fcbc199be405eae1596b4`. They occur at byte offsets
60650644, 60653588, 60655497, 60658441, 60662544, 60665488,
60668432 and 60669184. Each has two shader stages, an empty input layout,
triangle topology, six MRT formats 91/11/87/26/87/34, DSV format20 and
sample count1/quality0. They retain their distinct complete 656-byte native
descriptor, including blend, raster, depth/stencil and remaining fixed state.

All eight reference serialized root key
`e92b991fc87fcb0b575fdc9e86979533`, blob SHA256
`dc65a1fc5ca7cb3fbcd100895637c8eab667904e6f613625597bad8890d0031c`.
The extracted material PS SHA256 is
`0205c80d9035cd8de989cd758a3cf987eda459156ed2bdd9f6224c5ead258268`.
Original/candidate VS SHA256 remain the exact COMPOSITION.md pins.

`pipeline_extract.py` validates selected PSO and SHDR/RSIG chunk checksums
using the actual journal Hash128 algorithm, root blob key and shader DXBC
digest, and refuses partial chunks or conflicting duplicate dependencies.
Unrelated payload checksums are explicitly not certified. It verifies the
source journal remains unchanged during extraction. It writes only private
`out/pipeline` fixtures and a binding receipt, not installable packs.

## Small executable gate

`pipeline_admit.cpp` queries a hardware D3D12 adapter, driver version,
WaveOps/lane limits and SM6.5 support. It creates the recorded native root and
each exact original/candidate graphics pipeline pair through
CreateGraphicsPipelineState. It refuses unexpected serialized pointers,
lengths or input layouts; only root/VS/PS pointers are rebound. There is no
window, command queue, render target allocation or draw. `pipeline_run.py`
validates fixture hashes, builds with MSVC /W4 /WX and bounds admission to
120 seconds, retaining actual HRESULTs and helper hashes.

Commands from the repository:

```
py -I -B gpu-shaders/pb2guard/pipeline_extract.py "C:\Program Files (x86)\Steam\steamapps\common\Nioh3\Nioh3EngineFix_pj\pj_win_dgpu_sm68.bin"
py -I -B gpu-shaders/pb2guard/pipeline_run.py --build-only
py -I -B gpu-shaders/pb2guard/pipeline_run.py
```

Extraction and /W4 /WX build PASS. After root confirmed local game closure
and restoration, the actual admission run also **PASS**: eight root creations
and all sixteen original/candidate PSO creations returned S_OK. Adapter was
AMD Radeon RX 9070, vendor1002/device7550, CheckInterfaceSupport driver value
9007201289044972 (S_OK). WaveOps query returned enabled, lane limits32..64;
SM6.5 query returned supported0x65. No draw executed.

Private receipt: `out/pipeline/admission.json`, binding SHA256
`a7cd9c0775f060af16e89f76b2a3d95e96e11636ac4f78356ed7f3d8a731ef1c`;
helper source SHA256
`eb23ec8e978e81f2aa51bfd3e369db454acccc9586e50e67fd83a262c07db7a1`,
helper executable SHA256
`c963c4b175125a993a7ce13b43be8cf04e577708bb466f10114ad0d199490abe`.

## Scope of this PASS

This closes Windows compiler admission for these eight actual recorded states,
not draw correctness, rendering equivalence or savings. The journal identifies
recorded shader bytes/state, not which duplicate packed container a live draw
uses. It does not contain draw descriptors or runtime vertex/resource data.
The original-vs-candidate comparison must both succeed; original rejection is
a broken baseline rather than evidence against the rewrite alone.

Windows acceptance cannot substitute for the Ally Proton/RADV translation gate.
The same exact fixtures can support a separately built D3D12 helper under the
actual Proton version, but no remote operation or Vulkan admission has run here.
An eventual Proton receipt still needs installed vkd3d identity and emitted
vertex subgroup/nonuniform indexing evidence, followed by controlled native
visual/motion smoke and matched A/B under root ownership.
