# Codex rendering progress: takeover to 2026-10-11

Snapshot: takeover brief dated 2026-10-10 17:10 through the 2026-10-11 coverage audit. This report distinguishes generated decompilation, function naming, tested fixes, and unresolved work.

## How much rendering is decompiled?

Every function in the current renderer catalog has a generated C export: **54,295 / 54,295**, including all **5,932 core functions**. Each has an `ok` index record and a corresponding file under `D:/nioh3-re/decomp`. Generated C is analysis material; it does not establish a correct, rebuildable renderer or complete semantic understanding. The catalog includes neighbouring code whose renderer membership is unverified.

| Coverage measure | At takeover | Now | Change |
|---|---:|---:|---:|
| Core functions with real names | 2,382 / 5,932 (40.2%) | 2,382 / 5,932 (40.2%) | 0 |
| Core code bytes with names | 1,131,316 / 1,976,582 (57.24%) | Same | 0 |
| Broader renderer code bytes with names | 2,753,547 / 13,294,972 (20.71%) | Same | 0 |
| Broader renderer functions with names | 8,331 / 54,295 (15.34%) | Same | 0 |
| Generated C exports present and indexed `ok` | Historical count not independently established | 54,295 / 54,295 (100%) | Unknown |

The symbol database and partition have no changes since baseline `996ee73`. Codex has expanded ordering/lifetime research in notes rather than merged new function names. **3,550 core functions remain unnamed**, including 1,014 meshlet, 518 volumetric, 440 lighting, and 437 post functions.

The README chart's “understood” measure checks names, not semantic proof: 227 counted names have low confidence and 5,085 cite the renderer autonamer. Older stricter coverage figures in `00-map.md` use different rules. Four timeout entries in the export index lie outside the current renderer partition; an older 54,294-target log should not replace the current 54,295-function denominator.

Evidence: `re/scripts_re/renderer_progress.py --check`, `re/scripts_renderer/progress_chart.py`, `re/scripts_renderer/partition.tsv`, `re/symbols/symbols.tsv`, `docs/progress/renderer_table.md`, and external `D:/nioh3-re/decomp/index.tsv`. The audit used scripted aggregates and existing exports; no Ghidra or game launch.

## What changed during Codex's takeover?

| Work | Before | Current result |
|---|---|---|
| Production integration | Constant-upload fix and Extra Performance on separate branches | Merged/build **1.7.61**; 255 pattern sites and all 36 full-release harnesses passed. CBN/CBT on; CBF, FO and visual presets off by default. |
| PC validation | Merged build untested | PC smoke PASS: constant-upload **+0.55% ±0.46% SE**, neutral; opt-in Performance preset **+3.14% ±0.08% SE**. Combined smoke: 3,179 frames PASS. Saves/settings restored and hashes verified. |
| Experimental frame overlap | Inherited pc2 fault; drafted fo10 untested | Reproduced fo10 fault/hang; corrected effect-release transition in isolated fo11. PC transition stress PASS, 6,299 advancing frames. Emulator A/B **+11.74% ±1.54% SE**, not a native-device gain. Shutdown-only watchdog retained as a diagnostic. |
| FO safety audit | Important lifecycle/bounds gaps untested | 11 negative controls reproduced overflow, bounds and late-producer hazards. Fail-closed startup guard implemented separately; 18 actual-code startup cases and 255 patterns passed. **FO remains NO-GO and unmerged.** |
| Rendering research | Lifetime/dispatcher ordering incomplete | Native emitter, locks, retirement, reference ownership, dispatcher drain, cursor reset and ECL close/recycle paths traced. Provenance checks validate those traces; producer closure, complete GPU lifetime and retirement restart remain open. |
| Tooling and continuation | Tour reporting failed; Claude paused | Fixed tour reporter diagnostics handling. Installed separate serial Codex continuation and phone chat; removed unrequested daily caps. Claude auto-wake stays off; no further Claude calls. |
| Training mode | Desired arena switching and enemy/boss summoning | Inventory and gated prototype plan prepared using BossRematch. No training spawner implemented; actor/asset/reset/save contracts still require proof. |

The inherited **pc2 +15.9%** constant-upload result predates Codex and came from a slow PCIe chipset link. It is not a new measurement or a normal x16-PC claim. Presets can change the picture and remain opt-in.

Evidence: `notes/handoff-codex-step1.md`, `notes/handoff-codex-step2.md`, `notes/handoff-codex-step2b.md`, `notes/handoff-codex-fo98-audit.md`, `notes/handoff-codex-fo-startup.md`, `notes/handoff-codex-tour-report.md`, `notes/training-mode-inventory.md`, and the dated FO98 handoffs.

## Open-source references and next work

The user clarified the new public repository: **AhmxdNYC/Nioh-3-Engine**, staged at `C:/Users/Ahmad Hamza/nioh3-public`, main `76d85ac`. It is a curated open-source copy of our mod, symbols, selected research and chart tooling, with a published EngineFix1.7.61 snapshot. It is separate from the private engine repository and does not add original game source or increase game decompilation coverage. `notes/handoff-claude-public-repo.md` is the publication source of truth; it stays private. Public `docs/BENCHMARKS.md` confirms the smoke/emulator numbers and their limits above.

Other local references found: `D:/refs/DirectX-Specs`, `vkd3d-proton`, `dxvk`, `dxil-spirv`, `safetyhook`, plus `data/RDBExplorer`. These are third-party implementation references/tools.

Two parallel follow-up audits checked allocator lifetime requirements and live patch failure handling: `notes/handoff-codex-parallel-safety.md`. In particular, pinned vkd3d-proton `command.c:2939` can return successful allocator Reset while submissions remain pending; that return alone cannot prove lifetime safety. The production ApplyBatch also continues to byte writes after failed thread inspection. Its isolated failure-policy repair/proof is the next source unit; no fix or crash attribution is claimed by the audit. Compare actual game ownership/fence evidence with both native D3D12 and Proton contracts before promoting FO.

The user resumed Claude sessions on PC,pc2,Ally for device testing. Replies1824-1826 accept Codex source/build ownership and separate device test roles. **pc2 #1767** and **Ally #1768** main smokes are claimed and running; no final results yet. Obsolete native FO5 **#1781 / kit225** is explicitly held/cancelled until a proven replacement. Codex's candidate sign-off stays held until required device passes; the existing curated public release is a separate publication. PC waits for the Ally shared-account run. Next measured gap: stock game versus the whole default mod. Parallel workers use static analysis only. Ghidra remains limited to one instance, at most 4 GB, and none while the game runs.

Public publication excludes shader `.file` containers, binaries/dumps from the game, extracted text, captures/saves, private MODLOG/STATUS/handoffs and bridge/phone/Discord tooling. Built ASIs require PDB-path scrubbing before upload; public author identity is `AhmxdNYC <104233015+AhmxdNYC@users.noreply.github.com>`. This report is private and must not be copied to the public repository.
