# Code map: what each part of Nioh3.exe is for

The full-game chart ([docs/progress/full_game.svg](progress/full_game.svg)) draws one block per **purpose region**: what the code is for, not how far it is decompiled. How far the names and hooks go is [notes/reversing-progress.md](../notes/reversing-progress.md): 12,679 of 211,206 functions named (6.00%) on the 2026-10-24 recount. The blocks come from `re\scripts_re\regions.tsv`, written by `re\scripts_re\regions.py` (about 15 s, no Ghidra needed). The "unknown purpose" share is **0.7 % of code bytes** (goal: < 5 %).

## How the regions are made
1. **Functions**: every `.pdata` start in `re\Nioh3.dump.exe`, with chained unwind entries folded in. That gives 211,206 functions and 52.7 MB of code.
2. **Evidence per function** (each piece votes for a label through the keyword rules `RULES` in `regions.py`):
   - Strings it references. These come from `re\out\strings.tsv` plus our own `[rip+disp32]` scan, which finds 10k more refs and agrees 100 % with Ghidra's. Strings that sit next to a function pointer in `{name, fn}` data tables also count. Generic strings referenced by many functions are down-weighted.
   - Imports it calls (`re\out\imports.tsv`). Common ones such as `EnterCriticalSection` barely count.
   - Names and descriptions from `re\symbols\symbols.tsv`. These include the merged auto-names (RTTI `Class::vfunc_N`, ctors). `re\out\autoname_low_hints.tsv` counts at a low weight.
   - The **RTRig table**: 124 `{flags, fn, 0, "UnknownRTRig", hash, hash, ptr}` entries in `.data` (0x4BC7A00-0x4BC94E8).
3. **Links between functions**:
   - Direct calls and tail-jumps (E8/E9 rel32 to a `.pdata` start).
   - Consecutive slots of each vtable, and a ctor's `lea` of its vtable.
   - **Shared module-local globals**: functions that reference the same static, vtable or constant, where only 2-30 functions reference it, are almost always in the same `.cpp`.
4. **Propagation**: labels flow caller → callee at full weight (a helper used only by module X belongs to X). They flow callee → caller at 0.3, and never for service labels such as CRT, math, memory, jobs and zlib, because calling `malloc` does not make a function an allocator. Hubs are damped by their degree. Address neighbours then smooth the result over ±6, ±48 and ±256 functions, because MSVC links code roughly per object file. Three rounds feed the provisional labels back in as a weak prior.
5. **Regions**: contiguous runs of one label in address order. Runs under 2 KB with no direct evidence are folded into the larger neighbour. The result is 3,023 rows with `start end label confidence evidence funcs code_bytes`.

**Confidence** (per region, by code bytes):
- **high**: ≥ 25 % of its bytes carry their own evidence for this label.
- **medium**: ≥ 30 % have own evidence or are at most about 2 call/global hops from evidence with a dominant vote.
- **low**: the label comes from wider graph and address context.
- **none**: the code is not linked to any evidence. Those regions are labelled `unknown`, and the `evidence` column shows the address-only guess.

**Validation (hold-out)**: we hid the evidence of a random 20 % of the evidence-bearing functions (1,754) and re-inferred their labels. Medium-confidence inferences were right 91 % of the time. Low-confidence ones matched the exact label (out of 41) 53 % of the time, and the coarse class (render / gameplay / library) 79 % of the time. Read a "low" block as "mostly this, with neighbours mixed in".

Commands:
- `py -I re\scripts_re\regions.py [--md]` rewrites `regions.tsv` and prints this table.
- `py -I re\scripts_re\regions.py --why 0xRVA` prints all raw evidence for the region holding the RVA: strings, imports, names, and the labels it calls and is called from.
- `py -I re\scripts_re\regions.py --survey` prints a per-MB evidence digest, for writing new rules.
- `py -I re\scripts_re\progress_treemap.py` redraws the charts. If `regions.tsv` is missing it falls back to the old name-keyword areas. The perf and GPU charts are unchanged.

