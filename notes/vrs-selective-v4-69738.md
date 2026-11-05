# Selective VRS v4 — task 69909 / thread 69738

## Purpose and settings
Offline candidate based on VRS v3, branch cx-pc/vrs-selective-v4. The lead reported v3 tagged 44% of calls but produced dotted cloth and speckled red emissive material on PC2. This change exposes layout isolation and gentler rates; it does not establish that those visual defects are fixed.

All settings are in Nioh3EngineFix.ini, section [Perf], and require a restart:

```ini
[Perf]
VrsCeiling = 3
VrsLayouts = 7
VrsRate = 0
VrsSkipSkinned = 1
VrsGBufferMotion1x1 = 0
```

- VrsCeiling=0 is OFF; 3 uses the selective options below. Modes 1/2 retain their existing whole-frame 2x2 / supported 4x4 behavior.
- VrsLayouts bitmask: 1 admits six targets [91,11,87,26,87,34]; 2 admits seven [91,11,87,26,87,26,34]; 4 admits eight [91,11,87,87,26,26,87,34]. Add bits to combine; default 7 admits all, 0 vetoes all. The existing verified seven-target fallback also belongs to bit 2. Invalid values revert to 7.
- VrsRate: 0=2x2 (default), 1=1x2 (one pixel wide, two tall), 2=2x1 (two wide, one tall). This applies only in selective mode. Invalid values revert to 0. Both gentler rates work at VRS Tier 1 without additional-rate support.
- VrsSkipSkinned=1 (default) forces 1x1 when the graphics PSO input layout has an explicit per-vertex BLENDWEIGHT, BLENDWEIGHTS or BLENDINDICES semantic, case-insensitive. SemanticIndex may be nonzero. Either weights or indices is sufficient to exclude conservatively. Per-instance inputs and similarly named prefixes do not classify the draw. 0 disables this exclusion.
- VrsGBufferMotion1x1=1 vetoes the entire draw with motion targets. It cannot preserve one MRT at 1x1 while coarsening the other targets. Leave 0 to compare the three captured surface layouts.

Blend/logic-operation, unsupported/unknown metadata, missing PS, sample/coverage and binding exclusions remain. Layout masking and skin exclusion add rejection bits 0x200 and 0x400 respectively. Metadata version 3 prevents stale metadata interpretation. Excluded draws and reset always receive 1x1 so coarse shading cannot leak into UI/particles.

## What skin detection proves
Existing v3 captures have no input-layout or VS identity evidence for the G-buffer. Opus read-only review found explicit skin semantics in shadow shaders in notes/gpu-rewrite-round3.md, and compute skinning documented in notes/render-architecture.md. This is enough to implement a positive semantic exclusion, not enough to claim the player is excluded. Compute-skinned vertex buffers, generic TEXCOORD packing and vertex pulling may expose no bone semantics. No guessed PSO family is blacklisted. Unknown/unreadable input metadata does not establish a static mesh and is reported as unknown; current layout eligibility otherwise remains.

Input descriptors and names are read through SafeCopy only at PSO creation; the draw path uses copied scalar metadata. Valid vertex shader container keys are retained as an opaque 128-bit diagnostic token, not a new allowlist. Surface records print skin flags (weight=1, indices=2), input-known/count and VS token. Per-layout cumulative skinCalls and skinRejected distinguish detected draws from actually rejected draws. A zero must not be interpreted as proof of a clean player image.

The existing first-200-draw capture was dominated by shadow passes. It remains, with an additional budget of 16 records per captured surface layout per recording interval (48 total), so surface diagnostics are not starved. Extra records are marked surfaceSample=1 and are samples of existing calls, not additional draw counts. Watcher thread prints them; render threads only copy scalars. Coverage prints selected layout/rate/skin settings and tagged_calls/total_calls. Signature counts are now named taggedCoarse rather than tagged2x2 because the rate can be 1x2 or 2x1.

## Validation plan and limits
Build.bat includes HUD, real DXBC/DXIL shader-policy fixtures, production selective-state hook harness and replay of the captured PC2 27-row histogram. New tests cover all 8 masks across 3 layouts, all 3 rate enum mappings, explicit/ambiguous/instance input semantics, safe-read failure, skin toggle transitions and fine shading on exclusions/reset. Captured replay has no skin metadata: mask-only eligibility is 2,808,226 calls for six-target, 1,323,341 for seven-target, and 1,274,599 for eight-target, out of 13,297,664; all three total 5,406,166 (40%). These are offline eligibility counts, not measured v4 coverage or performance.

Lead owns installation, screenshots and game testing. Use masks 1/2/4 to isolate cloth/glow damage, then select a gentler rate as needed. Confirm settings, skinRejected and surface VS records in the log. Compare OFF VrsCeiling=0 against each chosen ON arm; preserve other settings. No game process was accessed and no ASI installed by this worker.

Latest supplied main-PC installed snapshot: EF 85ebffdc / TR 0e30328a / BR 4c219306 / Cam 7482c03d. Public remains v1.8.6 (EF 1f8d71b6, TR 04781485); this candidate is not a release.

## Final artifact and validation
Source 64fd44ff; EF SHA256 1bddf2565cbed4b952fffe22187ef74410633e9beda4f12236f4c169c8654236; bridge ASI #5216, build log #5217. Full build.bat exit 0; four executables (HUD, policy with actual DXBC/DXIL, production state hooks, captured histogram replay) PASS. The DXC invalid-bitcode stderr is an intentional rejected-input fixture. No in-game validation.
