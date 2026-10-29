# PC2 hook startup diagnosis (#10995)

The generic x86_64-windows-gnu target in Zig 0.16 lowers the 128-bit
compare-exchange to a runtime helper with an incompatible Windows ABI.
The caller puts the desired low word in R8; the helper dereferences R8.
The debug rematch harness exception at RVA 0xCAC74 is that dereference.
BossBattleModeEnd has aligned RVA 0x23A2F70 and head length 9, selecting
this path; BossBattleMode immediately before it uses a different path.
This explains the startup breadcrumb but is not a live-game proof.

Fix: enable cx16 on PatchAtomic for Clang x64, emitting CMPXCHG16B locally.
MSVC behavior is unchanged. The guarded declaration avoids a Clang warning
when intrin.h omits the intrinsic for the generic target.

Offline PC2 validation (no game access):
- Unfixed generic-target patch harness exits 0xC0000005.
- Fixed harness: 225 placements pass, including aligned 9-byte End shape;
  exact patched bytes, surrounding bytes and restored RX permissions checked.
- Existing rematch harness: 63 checks, 0 failures.
- Entire Rematch ASI compiles using Zig 0.16, generic Windows x64 target.
- ASI SHA256: 276653c1d76dc6881060a95583b98409901203c3acec7c57edb291c8db1d8bce.

Build regression from repo root:
```
py -m ziglang c++ -target x86_64-windows-gnu -std=c++17 -O2 -fms-extensions -DUNICODE -D_UNICODE mod/Nioh3BossRematch/tests/hook_patch_harness.cpp -o mod/Nioh3BossRematch/build/hook_patch_after.exe -luser32 -lkernel32
```
Run the generated EXE. The harness uses only its own VirtualAlloc pages.
Offsets 0..14 exercise both atomic widths and the existing spin fallback;
offset 15 is excluded because the existing two-byte head cannot fit an
aligned 16-byte window there. This patch does not change that limitation.

Integration: cherry-pick this fix onto the currently installed full build;
the candidate here is based on 3602481 and is not a replacement player build.
Candidate: mod/Nioh3BossRematch/build/Nioh3BossRematch.asi in worktree
C:/Users/User/projects/nioh3-hook-atomic-10995. PC2 owns exclusive startup
validation. No game files, game processes, saves or locks were touched.
