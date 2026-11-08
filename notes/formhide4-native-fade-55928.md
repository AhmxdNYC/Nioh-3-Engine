# Full-player hide: native fade and FormHide 4 proposal

Task #55928. Offline Ally analysis, based on `pc/yokai-combo` 3d1dcd55.
No game access, installation, ASI build, benchmark or visual pass. The revised
source is experimental and awaiting the pc MSVC build and owner-run test. Existing device build reported
by the fleet: BossRematch dbeb1d3c, EngineFix ab2e42d4, Training 956f7b09;
those installed files were not read or changed for this task.

## Finding

There is **no B440/B444/85860 flag combination that zeros both complementary
part opacities** when the common fade multiplier is positive. B444 is a body
crossfade, not a universal visibility switch. For ordinary parts `alpha=1-b`;
for special slot 46 `alpha=b*k`; slot 33 uses `b*k` when 85860 is true and
`max(b,1-b)*k` otherwise. Here `b=chr+B444`,
`k=renderContext[+248]*renderContext[+3A8]`. Setting b=0 fixes the true branch
but restores ordinary parts and leaves the false branch at k. Setting b=1
hides ordinary parts but leaves both special branches at k.

Use the separate **native fade controller** instead. `Character::FadeOut`
(binding RVA 238DC40, call 238DCC8) resolves the character's `[chr+20]` render
object and calls `2BC5DC(renderObject+240, target=0.f, duration)`.
`Character::FadeIn` at 238DB64 calls the same setter twice, first zero immediately,
then target 1 over the requested duration. These are character fades, not the
screen-wide Graphics::BlackFadeOut functions.

Controller layout (five floats): +240=start, +244=target, +248=current,
+24C=elapsed, +250=duration. Setter 2BC5DC with duration 0 makes current=target
immediately. Update 27E92C/27E986 and generic update 468FC/4692A retain target
when duration=0. Therefore clearing only +248 is insufficient: next update
can restore a nonzero +244. Zeroing the complete controller is a stable zero
until another writer explicitly changes it. It is equivalent to the zero
endpoint of the native fade, with an irrelevant start=0 for duration=0.

## Special branch and state flags

`85860(chr,1,0)` at EFE45:
- Requires non-null ext=`[chr+3A0]`.
- Tests index `ext+2470` in [0,1], `ext+248F==0`, `ext+248D[index]!=0`.
  Cold block **192F6FE sets AL=1, then rejoins 8589F**; it is not an early return.
- Because argument DL=1, 858A3..858BE replaces AL with whether the active set's
  slot-47 pointer at **activeSet+98D0** is non-null. Thus the earlier ext flags
  do not force true at this call site.
- Requires `chr+9D4==0` and absence of the excluded global context checked
  through 4709B50 (compare type address 44C9140).
- Third argument is zero here, so the ext+188 bit 1 / B440 conditional at
  858FA..85924 is skipped. Setting the LA bit does not fix this branch.
- The active set is `[chr+A950]` only when non-null and `DWORD[chr+A95C]!=0`;
  otherwise `chr+A10` (same selection as F5358 on `chr+9F0`).

Do not manufacture a slot-47 pointer or alter LA, cutscene, death, or global
mode flags to force this predicate. With common fade current=0, BOTH its
branches and the other special slot give zero regardless of its result.

## Other native writers and limits

- EFC30/FDE690 write part renderObject+C every update. A one-shot alpha write
  can be undone. Part objects are `*(activeSet+i*340+10)`; render object is
  `*(part+20)`. Special pointers +6B50 and +9590 are exactly indices 33 and 46.
- 40169C calls 401A78 (ordinary part alpha), sets root renderObject+C at 40199F,
  calls 400B64 (extra part scaling), then F54B4 -> EFC30 at F5CB0.
- Root +3A8 is independently regenerated at 567568/56778C, including factors
  from chr+8C0 and chr+B3B0. Do not use a one-shot +3A8 write as the latch.
- Scripted/cinematic fade sequence 28B23CC reaches the same +240 setter at
  28B2527 and 28B25ED. The actor fade-out helper FDBCA0 similarly writes the
  full +240 controller to target 0. Its caller family is not proven to be a
  death-only path. No death-state flag recommendation is justified here.
- Multiple consumers form effective opacity from +3A8 * +248 * +270 * +C
  (16045D..160475, 5C219F..5C21BA, 5E939E..5E93B6). This supports the zero-fade
  mechanism but does NOT prove every visual attachment inherits root fade.

## Mode 4 revision after pc Opus cross-check #55953

