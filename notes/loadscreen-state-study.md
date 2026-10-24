# Cross-world Fight now: loading widget, black fader and sequencer tail (static study, game NOT run)

Inputs: re/Nioh3.dump.exe (Ghidra decomp.cmd + capstone), probe run captures/pc/cover/{training,lum,shot}_clsend*
(pc/loadscreen-end 2de6775, LoadingScreenEndMs 0, CoverReleaseMs -1). RVAs, image base 0x7FF74F410000 in Ghidra names.

## Answer in one line
Nothing re-asserts the loading widget. The manual UiLoadingScreenEnd **did** close it (shot_clsend_1+10s has no coin icon;
shot_ccov2_1+10s has it). The screen stays black because of a **second layer: the game's black fader**, faded out at
the warp start and faded back in only in sequencer state **0x12** (223ACF5 `BlackFadeIn(0, 30.0f, 0.0f)`).

## Layer 1: the loading widget [[45C84A8]+748h] (coin icon)
- vtable **40829C0**: +30h open 70781C, +38h close **3B1B90**, +50h canOpen 6376B8, +58h canClose 3B1C80, +348h 637584.
- State dword **+2Ch**: 0 = never opened, 1 = open (70781C writes 1), 3 = closed (7077B8 writes 3).
- UiLoadingScreenEnd 3B1B6C -> vfunc +38h(0) = 3B1B90: acts only if 3B1C80/7060C8 says +2Ch is not 0 and not 3, then
  7077B8 (+2Ch = 3) and plays layout anim 0x4B. A second close is a no-op (idempotent).
- UiLoadingScreenStart 6372A0 -> 6372C4: opens if canOpen or +2Ch == 3. In the sequencer path only **seq 1 223BDD5**
  calls it (other caller in 223Axxx is the game-over seq 223A297). So nothing reopens it during seq 2..0x11.
- Sequencer close sites: 0xB **223BE53 only if tips byte 45BC467 != 0** (with TipPageSetLoadDone 1D010E4), **0x12 223AD10
  unconditional**, 0x15 223AE25. For cross-world Fight now the visible close is the 0x12 one (same frame as BlackFadeIn).

## Layer 2: the black fader (the real last releaser)
- Graphics::BlackFadeOut 239EB18 -> **3456E0**(id, float time); Graphics::BlackFadeIn 239EA7C -> **345508**(id, float time,
  float p3). Both build a request and submit via 345784 -> 10F8990 into the fader object **[4758BA0]**.
- Fade out: warp pre-step **223CC40** (run from seq 0x14 via 223A504): control-off reason 0xFE (9DC6D0 on [45BBD58]),
  then 223CD4F `BlackFadeOut(0, 30.0 or [45BC474])` if byte 45BC460; sub-state 0xB waits 9BB40C(0,0xB), A504CC([45C84C8],1),
  then seq 22 -> 1. Matches the log: dark at click +0.3..0.6 s.
- Fade in: **seq 0x12 223AC50**: timer 4 dt60 (34 if area byte == 0xC0), then if [[45B9E40]+48h]+110h == 0 &&
  [45B9E40]+26B8h == 0 && byte 45BC461 && (seq+11h || (45BC424 == 0 && 45BC428 == 0)): `BlackFadeIn(0, 30.0f, 0)` at
  223ACF5 (else camera +13Ch = 0 path), then UiLoadingScreenEnd 223AD10. 30 dt60 = 0.5 s ramp.
- clsend Shingen lum: overlay released 21:19:09.94, lum stays 2.6 (pure black) until 12.75 (seq 8->9), 4.0-6.6 until 13.5,
  then 36 at 13.82 / 55 at 14.75 = seq 0x12 (13.37) + 0x13 + 0.5 s fade. The small 4-6.6 bump at seq 9 is unexplained
  (not the widget, already closed).

## Sequencer tail (what keeps seq from 0xB, clsend Shingen, s after click)
| seq | handler | waits on | time |
|---|---|---|---|
| 5 | 223AF50 | 1FC6674(stage+500h) && LoadIsLoadEnd 3B3424 (G8 placeables + G9 loader) | 1.2 -> 17.0 (**15.8 s**) |
| 6 | 223C53C | 1C1C61C actors +138h == 8 | 0.4 s |
| 8 | 223C4A8 | LoadIsLoadEnd 3B3424 (log gates 0x100 = G9, 0x108 at end) | 17.4 -> 24.9 (**7.5 s**) |
| 9 | 223C684 | 282B5C(0) == 0 (list [45CE800]+40h, stride 308h); -> 0xA if 1C9DBA0 | 15 ms |
| 0xB | 223BE08 | 219F3A0: [47566E8]+28h/+30h == 0 | 15 ms |
| 0xC | 223BEA8 | tip page only if 45BC467 | 16 ms |
| 0xF | 223C308 | timer 30 dt60 (100 / +60 variants), then StopFlag(2,0) = GAMESTATE[2] 0, control reasons 0x30/0x65/0x4F/0x3F cleared | 0.5 s |
| 0x10 | 223C2D4 | 1C1C5C4([45B9E40]) == 0 | 31 ms |
| 0x12 | 223AC50 | timer 4 dt60, BlackFadeIn + widget close | 0.1 s + 0.5 s fade |
After seq 8 everything is ~1.2 s of fixed timers; the black is entirely seq 5 + seq 8 (gates already in
notes/seq-state6-gate-RE.md, P1/P2 there are still the only way to make the GAME reach 0x12 sooner).

## Smallest testable change (Training, extends the LSEND probe on pc/loadscreen-end)
At the existing LSEND site (ArenaTick, cross-world, g_ar.tReady + LoadingScreenEndMs, game thread), add after the 3B1B6C call:
```c
((void (*)(uint32_t, float, float))(B + 0x345508))(0, 30.0f, 0.0f);   // BlackFadeIn, same args as seq 0x12 223ACF5
FadeWant(0, "lsend");                                                  // drop our own overlay at the same time
```
Keep CoverReleaseMs -1 (GAMESTATE[2] untouched, so seq 5/8 are not slowed) and the BhStart boss hold (boss AI stays off
until seq >= 16). Measure lum + shots as for clsend, 1 run per boss on pc.
- Expected: picture ~0.5 s after the call: Shingen ~+7 s instead of 25.5 s (OFF 22-26), Otoroshi ~+10 s instead of
  14.8-17.8 s. The game's own 0x12 later re-issues BlackFadeIn/close: no-ops.
- What you will see: the arena with props still streaming in (3,080 placeable loads at ready), player frozen until our CONTROL
  release / seq 0xF (GAMESTATE[2] = 1, control-off reasons still set), boss idle. If that is acceptable, pair it with
  an earlier CONTROL release later; the input stays blocked by the game's control-off reasons until seq 0xF either way.
- If lum stays black after this: the scene itself is not drawn before seq 0x12 (render-side), not a fade. Next step then:
  read the fader object [4758BA0] (+8 / +0Ch / +10h request fields written by 10F8990) once per second to confirm alpha 0.
- Risk: BlackFadeIn also clears camera +13Ch (345AF0 / 2BEDDC target); same as the native call, but issued while seq is 5.
