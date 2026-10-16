# PB2 pixel candidate: exact offscreen draw prerequisite

2026-10-12 bounded retained-input audit. **STOP: existing fixtures cannot
support an exact original/candidate draw.** No helper was built, no shader
compiled, no draw or game run, and no installation/source changes were made.

## Concrete retained evidence

- Current 67,108,773-byte native journal SHA256 is still
  `f1845f5f3d61f7f7d59538cb187fef57df433f6e3f3fced9c852b75cec97c40d`.
  Complete chunk-header walk reaches EOF exactly: 16 RSIG, 1,643 SHDR,
  15,576 PSO1 chunks, no other chunk types. This census is not another
  checksum/admission proof; those selected-record checks already passed.
- `mod/Nioh3EngineFix/src/psojournal.cpp:3` defines the format;
  `SerGfx` at line361 serializes pipeline fixed state, root/shader identities
  and optional input-layout metadata. It does not serialize command-list
  root bindings, descriptor heap entries or resource contents.
- `D:/nioh3-re/pb2pixel-20261011/native-pipeline` contains shader containers,
  eight 656-byte descriptors, eight 1,228-byte roots, receipts and the
  no-draw helper. No vertex/material/texture/target resource snapshots exist.
- Existing PC FCAP files, e.g.
  `captures/fcap/pc-20261010-0059/Nioh3EngineFix_fcap_5_tour_1_4_1.csv`,
  record commands and resource descriptions. `fcap.h:25-28` specifies their
  argument/view/handle/description logging. A GPU descriptor handle or virtual
  address from that old process is not a heap-entry definition or buffer copy.
  `notes/renderer/12-capture-check.md` explicitly notes missing RTV-to-resource
  mapping. FCAP does not fill the resource-content hole.
- Bounded file discovery under `D:/nioh3-re` and repository `gpu-shaders`,
  `notes`, `re` found the archived RGP profile but no .rdc/.wpix/.pix3 replay
  capture. This is not an exhaustive search of unrelated disks/directories.

## First missing field and smallest useful capture

First fatal gap: **the selected draw's graphics root descriptor-table values
and the descriptor entries/resources they identify**. The serialized root
signature describes the layout, not these values. This exact PS reads cb0,
t3 typed min-mip data, t31 raw material data, bindless texture2D t0/space3
and bindless sampler s0/space2. The paired original VS also needs its exact
draw and resource inputs; empty input layout does not mean input-free drawing.

Smallest useful request: retain ONE completed frame's replayable dependency
snapshot containing ONE selected draw with exact PS digest
`7dcfe3de3f3fcbc199be405eae1596b4` and VS
`1559714ca496fffe6ddd3c827e1d962e`, identifying its event and PSO. For that
event retain:

1. Draw/indirect arguments, index data where applicable, topology,
   viewport/scissor, root constants/descriptors/tables, relevant heap entries
   and actual sampler descriptors.
2. All reachable VS/PS buffers and texture subresources at that execution
   boundary, with dimensions/formats/strides/mips and descriptor mappings;
   include indirect argument/count buffers and their resolved values if used.
3. RTV/DSV descriptors, dimensions and **pre-draw contents**, resource states,
   blend factor/stencil reference and any relevant predication/queue dependency.
   Existing PSO formats alone cannot initialize blending/depth/stencil inputs.

Restore this identical captured state before each offscreen original/candidate
draw; compare every written attachment/readback, not only final display color.
Capture must be complete and replayable before admission; undefined/stale GPU
addresses, invented material textures or fabricated triangles would only make
a synthetic test. No new runtime observer or capture hook was implemented.
Lead owns any future bounded capture/device lease. Proton and performance
gates remain separate even if an RX9070 draw comparison later passes.
