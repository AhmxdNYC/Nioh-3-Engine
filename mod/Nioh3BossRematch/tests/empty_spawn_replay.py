#!/usr/bin/env python3
"""Replay the 11:19-11:26 pm soul-core orders from Temp/h72299.

A later spawn replaces a live seal only when LaterSpawnReplacesLiveSeal says it is a new seal:
a motion, or a distance of 1 m or more. motion 0 under 1 m does not end the form.
A missing distance is not far. A body already kept for this seal stays kept.
A capture of a different body still ends the form.
Never touches the game.
"""
import re
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
src = (here.parent / "src" / "bosslog.cpp").read_text(encoding="utf-8")
root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("C:/Users/Ahmad Hamza/nioh3-engine")
log = root / "Temp" / "h72299" / "BossRematch.log"

fails = []

def check(ok, what):
    print(("  ok   " if ok else "  FAIL ") + what)
    if not ok:
        fails.append(what)

def replaces(motion, dist):
    if motion != 0:
        return True
    if dist >= 1.0:
        return True
    return False

def keep_capture(noted, motion, dist, already):
    if already:
        return True
    if noted and not replaces(motion, dist):
        return True
    return False

fn = src.find("static bool LaterSpawnReplacesLiveSeal")
use = src.find("!LaterSpawnReplacesLiveSeal(noteMot, noteDist)")
cap_end = src.find('reason = "capture-change"', use if use >= 0 else 0)
check(fn > 0 and use > fn and 0 < cap_end - use < 2200,
      "an empty later spawn is rejected before capture-change")
check("g_fkSealDist = dist;" in src, "spawn distance is kept with the motion")
check("KeptExtra(" in src and "already kept" in src,
      "a body already kept for this seal stays kept when the note is gone")
check("ClearKeptExtras" in src, "kept bodies are forgotten when the form ends")

check("dist >= 1.f" in src, "the live gate keeps a motion-0 spawn under 1 m")
check("dist=%.2f keep" in src and "dist=%.2f new seal" in src,
      "the keep-or-new decision logs the extra body's distance")
check(not replaces(0, 0.0), "motion 0 dist 0.0 does not replace the live seal")
check(not replaces(0, 0.2), "motion 0 dist 0.2 does not replace the live seal")
check(not replaces(0, 0.7), "motion 0 dist 0.7 does not replace the live seal")
check(replaces(0xC52, 0.0), "a spawn with a motion is a new seal even at dist 0")
check(replaces(0, 1.0), "motion 0 at 1 m is away and is a new seal")
check(replaces(0, 1.5), "motion 0 at 1.5 m is a new seal")
check(not replaces(0, -1.0), "a missing distance does not count as far")
check(keep_capture(True, 0, 0.0, False), "first look at a motion-0 body under 1 m is kept")
check(keep_capture(False, 0, -1.0, True), "the same body stays kept after the note is gone")
check(keep_capture(True, 0, -1.0, False), "a noted body with no distance is kept")
check(not keep_capture(False, 0, -1.0, False), "a different body with no note still ends the form")
check(not keep_capture(True, 0x4F4D, 1.9, False), "a different capture with a motion still ends the form")

if not log.exists():
    print(f"missing {log}")
    sys.exit(2)

spawn_re = re.compile(
    r"\[(\d\d:\d\d:\d\d\.\d+)\] FormKey seal spawn hit .* chr=([0-9A-F]+) desc\[0\]=([0-9A-F]+) motion=([0-9A-F]+) dist=([0-9.]+)"
)
begin_re = re.compile(
    r"\[(\d\d:\d\d:\d\d\.\d+)\] FormHide=4 begin player=\S+ seal=([0-9A-F]+) "
)
end_re = re.compile(r"\[(\d\d:\d\d:\d\d\.\d+)\] FormHide=4 end (\S+)")
pad_re = re.compile(r"\[(\d\d:\d\d:\d\d\.\d+)\] FormBlockPad: released, (\d+) blocked")

