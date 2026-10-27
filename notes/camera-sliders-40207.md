# Camera sliders: calibrated defaults (#40207 / #40211)

Runtime commit: 84722ec1 on cxl-pc/camera-sliders-40207, based on the
input-cap build dbed70b7. EngineFix artifact: bridge file 2871, SHA256
16ce50495bc7ef20f6611bd14ad76fef577db5f828f4072a9ea2b045db1951e9.

Four rows control real CamKiProbe overrides. Left/right changes the value;
Enter/A resets that row. Return to default resets all four. Zero deletes the
corresponding key, so the game recomputes its native camera. No stick simulation.

- Lateral: CamOffsetRight, equal -20/+20 world-unit steps, range -400..400.
- Vertical: CamOffsetUp, equal -20/+20 world-unit steps, range -400..400.
- Distance: CamDist, equal -60/+60 steps around calibrated 585 world units.
- Lens: CamFovDeg, equal -5/+5 degree steps around calibrated 47.5 degrees.

Left means closer/narrower; right means farther/wider. The old 629 reference
is not used. Distance and lens retain the backend's absolute bounds of 50..4000
and 20..90 respectively; those physical endpoints are not symmetric about the
calibration. Stepping toward zero always lands on zero, including values from
older INIs or clamped endpoints that do not lie on the normal step grid.

Per-scene limitation: nonzero distance/lens values are calibrated absolute
settings, not offsets from a dynamically sampled native camera. Native values
can differ by scene or camera state. Only zero guarantees the game's own current
default because it removes the override rather than writing 585 or 47.5.
The lead explicitly accepted these references and this limitation in #40211.

Release build and both offline menu suites passed. Tests cover all four zero
crossings, reset/key deletion, persistence, clamps, old off-grid values and menu
navigation. Offline layout preview was sent to Discord. No game launched or
installed files changed; pc owns integration and in-game acceptance.
