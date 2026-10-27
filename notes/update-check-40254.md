# Background update check — task 40254

Branch: cx-pc/update-check-40254, based on published combined tip d874f32b (pc/final-combined-4).
Initial implementation: f84c3cb6. No integration into main, deployment, installation, or game access.

## Contract

EngineFix requests https://nioh3-mod-feedback.vercel.app/version.json from a dedicated worker, first on the existing stats worker tick, then six hours after each completed attempt. Only one request runs at once. Allocation/thread failure backs off thirty minutes. DNS timeout is 1 second; connect/send/receive timeouts are 2 seconds each; connect retries are disabled. The read loop checks an 8-second elapsed budget between reads; this is not an absolute whole-request cancellation deadline.

HTTPS origin and path are fixed. Redirects, cookies and automatic authentication are disabled. Responses must be HTTP 200, at most 512 bytes, and a strict object containing only a numeric major.minor.patch version. Offline/error/malformed responses cause no UI or log noise and retain any previously validated notice. No download or self-update is performed.

Comparison is numeric. The built-in 1.7.80-settings37433 compares as 1.7.80; suffixes do not falsely advertise a new release. A successful equal/older manifest clears the notice.

The existing mod-menu corner hint adds Update available vX.Y.Z and the fixed site URL only when a newer release is known. Existing PauseHint visibility remains respected. Width is measured and scaled to fit the viewport. Rendering never waits for the network.

## Site preparation (NOT deployed)

Prepared file: site/version.json, currently {"version":"1.7.80"}.
Copy this static file into the site's deployed root (the site's current project root is nioh3-mod-site/site).
Publish a higher numeric version only when the matching downloadable release is available, and bump MOD_VERSION in the corresponding source release. Keep this manifest version-only; extra fields are deliberately rejected.
Until the file is deployed, a 404 is silent and no notice is shown.

## Offline validation

- 70 assertions: numeric versions and suffixes, strict/bounded JSON, HTTP setup failures/status codes, chunking/body limits/read failures, handle cleanup, publication/clearing, and scheduler busy/due/backoff behavior.
- Both real settings/controller menu and Boss UI harness suites PASS. Hint regression verifies the newer-version text, fixed URL, and viewport bounds at 800x600.
- Windows Release EngineFix build PASS.
- Built artifact: mod/Nioh3EngineFix/bin/Nioh3EngineFix.asi
- SHA256: 549f11cd4c340223275f695491077801fd612b89b6f33c8742f9239627a74c03
- Offline hint preview: mod/Nioh3EngineFix/build/update-hint-preview.png. Sent to Discord using the universal notify tool; example version 1.8.0 is preview data only.
- Native network and in-game behavior remain for pc validation. All HTTP tests here use mocks, as assigned.
