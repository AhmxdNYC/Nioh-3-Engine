# Optional VRS menu setting (70620)

Base: origin/cx-pc/vrs-selective-v4 at bfaab996 (EF f605afb8). No Fight now, BossRematch or Training edits.

One row: **World shading optimization (VRS)**, under the GPU settings / Extra performance pages (and the look-changing flat page). Defaults off. Changes apply **at the next launch**, because PSO classification and command-list hooks are selected at startup. The row says next launch; close-menu autosave and Save to ini use existing persistence. Remote key `VRS` uses the same available/change path.

On writes [Perf] VrsLayouts=7, VrsRate=0, VrsKeepCharacters=1, VrsSkipSkinned=1, VrsGBufferMotion1x1=0, then VrsCeiling=3. Off writes VrsCeiling=0. Merely opening/saving another setting preserves existing VRS experiments until this row is changed. The bundled INI already defaults VrsCeiling=0 and is unchanged.

Availability is queried on the actual presenting queue's D3D12 device during overlay setup, including when VRS is off: CheckFeatureSupport(OPTIONS6), Tier >= 1. Unknown renderer or failed/unsupported query displays unavailable and rejects activation. No adapter IDs, GPU lists or dummy-device inference. Existing per-command-list runtime validation remains authoritative, so unavailable command-list interfaces still fail closed. No render-thread memory scans or device creation were added.

Validation: build.bat includes vrs_menu_test (query failure, unsupported tier, Tier1/Tier2, exact six-key enable preset, enabled-to-off and fresh-off persistence), plus existing shader classification, command-list state, captured-draw replay and HUD suites. Full production compilation validates menu integration. See build log uploaded with artifact for final outcome.

Live checks belong to the lead: capture the new row, toggle on, close/save and restart, verify all six keys and runtime mode 3; toggle off/restart and verify mode 0. Confirm unsupported hardware displays unavailable. This worker did not launch the game or produce an in-game screenshot. No new FPS claim is made.
