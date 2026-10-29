# Save-gate delivery 41493

Code: 3f68800f (stage1 2e8a49c5), branch cx-pc/save-gate-41493.
EF: bridge3073 SHA256 d84bf5a28e7b48e9907b7b4cb406a4b5e5410d99b28510e2b4ba63063f611f35.
Helper/test bundle: bridge3071, tools/saves/dist/SaveGate-41493.zip.
Runbook: tools/saves/README-41493.md. Defaults Enabled=0 RealRoot=0; staged native acceptance belongs to pc. No game/installed mod/save access by cx-pc.
Release PASS (existing nt-shadow warning). Offline: 106 core cases, 40 native action/SEH checks including changed state before final dispatch, both menu suites including cancellation outside title, 74 Python tests. Package CLI and synthetic setup run successfully.
Queue design per pc41536/41557: title gate only for new action/dispatch; pending request survives later menu/gameplay states; explicit Cancel and persistent queued status. Helper rejects another PID/nonzero exit, then verifies backup and switches after clean exit. Real-root adapter remains opt-in and requires human approval before installed-save testing. Whole-root backups and dated old trees are retained; interrupted transaction requires recovery. No speculative Quit-state allow path remains.
Two actual-menu offline renders (normal + queued/Cancel) sent to Discord. Initial notify.py -I import failed (media module); retry without -I succeeded twice.
Next pc: merge sandbox test INI into preserved test configuration, use bridge3073 with helper3071, confirm main-menu-only admission and sandbox queue/cancel/clean-exit. Then adapter on copied root before Ally; no actual saves until approval.
