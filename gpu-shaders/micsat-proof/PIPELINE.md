# MIC recorded compute pipeline prerequisite

2026-10-11: **MISSING_NATIVE_MIC_PSO**, not a compiler rejection or admission
PASS. No device helper ran, no game launched, no installed files changed.

`pipeline_scan.py` structurally parsed the entire stable installed journal
`Nioh3EngineFix_pj/pj_win_dgpu_sm68.bin`, SHA256
`f1845f5f3d61f7f7d59538cb187fef57df433f6e3f3fced9c852b75cec97c40d`.
Its 67,108,773 bytes contain 16 RSIG, 1,643 SHDR and 15,576 PSO1 chunks:
15,511 graphics and 65 compute. Neither a shader chunk nor a pipeline reference
matches original MIC digest `e31c389f6d4a38de7b1b784b87c710d2`.
This census validates structure and source stability, not payload checksums.
Searches for `pj_*.bin` under D:/nioh3-re, D:/nioh3-wt and the private repository
found only additional tiny pjtest fixtures, not another native capture.

The journal is 91 bytes below the default 64 MiB cap. Saturation is a plausible
reason for missing later state, not proof MIC was created or used. In
`mod/Nioh3EngineFix/src/psojournal.cpp`, SerCs (line390) records the real root,
CS, NodeMask and Flags; WriteItem (line999) rejects new chunks past maxBytes;
PJ_MaxMB (line1814) defaults64 and clamps4..512. PJ=2 journals without title
prewarm. Compute capture is supported; inventing a descriptor is unnecessary.

## Smallest future capture and admission

Under the lead's existing save/game/entry-state harness, retain the current
journal and capture original MIC creation into a journal with sufficient room
(for example a separately preserved capture with PJ=2, PJ_MaxMB=128). This is
a prerequisite recipe, not authorization to change settings or launch here.
Require the exact digest above in a PSO1 shader reference plus its SHDR and
referenced RSIG dependencies. Creation evidence alone is not dispatch evidence.
Missing digest means this gate remains inconclusive even if the scene looks fine.

Once available, extend the existing PB2 extraction pattern to verify selected
PSO/dependency checksums, root key and original DXBC SHA256
`3566a0efaaeac7724d0354c6a79bd0007ff15db33629f23b0555a324a80aeffc`.
For kind2 require the exact captured 56-byte x64 compute descriptor, one CS
reference at offset8, root offset0, and no unexpected pointers/cached PSO.
Preserve captured NodeMask/Flags. A kind3 stream must instead be interpreted
as its actual stream; do not relabel it a kind2 descriptor.
Create the captured root and original/candidate compute PSOs on the same actual
Windows adapter using safe candidate SHA256
`9a64454f4f89d6571c01cbaf38229aca08ec66e2f0a206fc969a77a9e4409c66`.
Both must return S_OK. No queue, dispatch, window or installed replacement is
needed. Root/PSO acceptance still proves neither resource correctness nor FPS.
The generated RGA signature and another shader's recorded root are explicitly
insufficient substitutes for this exact runtime association.

Reproduce the census:

    py -I -B gpu-shaders/micsat-proof/pipeline_scan.py "C:/Program Files (x86)/Steam/steamapps/common/Nioh3/Nioh3EngineFix_pj/pj_win_dgpu_sm68.bin"
