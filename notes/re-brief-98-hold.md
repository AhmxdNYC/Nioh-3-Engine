# RE brief: GitHub #98 hold-move misfire (heavy -> martial art)

Status 2026-11-16 7:15 pm EST, reports-pc/0. Waiting on the human's DECISION (pc #111383): Claude RE for #98 only, or
DeepSeek credit. Whoever is assigned starts here. No game run is needed for the static part.

## Symptom
- Hold Y (heavy). Normally the heavy act **0x624D** switches to the martial art act **0x629D** about **195 ms** after the
  heavy starts (caught). Sometimes it switches only at **468-484 ms**, which matches the full heavy length (late = the
  hold branch did not fire, so the MA starts when the heavy ends).
- Only with EngineFix `[VRR] Tier120=1` (game's 120 fps tier running on our variable-rate ticks). Ally A/B v1: 5/40 late
  with it on, 0/38 with it off. v2: 4/40, v3: 6/40. The human rejected the 60 fps workaround (Plan B, #111057).
- 60 fps control on the v3 build (pc #111366) is running/queued on the Ally. If it is also ~6/40, the bug is not 120-only;
  re-read the player report before patching.

## What the trace ruled out (v1 #8137, v2 #8143, v3 #8153; tools/re/holdtrace_*.py on reports/98-trace 7614f228)
Recorded once per call of the input tick hook 2E6C34 (several calls per drawn frame, deduped to ~16 ms game frames):
ctrl[0..800h] (ctrl = [[pchr+3B0]+8]), AI block [pchr+3B0][0..400h], input state a[0..2000h] (command word +1068h),
act row [ctrl+E8][0..40h], character pchr[0..1000h], XInput pad bits, EF ticks120/ticks60/fps_i/tier.
- No u8/u16/i32/f32 field has a fixed value that separates late from caught holds from -50 to +190 ms (analyzer separation pass).
- No value or narrow value window is reached by every caught hold and missed by every late hold within 0-260 ms of
  the heavy start (holdtrace_window.py). This also rules out "an animation timer steps over a narrow window".
- Tick steps per frame are the same on late and caught holds (mostly 2, an occasional 1 or 3; holdtrace_ticks.py).
  ticks60 stays 0 under Tier120. fps_i 48-84 and frame-time spikes are the same on both.
- Y press lead: early Y (>=30 ms before the heavy) was 3/4 late in v2 but 0/6 late in v3, so it is refuted.
=> the branch decision reads state outside these regions (or inside a sub-step we never see between two records).

## Targets (in order)
1. **Who sets act 0x629D during a held heavy.** Find the writers of [ctrl+E8] (the current act row pointer; act id =
   dword [[ctrl+E8]]) and the action-transition function they sit in. Break/trace on the transition into 0x629D from 0x624D:
   the caller chain is the hold branch.
2. **The hold condition inside that branch.** Expected shape: "button still held" AND "act time inside window"
   (anim frame counter float [ctrl+78] is the action frame counter, see bosslog.cpp ~4909). Note which input it reads
   (a latched/buffered command, not the raw pad) and which time base: per tick (ticks120 / G+7D8 delta), per drawn frame,
   or a 60 Hz delta (EF caves P1/P7 GetDelta60 at vrr_getDelta60). A window evaluated with a 60 Hz delta while Tier120
   feeds 120 Hz ticks is the prime suspect.
3. **The fallback**: the MA at heavy end (~475 ms) is the normal chained input; confirm it is not a separate timer.
4. Compare the branch's time math with EF's VRR caves (mod/Nioh3EngineFix/src/caves.asm, vrr_acc120 / vrr_acc60 carries,
   vrr_scale = fps_i * interval). If the branch reads ticks60 (always 0 under Tier120) or a scaled delta, that is the patch.

## Known addresses (v2.02 exe, RVA)
- 2E6C34: input tick (HoldTrace hook site, BR). 358528: PlayerActId inlined reader. 134E68: ComputeMotionSpeed(ctrl, layer, flag).
- ctrl+E0 = actor, ctrl+E8 = act row ptr, ctrl+78 = action frame counter (float), ctrl+EA0 = i-frame flag.
- EF VRR: G+7D8 frame start ns, near page ticks120/ticks60, N3HoldTraceVrr export.

## Pass for the RE step
A named function + condition (address, which input and time fields it compares) that explains the late holds and is tied
to Tier120. Then reports-pc builds the patch (or a v4 per-tick record at exactly those fields) and pc routes the Ally A/B.
