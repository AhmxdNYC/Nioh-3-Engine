# View-distance sliders (4-core player) — RE log

Human asks: #102540 (4-core target), #102575 (no game-speed change), #102580 (look may drop), #102601 (arena-only view),
#102608 (sent third-party Nioh3Fix 0.0.6, bridge file #7654), #102635 (ours must beat it), #102646 (sliders, not presets),
#102651 (plus an "Arena size" button), #102675 (start now). Owner: pc/2 (NOTE #102677).

## Source
Nioh3Fix 0.0.6 (2026-08-25), unpacked read-only at D:/nioh3-re/ext-mod-102608/x (never executed). Ini "[Graphical Tweaks]"
multipliers 0.1-10: BackgroundMeshQuality, TerrainQuality, CharacterDrawDistance, ModelDrawDistance, PartsDrawDistance,
HLODDrawDistance, MotionQuality. Byte patterns pulled from the ASI strings (D:/nioh3-re/ext-mod-102608/strings.txt, 0xa1c18..0xa1ef0).

## Pattern hits in re/Nioh3.dump.exe (WRONG ROW LABELS: shifted one row, see Hook decode below) (file offset == RVA; dump dated 11-04, NOT yet checked against the live 11-14 exe)
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

## Hook decode (Nioh3Fix)
Static read of Nioh3Fix.asi 0.0.6 (installer at ASI RVA 0x23920, lambdas 0x1720..0x1f70; work scripts in
D:/nioh3-re/ext-mod-102608/work). **Correction to the table above:** in the ASI each pattern string comes *before* its
"LOD: ..." label (installer does `lea r9,pattern` then `lea rcx,label`), so every row above is shifted down one row.
Correct: BG=a1bd0 -> 23554f, Terrain=a1c40 -> 39a758, Character=a1cd0 -> 133d8a, Model=a1d60 -> de8d0,
Parts=a1de0 -> 209edc, HLOD=a1e58 -> 1774ad, Motion=a1ea0 -> 39722b; a1ef0 (39a86c) is "Framerate: Menu Framegen" (NOP jne).
Each block is installed only if its ini value != 1.0 (ini getter called with default 1.0, min 0.1, max 10).
SafetyHook ctx offsets: xmm0 +0, xmm1 +0x10, xmm8 +0x80, rax +0x170, rbp +0x178, rip +0x190.

| setting (cfg) | hook (game RVA) | instruction there | value changed | math / extra |
|---|---|---|---|---|
| BackgroundMesh (123030) | mid 23554f (+0) | `mulss xmm0,xmm1` | xmm1 (per-quality factor from [rax+4/8/c]) | xmm1 = 0.5*roundf(2/BG), REPLACES game factor; quantized (BG 2..4 -> 0.5, BG>4 -> 0.0). Scales the 8-entry per-LOD table obj(rdi)+0xa108[i] |
| Terrain (123034) | mid 39a758 (+0), skips to 39a768 | `movss xmm0,[tbl3977a58+q*4]` | xmm0 -> [rbx+1978], eax -> [rbx+1970] | xmm0 = 0.8*T, eax = min(4000*T,10000); bypasses quality tables (vanilla 0.3..0.5/1000..2000, 0.8/4000 = tier 4) |
| Character (123038) | mid 133d8a (+0) | `or [rsp+80],-1` | xmm8 (= vanilla [obj+0x8c4], floor 6000) | xmm8 = min(5000*C, 20000); overwrites per-character value with a global |
| Model (12303c) | mid de8d0 (+0) + 2 data writes | `cmp byte [rax+11f7],0` | [rax+11f7]=1 (gate forced on); game .rdata 3c0450c (260.0) and 3c045d8 (15000.0) | thr = max(100, 260/M); dist = min(15000*M, 25000); xmm8 = max(dist, xmm8) when any [r13+90/94/98] >= thr. Both constants are SHARED (4 and 7 rip refs) |
| Parts (123040) | mid 209ede (pattern+2), FIRST hit only; 20aa10 not hooked | `cmp byte [rax+1774],cl` | cfg(rax) [+1754]=1, [+1750] | [+1750] = min(5000*P, 25000). Reader of 1750/1754 UNCONFIRMED (getter itself uses 1774/1770) |
| HLOD (123044) | mid 1774b8 (pattern+0xb) | `comiss xmm6,[rbp+78]` | float [rbp+78] (from 1770c8) | [rbp+78] = min(20000, [rbp+78]*H); cap can lower large vanilla values |
| Motion (123048) | byte patch 39722c (pattern+1) | `mov edx,8` | imm32 8 (highest-quality path only; 397246 copy untouched) | imm = (int)(8*M), truncated |

Re-implement plan: BG/Terrain/Character/HLOD as our own mid-hooks with the same register edits but scaling the
vanilla value (factor*vanilla, not a fixed base), so 1.0 is truly default and quality tiers still apply. Model: hook
de8d0 and scale xmm1/xmm0 in registers instead of writing shared .rdata. Parts: find the 1750/1754 reader first.
Motion: patch the imm (or both 8s). Risky for gameplay: Character (enemy despawn/AI range, can be BELOW vanilla
6000 at C=1.1), Model (shared constants hit 5+ other sites), Parts (breakables; collision/physics tie UNCONFIRMED),
Motion (anim rate; hitbox tie UNCONFIRMED).
