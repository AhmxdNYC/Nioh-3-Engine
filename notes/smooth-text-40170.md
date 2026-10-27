# Smooth UI text — task 40170

Branch base: bc6a281a (contains pc/menu-draw-fix 849d2a33 and the original GDI conversion). Offline only; no installed files, game process or settings touched.

## Implementation

- EngineFix OSD: replaced nearest-cell 8x16 bitmap coverage with analytic distance-to-stroke coverage in the pixel shader, round segment ends/joins and ~17% heavier strokes. Keeps 8x16 *layout metrics*, per-instance color, one quad per glyph, and the existing sprite pipeline. Shared 95 glyph/481 segment data occupy a 9,216-byte CBV; regenerated DXBC is included. Constant buffer upload and the GPU fixture changed together.
- Training/Rematch GDI: rasterize approved strokes into a 4x DIB and downsample premultiplied coverage. No system font, no integer final-pixel stroke snapping. Existing RGB values remain the source colors. Transparent GDI windows now use per-pixel-alpha presentation so AA edges do not blend against the magenta color key. Frame-meter partial text coverage composites with its existing 0x73 panel alpha.
- Frame meter minimum row height20, text18, legend line22; wider legend columns keep labels separate. Counts can use the available final part of their colored run rather than compressing several digits into one tiny pip. For extremely short runs the run bounds still take precedence. The panel's automatic position already grows upward as height increases.
- Negative Ki minimum text18 with row pitch derived from it; standalone Rematch text20. EngineFix menu uses its existing screen-fit scaling; analytic text renders smoothly at fractional scales without making the right-side panel exceed the screen.

## Validation / artifacts

All three ASIs built. MSVC EngineFix reports the existing dllmain local `nt` shadow warning; Training reports existing unused `g_noForceHide`/`AbFree`; Rematch build clean.

- `py mod/shared/tests/run_gdi_stroke_tests.py`: 285 glyph/color/clipping/DC-state cases, fractional-alpha/no-magenta regression, 12,960 count-position cases, 1,000 draw handle-stability check PASS. Runner now fails correctly for Windows negative assertion exit codes.
- Updated `mod/Nioh3EngineFix/tests/padviz_gpu_test.cpp`: actual compiled shader, offscreen D3D12, both controller atlases plus text PASS (0 failures); text each has777 solid and1,143 AA edge pixels. Debug layer unavailable on this host, so this is output/readback validation without debug-layer diagnostics.
- GDI and GPU offline previews sent to Discord. They are not in-game captures. Live appearance/performance remains pc's integration check.

Artifacts in `C:/Users/Ahmad Hamza/nioh3-smooth-text-40170`:

| ASI | Relative path | SHA256 |
|---|---|---|
| EngineFix | mod/Nioh3EngineFix/bin/Nioh3EngineFix.asi | c2df0527e1beec0e64aa6c337c052f0435ab2183e5d03aac1d86873032c7dfe5 |
| Training | mod/Nioh3Training/Nioh3Training.asi | b3c522aa3b6ca090a2fe8e66122c477fbc4aadb48410035a9661ab9eaefc3e85 |
| Rematch | mod/Nioh3BossRematch/bin/Nioh3BossRematch.asi | 65a26ee6a3f05edc8ed6ff06d18cf40f8385dc714ad425263bf4cdc761d50075 |

Build Training with its updated build.sh (adds -lmsimg32). MSVC gets msimg32.lib through the helper's pragma. Regenerate paths/OSD buffer with `py mod/shared/gen_gdi_stroke.py`; regenerate shader with EngineFix/tools/gen_osd_shaders.bat. No downloaded fonts or new runtime files needed.
