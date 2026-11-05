# VRS v4 character protection — note 69989

This addendum supersedes the default character behavior of EF 1bddf256 (bridge 5216). The source stays on cx-pc/vrs-selective-v4. No game access or installation by this worker.

## Exact controls
All keys are [Perf], restart per arm:

```ini
VrsCeiling=3
VrsLayouts=7
VrsRate=0
VrsKeepCharacters=1
VrsSkipSkinned=1
VrsGBufferMotion1x1=0
```

VrsKeepCharacters defaults to 1. It adds a conservative full-rate veto beyond explicit skin semantics: direct draw calls, nonempty or unreadable input layouts, explicit skin inputs, and unknown vertex-shader families stay at 1x1. Coarse shading requires an indirect call, a readable empty IA layout, and an exact full-container digest match to one of three previously analyzed original world meshlet VS families:

- 1559714ca496fffe6ddd3c827e1d962e — PB2 meshlet G-buffer.
- 70d0ee9feee590f9c48c40b808858e27 — PB2 meshlet G-buffer.
- f311c8e29e9822ee8c34d38aba8b5966 — PB2TREE2 meshlet G-buffer.

Provenance: notes/asset-audit.md (world meshlets), notes/material-pb2-next-opportunities.md (full original hashes and shader analysis), notes/gpu-rewrite-round3.md (meshlet vs IA-skinned paths), gpu-shaders/meshvs/out/msv_build_hot.csv (original family IDs), and the local shader index's PB2/PB2TREE2 Mshltv groups. Matching is full 128-bit container identity, never a short prefix or guessed resource-name substring. Replacements/new families stay fine until reviewed. Existing PSO replacement-enabled invalidation remains. Other RT/blend/motion/sample/binding exclusions still apply to admitted world draws.

This protects ordinary compute-skinned vertex buffers even when BLENDINDICES is gone. Unknown vertex-pulling shaders also stay fine. The tradeoff is intentional over-exclusion of non-character draws, including other world families. This is a conservative candidate policy, not an in-game proof that every visible character pass is covered. Captured v3 histogram rows have no per-draw VS/IA evidence and cannot predict this policy's eligible fraction.

VrsKeepCharacters=0 restores the prior layout policy. VrsSkipSkinned=1 still independently protects explicit per-vertex BLENDWEIGHT(S)/BLENDINDICES; set both to 0 only to reproduce the earlier fully relaxed arm. VrsKeepCharacters=1 protects explicit skin even if VrsSkipSkinned=0. The new option applies to selective mode 3; diagnostic whole-frame ceiling modes 1/2 remain unchanged.

## Diagnostics and validation
Metadata version 4; rejection bit 0x800 means character-protection policy. Surface sample records include worldVs. Coverage prints keepCharacters, and signature reports characterRejected. That counter counts this policy's vetoes (including unknown/world over-exclusion), NOT identified character draws. Existing skinRejected remains positive-semantic evidence only.

Production-hook tests cover coarse indirect world -> full-rate direct, full-rate ordinary/compute-skinned IA, full-rate unknown pulling family, explicit skin regardless of the narrower toggle, and OFF restoration. Policy tests check exact hash versus one-bit mismatch, readable empty IA requirement, and direct-vs-indirect behavior. Existing layout/rate/shader/state/captured-replay tests remain.

## Required lead comparison
Use the SAME device, scene, layout mask and VrsRate for ON/OFF comparison; keep VrsSkipSkinned=1 for both arms. Compare VrsKeepCharacters=0 vs 1, plus VrsCeiling=0 as the original baseline if needed. Capture both-arm images of player cloth, face/hair and emissive material, along with characterRejected and worldVs sample records. Check gameplay motion, not just a stationary screenshot.

FPS OFF vs ON: NOT MEASURED by this offline worker. The suggested 1–2 FPS cost is a hypothesis, not evidence; conservative unknown-family exclusion can cost more. Lead owns the game and must measure/report the actual per-device delta. Main-PC supplied installed snapshot: EF 85ebffdc / TR 0e30328a / BR 8eae1606 / Cam 7482c03d.

Opus read-only review cross-checked gpu-shaders/post/out/runtime_inventory.csv: all three admitted originals are meshlet VS with no reflected bSkin field. The similar meshlet VS e3d06c933adc6a26556287ed3edb7a62 DOES contain bSkin and is explicitly rejected in the harness. Meshlet names/resource names alone are not an allowlist. ExecuteBundle (slot27) is not treated as ExecuteIndirect (slot59); existing bundle-state invalidation also remains. Rigid attachments through an admitted world path remain a live-visual-check limitation; no universal character identity claim.
