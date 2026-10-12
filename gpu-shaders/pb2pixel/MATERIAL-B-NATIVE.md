# Exact second-material private native preparation

Private packet: `D:/nioh3-re/deliveries/pb2-material-b-native-20261012`.
Nothing was run on D3D12, installed or launched in this preparation step.

The retained journal SHA256
`f1845f5f3d61f7f7d59538cb187fef57df433f6e3f3fced9c852b75cec97c40d`
contains **four exact VS70d0ee9feee590f9c48c40b808858e27 /
PS05ad17151de4f513fb82c7428874a7f9 records**, at offsets16116818,
16190406,16195298,16198120. Selected PSO, shader and root dependency checksums
were verified using the actual journal Hash128. No synthesized descriptor or
root was used. All four root blobs hash to
`dc65a1fc5ca7cb3fbcd100895637c8eab667904e6f613625597bad8890d0031c`.
The six RT formats are91/11/87/26/87/34, DSV20, samplecount1; descriptor bytes
and each record's original offset/hash are pinned in binding.json.

The native folder contains the exact original VS, original PS, candidate PS,
four descriptors and four roots. Candidate SHA256
`efb7895ca386d3e0b43f4d6b61aa20195cf0c0d50c79efc4baedab313b56444c`,
digest `c45e1b45259184e76dc0483e43d59b6d`. The helper derives from the pinned
existing no-draw helper with only PS substitution and recordcount4 changes.
It checks serialized pointer/length fields before creating roots/pipelines.
It creates no command queue, draw, window or image. Source is prepared, unbuilt.

Root's exact invocation after independent review:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'D:\nioh3-wt\mic-pipeline-admission\gpu-shaders\pb2pixel\run_material_b_native.ps1'
```

The runner requires game absence, pins binding.json SHA256
`8bd0e44288a48d95568f8d8126a0c5e11e4f291e5d9639e88accdd6851da564e`,
verifies every packet input before/after build and native invocation, then writes
a separate actual native receipt. A PASS needs4 root and8 original/candidate PSO
successes. No PS7dc verdict transfers even though some descriptor/root bytes
match. The runner was syntax-parsed, not executed.

Private one-record packed file targets `shader_dx12/0xbc09ad4a.file`, slot1:

- Before: `2fd6d3c82b14be2b5cc47e76dd36f0c65a1af394833f2b17737ddfb2fab89326`
- Candidate: `6415a1875837c5998ed2178327b02aec96b075c7fb7c78ff4b07df0f307fad6c`
- Companion slot0 record stays
  `62ae16a72dcc702bf0cda487bd12fa7be739c68ee33e000a7656b85a9914b4ee`.

`pack_check.prove` plus independent record/tail comparisons verify only slot1
changes, both CXDK tails remain exact, and replacing the original shader restores
the entire decompressed original payload. Wrong-slot and corrupted-candidate
negatives refuse. Exact original.file is retained for eventual ownership-checked
rollback; never restore over a target that differs from the candidate hash.
No live installer is included or authorized by this preparation. Existing
PS7dc live_switch.py has different fixed pins and MUST NOT be used for this pack.
Native PSO, draw participation, image and performance remain UNRUN.
