# Steam cloud dialog: covered-window diagnosis and bounded recovery

2026-10-11, tools-only; no live focus, screenshot, click, Steam configuration or
game launch performed by this task.

The old `tools/testmatrix/steam_cloud_click.ps1` OCRed CopyFromScreen pixels at
each Steam HWND's rectangle. Those pixels belong to whichever application is
visible there, not necessarily Steam. Its fallback raise ran only after OCR
already matched "Unable to Sync". A completely covered header therefore never
reached that branch and could produce `none`. `IsWindowVisible` does not prove
unoccluded desktop pixels. Root's later foreground Launching window with no
match does not prove this was the only cause: a separate dialog, blank CEF
rendering or OCR recognition failure remains possible. Steam Action286
syncfailed alone cannot identify a clickable button or its HWND.

The bounded fix adds paired `-ExpectedLaunchHwnd <integer>` and
`-ExpectedLaunchPid <integer>` inputs. Root must obtain both from its own pending
launch. Current PID must own that visible HWND, belong to steam/steamwebhelper,
and its title must begin Launching or Unable to Sync. Only this explicit window
may be raised once, after the unchanged default20-second user-idle gate. Steam
main/settings are not generic reveal targets. Explicit mode observes only this
HWND. DryRun reports a required reveal without focusing or clicking.

After reveal, and again after the final foreground verification, fresh screen
OCR must contain Unable to Sync and adjacent exact Play/anyway words. The old
unadvertised blue-button-left-of-Cancel fallback is removed. Stale pre-focus
coordinates are discarded. Ownership and foreground are rechecked immediately
before mouse-down. Nonforeground captures cannot authorize a click; no explicit
target means covered windows remain reported as unverified instead of silently
treated as evidence that no dialog exists. Blank foreground CEF remains refused.

Verification:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/testmatrix/test_steam_cloud_decision.ps1
git diff --check
```

20 pure decision/geometry/parser/source-integration checks PASS. The covered
explicit-target fixture now permits reveal-once and subsequently accepts only
fresh exact foreground evidence. Other fixtures reject wrong owner/PID,
unrelated Steam titles, user activity, covered OCR, blank rendering, missing
button and nonadjacent words; DryRun cannot request a real reveal. This proves
the decision path, not actual Windows focus success, CEF rendering or OCR/live
dialog acceptance. Foreground checks are observations, not an OS lock against
an external focus change between the last check and mouse-down. English titles
and OCR text remain required; no localization claim or launch retry was added.
