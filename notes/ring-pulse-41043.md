# Ring Perfect Ki Pulse check audit — 41043

Base: installed ring-size source cx-pc/ring-size-40972 c3fecd06 (Training8fb6b814).
The mark was already present, PLAYER-only, at the matching visible cell midpoint. Its width followed one frame's arc (minimum6px), while height and thick GDI strokes followed track thickness. This produced a tall squashed blob in the real renderer preview.

Change: Ring-only upright check, proportional to track thickness (65%), with thin antialiased strokes and a dark outline. It stays centred at the same pulse cell and is clipped to the outer PLAYER annulus. Row checks, pulse sampling, palette, sizes and other lanes are unchanged. The existing GDI check is a non-DIB fallback.

Validation:
- Full production renderer: 16,261,371 assertions PASS, all seven existing Training suites PASS.
- New offline footprint audit: 5,040 positions =120 cell angles x3 sizes x14 viewport heights (600..2160); every mark visible, zero pixels outside PLAYER annulus.
- Existing full-renderer assertion verifies changing a perfect-pulse sample changes only PLAYER.
- Six before/after production DIBs demonstrate Small/Medium/Large at1080p.
- Release Training build PASS, two pre-existing unused-symbol warnings.
- Preview sent media to Discord. Not a live game screenshot.
- No game, save, installed file or configuration access.

Artifact: mod/Nioh3Training/Nioh3Training.asi, SHA256 aed09cf0cc3df17c72fbe02104cacc0a0612120945cea0e3fd7a53bdc5d84f4f.
Cherry-pick the small ring-renderer change if another size/legend branch is now authoritative; this build retains the installed160/210/260 size design.
Reproduce audit: compile tests/frame_meter_pulse_ring_audit.c using the same zig cc Windows flags as run_frame_meter_draw.py; run from a scratch directory, then run tools/art/ring-pulse-41043/render-pulse.py beside its six pulse-*.bmp outputs.
