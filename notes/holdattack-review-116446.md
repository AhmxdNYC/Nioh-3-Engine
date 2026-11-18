# HoldAttackFix review (pc/0 Opus, 2026-11-18 8:40 am EST, human #116173 "leave for Claude")

Builds: df5e4376 (EF 5cfd07b6, installed on Main PC with HoldAttackFix=1), 6b848b20 (trial E05C53A9, the 13/15 count).

Verdict: NOT a proven fix. Do not ship.

1. The patched code is not shown to be the strong-attack charge. Per gx-pc #115810 the slot it tests
   (+0x3D4/+0x3D8) is copied from report bytes +0x1C/+0x1D, which follow four stick bytes seeded 128 and are
   seeded 0 with range 0..255, and the copy only runs for controller class 8..11. That layout is analog
   triggers (L2/R2) of a PlayStation-style report. With an Xbox pad the slot is always 0, so "fix off = 0
   finished" is the normal value of that counter, not evidence that holds fail.
2. The measurement is circular. 6b848b20 makes XInput Y set bpl, then counts the sete edge at 0x4362B5 that
   this same patch produces. 13 of 15 = "Y held longer than the stock window", which is true by construction.
   It says nothing about which attack the game played.
3. Hardcoded: XInput pad 0, button 0x8000. Keyboard, PS pads, remapped Y and pad index != 0 get nothing;
   on a remap it would fire on the wrong action. Breaks the no-hardcode rule.
4. Human feeling "holds work" on 5cfd07b6 is unconfirmed: gx-pc #116074 says that build's finished count stayed 0.

What would prove a fix (pass criteria for the next live test):
- Use the game's own action/move id for the PLAYER (same move logger Training uses for bosses), not our counter.
- Same recorded pad file, fix OFF, at 60 fps cap and at uncapped fps: count Y holds that play the charged move id
  vs the tap strong move id. If 60 fps gives ~all charged and uncapped gives taps, it is an fps bug and the real
  window is 0x43619C (X*fps/60). Only then design a fix there, on the action layer, not on a pad bit.
