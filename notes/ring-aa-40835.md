# Smooth ring edges — 40835

Branch cx-pc/ring-aa-40835, based on ring implementation b8665395.
Requested Opus specialist review was run using Claude Code with model opus, read-only. Its recommendation was cached coverage masks and premultiplied alpha rather than redrawing a supersampled screen every frame.

The ring renderer now tessellates each curved cell into eight vertices at quarter-pixel precision and caches a 4x4 coverage mask per cell/stripe when geometry changes. Colours remain dynamic and use the existing sampler, palette, labels, numbers, checks and visibility rules. Masks blend directly into the production DIB with the shared panel alpha. GDI fallback remains available if allocation fails or a compatible layered DIB is unavailable. No added per-frame tessellation or mask allocation.

Validation:
- All seven existing offline suites PASS.
- 5,927,461 production renderer assertions PASS across 800x600 through 3840x2160; palette checks now account for partial pixel coverage on narrow tracks rather than demanding opaque pixels.
- Added fractional-edge coverage, cached-mask reuse and byte-identical repeated rendering checks.
- Existing independent-track visibility, PLAYER-only Perf Ki changes, premultiplied alpha, persistence and menu visibility checks PASS.
- Preview-only harness path avoids running CPU timing measurements.
- Release Training build PASS (two pre-existing unused-function warnings).
- Artifact mod/Nioh3Training/Nioh3Training.asi, SHA256 97e9c21390d1d6628cd60d33ac953b9c4ea65bcab2f76326551b1290f2148d52.
- Preview is generated from the production DIB on a neutral backdrop, not a live game screenshot.
- No game, installed files, website or Linux opener touched. pc owns integration and runtime validation.

Checkout is D:/nioh3-wt/ring-aa-40835 because C: lacked checkout space.
