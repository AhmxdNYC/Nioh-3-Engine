# Renderer: PSO / shader / material layer (Nioh3.exe Steam v2.02)

Package of the renderer decompilation (module map: `research/renderer/00-map.md`). Scope: the shader file format, the
material data, the permutation pick, PSO description building and the PSO cache, how a draw binds material constants and
textures, and the chain "draw this mesh with this material" -> bound D3D12 PSO + resources. The D3D12 backend itself
(token table, descriptor rings, barriers) is `backend.md`; lighting, shadows and post are other packages.

Sources: decompiles from `<re-scratch>\decomp` and `<re-scratch>\decomp_ro.cmd` (2026-10-10: 9290D8, 929A44, 92992C,
9298B8, 9297B0, 9295AC, 2EEBC98, 1199A4, 2EFE2A8, 2EFE1CC, 3483E04, 34837DC, 79E990, 1D23E0, 1D2658, 118758, 1E0B64,
1E1D6C, 379BC, 37A3C, 547160, 696DA8, 79E6B8, 45A654, 3B80C, 3C77F4, A300B0), an offline parse of all 3,897
`shader_dx12` files (scratch script, logic below), sample G1M / `.mtl` / `.sid` files, the enum tables read straight
from the exe's `.rdata`, and earlier notes: `stutter-pso.md`, `boot-shader-compile.md`, `gpu-shader-replacement.md`,
`gpu-shader-sweep.md`, `gpu-materials-trim.md`, `asset-audit.md`, `rdb-index.md`, `d3d12-submission.md`,
`mainthread-drawprep.md`. The game was not run. Names are in `re/symbols/symbols.tsv` block `# renderer/materials`.

Confidence tags: **[dec]** read in a decompile, **[data]** verified over the shipped files, **[inf]** inferred.
A name with `*` is a proposal for a function another package owns (`re/scripts_renderer/partition.tsv`); it is not in
symbols.tsv (list in §9).

**Entry points:** CreateGfxPso 9290D8, PsoGetOrCreateGfx 119670, PsoGetOrCreateCompute 119A48 / Shared 58C000,
PsoListLoad 3483E04 (boot list), ShaderGroupFileLoad A300B0 (files), MaterialTechniqueBind 3C77F4 and DrawStateFlushDirty
1E0B64 (material -> permutation), ShaderConstWriteStream 1D7340 (constants).

---

## 1. Shader files: `shader_dx12\0x%08x.file`

### 1.1 Where they come from

