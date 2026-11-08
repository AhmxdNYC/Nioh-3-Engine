# HUD weapon slots and held soul cores — task #65330

Read GitHub issues #5 and #9 before implementation. Added two default-off settings on Display Overlays > Game HUD:
`[Menu] HideHudWeapons=0` and `HideHudSoulCores=0`. The existing catalog handles menu/controller input, remote `menu set`, loading and Save to ini.

The shared master and per-widget visibility hooks now resolve unique executable byte patterns on every game build, once at startup. Original call target and atomic patch alignment are checked; original bytes/displacement are saved and restored. Missing or ambiguous patterns are never guessed.

Constructor matches supply the widget identifiers: weapon slots 22 (constructor 1CD222C, update 2FD128 -> 2FD1B0 reads current equipment); held soul-core count 19 (constructor 1CD4A4C, update 8D2BCC -> 8D2EA4 -> F664C reads carried inventory with flag 0x400). These are UI widget identities, not weapon/core lists. Each works independently of equipped weapons or cores. Pattern header keeps semantic ID 19 to distinguish the otherwise identical money constructor. Changing that enum safely disables the soul-core row.

Validation on Main PC, offline only:
- build.bat: exit 0; existing dllmain.cpp C4456 shadow warning only. No new HUD warnings.
- Native shared-scanner test: all 4 patterns unique in recovered v2.02 image; call target and alignment correct. Missing/duplicate/truncated/wildcard checks pass.
- Native private-memory test: weapons OFF/ON, cores OFF/ON, both ON, both OFF restores original displacement; missing widget does not hide anything.
- Both real controller-menu harnesses: ALL PASSED, including all 13 per-element HUD rows exactly once.
- Generated catalog --check and git diff --check: pass.
- Build runs native HUD fixtures automatically. Set NIOH3_HUD_DUMP to a decrypted RVA-layout image for the full 4-pattern audit (set during this build).

Built ASI: `mod/Nioh3EngineFix/bin/Nioh3EngineFix.asi`
SHA256: `86dc3279c7967126defe0e5b765e7309940f40cef3bca914e931a89a89015b95`
Base: pc/report-crash-merge 43396e0c (includes report textbox and crash upload). Main PC installed public v1.8.4 remains EF 85a3f50a / BR 449db1aa / Cam 537d19ac / TR c2b54f19. Nothing installed, no game accessed.

Claude gate: install only EF; with HideGameHud=0 compare both toggles OFF, weapons ON only, cores ON only, both ON, then both OFF. Confirm only the intended widgets hide and restore. Save/reload settings, check `HUD patterns:` log (weaponsId=22 soulCoresId=19), and send OFF/ON screenshots to Discord. Check both samurai and ninja weapon display. Visual identity/behavior remains unproven until this gate passes.

Evidence: `D:/nioh3-re/hud-65330/` (build.log, menu-test.log, test.cmd, image.bin recovered read-only from Ghidra, widget-proof*.txt). No game launch, access, installation or benchmark by cx-pc.
