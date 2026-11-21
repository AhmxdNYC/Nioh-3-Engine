# Player step / move-speed writer (pc/1 Opus 5.5, 2026-11-21 6:40 pm EST, TASK #128506, human #128465)

Static (re/Nioh3.dump.exe + decomp_ro) and live read-only check on Main PC (slot 2's run, pid 12988, EF 0c05109e TR f3444ad8).

- Tick FUN 0x20DAD4(ctrl, res, slot s), per slot:  d = base x mult;  [ctrl+0xDC4+s*4] = d (move speed, 1.00 normal);
  step [ctrl+0x40+s*4] = d x [ctrl+0xDC0] (frame scale = 60*dt, 0.60 at ~100 fps), store at 0x20DE6C.
- mult = [ctrl+0x3CC+s*0x20], duration [ctrl+0x3D0+s*0x20] (remaining at +0x3C8); reset to 1.0 when +0x3D0 <= 0.
- mult writer = FUN 0x133AF0: up to 5 speed keys in the move resource, value FUN 0x133CF8(res,i) (percent), key time
  FUN 0x7C6160. A key inside [t, t+delta) sets mult = pct/100 and duration = end - key t (bit 16 of [ctrl+0xA8] measures from t).
- base = FUN 0x134E68: product of 19 modifiers ptr[ctrl+0x6B8+i*8] (active if [obj+8] > 0, factor [obj+0xC]; vtables
  0x38FEEF8/0x38FEF30/0x38FEFA0, 0x38FEF68 gates on byte [obj+0x10]) x world scale FUN 0x13555C x [[[ctrl+0xE0]+0x3B0]+0x684]
  (flagged); partner's +0xDC4 when synced; 0 while [ctrl+0x488] or [ctrl+0x48C] > 0 (freeze).
- Live: move 0x803 mult 0.500, timer 3.00 -> 1.00 over t 0.6 -> 3.4, dc4 0.500, step 0.30 at dc0 0.60. Other moves mult 1.000.
- Poller: tools/testmatrix/modpoll.py OUT.tsv SECONDS (read-only, no input). Log D:/nioh3-re/live-128506/pc2run.tsv.
- Open: GH #157 0x61D8 at 0.6x is either a designed key (mult 0.60) or a modifier; the Player log why line will tell.
