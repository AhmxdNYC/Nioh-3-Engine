# PB2 exact pack binding and remaining backend gate

2026-10-11. Offline only. `pack_check.py` reads five local game containers,
constructs two independent single-record replacements **in memory**, checks
round-trip identity, and writes only `out/pack-binding.json`. No packed shader,
replacement directory, installed file or source candidate is written.

## Completed pack proof

Original VS digest `1559714ca496fffe6ddd3c827e1d962e`, SHA256
`5a7d10634d334e35ed62a5706fa784c905793f8d8a20a60539be9abd0ea970cc`.
Candidate digest `11b0311eb9ff27c2db64bd7647d2de5e`, SHA256
`0d44ddcd220d87d17d2e38bfff2533550fb2e72595dee71f5037d061466985be`.

Both alternatives contain exactly one original match, at zero-based record10
of38. After replacement/compression/decompression, record10 is the exact
candidate, all37 other complete records are byte-identical, and the rewritten
CXDK auxiliary tail remains identical. Payload headers differ only in allowed
sizes/table offsets; IDRK resource/type/flags remain identical. Wrong-slot and
corrupted-candidate attempts are rejected. All five input files and candidate
are reread unchanged at completion.

| Container | Current original SHA256 | Hypothetical packed SHA256 |
|---|---|---|
| 0xa79af848.file | `69d6df734a873893fc25f3769d491229c06278c2d822704190b0da2b2b51cc7d` | `a80ec70cf116edecb6024c00826ace7be65adebcb370af3c903d9f4603611bab` |
| 0xf00d5c8c.file | `722478d330337a28fdff12c166bd1e8ff628c079096f838531f24543d7df608f` | `a4fe94d5d366ce405815c428c3e4d2bac114f10893fe28ccddba2c836f812e4b` |

Associated PS digest `7dcfe3de3f3fcbc199be405eae1596b4` is independently
confirmed at record1 in `0x46f41041.file`, `0xb4751b0f.file`, and
`0xdf8c9879.file`; their complete file/record hashes are in the receipt.
No PS replacement is constructed. Identical VS bytes in two containers do not
identify the material permutation of a live draw. These are two alternatives,
not authorization to install both. A future installer must bind the complete
original file hash as well as digest and slot, refusing an unknown/modded file.

Reproduce: `py -I -B gpu-shaders/pb2guard/pack_check.py`.

## Backend requirements, grounded in source

Ally receipt2043 supplies an actual backend snapshot. Root downloaded private
file242 (`D:/nioh3-re/vulkaninfo_ally_242.txt`), SHA256
`0159b23a0346c4a41d072f793d9a87266e5e373d929875883682e767da752eee`,
and checked vertex subgroup support plus BASIC/VOTE/BALLOT operations.
Reported subgroup size64, min32/max64; requiredSubgroupSizeStages excludes
VERTEX, so this does not authorize pinning a vertex wave size. Driver is
RADV PHOENIX Mesa26.2.2. Device report identifies Proton Experimental
1790839227 / experimental-11.0-20261001-x86_64, vkd3d build
44cf7c2042168f3b8ee37249f0a53ac091e48fd9. Actual candidate DXIL translation,
pipeline acceptance and live material permutation remain open. No native PB2
test or promotion is claimed from this capability snapshot.

The candidate remains VS6.5 and adds only WaveOps to its feature declaration.
On the actual D3D12 adapter, record `CheckFeatureSupport` for shader model and
`D3D12_FEATURE_D3D12_OPTIONS1`: WaveOps must be supported; record lane limits
without forcing a32/64-lane assumption. Microsoft defines the explicit
[WaveOps capability](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_d3d12_options1).
The presence of original SM6.5 bytecode is not itself that query receipt.

[WaveActiveAllEqual](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/waveactiveallequal)
compares currently active lanes;
[WaveReadLaneFirst](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/wavereadfirstlane)
selects the lowest-index active lane. Both support vertex shaders. The guarded
index is therefore identical per participating lane regardless of sparse masks
or nonconsecutive vertex IDs. No lane-participation-changing operation lies
between the guard and first-lane read. This is a semantic argument independent
of the finite192-wave tests.

The [DirectX specification's general resource-indexing rule](https://microsoft.github.io/DirectX-Specs/d3d/WorkGraphs.html#nonuniformresourceindex-semantics)
requires uniformity over active wave threads unless the index is marked
NonUniformResourceIndex. Thus clearing that flag only in the all-equal branch
is justified; the mixed branch must retain it. Original `!5` declares16 raw
buffers starting at t1, space0; root signature and descriptor bindings must
cover this original range. Candidate resource metadata is unchanged. Descriptor
validity, resource lifetime and synchronization remain the original native
draw's obligations; hashing synthetic memory cannot establish them. The exact
read-address/mask equivalence is conditional on defined original inputs and
stable read-only resource contents, not a newly introduced bounds check.

For Proton/RADV, current upstream
[vkd3d device.c](https://raw.githubusercontent.com/HansKristian-Work/vkd3d-proton/master/libs/vkd3d/device.c)
provides a concrete caution: `d3d12_device_caps_init_feature_options1` exposes
WaveOps when maximum shader model is at least6.0. Its shader-model admission
tests subgroup basic/arithmetic/ballot/shuffle/quad/vote support but required
stages are COMPUTE and FRAGMENT, not VERTEX. A global WaveOps result alone is
therefore not evidence of this particular VS lowering. This was a current
upstream source inspection, **not identification of the installed Proton
build**, and does not establish that vertex waves are unsupported. Tie the
actual target's vkd3d/dxil-spirv/Mesa versions to its VS translation, emitted
subgroup operations, nonuniform descriptor path, and accepted pipeline.
If translation changes resource representation, check the resulting indexing
capabilities for that representation rather than assuming all raw buffers
become one particular Vulkan descriptor type.

## Concrete next admission receipt

Pack identity is now PASS; actual target codegen remains UNPROVEN. The next
bounded gate is one original-versus-candidate **actual material pipeline**
compiler/admission receipt on the chosen backend, binding exact VS bytes,
paired PS/root signature/input/topology state, adapter/driver versions and
WaveOps feature results. On RADV it must include the actual vertex subgroup
lowering and mixed-descriptor path; Windows sink-PS RGA output is insufficient.
Also resolve which of the two container permutations that draw uses before
creating any installation artifact. No new SPIR-V, real pipeline receipt or
device capability query was produced here. Native gate remains closed; no
visual-equivalence or performance result follows from pack binding.
