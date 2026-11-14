# View-distance sliders (4-core player) — RE log

Human asks: #102540 (4-core target), #102575 (no game-speed change), #102580 (look may drop), #102601 (arena-only view),
#102608 (sent third-party Nioh3Fix 0.0.6, bridge file #7654), #102635 (ours must beat it), #102646 (sliders, not presets),
#102651 (plus an "Arena size" button), #102675 (start now). Owner: pc/2 (NOTE #102677).

## Source
Nioh3Fix 0.0.6 (2026-08-25), unpacked read-only at D:/nioh3-re/ext-mod-102608/x (never executed). Ini "[Graphical Tweaks]"
multipliers 0.1-10: BackgroundMeshQuality, TerrainQuality, CharacterDrawDistance, ModelDrawDistance, PartsDrawDistance,
HLODDrawDistance, MotionQuality. Byte patterns pulled from the ASI strings (D:/nioh3-re/ext-mod-102608/strings.txt, 0xa1c18..0xa1ef0).

## Pattern hits in re/Nioh3.dump.exe (file offset == RVA; dump dated 11-04, NOT yet checked against the live 11-14 exe)
| setting | hits | RVA | what the code there does (first read) |
|---|---|---|---|
| BackgroundMesh | 1 | 39a758 | loads per-quality table [rcx+rax*4+3977a58] -> [rbx+1978] (float) and [..+3978390] -> [rbx+1970]; sets [rbx+15cc]=1.0f |
| Terrain | 1 | 133d8a | `or [rsp+80],-1` before a tessellation check, flag [cfg+11f1] |
| Character | 1 | de8d0 | gate [cfg+11f7]; compares [r13+90/94/98] vs a constant, maxss with xmm8 (distance) |
| Model | 2 | 209edc, 20aa10 | twin getters returning float [rbx+29c] / [rbx+2a0] (draw / fade distance?) |
| Parts | 1 | 1774ad | 1770c8 fills [rbp+78] = parts distance; `comiss xmm6,[rbp+78]` = cull test |
| HLOD | 1 | 39722b | picks HLOD level count 8/6/4 by quality |
| Motion | 1 | 39a86c | full-rate-animation distance (gameplay-visual only; skip for now) |

## Next
1. Read the ASI's hook code for each pattern (offset into the match + the math applied) -> exact value to scale.
2. Check every pattern against the live exe (process memory) before any build.
3. Build EF sliders (one per setting, live, no restart), keep Character at 1.0 by default.
4. Measure CPU cores + fps per slider on Main PC (4-core affinity test, same as performance-pc #102540 job).
