# GDI angular stroke font — 40138

Base: pc/menu-draw-fix 849d2a33. Shared C/C++ helper: mod/shared/gdi_stroke_text.h; generated 95 printable ASCII glyphs / 481 segments from EngineFix's approved gen_font_stroke.py (including inputhist_glyphs.inc). Regenerate with `py mod/shared/gen_gdi_stroke.py`.

Replaces system-font drawing/measurement in Training frame labels/readouts/legend, pip counts, negative Ki labels and standalone BossRematch menu/toast/footer. Uses thick GDI line pens in the existing DC text color. Preserves clipping, pen/current-point state and releases each temporary pen. Cell bearings and descender space follow the atlas transform. Standalone row shrink/middle elision and footer word wrap use the new metrics. Hosted EngineFix rendering is unchanged.

Validation: both ASIs build; BossRematch MSVC clean, Training Zig has only existing unused g_noForceHide and AbFree warnings. `py mod/shared/tests/run_gdi_stroke_tests.py`: 285 glyph/color/clipping/DC-state cases, 12,960 count placement cases, 1,000 repeated draws without GDI handle growth. CPU GDI preview BMP produced by that test. No game or installed ASI touched.

Artifacts in this worktree:
- mod/Nioh3Training/Nioh3Training.asi SHA256 f91fa3d9bdf2234e56e6348471d6c44045bb29be007f2e77e642e7450f5fc4d0
- mod/Nioh3BossRematch/bin/Nioh3BossRematch.asi SHA256 39fb731f0e24177617223c7ca3b5e2dc113612b084c7d38ed34ea1029d78147a

Native GDI rasterization is pixel-snapped, unlike the OSD supersampled atlas; geometry and colors match, not per-pixel antialiasing. Runtime appearance belongs to pc's integration test. Non-ASCII bytes render a question-mark fallback, matching the printable ASCII scope of the approved font.
