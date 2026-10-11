# Code map: what each part of Nioh3.exe is for

The full-game chart ([docs/progress/full_game.svg](progress/full_game.svg)) draws one block per **purpose region**: what the code is for, not how far it is decompiled. The blocks come from `data\regions.tsv`, written by `tools\progress\regions.py` (about 17 s, no Ghidra needed). The "unknown purpose" share is **0.7 % of code bytes** (goal: < 5 %).

## How the regions are made
1. **Functions**: every `.pdata` start in the game image (`data\local\Nioh3.dump.exe`), with chained unwind entries folded in. That gives 211,206 functions and 52.7 MB of code.
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
5. **Regions**: contiguous runs of one label in address order. Runs under 2 KB with no direct evidence are folded into the larger neighbour. The result is 3,649 rows with `start end label confidence evidence funcs code_bytes`.

**Confidence** (per region, by code bytes):
- **high**: ≥ 25 % of its bytes carry their own evidence for this label.
- **medium**: ≥ 30 % have own evidence or are at most about 2 call/global hops from evidence with a dominant vote.
- **low**: the label comes from wider graph and address context.
- **none**: the code is not linked to any evidence. Those regions are labelled `unknown`, and the `evidence` column shows the address-only guess.


Commands (see [tools/progress/README.md](../tools/progress/README.md) for setup):
- `py -I tools\progress\regions.py [--md]` rewrites `regions.tsv` and prints this table.
- `py -I tools\progress\regions.py --why 0xRVA` prints all raw evidence for the region holding the RVA: strings, imports, names, and the labels it calls and is called from.
- `py -I tools\progress\regions.py --survey` prints a per-MB evidence digest, for writing new rules.
- `py -I tools\progress\progress_treemap.py` redraws the charts. If `regions.tsv` is missing it falls back to the old name-keyword areas. The perf and GPU charts are unchanged.

## What each section is
| what it is (tech / plain) | code MB | share | confidence (high / medium / low, by bytes) |
|---|---:|---:|---|
| RTRig codegen / compiled animation rigs | 11.17 | 21.2 % | **medium** (0 / 97 / 2 %) |
| Game menus·text markup / menu screens & item text | 5.74 | 10.9 % | **low** (16 / 36 / 47 %) |
| Obj·Stage / object & scene update | 3.16 | 6.0 % | **low** (7 / 29 / 62 %) |
| Nature·Ocean / sky, weather & water | 2.85 | 5.4 % | **low** (45 / 1 / 52 %) |
| KIDS script binds / game logic scripting | 2.61 | 4.9 % | **low** (31 / 12 / 56 %) |
| Mission·Quest·Items / game rules & logic | 2.02 | 3.8 % | **low** (8 / 14 / 76 %) |
| SynCtx·D3D12 / GPU submission | 1.87 | 3.6 % | **low** (16 / 4 / 78 %) |
| Ktsl2·XAudio2 / sound engine & mixer | 1.80 | 3.4 % | **medium** (25 / 50 / 24 %) |
| Meshlet·Culling / geometry & culling | 1.64 | 3.1 % | **low** (18 / 21 / 59 %) |
| PB2·render params / materials & render settings | 1.51 | 2.9 % | **low** (17 / 11 / 71 %) |
| Effect·Particle / visual effects | 1.40 | 2.7 % | **low** (14 / 19 / 66 %) |
| ktgl Online·P2P / online & matchmaking | 1.39 | 2.6 % | **low** (3 / 8 / 88 %) |
| Online:: sync / co-op gameplay sync | 1.27 | 2.4 % | **low** (4 / 7 / 87 %) |
| ktgl ScreenLayout / menus & HUD | 1.24 | 2.4 % | **low** (8 / 5 / 86 %) |
| Stream·RDB / resource loading | 1.21 | 2.3 % | **low** (9 / 6 / 84 %) |
| Collision·SoftBody / physics & collision | 1.12 | 2.1 % | **low** (17 / 11 / 70 %) |
| Player·Equip·Skill / characters & combat | 0.95 | 1.8 % | **low** (12 / 19 / 68 %) |
| PostEffect·TAA / post-processing | 0.93 | 1.8 % | **low** (28 / 5 / 65 %) |
| KIDS jobs / job system & threads | 0.88 | 1.7 % | **low** (14 / 6 / 79 %) |
| Anim·Motion·Rig / animation | 0.87 | 1.6 % | **low** (14 / 37 / 47 %) |
| SaveData / saving & config | 0.74 | 1.4 % | **medium** (1 / 78 / 20 %) |
| MSVC CRT·STL / C runtime & std library | 0.68 | 1.3 % | **low** (0 / 6 / 92 %) |
| RefProbe·GI / lighting | 0.64 | 1.2 % | **low** (29 / 10 / 59 %) |
| XInput·HID / controls | 0.63 | 1.2 % | **low** (6 / 27 / 66 %) |
| Vec·Mtx / math & containers | 0.59 | 1.1 % | **low** (7 / 2 / 89 %) |
| WinMain·Win32 / app, window & OS | 0.54 | 1.0 % | **low** (7 / 9 / 82 %) |
| FSR3·XeSS·Streamline / upscalers & frame-gen | 0.53 | 1.0 % | **low** (23 / 2 / 74 %) |
| ktul / strings, text codecs & serializer | 0.47 | 0.9 % | **low** (0 / 41 / 58 %) |
| unknown / no evidence | 0.38 | 0.7 % | **none** (0 / 0 / 0 %) |
| AI·Behavior / enemy & NPC AI | 0.28 | 0.5 % | **low** (15 / 13 / 71 %) |
| Shadow·ESM / shadows | 0.25 | 0.5 % | **low** (40 / 12 / 46 %) |
| Alloc·Pool / memory | 0.24 | 0.5 % | **low** (8 / 27 / 64 %) |
| Camera / camera control | 0.23 | 0.4 % | **low** (15 / 2 / 82 %) |
| DXC·D3DCompiler / shader compile & PSOs | 0.20 | 0.4 % | **low** (19 / 12 / 68 %) |
| Steamworks / Steam platform | 0.17 | 0.3 % | **low** (30 / 0 / 68 %) |
| MediaFoundation / video playback | 0.17 | 0.3 % | **low** (26 / 4 / 69 %) |
| Debug·profiler / dev & debug tools | 0.08 | 0.1 % | **low** (15 / 0 / 84 %) |
| libvorbis / Ogg audio decoding | 0.07 | 0.1 % | **low** (0 / 0 / 100 %) |
| BCrypt·hash / crypto & checksums | 0.05 | 0.1 % | **low** (15 / 0 / 84 %) |
| zlib 1.1.4 / decompression | 0.05 | 0.1 % | **medium** (3 / 72 / 23 %) |
| PhotoMode / photo mode | 0.04 | 0.1 % | **low** (10 / 39 / 49 %) |