def sec(ts):
    h, m, s = ts.split(":")
    return int(h) * 3600 + int(m) * 60 + float(s)

spawns, begins, ends, pads = [], [], [], []
for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
    m = spawn_re.search(line)
    if m:
        spawns.append((sec(m.group(1)), m.group(1), m.group(2), m.group(3), int(m.group(4), 16), float(m.group(5))))
        continue
    m = begin_re.search(line)
    if m and m.group(2) != "0":
        begins.append((sec(m.group(1)), m.group(1), m.group(2)))
        continue
    m = end_re.search(line)
    if m:
        ends.append((sec(m.group(1)), m.group(1), m.group(2)))
        continue
    m = pad_re.search(line)
    if m:
        pads.append((sec(m.group(1)), int(m.group(2))))

def after(rows, t0, t1):
    return [r for r in rows if t0 < r[0] <= t1]

cuts = []
for t, ts, seal in begins:
    end = next((e for e in ends if e[0] >= t - 0.001), None)
    if not end:
        continue
    later = after(spawns, t, end[0] + 0.02)
    empty = [s for s in later if not replaces(s[4], s[5])]
    if end[2] == "capture-change" and empty:
        pad = next((p for p in pads if abs(p[0] - end[0]) < 0.02), None)
        cuts.append((ts, seal, empty[0], pad))

print(f"capture-change cuts that an empty later spawn explains: {len(cuts)}")
for ts, seal, sp, pad in cuts:
    print(f"  {ts} seal {seal} then {sp[1]} desc {sp[3]} motion {sp[4]:X} dist {sp[5]} pad {pad}")
check(len(cuts) == 2, "two failed forms, both an empty later spawn")
check(all(c[2][3] == "C17E3" and c[2][4] == 0 and c[2][5] == 0.0 for c in cuts),
      "both extras are C17E3 with motion 0 and dist 0.0")
check(all(c[3] is not None and c[3][1] == 0 for c in cuts),
      "both cuts released the pad with 0 blocked reads")

# 23:25:20 both spawns land before the begin, and the form runs to seal-despawn.
full = [b for b in begins if b[1].startswith("23:25:20")]
check(len(full) == 1, "23:25:20 has one real begin")
if full:
    t = full[0][0]
    end = next(e for e in ends if e[0] >= t)
    before = [s for s in spawns if t - 1.0 <= s[0] <= t]
    check(len(before) >= 2 and end[2] == "seal-despawn",
          "both captures before the bind still finish as seal-despawn")
    check(not after(spawns, t, end[0]), "that full form has no later spawn to reject")

# #73244: the same body was kept, then cut when a later world spawn replaced the one note.
live = root / "Temp" / "h73203" / "Nioh3BossRematch.log"
check(live.exists(), "h73203 log is present")
if live.exists():
    dec = re.compile(
        r"FormHide=4 later spawn ([0-9A-F]+) motion=([0-9A-F]+) dist=([-0-9.]+) (keep|new seal)"
    )
    rows = []
    for line in live.read_text(encoding="utf-8", errors="replace").splitlines():
        m = dec.search(line)
        if m:
            rows.append(m.groups())
    pairs = [(a, b) for a, b in zip(rows, rows[1:])
             if a[0] == b[0] and a[3] == "keep" and b[3] == "new seal" and float(b[2]) < 0]
    print(f"h73203 same-body cuts after a keep: {len(pairs)}")
    for a, b in pairs:
        print(f"  {a[0]} dist {a[2]} keep, then dist {b[2]} {b[3]}")
    check(len(pairs) == 2, "two forms kept a body and then cut that same body")
    check(all(keep_capture(False, 0, float(b[2]), True) for _, b in pairs),
          "each of those second looks stays kept")

print("\nFAIL: %d" % len(fails) if fails else "\nALL OK")
sys.exit(1 if fails else 0)
