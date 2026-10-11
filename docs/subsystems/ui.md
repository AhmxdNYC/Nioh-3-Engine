# UI (text, glyph atlas, quad builder)
CPU share 2.4 % (Ally 3.1 %, PC 1.7 %) as profiled in the UI area; coverage 98.8 %. The dominant UI cost WcUploadZeroFill 61B1C (3.7 %) is counted on the GFX page. Threads: [App]Game (UI is ~6-8 % inclusive of the main thread via 365AA98 -> ... -> 253C68 -> 254440) (365AA98 is EffectViewRecordSync, so the 6-8 % is mostly effects).

## Role
KIDS UI node tree (1750-slot UI vtable block at 38E7188), text layout, a shelf-packed glyph atlas, and a quad/vertex builder that writes into write-combined upload buffers.

## Key data
- Glyph cache: per-code tree, shelf packer (`GlyphCacheAllocSlot` 62000), atlas reset 96A2C0 from 4A5420 (render begin) every frame even though persistent-atlas mode is on; padding clear `WcUploadZeroFill` 61B1C (rep stosd with count = pad = 1, hot at 61BBE); rebuild 61EA0. Staging via 62D688 (upload heap, placed resource GENERIC_READ).
- Quad builder `UiQuadBuild` 1CD690: maps `[obj+378h]` via 4FD84 mode 3 (write-only); cursor `[obj+358h]`, used `+39Ch`, capacity `+394h`; alpha multiplied in place (uncached reads, 1CD8BC..1CD914). `WriteQuadVerticesWC` 1371D0 had the same RMW bug.
- Node virtuals: `UiNodeUpdateWorldMatrix` 66D00 (2.2 KB), `UiNodeDrawIfVisible` 6642C (state n+140h), `UiNodeDrawRecursive` 66A98 (mode n+144h), `UiTextLayoutGlyphs` 62408 (6.3 KB).
- 1CE818 called with rasterize=0 is a pure no-op walk over every UI element (candidate R).

## Hot functions
| RVA | name | % |
|---|---|---|
| 62000 | GlyphCacheAllocSlot | 1.30 |
| 1CD690 | UiQuadBuild | 0.27 |
| 66D00 | UiNodeUpdateWorldMatrix | 0.25 |
| 6642C | UiNodeDrawIfVisible | 0.23 |
| 62408 | UiTextLayoutGlyphs | 0.14 |
| 1371D0 | WriteQuadVerticesWC | 0.11 |

## What drives the cost
Visible glyph count x the per-frame atlas wipe; uncached reads of write-combined memory. W1/W2 were fixed live; W1b (plain store loop at 61BAC), E (scratch buffer) and C (skip wipe) are ranked candidates.

## Deeper notes
notes/ui-mainthread.md (W1b, E, C, R), mainthread-drawprep.md (C, E), cpu-profile.md (write-combined hunting recipe).
