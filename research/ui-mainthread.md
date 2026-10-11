# Main thread: UI/HUD per-frame work (static analysis, 2026-10-06)

Static only: Ghidra and the dump. Nothing was attached, read live, or applied. Raw outputs and the cave builder draft (`ui_lib.fsx`) stay in the private dev repo. The draft has not been assembled or tested yet, and there is no offline harness.

## Ranked candidates

| rank | id | what | site | cost now (r5–r7) | expected saving | risk |
|---|---|---|---|---|---|---|
| 1 | **W1b** | W1's `rep stosd` in the glyph padding clear has count = pad = **1** (the atlas is created with pad=1), so every glyph row pays a rep-string startup on WC memory. 97–100 % of 61B1C's samples are at 61BBE, the instruction right after `rep stosd`. Fix: a plain store loop, 22 bytes in place | 61BAC (currently holds the W1 bytes) | 61B1C 1.1–2.4 % | **~1–2 %** | very low |
| 2 | **E** | Alpha read-modify-write on the mapped UI vertex buffer. Fix: run the whole build in a cached scratch buffer, then do one streaming copy before Unmap | 1CD690: S1 1CD74F, exits 1CD942 / 19602C8 / 19602DC | 1CD690 1.1–1.7 % | **~1–1.5 %** | low–medium |
| 3 | **C** | The glyph atlas is wiped every frame even though the engine's persistent-atlas mode is ON. Fix: skip the wipe while there is headroom, and upload only the rows that can have changed | 4A5420, 4A2EBA, 196051A | rest of 62000/61B1C/memset, plus GPU upload | ~0.5–1 % after W1b (1.5–3 % without W1b), plus less PCIe copy | medium |
| 4 | **R** | The render path calls 1CE818 with rasterize=0. That call is a pure no-op walk over every UI element | 7D640E | unknown, part of the ~3 % in 7D622C outside 1C0FFC | ~0.2–0.8 % | low |
| 5 | W2e | `and dword [rax],0` loop on the mapped VB in helper 1371B8 (called from 1A13665, 30E1472, 30E173E). Same class of bug as W2 | 1371BC, 19 bytes in place | not in the top 30 | small | very low; redundant if E is applied |

The chain 365AA98 → … → 253C68 → 254440 is about 6–8 % of the main thread inclusive, but 365AA98 is EffectViewRecordSync (effect GPU recording, see particles.md section 6), not a UI root. Most of that 6–8 % is effect recording; the UI share itself is much smaller.

## E: where the vertex buffer comes from (confirmed)
- 1CD690 maps `[obj+378h]` with 4FD84(mode 3 = write-only). 4FD84 gets its staging from **62D688 with the upload-heap flag**. 62D688 suballocates from `[dev+4678h]` and creates a placed resource in GENERIC_READ (an upload heap, i.e. WRITE_COMBINE). 69A720 uses the same allocator for the atlas staging when `tex+0xA8 == 0`.
- 4FD84 then calls `Map` (vtbl+0x40) and stores pointer+offset only into `[obj+358h]`. Nothing else keeps the base.
- `[obj+358h]` is a **cursor**. Every producer advances it: 136F60, 137434, 7B35EC, 875380, 1A135FA, 30E13A8 and 30EC4E8 all do `add [r+358h],bytes; add [r+39Ch],bytes`. The capacity check is `[obj+394h]-[obj+39Ch]` (1373E8). 1CD690 keeps the base in rbp before the first pass.
- Unmap 4FBD4 copies `[B+78h]` bytes (the whole map size) on the GPU, so the GPU only reads what the CPU wrote into the staging.
- Fix design (ui_lib.fsx):
  - S1 replaces `mov rbp,[rsi]; test; je`. If the feature is on, `[obj+394h] <= scratchCap` and a `lock cmpxchg` busy flag is acquired, it saves the real pointer and puts the scratch base into both rbp and `[obj+358h]`.
  - The three exits (`mov rcx,[rdi+378h]` right before each Unmap) call a release routine. It copies `max([obj+39Ch], rbp-scratch)` bytes, clamped to `[B+78h]`, with movdqu ×4 plus `rep movsb`, then sfence. It also rebases the cursor and frees busy.
  - Fallbacks: capacity too large → stock path; busy → stock path.
  - rbx (=3) is preserved across S1. Only volatile registers that are dead are clobbered.