The first review diff (5b9469ec / ZIP #4171) zeroed fade controllers on all
occupied parts. **Superseded; do not apply that diff.** This revision implements
pc's narrower two-scalar approach directly in bosslog.cpp, with a refreshed
base-relative patch alongside it. It preserves the native +240 controller;
FE37A0/FE37D9 tests its +248 value to produce a fade boolean.

Hold B440=1 and B444=1.0, and root=`[player+20]`, float[root+3A8]=0.
The 17 ordinary entries in EFC30's three ranges become `1-1=0`; both special
entries (absolute indices 33/46, pointers +6B50/+9590) become `1*k6*0=0`.
This works for both 85860 outcomes. Keep b exactly 1: b=0.9999 would leave
ordinary alpha about 0.0001 and is NOT silently enabled as a fallback.
The root+C snap to 1 at 40198E is expected; its independent zero k8 multiplier
is the chosen root suppression. Unmapped parts and shadows remain unproven.

Six guarded call sites into 40169C invoke the hold BEFORE and after the native
part update. A seventh hook at **162200 -> 567568** preserves the native
computation, then zeros +3A8 again. This addresses the independently found
writer **56778C**, which regenerates +3A8 from state-dependent factors. Native
call arguments at 1621FA/1621FD are RDX=root and RCX=character. The wrapper
records the native result, including zero, so restoration uses the latest
observed native multiplier instead of an obsolete start-of-hide value.

No global predicate patch or death/cutscene/LA flag modification. No slot or
part fade-controller writes. Startup requires all seven call-site guards and
field-bearing opcode guards; partial hook installation cannot arm the hold.
A shared player/seal root, invalid read, nonfinite value or model replacement
ends the hold. Snapshot B440/B444 and root identity before writes. Restore
only if the current player/root still match and the held values remain ours;
failed writes retain the backup for retry. Player replacement abandons stale
pointers. Root replacement never writes the replaced object. The native body
part update then resumes its ordinary opacity calculation after release.

The input path supports FormHidePress=2 (known seal action) and the existing
no-seal timeout/capture adoption/end observation/pin lifecycle. Runtime mode
4 enabling without startup hook installation is intentionally unavailable.
The hold logs B440/B444/k6/k8 once per second. It is a hide+puppet fallback,
not the requested real body swap, and has no visual pass.

Recommended owner configuration: FormKey=1, FormSlot0=9, FormHide=4,
FormHidePress=2, FormHideUntilEnd=1, FormPin=1, SealDefer=1. Compile on pc,
then one test owner records +0/+10/+20 seconds and restoration; also inspect
special-part +C after the update, shadows, moving/attacking and all video
frames. Verify timeout, mode off, root/player replacement and death/load.
Apply the mission/control/fullscreen/focus gate for every measured interval.
Pass requires no human visible in any frame, yokai at the player, attacks
working, and video delivered. No OFF/ON performance comparison was made.

## Reproduction / checkpoint

Static input: archived `/var/home/bazzite/tmp-archive/re-dump/Nioh3.mem.bin`,
SHA256 f8a4fb31b691aaf9bb829fca4a32b49a4ad4c1c780f6c11cc516108b75b5d968.
RVAs apply to this mapped image only. Proposed call sites and offset-bearing
instructions are byte-checked; unknown layouts must skip mode 4.
The existing decompile-derived label for 85860 in symbols.tsv is low-confidence;
the branch account above comes from the actual instructions, including cold code.

2026-10-30 12:11 pm EDT | Native fade trace revised per #55953 to B444=1 +
root+3A8=0; seven guarded hooks and eight offline cases; visual behavior unproven | Next: merge the revised source into
pc/yokai-combo, MSVC build, give one owner a gated visual/restore test.

Validation of the revised source: base-relative patch reverse-applies to the
working source; added helpers compile with C++17 warning-as-error declaration
stubs. `python3 mod/Nioh3BossRematch/tests/formhide4_offline.py` builds and runs
8 local-buffer cases: press arm, pre/post native part-update hold, native
multiplier overwrite/reassert, unrelated actor unchanged, exact restoration
with k6 preserved, no-seal timeout, root replacement, player replacement.
This does NOT validate MSVC ABI, actual game threads, hook installation or visuals.
The six 40169C callers ignore its return value; 567568 is also used as a void
setter at 162200. Default mode is unchanged. No game files were touched.

Remaining limits: allocator reuse without a generation counter; arbitrary
native writes outside the mapped call sites; model attachments outside the
traced ordinary/special branches; effects and shadows; first rendered frame
before action recognition. See the owner-run checks above. A readback or an
offline fixture pass is not a completed invisibility feature.