## Notable findings
- **21 % of all code is the RTRig code generator's output, about 11.2 MB.** Most of it sits in two runs, 0x1352470-0x1821CA1 and 0x29AE720-0x2E1BC28, plus some smaller ones. Each function is a huge machine-generated body: up to 1.9 MB, a big `__chkstk` frame, almost only calls into a handful of float4-array primitives (unnamed helpers around 0x504D7C-0x50521C and others). None has a direct caller. They are reached through the 124-entry `UnknownRTRig` table, often via `jmp` thunks. The string `AutomatonRigScriptCodeObjectHash` points the same way: these are compiled "automaton rig" programs, procedural or secondary animation rigs turned into native code. The table membership is certain. The "animation" purpose is read off those names, so it is rated **medium**.
- **The libraries are linked together at 0xB2D000-0xC60000**: zlib 1.1.4 (0xB2D110), the ktul codepage/serializer, the MSVC CRT and locale code (0xB65074-0xBB4BB5), and libvorbis 1.3.7 (0xC0E960).
- **Menu screens are about 5.7 MB.** They build their text with `^01~WHITE~` / `^05~STYLE_SAMURAI~` markup and sit mainly at 0x1D30EC4-0x1F77751 (about 2.1 MB in one stretch), plus 0x22A9558-0x2340FA9 and 0x2778278-0x27E26EA. They are separate from the ktgl ScreenLayout library (`Scl*`, 0.9 MB) that draws them.
- **Online is about 2.7 MB**: the ktgl Online/P2P session layer plus the `Online::` co-op gameplay sync (`Online::SynchroObject*`, `Online::Player*`).
- No Lua, Havok, PhysX, Wwise or CRI middleware was found. Audio is KT's own Ktsl2 over XAudio2. Physics is KT's own code (`KTGLSoftBodyWorker`). A string search for CRIWARE, Havok, PhysX, Wwise, FMOD, Bink, Lua, Scaleform and SpeedTree finds nothing.