## C: atlas lifecycle (precise)
- **G** (the glyph cache) is created by 2EDAF58 → ctor 2EDAE7C:
  - size 0x260C8; code table at +0xB0 (64K×u16); 1025 nodes ×0x18 at +0x200B0; cap `+78 = 1024`;
  - pad `+7C = 1`; **`+A8 = persist flag`**.
  - For the UI atlas (32FAE8C) the persist flag is `g+0x37C`, where g = `[Nioh3+4BD0AC0]`, the UI settings set from the init parameters (30C669A).
  - 32FAE8C creates 3 textures (ring at G+0x18..0x40) and attaches them via 26B66B0.
- Texture: ctor 2502B0 sets `tex+0xA8 = (desc+0x24 == 1)`. 32FAE8C passes desc+0x24 = **1**, so the UI atlas textures **statically use dedicated, persistent staging** (69A720 → 9C2EE0, never released by 69A0B4).
- Per frame:
  - 4A529C sets `R+0x4A0 = 1` (esi = 1, constant) and calls 96A2C0(R), which wipes G = `[R+28h]`: count/maxY/x/y.
  - 254440: if `R+4A0 && g+37C`, it runs 30DD720 (map: 69A36C → 69A3DC; no reset there when `G+A8 != 0`), then 7D6B20 ×64 slots (tail-jumps to 1CE818 with rasterize=1 → 61EA0 → 62000 → decode + 61B1C pad). After that comes 30DE7E8 → 4A2E44 → 4A2E94, which uploads rect {0,0,W,maxY} via 699EC8 → 69A1B4 (DstX/DstY = rect.left/top). With `G+A8 == 0` it would instead rotate textures (4A2F24).
  - Profiles show rasterization happening, which only happens on the 7D6B20 path (requires `g+37C != 0`). So persist mode is ON, and the per-frame wipe is the only reason glyphs get re-decoded.
- Slot allocation is a shelf packer in 62000:
  - x `+84`, shelf y `+88`, maxY `+8C` (includes pad), W/H `+90/+94`.
  - Key = code | page(+72) | palette(+73).
  - Overflow (count ≥ cap, or no room left in H): 61EA0 or 62000 returns 0, then 1CE818 → 196051A clears r14b and **the rest of the frame's text is dropped**. There is no retry.
- Cave design (ui_lib.fsx):
  - **C1** (4A5420, call → cave). Validate `G+A8`, `T=[G+40]`, `T+A8`. Check generation: texture, font `G+10`, font data `[G+10]+18`. Reset (via the original 96A2C0) if the last frame overflowed, if count ≥ 512, or if maxY ≥ H/2. Otherwise keep the glyphs and record `dirtyTop = max(0, G+88 - G+7C)`.
  - **C2** (4A2EBA, `and [rsp+34h],0` → call). rect.top = min(dirtyTop, maxY), only for the tracked G. Only r10/r11 are used because eax (=W), rdx, r8 and r9 are live here.
  - **C3** (196051A). Set failFlag so the next frame resets.
- Remaining risk: one frame of missing text if a single frame needs more new glyphs than the headroom left (the stock code has the same failure mode at full capacity).

**Live values to read before applying C (read-only):** `byte [[Nioh3+4BD0AC0]+37Ch]` must be 1.

The cave also validates `[[R+28h]+A8h]` and `[[[R+28h]+40h]+A8h]` at runtime and falls back to the stock path if either is 0. It exports W/H/count/maxY in its data block for sizing.

## R: render-path walk is a no-op
- 7D622C calls 7D63E4 with `r9d=0` (7D62A6), and 7D63E4 passes r9 through to 1CE818.
- With bpl=0, 1CE818 only does three things: it calls type() on each element; for type 2 it calls 18C6F78 (init a local) and vfunc+0x68 (copy into a local); for type 9 it does nothing.
- All type-2 vfunc+0x68 implementations found are pure: 1CB86C/1CB8A0, 1CA360, A9AB60, A9AB40, 744B60, 744B30.
- The other caller, 30DDD18, passes r9b=1, so the patch must gate on r9b: call → cave, `test r9b,r9b; jnz → jmp 1CE818; else ret`.

## Not found / not safe
- Static HUD meshes are rebuilt every frame, but the VB is a fresh ring allocation each frame. There is no cheap way to detect "unchanged", so this can't be skipped safely. E makes the rebuild cheap instead.
- No per-frame string formatting or hashing hot spot was visible in the UI path.
