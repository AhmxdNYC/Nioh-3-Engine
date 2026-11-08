# Player tab and page defaults (#83530)

Development candidate for 1.8.12 atop favorites b4c5ab4c. No game installation or publication.

Player settings are a separate top-level hosted tab. OpenPlayer/ResumePlayer append bridge actions 14/15 without changing existing action numbers or ABI layout. Player tab remembers its page/selection independently of Boss Matches and Training, and does not change the selected game mode. Standalone BR also exposes Player at its root. Player and Soul core tuning links move out of the Training hub; Soul core tuning is nested under Player. Existing gameplay scope of individual controls is unchanged and retained in help text.

Reset to defaults is a page action, not a global wipe. Editable settings pages get one action; pages already owning a reset retain theirs. Settings use shipped INI values (or shipped code fallback when a key is omitted), not user startup values. Filters, favorites, saves, catalogue/manual path rows, travel/launch/refill actions and unrelated pages are preserved. Live setters and existing persistence/restart semantics are reused. EF changes save on menu close as before; BR startup preferences save asynchronously and apply at restart. Session-only boss controls remain session-only.

BR covers generic configuration groups, Player, Soul core tuning, fight rules (including mixed Rematch/gate pages), boss health/Ki, boss behavior, move groups/chains, Training freeze and stance timer. Player reset restores shipped infinite-health ON and per-core transformations ON; it preserves learned/equipped identities and the current press's decision. Soul defaults match shipped reveal100ms, forward100, hide400ms, speed1.4, tail4; the old display comparison defaults were inconsistent and are corrected. Soul speed hook absence saves preferences without claiming live activation.

Offline validation: BR release build and real menu/provider harnesses PASS; page dispatch63, shipped default cross-checks29, soul timing346, core picker134, favorites32. Existing equipment, first-use, primary ghost, vertical camera, face-list and rcore suites PASS. EF validation/build details and artifact hashes recorded below after final build.

Live validation remains for the fleet after the current release gates: tab traversal/resume, a changed value then page reset, persistence after restart, and visual checks. This candidate is not a replacement for 1.8.11 gate assets.

EF release build, both menu harnesses, defaults/Player regression and tabbar PASS; 367 shipped defaults checked for freshness. Preview sent to Discord using the actual offline renderer. BR SHA256 837cacb135d271e9e76ac8b8546254a36ccdbaffd1e016efb3aaf58feaf57817; EF SHA256 769c41ac2c380aa40b3727e7a2594dd6f745596a10830037bea76fe02ba9c290.