- 69,505 rdb entries of type `G1SFile` (type hash 0x7BCD279F) have size 0: the bytes are loose files in `shader_dx12\`.
  Only 3,666 of those KTIDs have a file. 231 more files have no rdb entry (engine built-ins such as GHDM). One fallback
  file `PB2Unknown.file`. **[data]**
- ShaderGroupResourceResolve 10E50F0 -> ShaderGroupFileLoad A300B0 builds `<root>../../shader_dx12/0x%08x.file`.
  - A hard-coded **alias table** remaps 14 group hashes before the path is built (for example 0x347300BE -> 0x9C481F79,
    0x23625BF7 -> 0xAC9ED13E, 0x58C2D622 -> 0xF0FFD1C1, six hashes -> 0xEC7B0597, 0x69B8CB0F -> 0xF0608E3A). **[dec]**
  - 0x6C0B209D / 0xA54B3E4D can return "not loaded" (checks 2E0B74) or use a per-byte subfolder `%s%02x/`. **[dec]**
  - A missing file (KTGL fs error -0x1A) loads `PB2Unknown.file` instead. **[dec]**
  - No checksum anywhere: loose-file replacement works (GHDM, WDO#28, fog, MSV; see `gpu-shader-replacement.md`).
- The group KTID a model uses comes from object-database properties, not from the G1M:
  `G1SFileResourceHash`, `GBufferG1SFileResourceHash`, `ShadowmapG1SFileResourceHash`, `ShadowmapLSPG1SFileResourceHash`,
  `PreLightG1SFileResourceHash`, `GBufferForDeferredShadingG1SFileResourceHash` ... (`data/tools/extra_props.tsv`). So a
  model carries one shader group **per render pass**. Engine passes (SSAO, HeightFog, LightPass ...) register their
  own shader-object types (`*RegisterType` rows in symbols.tsv) with a `...ShaderObjectNameHash` field in the pass schema.

### 1.2 IDRK container (as in `gpu-shader-replacement.md`)

`"IDRK0000"`, u64 file size, u64 compressed size, u64 uncompressed size, u32 0, u32 resource hash (= file name),
u32 type hash 0x7BCD279F, u32 flags 0x00100000, u64 0; then zlib chunks `u32 zsize + "78 9C" stream` of 16 KiB
uncompressed, terminated by u32 0. 2 of 3,897 files use flags 0x00400000 with an extra header and do not unpack with
`gpu-shaders/idrk.py`. Tools: `gpu-shaders/idrk.py`, `gpu-shaders/sweep/s2g.py`.

### 1.3 `_S2G7000` group payload (one technique, all permutations)

At runtime the payload sits at **object+0x40**: every engine offset below is `payload offset + 0x40` (e.g. the dims
the decompile reads at `group+8Ch..A0h` are payload `0x4C..0x60`). Offsets inside the payload are relative to the
payload start. Validated: the table below parses **all 3,872 unpackable groups** with the permutation-entry stage
indices always inside the per-stage counts (0 failures), 36,310 records = the 36,312 DXBCs of `dxil_index.csv` minus
the 2 non-unpacking files. **[data]**

| payload off | type | meaning |
|---|---|---|
| 0x00 | char[8] | `_S2G7000` |
| 0x08 | u32 | payload size |
| 0x0C | u16,u16 | 1, 0x10 (always) |
| 0x10 | char[32] | group name, e.g. `PB2-GbiSfbTsspdVl` = technique + feature abbreviations (`Gbi`, `Sfb` = sampler feedback, `Tssp`, `Vl` = velocity, `Mshlt` = meshlet ...) |
| 0x30 | char[4] | `0000` |
| 0x34 | u32 | 0x65 (format tag) |
| 0x38 | u32 | stage-block tag 0x00010065 (VS/PS block) |
| 0x3C | u32 | **passes per permutation** (stride; `Material+12E4h` picks one) |
| 0x40..0x44 | u32[2] | per-group values (PB2: 6, 0x2008); not decoded |
| 0x48 | u32 | count of selector s0 (outermost; not used in the index) |
| 0x4C..0x60 | u32[6] | **radices of selectors s1..s6** (0 means 1) |
| 0x64 | u32 | size of the per-variant constant block (selector s6) |
| 0x68 / 0x6C | u32 | VS / PS record counts |
| 0x78..0x8C | u32[6] | offsets of sub-tables between the header and the variant blocks (sampler/aux lists; partly decoded) |
| 0x90 | u32 | offset of the per-variant constant blocks (`radix(s6)` x size(0x64)) |
| 0x94 | u32 | offset of the **permutation table** (0x14-byte entries) |
| 0x98 / 0x9C | u32 | offsets of the VS / PS record lists |
| 0xA0 | u32 | offset of the **technique property tree** (§1.5) |
| 0xB0 / 0xB4 | u32 | blob base, blob bytes (records live in `[base, base+bytes)`) |
| 0xB8, 0xBC, 0xC0 | u32 | tags 0x00020065 / 0x00040065 / 0x00080065 |
| 0xC4 / 0xC8 / 0xCC | u32 | HS / DS / GS record counts |
| 0xD0 / 0xD4 / 0xD8 | u32 | HS / DS / GS record-list offsets |
| 0xDC | u32 | tag 0x00100060 (CS) |
| 0xE0 / 0xE4 | u32 | CS count / CS record-list offset |
| 0x100.. | u32[] | offsets of the 16-byte per-record digests (one group of digests per stage) |
| 0x118, 0x11C | u32 | tags 0x00200065 / 0x00400065 |
| 0x120 / 0x124 | u32 | **MS / AS** record counts |
| 0x128 / 0x12C | u32 | MS / AS record-list offsets |
| 0x130..0x17F | | 0xFFFFFFFF padding |

The stage tags are identical in all 3,872 groups. Stage usage over the shipped set: PS in 3,825 groups, VS 2,878,
MS 130, GS 76, CS 47, HS 10, DS 10. **[data]**

**Record list** (one per stage): `{u32 offset, u32 size}` pairs relative to the blob base, contiguous in table order
(VS list, PS list, HS, DS, GS, CS, MS, AS). A record is a bare DXBC (compute groups, padded to 8) or
`"CXDK2000" | u32 dxbcSize | u32 tailSize | DXBC | tail` (graphics). The DXBC is signed DXIL SM 6.x with reflection and
ILDN debug names. The **CXDK tail** is the engine's interface metadata: input semantics, `__smp<sName>` sampler names
and the `$Globals` member names (`cSpec`, `nMtrID`, `fSFBMipBias`, `nSFBCtrlFlags` ...). It does not depend on the
DXBC bytes; the runtime reflection lookups (DrawStateBindResourceSlot 1E5854: record `s+20h`, table `+1A4h`, 0x14-byte
entries) read it. **[data]** / [dec]

**Permutation table entry (0x14 bytes)**: **[data] + [dec]**

| off | type | meaning |
|---|---|---|
| 0x00 | u16 | VS index in the VS list (0xFFFF = none) |
| 0x02 | u16 | PS index |
| 0x04 | u32 | offset of the **program block** (texture/sampler slot list, §1.4) |
| 0x08 | u16 | HS index |
| 0x0A | u16 | DS index |
| 0x0C | u16 | GS index |
| 0x0E | u16 | CS index |
| 0x10 | u16 | MS index |
| 0x12 | u16 | AS index |

Entry count = `passes(0x3C) * prod(radix s1..s6)`. Over all groups: 99,832 entries, median 4 per group, max 14,784.
Examples: GHDM (compute) has `passes = 2`, s6 radix 3 -> 6 entries `{CS = 0,1,2,1,3,1}` (the 4 CS records are the
Texture2D / SrcLevel / Array / CubeArray variants). `PB2-GbiSfbTsspdVl` has 1 pass, radices s2 = 2 and s6 = 2 -> 4
entries over 2 VS, 2 PS, 2 HS, 2 DS.

### 1.4 Program block (texture and sampler slots)

Pointed to by entry+4. Read by DrawStateBuildStageNameTable 1D23E0 **[dec]**:

```
u16 count; u16 extraSamplers;            // extra is added to Material+12ACh (total sampler count)
struct { u16 type; u16 subType; u16 slot; u16 pad; u32 nameOff; } e[count];   // name = payload+nameOff, e.g. "sNMap"
// further per-stage lists follow in the same shape (PB2: a DS list {type 3, sub 8, slot 0, "sNMap"});
// compute groups use {0x80, 0x80, 8, 8, name} entries for engine-bound inputs (GHDM: tSrc, tSrcArray, tSrcCubeArray)
```

- 1D23E0 fills `Material+128h[slot]` (16 name pointers): first the engine's base names (`sStage0..` from a static
  table, count `Material+1268h`), optional `sShadow*` names and `sJitter` (shadow-receiving passes, `Material+1238h`),
  then each block entry at its register `slot` (slots 0x40..0x7F map to `0x40 - slot`). **[dec]**
- **The `{type, subType}` pair is the join key with the G1M material textures** (§2.1). PB2 example:
  `{5,5} -> slot 2 "sOcc"`, `{3,8} -> slot 3 "sNMap"`, `{2,0x3B} -> slot 1 "sRefRough"`, and a G1M material in the same
  family lists textures `{type 1, sub 1}`, `{type 2, sub 0x3B}`, `{type 3, sub 8}`. **[data]**, join semantics [inf].

### 1.5 Technique property tree (payload `[0xA0]`) and the name hash

- A list of named feature records, each `{u32 nameHash, u32 type, u32 childCount, {u32 childHash, u32 value} x n,
  name string}`. PB2 example: `Velocity {Type=2, Flag=0, ParamPack=1}`, `GBuffer {ShaderType, DepthInMRT, MRTNum=6,
  MaterialID, ... 78 children}`, `Tessellation {PNTriangles, ShadowMapPos, ...}`, `SamplerFeedback {ConstBuffer, TexCap
  ...}`. Child hashes resolved against exe strings: Type, Flag, ParamPack, ShaderType, DepthInMRT, MRTNum, MaterialID,
  PNTriangles, ShadowMapPos, ConstBuffer, TexCap. **[data]**
- **Hash = `h = h*27 + c`** over the name bytes (seed 0). This is the same hash MaterialFindParam 3C768C uses for
  per-material parameters. Verified: GBuffer 0xA3D05FFB, Tessellation 0x7F7F6AED, SamplerFeedback 0xCD5BB6DD,
  PNTriangles 0x3594C359, Velocity 0xF0117011. **[data]**
- These are what MaterialTechniqueBind 3C77F4 asks the group for by name (`GBuffer`, `Weathering`, `ParallaxMap`,
  `PNTriangles`, `ShadowMask`, `IndexedRim`, `SSSTransmit`, `ThinSurfaceBSDF` ...). **[dec]**
- The section also holds `{hash, value}` default tables (floats such as 3.75, 32.0, 1.0). **[data]**, owner not traced.

## 2. Material definitions

### 2.1 In the model (G1M `_M1G` v9300, chunk G1MG v0045)

| G1MG section | content |
|---|---|
| 0x10002 materials | per material `{u32 0, u32 texCount, u32 1, u32 1}` then texCount x 12 bytes `{u16 texIndex (into the model's G1T), u16 layer, u16 type, u16 subType, u16 tileU, u16 tileV}` |
| 0x10003 shader params | per material `u32 count` then records `{u32 size, u32 nameLen, u32 0, u16 type, u16 count, char name[nameLen], values}`; e.g. `nMtrID` (int), `vDithAlp` (float 1.0); names match `$Globals` members in the CXDK tail |
| 0x10005 vertex layouts | 8-byte elements (stream, offset, type, semantic, layer); same shape as the vertex-declaration elements of the boot PSO list (§4.4) |
| 0x10008 submeshes | 14 u32 incl. flags (0x4000 = meshlet), vertex buffer, material index, shader-param index, index buffer |
| 0x10009 mesh groups / LODs | hashed names `@E1139944` (the same hashes appear in `.sid` files) |

**[data]** for the layouts above (sample `0xaa01dba0.g1m` and `asset-audit.md`); field names for 0x10008 follow the
public G1M layout [inf].

### 2.2 Bind tables (rdb)

- **`.mtl` MaterialGroupBindTableBinaryFile** (10,025 files): `{u32 numNames, u32 numMaterials, u32 numCloths,
  u32 numPonyTails}`, `{u32 nameHash, u32 n, u32 materialIndex[n]} x numNames`, then cloth/ponytail `{g1mId, xsiId}`.
  It groups G1M materials under name hashes for material-group swaps (`EnqPropertiesWithMaterialGroupID`). **[data]**
- **`.sid` ShaderBindTableBinaryFile** (17 files): `{u32 0, u32 version, u32 count}` then zero-separated groups of
  name hashes (contains the G1M mesh-group hashes such as 0xE1139944). Referenced as `KTGLShaderBindTableResourceHash`
  (+ `...ForSwap`, `...ForCustom`). Layout of the groups not decoded. **[data]**
- Textures: `TexContext` / `StreamingTexContext` (`.g1t`, 4,868 + 39,195). Materials stream via sampler feedback
  (`MinMip` buffers, `SamplerFeedback` feature, TextureStreamUpdate 1E7050).

### 2.3 At runtime: the Material object (KTGL shader instance)

One object per (model instance, submesh, pass). ModelResolveSubmeshMaterial 3B80C picks it: an override table at
`model+1B0h`, else three per-pass tables `model+88h` (default), `+98h` (meshlet submeshes, flag bit 14) and `+A8h`
(selected by a flag argument), indexed `variant * count(+158h or data+49h) + submeshBase(+C0h[part])`;
the object is `entry+28h -> +8`, and the param block comes from `+B0h / +D0h`. **[dec]**

| Material off | meaning |
|---|---|
| +0x20 | S2G group (payload at +0x40) |
| +0x128 | `const char* samplerName[16]` built by 1D23E0 |
| +0x11C8 / +0x11D0 | binding-set id / current **program block** pointer |
| +0x1238 / +0x123C | shadow-sampler config (bit 2 = `sJitter`) |
| +0x1264 / +0x1268 | base-sampler flags / base sampler count |
| +0x1290 / +0x1298 | current / shadow copy of the per-variant constant block |
| +0x12A0..+0x12AC | sampler counts (first shadow slot, shadow count, total) |
| +0x12B0..+0x12C8 | **permutation selectors s0..s6** |
| +0x12D0 | current permutation-table entry |
| +0x12E4 | pass index inside the entry stride |
| +0x12E8 | dirty bits: 0x10 permutation, 0x40 binding set, 0x08/0x20 constants, 0x200, 0x400 sampler layout |
| +0x1330 | named parameter list (MaterialFindParam vtbl+F8h) |

## 3. Picking the permutation

- Index (MaterialResolvePermutation* 1E1D6C, also inlined in DrawStateFlushDirty 1E0B64) **[dec]**:
  ```
  r_i = max(radix_i, 1)                         // radix_i = payload u32 [0x4C + 4*(i-1)], i = 1..6
  idx = s0*r1*r2*r3*r4*r5*r6 + s1*r2*..*r6 + s2*r3*..*r6 + s3*r4*r5*r6 + s4*r5*r6 + s5*r6 + s6
  entry = payload + u32[0x94] + (idx * passes + passIndex) * 0x14
  ```
  On a change of `entry+4` the material rebuilds its sampler table (1D23E0) and resets its texture-slot cache (MaterialResetTextureSlotCache* 1D2658).
- Selectors are set by material methods with ~190 callers each:
  - **s6 = variant**: MaterialSetVariant* 379BC / MaterialSetVariantEx* 37A3C also copy the variant's constant block
    (`payload[0x90] + s6 * payload[0x64]`, PB2: `{0, 0, 0xFEDCBA97, 0x0F / 0x2F}`) into +1290h/+1298h and set the
    permutation dirty bit.
  - **s0 by name**: MaterialSetSwitchByName* 547160 resolves a name to an index in the group (54719C) and stores it via
    8DD940.
  - The others are written by the technique/pass code (MaterialTechniqueBind 3C77F4 per draw group) from feature
    lookups such as `GBuffer`, `Weathering`, `PNTriangles`.
- The pass index (`+12E4h`) selects GBuffer / shadow / prelight style passes inside one entry stride (GHDM uses 2).

## 4. PSO description building and the cache

### 4.1 The 0x80-byte graphics PSO key (SynCtx+0x748; record = `{u32 hash, pad, key[0x80], ID3D12PipelineState*}` at SynCtx+0x740)

**[dec]** from CreateGfxPso 9290D8 and its helpers, the preload path and the state-token handlers:

| key off | bits | meaning -> D3D12 |
|---|---|---|
| 0x00 | ptr | VS shader object (bytecode `+10h` ptr / `+18h` size; `+58h` = input-signature object for the input-layout cache) |
| 0x08 | ptr | PS |
| 0x10 | ptr | HS (-> desc.HS) |
| 0x18 | ptr | DS (-> desc.DS) |
| 0x20 | ptr | GS (`+58h` = stream-output declaration, copied to desc.StreamOutput) |
| 0x28 | ptr | AS (mesh path) |
| 0x30 | ptr | MS; **non-zero selects the mesh-shader path** (pipeline-state stream, ID3D12Device2::CreatePipelineState vtbl+178h) |
| 0x38 | b0 | DepthEnable (only if DSV format != 0) |
| | b1 | DepthWriteMask ALL |
| | b2-4 | DepthFunc (cmp table) |
| | b5 | StencilEnable |
| | b6-8 / 9-11 / 12-14 / 15-17 | front Fail / DepthFail / Pass op, front Func |
| | b18-20 / 21-23 / 24-26 / 27-29 | back Fail / DepthFail / Pass op, back Func (mirrors front unless two-sided, SynCtx+480h) |
| | b30 / b31 | IBStripCutValue enable / 0xFFFFFFFF (else 0xFFFF) |
| 0x3C | b0-7 | StencilReadMask |
| | b8-15 | StencilWriteMask (forced to 0 when no stencil op writes: PsoKeyNormaliseStencilWriteMask 79E990) |
| | b16 | FillMode WIREFRAME (else SOLID) |
| | b17-18 | CullMode (cull table) |
| | b19 / b20 | MultisampleEnable / AntialiasedLineEnable |
| | b21-23 | PrimitiveTopologyType (set from the draw topology by SynFlushPsoAndBindings) |
| | b24-25 | sample count index (1/2/4/8) and quality from GfxDevice+5478h |
| | b26-29 | NumRenderTargets |
| | b30 | AlphaToCoverageEnable |
| | b31 | ConservativeRaster ON |
| 0x40 | b0-25 | DepthBias (sign-extended 26 bits) |
| 0x44 | 4 bits/RT | RenderTargetWriteMask per RT |
| 0x48 | u8 | BlendEnable mask per RT (IndependentBlendEnable is always TRUE) |
| 0x49 | u8 | DSVFormat (DXGI) |
| 0x4C..0x53 | u8[8] | RTVFormats (DXGI; from the KTGL format table 3BEA40C) |
| 0x54..0x73 | u32[8] | per-RT blend: b0-3 BlendOp, b4-7 BlendOpAlpha, b8-11 SrcBlend, b12-15 SrcBlendAlpha, b16-19 DestBlend, b20-23 DestBlendAlpha (default 0x1100 = ONE/ZERO ADD) |
| 0x74 | f32 | SlopeScaledDepthBias |
| 0x78 | u32 | per-element enable mask passed to the input-layout builder (32 flags) |
| 0x7C | u32 | vertex-declaration id (list GfxDevice+4750h, node `+18h` id, lock +4760h); 0 = no input layout |

Fixed by CreateGfxPso: `pRootSignature = GfxDevice+6E8h` (**one graphics root signature for every classic PSO**; the
mesh stream uses the same), DepthBiasClamp 0, DepthClipEnable 1, FrontCounterClockwise 1, ForcedSampleCount 0,
LogicOp NOOP disabled, SampleMask/NodeMask/CachedPSO zero (the 0x288-byte desc is memset), Flags 0.

Enum tables (read from `.rdata`, KTGL index -> D3D12 value) **[data]**:
- blend factor 3FD28C8: ZERO, ONE, SRC_COLOR, INV_SRC_COLOR, SRC_ALPHA, INV_SRC_ALPHA, DEST_COLOR, INV_DEST_COLOR,
  DEST_ALPHA, INV_DEST_ALPHA, SRC_ALPHA_SAT; alpha variant 3FD28F8 maps the colour factors to their alpha twins.
- blend op 3C49BE0: ADD, SUBTRACT, REV_SUBTRACT, MIN, MAX. Compare 3BEA100: NEVER..ALWAYS (index 0..7).
- stencil op 3C49BF8: KEEP, ZERO, REPLACE, INCR_SAT, DECR_SAT, INVERT, (6 -> KEEP). Cull 3FD2924: {0, NONE, BACK, FRONT}.
- sample count 3BEA250: 1, 2, 4. Topology type by KTGL topology 3C49C20: POINT, LINE, LINE, TRIANGLE x3, -, -, PATCH.

Who writes the key (dispatcher-thread token handlers, all set `SynCtx+B24h |= 20h` = PSO dirty) **[dec]**:
SynTokSetPipelineLayout 1164C4 / SynSetShaderProgram 115484 (stage objects, +748h), SynTokSetFlag16F 45B208
(= key DepthEnable bit 0), SynTokFlushDrawsIfStateChangedB 45E5E0 (= DepthWrite bit 1), SynTokFlushDrawsIfStateChangedC
797384 (= StencilEnable bit 5), SynOMSetStencilRef 696DA8 (func, read mask, OMSetStencilRef), SynCmdSetStencilOps
79E6B8, SynTokFlushDrawsIfStateChanged 45D020 (= per-RT BlendEnable bit in +48h; these four names belong to the
backend block, the bit meanings are from this package), SynUpdatePassRtFormatState 45A654
(NumRT, RTV/DSV formats, per-RT blend desc and write mask from the bound render-target set), the render-target bind
paths 4587AC / 458A50 (depth bias +40h/+74h), SynUpdateRtWriteMaskDirty 11A218, and the topology switch in
SynFlushPsoAndBindings 118604 / SynFlushPsoAndTopology 118758.

### 4.2 Input layouts

CreateGfxPso calls InputLayoutCacheGetOrCreate 9295AC(`VS+58h`, &desc.InputLayout, vertexDecl(key+7Ch), enable
flags(key+78h)): a per-VS-signature list of `{next, decl, ...}` nodes under a lock; a miss builds the layout from the
declaration (929AF0) and 929644 fills the desc. So input layouts = VS input signature x vertex declaration x element
mask. **[dec]**

### 4.3 Cache and lookup

- PsoGetOrCreateGfx 119670: `hash = PsoKeyHash(key, 0x80)` (XOR of dwords, or CRC32C when `SynCtx+2490h`), lock
  `[SynCtx+818h]+10h`, B-tree find (PsoCacheFindGfx 11976C -> PsoCacheBTreeFind 119820: u32 hash, then 128-byte memcmp),
  on a miss grow if full (PsoCacheGrow 1899A54) and **create synchronously under the lock** (CreateGfxPso), insert
  (PsoCacheInsertGfx 9B05BC). NULL -> the draw is skipped and the dirty bit stays (`stutter-pso.md` §2). **[dec]**
- Compute: key = the 8-byte CS object pointer; per-context cache SynCtx+128h (PsoGetOrCreateCompute 119A48) and the
  shared one at SynCtx+7D8h (PsoGetOrCreateComputeShared 58C000); CreateComputePso 58C2B0. **[dec]**
- No ID3D12PipelineLibrary, no CachedPSO, no disk cache in the game (the mod's PL / PJ / PJH add them).

### 4.4 Boot PSO list (resource 0x17A4, ~17.6k PSOs)

PreloadPsoBootLoad 1101B90 -> PsoListLoad 3483E04. Format **[dec]**:

```
header (0x18): u32 version(=0), u32 nGroups, u32 nDecls, u32 nDeclElems, u32 nGfx, u32 nCompute
body:  PsoKey gfx[nGfx]            // 0x80 each, shader slots 0..6 hold {u32 groupKtid, u32 stageIndex} instead of pointers
       u64 compute[nCompute]       // {groupKtid, csIndex}
       u64 declElem[nDeclElems]    // 8-byte vertex elements (G1M 0x10005 shape)
       u32 declElemCount[nDecls]   // elements per vertex declaration
       u32 groupKtid[nGroups]      // shader groups to load first
```

PsoListResolveGfx 34837DC turns each `{ktid, index}` into a stage object: group 0x56CB1360 is the engine built-in
group (3483750), any other is looked up among the loaded groups (348370C) and the per-stage getters ShaderGroupGetVS
303465C / PS 303464C / HS 303462C / DS 303460C / GS 303461C / AS 30345EC / MS 303463C (all -> 3033124). MS/AS slots
are skipped when `GfxDevice+6138h` (mesh shaders) is 0. Then PsoPreloadGfx 2EFE2A8 / PsoPreloadGfxCreate 2EFE440 /
PsoPreloadComputeCreate 2EFE1CC create outside the lock (`boot-shader-compile.md`).

## 5. How a draw binds material constants and resources

Main thread (records KTGL tokens into `KtglContext+3C40h`):
1. Constants: per-draw material constants are written by ModelDrawSubmeshApplyMaterialScales 5103C (`vDifScl`,
   `vSpcScl`, `vAmbScl`, `detailUVScale` looked up by name, GpuBufferMap/Unmap) and the technique binders
   (MaterialBindS2FAcclParams 3C4B74, MaterialBindPb2AmbientScaleParams 3C5914, MaterialBindReflectionProbeLightingParams
   3C3F7C, MaterialResetPb2SpecialParams 3C4598, DrawBindMaterialParams 1E6664). ShaderConstFlush 1DF880 ->
   ShaderConstWriteStream 1D7340 emits tokens 0x2B / 0x2C / 0x37 / 0x38 with the constant bytes inline.
2. Textures: GfxBindTextureToShaderSlot 1E5CB8 / DrawStateBindResourceSlot 1E5854 find the register through the
   CXDK reflection (`s+20h`, table `+1A4h`) and bind the view; the material's named texture params (`AlbedoRetouch`,
   `Illustration` ...) go through the param node vtbl+58h / material vtbl+170h (MaterialTechniqueBind 3C77F4).
   Meshlet / G-buffer `Mshlt` variants are bindless (`tBindlessTexture2D` + `sBindlessSamplerState`, t0 space3
   unbounded, `gpu-shader-sweep.md` §4); the slot-bound variants use `sXxx` + `__smpsXxx` pairs.
3. Program: DrawStateFlushDirty 1E0B64 (permutation, sampler table), DrawStateCommit 1DAF6C, StageBindingsApply2578
   1D69CC (token 0x29 with the new program), then DrawSubmit 1D2F60 (token 0x6A, or the meshlet path 1D2AAC).

Dispatcher thread (SynCtx, `backend.md` owns the details):
4. SynTokSetPipelineLayout / SynSetShaderProgram store the stage objects in the PSO key and grow the per-stage table
   sizes; SynBindTextureSlot 115B9C transitions + copies each SRV into the staging table.
5. Per draw SynFlushGfxStateForDraw 1180D0 -> SynFlushPsoAndBindings 118604: SynFlushCbvTables 118DA8 ->
   SynUploadCbvTable 118E28 (CreateConstantBufferView per dirty stage, ShaderVisibleDescRingAlloc, CopyDescriptorsSimple,
   SetGraphicsRootDescriptorTable), SRV / sampler table flushes, topology, then PsoGetOrCreateGfx + SetPipelineState.

## 6. Call graph: "draw mesh M with material X" -> bound PSO + resources

```
SceneRenderView / render nodes
 └ DrawListWalk 39E20                       group draw items by material (+18h)
    ├ MaterialTechniqueBind 3C77F4          feature lookups in the S2G property tree, texture params, selectors
    │   └ MaterialSetVariant* 379BC / MaterialSetSwitchByName* 547160 ...
    ├ ModelResolveSubmeshMaterial 3B80C      pick the Material per model / LOD / submesh / pass table
    ├ ModelDrawSubmeshApplyMaterialScales 5103C   per-draw constants
    ├ ShaderConstFlush 1DF880 -> DrawStateFlushDirty 1E0B64
    │   ├ MaterialResolvePermutation* (inline / 1E1D6C)  selectors -> 0x14-byte entry -> stage indices + program block
    │   ├ DrawStateBuildStageNameTable 1D23E0, MaterialResetTextureSlotCache* 1D2658
    │   └ DrawStateBindResourceSlot 1E5854 / GfxBindTextureToShaderSlot 1E5CB8
    ├ ShaderConstWriteStream 1D7340          tokens 2B/2C/37/38
    ├ StageBindingsApply2578 1D69CC          token 29 (program)
    └ DrawSubmit 1D2F60                      token 6A (or MeshletDrawRecord 1D2AAC)
          ... KtglCmdStreamFlush -> SynergisticDispatcher ...
 SynTokSetPipelineLayout 1164C4 / SynSetShaderProgram 115484 / state tokens (§4.1)   -> PSO key @ SynCtx+748h
 SynFlushGfxStateForDraw 1180D0
  └ SynFlushPsoAndBindings 118604
     ├ SynFlushCbvTables 118DA8 -> SynUploadCbvTable 118E28
     ├ SRV / sampler / table flushes (118C10, 118BA0, 1188AC)
     └ PsoGetOrCreateGfx 119670 -> PsoKeyHash 1199A4, PsoCacheFindGfx 11976C
          └ miss: CreateGfxPso 9290D8
               ├ PsoDescFillShaderBytecode 929A44 | PsoStreamFillMeshShaders 2EEBC98
               ├ PsoDescFillBlendState 92992C, PsoDescFillRasterizerState 9298B8, PsoDescFillDepthStencilState 9297B0
               ├ InputLayoutCacheGetOrCreate 9295AC
               └ ID3D12Device::CreateGraphicsPipelineState (vtbl+50h) | ID3D12Device2::CreatePipelineState (vtbl+178h)
        -> ID3D12GraphicsCommandList::SetPipelineState (vtbl+C8h)
```

## 7. What a new renderer needs from this layer to reuse the game's shaders and materials unchanged

Minimal interface, in dependency order:

1. **Shader-group loader**: read `shader_dx12\0x%08x.file` (IDRK + `_S2G7000`), apply the 14-entry alias table and the
   `PB2Unknown.file` fallback, expose per stage (VS PS HS DS GS CS MS AS) the record list -> DXBC bytes plus the CXDK
   tail (cbuffer member names/offsets, sampler names, input semantics).
2. **Permutation resolver**: `(group, s0..s6, passIndex) -> entry` with the formula of §3, returning the 8 stage
   indices and the program block. The caller must provide the selector values the game would set (variant s6, named
   switches, technique features such as GBuffer / Tessellation / PNTriangles). Copy the s6 constant block.
3. **Root signatures**: the game's graphics root signature (GfxDevice+6E8h) and its compute ones, byte-identical. The
   shaders are compiled against them (bindless space3 tables, per-stage CBV/SRV/sampler tables). The mod's PSO journal
   already captures the 17 serialized blobs (`RSIG` records in `psojournal.cpp`); reuse those.
4. **PSO key -> desc translation**: the 0x80-byte key of §4.1 with the enum tables, IndependentBlend always on,
   FrontCounterClockwise = TRUE, DepthClip on; mesh path through a pipeline-state stream. A renderer that reproduces
   the key can also replay the boot list (§4.4) to prewarm the same ~17.6k PSOs.
5. **Input layouts**: vertex declarations (8-byte G1M 0x10005 elements) matched against the VS input signature with
   the 32-bit element mask (key+78h).
6. **Material binding**: G1M 0x10002 textures joined to program-block slots by `{type, subType}`; G1M 0x10003 named
   params and the engine's per-draw scalars written into `$Globals` by name (CXDK tail); bindless variants need the
   texture-index scheme and the sampler-feedback `MinMip` buffers.
7. **Engine-provided inputs** each technique expects: `sStage*`, `sShadow*`, `sJitter`, the per-view / per-frame
   constant buffers, G-buffer layout (`GBuffer.MRTNum = 6`, `DepthInMRT`), and render-target formats that match the
   compiled `SV_Target` outputs.

Not needed to reuse the shaders: the KTGL token stream, the B-tree caches, the dispatcher thread.

## 8. What the GPU gets, dependencies, verdict

- **Per draw on the GPU** (checked against FCAP1 in the Capture-check section at the end): one SetPipelineState only when the 0x80-byte key
  changed (dirty bit 0x20), the graphics root signature from GfxDevice+6E8h (set by the backend), CBV tables per dirty
  stage (a fresh CBV per draw: SynUploadCbvTable), SRV / sampler tables, topology. FCAP1 "PSO sets" and "top PSO shader
  sets" per marker should show (a) few distinct root signatures, (b) PSO sets far below draws inside G-buffer passes
  (draws sorted by material in DrawListWalk), (c) the shader digests of `dxil_index.csv`.
- **Depends on:** RES (shader_dx12 / G1T / G1M loading, KTGL format -> DXGI table 3BEA40C), DRAW (draw lists, per-draw
  commit), BACKEND (token replay, root signature creation, descriptor rings), object database (per-pass G1S KTIDs).
- **Keep / replace for a new renderer:** keep the shader files, the S2G permutation scheme, the PSO key and the root
  signatures byte-for-byte (they are the contract the DXIL was compiled against); replace the B-tree caches, the
  synchronous create under the lock and the per-draw CBV creation (§7 is the minimal interface to keep).

## 9. Proposed names for functions other packages own

| RVA | owner (partition) | proposed name | evidence |
|---|---|---|---|
| 1E1D6C | BACKEND (region fill; really material code) | MaterialResolvePermutation | §3 formula, 311 callers, calls 1D23E0 / 1D2658 |
| 1D2658 | DRAW | MaterialResetTextureSlotCache | resets the 15x8 slot cache to 0x80010, vtbl+180h |
| 379BC / 37A3C | BACKEND (region fill) | MaterialSetVariant / MaterialSetVariantEx | write s6 (+12C8h), copy the variant block |
| 547160 / 54719C | VOLNAT (region fill) | MaterialSetSwitchByName / ShaderGroupFindSwitchByName | s0 by name |
| 45B208, 45E5E0, 797384, 45D020 | BACKEND | the existing names are generic; they set PsoKey depth enable (+38h b0), depth write (b1), stencil enable (b5), per-RT blend enable (+48h) | §4.1 |

## 10. Open

- Decode the remaining S2G sub-tables (payload 0x78..0x8C offsets, 0x40/0x44 values) and the per-record 16-byte digests.
- Decode the `.sid` group layout and the exact object-database path from a model to its per-pass G1S KTIDs.
- Name the s1..s5 selector setters (they are virtual methods on the Material; ~190 callers each).
- 0x10003 parameter `type` codes (5 = int, 1 = float seen) and how the engine maps them to the cbuffer at draw time.

## Capture-check (package 12, 2026-10-10; table in 12-capture-check.md section 5)

The three checks listed in section 8 against FCAP1 (pc2, Ally, PC):
- (a) **few distinct root signatures: confirmed.** One engine graphics root signature (set once per list: 32-35 `SetGraphicsRootSignature` per frame pair), plus 2-4 sets from foreign modules (FFX / present layers); one compute root signature on pc2 and the PC, three on the Ally (two are FFX-owned).
- (b) **PSO sets far below draws in G-buffer passes: confirmed.** G-buffer phase 1 is one list: pc2 537 draws + 156 ExecuteIndirect -> 195 PSO sets; PC 675 + 101 -> 185. Whole frame ~0.5 PSO sets per launch (Ally ~1.3, with 1039 dispatches in one list).
- (c) **shader digests: confirmed.** The vs / ps / cs ids in the PSO sets are MD5s and the 8-hex ids quoted in post.md, lighting.md and render-architecture match their prefixes.
- SetPipelineState callers: 118737 (graphics, inside SynFlushPsoAndBindings 118604, 1235 per frame pair), 58BFEF (compute, 371), 119C95 (6). SetGraphicsRootDescriptorTable: 119094, 118D68, 118B4D (~2.8 table sets per draw, i.e. a fresh CBV table plus SRV / sampler tables per draw as in section 5).
