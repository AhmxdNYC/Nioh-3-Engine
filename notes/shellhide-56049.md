# Dark seal-throw figure: fixed part-46 shell and guarded output clamp

Task pc #56049, Ally offline analysis. Base: mode 4 build #4177, source
6692713e. Mode 2 comparison: #4169 / 8dc7560b, existing test #56002.
No game access or installation; ally/0 owns the live test. Installed inventory
provided by fleet: BossRematch acf972c2, EngineFix ab2e42d4, Training 956f7b09.
No OFF/ON performance measurement. This is a source/fixture result, not a
visual pass or real soul-core body swap.

## Finding and certainty

The proven path is the **fixed part-46 armour shell**, selected by the very
B440/B444 values previously called “hide.” B440=1 selects its animation/body
path; B444=1 makes its native opacity full when fade/multiplier are 1.
It does not ignore those fields: their meaning was inverted for this purpose.
There is no action-id check in this opacity path, so action 0x28FD can animate
it while ordinary parts have faded out. No FormKey or LA-type flag is needed
at the part-46 opacity writer.

I inspected archived `captures/ally-55974/six.png`: the top-middle frame has
an armoured humanoid on the player position throwing the golden arc. The
archived tracev.log has B440=1/B444=1 from +0.20 seconds through this interval.
That fits the fixed-shell path. **The precise mesh producing those pixels is
still an inference**, because that capture did not record the part-46 handle
or a draw-call resource. The new shell46 log supplies handle/render/alpha
information for an owner-run confirmation. Static analysis cannot identify
pixels conclusively without that correspondence.

## Verified instructions (RVAs in archived mapped image)

- F3CD6..F3D62 chooses four hard-coded type-5 armour IDs and queues slot 46
  at `activeSet+9580`: 58C2A4, 076D58, 6CCDE0, 295A89. The latter follows
  `(6CCDE0 + FFBC8CA9) & FFFFFFFF`; the older note's 289A89 is a typo.
- 406EC6 first checks 85860(chr,1,1). Its false branch reaches 406EDC,
  testing `[chr+B440]`; nonzero calls 7C5680. That calls 7C57DC to fetch
  the slot-46 object at `[activeSet+9590]`.
- **406EDC/7C5680 is bone copying, not the final GPU draw call.** 7C56F0
  iterates bone indices 134..165 and copies transform data. Skipping it
  would risk stale poses; it is not the proposed hide.
- EFC30 reads owner `[parts]`, where parts=chr+9F0. EFC99 reads chr+B444.
  FDE690 applies ordinary alpha=1-B444 to the ordinary ranges.
- EFD74..EFD8D selects the active set exactly as F5358: use `[parts+9F60]`
  only when nonzero AND dword[parts+9F6C]!=0, else parts+20.
- EFD8D gets part=`[activeSet+9590]`; EFD99 gets render=`[part+20]`.
  EFDA2..EFDAF writes **float[render+C] = B444 * context[248] * context[3A8]**.
  EFDB5..EFDBA decodes model ID as dword[part] >> 4.
- EFE45's 85860(chr,1,0) test is for separate slot **33** (`+6B50`).
  It does not gate slot 46. ext+2470/+248D controls the LA-type/slot-47
  predicate described in formhide4-native-fade-55928.md; changing those
  equipment fields is not a shell hide.
- F5CB0 is `call EFC30`, with RCX=parts, DL=flag, R8=render context.
  Its return is unused. The next instruction reloads RCX=[parts].

## Applicable patch

Apply this commit onto 6692713e (mode 4). It adds an eighth call retarget at
F5CB0 to `HkFh4Alpha(parts, uint8_t flag, context)` in bosslog.cpp. Native
EFC30 always runs with the original arguments. Afterwards, while mode 4
owns the current player, clamp only the recognized slot-46 shell's
**float[[[activeSet+9590]+20]+C] to 0**. Preserve the existing B440=1,
B444=1, root+3A8=0 hold for the root and other parts.

Runtime guards: current FkPlayer and captured owner match; parts==chr+9F0;
current root equals the captured root and context; hide/blend are exactly
1/1.0; seal remains present with a separate root; finite native alpha;
slot-46 handle is one of the four native models; shell render is neither
player nor seal root; attachment/render identities rechecked before write.
Unknown model/read/layout failures do not write the shell and request the
existing mode-4 release path. Missing shell is a valid no-op.

Startup adds exact byte guards EFD74 (75 bytes), F5CA2 (22), EFC41 (25)
to the ten existing guards; validates all eight call targets before any
installation. Partial installation cannot arm mode 4. No knownBuild override,
unchecked raw address, global 85860 patch, equipment write, model-pointer
swap, action cancellation, or persistent alpha flag is introduced.

The output alpha is recomputed by native EFC30 every frame. On release the
hook passes through; the next native call restores its computed value. No
attachment pointer survives in new state, and no obsolete attachment alpha
is written back. Existing B440/B444/root-multiplier restoration is unchanged.
This can take the next native update; it is not a new fade-controller state.
The direct clamp is redundant when the existing root multiplier hold reaches
this writer correctly, and it adds observability/coverage if that ordering
assumption fails. It does not prove mode 4 needed this extra clamp.

## Validation and next owner action

`python3 mod/Nioh3BossRematch/tests/formhide4_offline.py` compiles extracted
production helpers with g++ C++17, -Wall -Wextra -Werror and runs 16 cases:
8 prior lifecycle/restore cases plus native alpha overwrite, active-set
selection, disabled override, unrelated actor, release/recompute, unknown
model refusal, shared-root refusal, and resolving replacement attachments.
All 13 byte guards and 8 call targets match archived Nioh3.mem.bin SHA256
f8a4fb31b691aaf9bb829fca4a32b49a4ad4c1c780f6c11cc516108b75b5d968.
These are local-buffer checks, not an MSVC build, hook ABI/thread validation,
or runtime invisibility proof.

Next: pc reviews/cherry-picks, builds MSVC, assigns one test owner after its
current run. Retain FormHide=4, FormHidePress=2, FormHideSealAct=28FD and
existing end/pin settings. Check hooks=8/8 and shell46 id/nativeAlpha/zero
logs against frames at +1.3..1.6 seconds; inspect all frames and release.
If nativeAlpha is already zero while the figure remains, reject part-46 alpha
as the remaining visible layer; investigate slot 33/47, effects or a later
writer. Do not call this a complete transform: the player must become the
yokai, attacks must work, and video must be delivered. Apply all gameplay,
control, fullscreen and focus gates to any new measured interval.
