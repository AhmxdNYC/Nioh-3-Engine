# Nioh 3 engine research and game fixes

Private development repository for Nioh3EngineFix, Boss Rematch and renderer research on Steam Nioh 3 v2.02. Default engine fixes keep the picture identical. Optional Extra Performance presets change rendering quality and require explicit opt-in.

The curated public source repository is [AhmxdNYC/Nioh-3-Engine](https://github.com/AhmxdNYC/Nioh-3-Engine). Game binaries, dumps, decompiled game code, shader containers, saves, captures and private coordination notes stay out of public releases. This project does not contain Team Ninja's original renderer source.

The tested baseline is **1.7.61**, with CBN=1, CBT=1, CBS=0, CBF=0, FrameOverlap=0, Extra Performance preset=0, ShaderSwap=0 and ShaderHot=0. PC, pc2 and Ally component smokes passed. A complete benchmark of this default stack against EngineFix absent is still pending. A new patch-installation safety candidate is under isolated validation; it is not the tested baseline.

## Measured component results

These are single-feature A/B measurements at fixed gameplay spots, not whole-mod gains. Values are paired percent differences with standard errors. PC and remote trials each used three adjacent pairs in one launch.

| Device | Constant-upload fix, CBMODE 0 to 6 | Optional Performance preset, XPRE 0 to 1 |
|---|---|---|
| PC | +0.55% +/-0.46% SE; neutral | +3.14% +/-0.08% SE |
| pc2 | +16.50% +/-0.16% SE | -0.44% +/-0.22% SE; neutral |
| ROG Ally X | +0.19% +/-0.19% SE; neutral | +4.16% +/-0.18% SE |

PC: Ryzen 5 7600X/RX9070, Windows. pc2: Ryzen 5 5600X/RX6650XT, Windows, with an unusually slow chipset GPU link of approximately 0.8 GB/s; its large constant-upload gain is specific to that configuration. Ally: Z1 Extreme/780M, Bazzite/Proton. No NVIDIA, Intel Arc or Intel CPU runtime result is established.

The XPRE tests used CBMODE0. The CBMODE6/XPRE1 compatibility smoke does not establish an additive gain or the preset's gain on the default stack. Optional presets can alter GI, LOD, fog or AO depending on their levels. Captured component tests do not establish complete visual, combat, input-latency or loading validation.

Evidence: [PC smoke handoff](notes/handoff-codex-step1.md), [remote archive audit and metrics](notes/handoff-codex-rc1761-evidence.md), and [external-capture alignment limits](notes/handoff-codex-rc1761-capture.md). See the [Codex progress report](notes/codex-progress-report.md) and newest section of [STATUS](STATUS.md) for current gates.

For the returning Claude agent: [Codex catch-up handoff, October 12](notes/handoff-codex-to-claude-2026-10-12.md) consolidates source work, device tests, failures, coordination workflows and evidence. Its first section states whether the current live trials have finished.

## Renderer progress

All 54,295 renderer catalog entries have local C exports. Core naming coverage is 2,382/5,932 functions (40.2%); broader named byte coverage is 20.71%. These counts describe exports and symbol naming, not fully understood or rebuilt renderer code. They are unchanged from the Codex takeover baseline. See [the rendering progress audit](notes/codex-rendering-progress-2026-10-11.md) for definitions and caveats.

Frame overlap remains experimental and **NO-GO**. The +11.74% +/-1.54% SE experiment used an artificial 8 ms replay delay; it is not a native game gain. Producer closure, resource lifetime and retirement ordering still need proof before another candidate can be shared. See [the continuation ledger](notes/codex-task-ledger.md).

## Training mode research

Three parallel research units traced enemy activation, mission/arena loading and add-on integration. Existing BossRematch provides native mission-based arena entry; arbitrary spawning, cross-stage boss placement, in-place resets and reward-free practice remain unproved. The next implementation is a default-off read-only observer. See [training progress and contracts](notes/training-mode-progress.md); no operational training mode has shipped.

## Development and validation

Read [AGENT_BRIEF](AGENT_BRIEF.md), the newest [STATUS](STATUS.md) section and the relevant handoff before editing. Codex owns source/build integration; device agents own their tests. Only one writer changes EngineFix source at a time.

EngineFix lives in [mod/Nioh3EngineFix](mod/Nioh3EngineFix). Use its build.bat and tests/test_patterns.py, then release/check_release.py --full and required native smoke gates before sharing a new build. The current pattern catalog contains 255 sites. The pinned whole-mod Windows benchmark and its recovery/ownership checks are documented in [the benchmark driver handoff](notes/pinned-default-benchmark-driver.md).

All game launches use the existing game lock and harness guard. PC and Ally launches also respect their shared Steam account. Never run Ghidra while Nioh3.exe runs. Game-side optimization only: no driver, BIOS, power, affinity or hardware tuning. Preserve and verify saves and entry ASI/INI/config before releasing a test lock.

Older investigations and withdrawn headline results are preserved in [the historical README](notes/archive/readme-before-2026-10-11.md). Their setup gains, shader figures and cross-launch observations must not be presented as current default-mod results.