## What each section is
| what it is (tech / plain) | code MB | share | confidence (high / medium / low, by bytes) |
|---|---:|---:|---|
| RTRig codegen / compiled animation rigs | 11.52 | 21.9 % | **medium** (0 / 96 / 3 %) |
| Game menus·text markup / menu screens & item text | 6.38 | 12.1 % | **low** (5 / 43 / 50 %) |
| Obj·Stage / object & scene update | 3.25 | 6.2 % | **low** (3 / 26 / 69 %) |
| Nature·Ocean / sky, weather & water | 2.92 | 5.5 % | **low** (44 / 4 / 51 %) |
| KIDS script binds / game logic scripting | 1.84 | 3.5 % | **low** (37 / 14 / 48 %) |
| Meshlet·Culling / geometry & culling | 1.82 | 3.5 % | **low** (14 / 16 / 68 %) |
| SynCtx·D3D12 / GPU submission | 1.76 | 3.3 % | **low** (11 / 5 / 82 %) |
| Mission·Quest·Items / game rules & logic | 1.71 | 3.2 % | **low** (12 / 12 / 74 %) |
| Online:: sync / co-op gameplay sync | 1.52 | 2.9 % | **low** (2 / 10 / 87 %) |
| PB2·render params / materials & render settings | 1.49 | 2.8 % | **low** (14 / 16 / 68 %) |
| Ktsl2·XAudio2 / sound engine & mixer | 1.41 | 2.7 % | **low** (11 / 35 / 52 %) |
| KIDS jobs / job system & threads | 1.39 | 2.6 % | **low** (9 / 3 / 87 %) |
| ktgl Online·P2P / online & matchmaking | 1.39 | 2.6 % | **low** (2 / 5 / 91 %) |
| Effect·Particle / visual effects | 1.30 | 2.5 % | **low** (9 / 12 / 77 %) |
| Collision·SoftBody / physics & collision | 1.27 | 2.4 % | **low** (10 / 6 / 82 %) |
| PostEffect·TAA / post-processing | 1.13 | 2.2 % | **low** (18 / 4 / 76 %) |
| Player·Equip·Skill / characters & combat | 1.07 | 2.0 % | **low** (14 / 11 / 74 %) |
| ktgl ScreenLayout / menus & HUD | 0.93 | 1.8 % | **low** (11 / 6 / 82 %) |
| SaveData / saving & config | 0.83 | 1.6 % | **medium** (1 / 72 / 25 %) |
| RefProbe·GI / lighting | 0.74 | 1.4 % | **low** (20 / 6 / 72 %) |
| MSVC CRT·STL / C runtime & std library | 0.67 | 1.3 % | **low** (0 / 5 / 93 %) |
| Anim·Motion·Rig / animation | 0.65 | 1.2 % | **low** (10 / 27 / 61 %) |
| WinMain·Win32 / app, window & OS | 0.63 | 1.2 % | **low** (6 / 4 / 89 %) |
| FSR3·XeSS·Streamline / upscalers & frame-gen | 0.62 | 1.2 % | **low** (20 / 3 / 75 %) |
| XInput·HID / controls | 0.61 | 1.2 % | **low** (3 / 26 / 69 %) |
| Stream·RDB / resource loading | 0.61 | 1.2 % | **low** (13 / 15 / 71 %) |
| ktul / strings, text codecs & serializer | 0.54 | 1.0 % | **low** (0 / 36 / 63 %) |
| unknown / no evidence | 0.36 | 0.7 % | **none** (0 / 0 / 0 %) |
| AI·Behavior / enemy & NPC AI | 0.35 | 0.7 % | **low** (12 / 12 / 74 %) |
| Vec·Mtx / math & containers | 0.32 | 0.6 % | **low** (4 / 0 / 95 %) |
| Shadow·ESM / shadows | 0.29 | 0.6 % | **low** (31 / 7 / 61 %) |
| Alloc·Pool / memory | 0.27 | 0.5 % | **low** (3 / 10 / 85 %) |
| DXC·D3DCompiler / shader compile & PSOs | 0.22 | 0.4 % | **low** (15 / 11 / 72 %) |
| MediaFoundation / video playback | 0.21 | 0.4 % | **low** (21 / 5 / 73 %) |
| Steamworks / Steam platform | 0.20 | 0.4 % | **low** (26 / 0 / 73 %) |
| zlib 1.1.4 / decompression | 0.09 | 0.2 % | **low** (0 / 44 / 55 %) |
| Camera / camera control | 0.08 | 0.2 % | **low** (27 / 8 / 63 %) |
| Debug·profiler / dev & debug tools | 0.08 | 0.2 % | **low** (14 / 0 / 85 %) |
| libvorbis / Ogg audio decoding | 0.07 | 0.1 % | **low** (0 / 0 / 100 %) |
| BCrypt·hash / crypto & checksums | 0.06 | 0.1 % | **low** (15 / 0 / 84 %) |
| PhotoMode / photo mode | 0.04 | 0.1 % | **low** (10 / 35 / 54 %) |

## Notable findings
- **22 % of all code is the RTRig code generator's output, about 11.5 MB.** Most of it sits in two runs, 0x1352470-0x1821CA1 and 0x29AE720-0x2E1BC28, plus some smaller ones. Each function is a huge machine-generated body: up to 1.9 MB, a big `__chkstk` frame, almost only calls into a handful of float4-array primitives (0x504D7C, 0x504EA0, 0x50521C, 0x53B38 and others). None has a direct caller. They are reached through the 124-entry `UnknownRTRig` table, often via `jmp` thunks. The string `AutomatonRigScriptCodeObjectHash` points the same way: these are compiled "automaton rig" programs, procedural or secondary animation rigs turned into native code. The table membership is certain. The "animation" purpose is read off those names, so it is rated **medium**.
- **The libraries are linked together at 0xB2D000-0xC60000**: zlib 1.1.4 (0xB2D110), the ktul codepage/serializer, the MSVC CRT and locale code (0xB65074-0xBB4BB5), and libvorbis 1.3.7 (0xC0E960).
- **Menu screens are about 6.4 MB.** They build their text with `^01~WHITE~` / `^05~STYLE_SAMURAI~` markup and sit mainly at 0x1D30EC4-0x1F77751 (about 2.1 MB in one stretch), plus 0x22A9558-0x2340FA9 and 0x2778278-0x27E26EA. They are separate from the ktgl ScreenLayout library (`Scl*`, 0.9 MB) that draws them.
- **Online is about 2.9 MB**: the ktgl Online/P2P session layer plus the `Online::` co-op gameplay sync (`Online::SynchroObject*`, `Online::Player*`).
- No Lua, Havok, PhysX, Wwise or CRI middleware was found. Audio is KT's own Ktsl2 over XAudio2. Physics is KT's own code (`KTGLSoftBodyWorker`). A string search for CRIWARE, Havok, PhysX, Wwise, FMOD, Bink, Lua, Scaleform and SpeedTree finds nothing.
