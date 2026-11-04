# Live HUD widget sweep — task #65638

Main-PC game test of EF 86dc3279 failed the original feature mapping: widget 22 hides no visible weapon silhouette; widget 19 hides the carried-core orb/count near amrita, not the two circles requested. The old two named toggles are NOT validated fixes and must not ship as such. This change adds diagnostic live raw IDs; Claude owns the sweep and final identification.

Existing vpad pipe commands (no relaunch between candidates):
```
menu set HideGameHud 0
menu set HideHudWeapons 0
menu set HideHudSoulCores 0
menu set HideHudIds none
menu set HideHudIds 7
menu set HideHudIds 8
menu set HideHudIds 7,8,255
menu get HideHudIds
menu set HideHudIds none
```
Turn off any other named HUD toggles for an isolated sweep. Comma-separated IDs have no spaces; valid IDs are 0..255. `none` clears; numeric `0` hides widget zero. Each valid list REPLACES the previous raw mask; named toggles are unchanged. Commands are session-only and do not write the ini. The existing configured HideHudIds value will load again next launch.

The pipe preserves the text, queues it through the existing serialized remote request, then applies on the Present thread before returning. Replies give all eight 32-bit mask words and redirected/off/unavailable state. Each set is logged as `HUD raw IDs -> ...`. Malformed IDs (negative, >255, junk, empty segment, trailing comma, overlong list) are rejected without changing the existing mask. Clear restores the original call if no named toggles remain active.

Offline validation:
- Native HUD test: raw single/list replacement, duplicate handling, highest ID 255, invalid input preserves mask, clear restores original displacement; 4 patterns unique in offline image.
- Both real controller-menu harnesses pass.
- Native vpad transport/parser/composer/pipe harness passes, including the new list/get/clear route.
- The wrapper's unrelated IsAppActive clobber subcheck cannot find its old hardcoded re/scripts_re/../Nioh3.dump.exe path; its other image/wiring checks pass. No game touched.

Next: Claude installs this diagnostic EF and sweeps IDs 1-3, 7-9, 14-16, 20, 22-31 individually in the same run; restore with `none` after each comparison. Capture the two intended HUD elements and report the correct IDs before adjusting the named toggle mapping.

Build result: build.bat exit 0, existing dllmain C4456 warning only. ASI SHA256 d9faf353454c426bd066a66cce401186d663339f6f0aa6ccb2d3f4fcee410a91, bridge file #4799. Implementation commit 46a69f87. Build-time native mask/parser/pattern tests pass. Build log D:/nioh3-re/hud-65330/build-live.log. Main PC installed baseline per task remains public v1.8.4 EF 85a3f50a / BR 449db1aa / Cam 537d19ac / TR c2b54f19; Claude tested candidate EF 86dc3279. cx-pc did not inspect or alter the live game.
