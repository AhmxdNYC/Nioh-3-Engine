"""Pair Y holds (pad CSV) with player actions (act CSV). Usage: py -I holds.py ACT.csv PAD.csv [--last N]"""
import csv, datetime, sys
def ts(w): return datetime.datetime.strptime(w, "%H:%M:%S.%f")
act, pad = sys.argv[1], sys.argv[2]
last = int(sys.argv[sys.argv.index("--last")+1]) if "--last" in sys.argv else 0
A = [r for r in csv.reader(open(act)) if len(r) >= 6 and not r[0].startswith("#") and r[0] != "t_ms"]
P = [r for r in csv.reader(open(pad)) if len(r) > 10 and r[0] != "t_ms"]
acts = [(ts(r[1]), r[2], r[5]) for r in A]
ys, down = [], None
for r in P:
    y = int(r[4], 16) & 0x8000
    if y and down is None: down = ts(r[1])
    if not y and down is not None: ys.append((down, ts(r[1]))); down = None
def stick(t0, t1):
    m = 0; btn = set()
    for r in P:
        t = ts(r[1])
        if t0 <= t <= t1:
            m = max(m, abs(int(r[7])), abs(int(r[8]))); b = int(r[4], 16) & ~0x8000
            if b: btn.add("%04X" % b)
    return m, sorted(btn)
out = []
for d, u in ys:
    held = (u - d).total_seconds()
    if held < 0.8: continue
    seq = [(round((t - d).total_seconds(), 3), a, f) for t, a, f in acts if 0 <= (t - d).total_seconds() < 1.5]
    heavy = next((s for s, a, f in seq if a == "0x624d"), None)
    ma = next((s for s, a, f in seq if a == "0x629d"), None)
    late = None if heavy is None or ma is None else round(ma - heavy, 3)
    ls, bt = stick(d, d + datetime.timedelta(seconds=0.6))
    out.append((d.strftime("%H:%M:%S"), round(held, 2), heavy, ma, late, "LATE" if late and late > 0.3 else "ok",
                "fps " + (seq[0][2] if seq else "?"), "Lstick max %d" % ls, "other btn " + ",".join(bt), [(s, a) for s, a, f in seq[:6]]))
for o in out[-last:] if last else out: print(*o)
late = sum(1 for o in out if o[5] == "LATE"); print("holds", len(out), "late", late)
