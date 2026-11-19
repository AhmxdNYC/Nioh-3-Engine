"""score.py ARM: Y holds in the replayed recording vs player actions (act-ARM.csv, sent-ARM.csv)."""
import csv, datetime, sys
D = r"D:\nioh3-re\rep-117667"
arm = sys.argv[1]
ts = lambda w: datetime.datetime.strptime(w, "%H:%M:%S.%f")
S = [r for r in csv.DictReader(open("%s/sent-%s.csv" % (D, arm)))]
A = [(ts(r["wall"]), r["act"], r["fps"]) for r in csv.DictReader(open("%s/act-%s.csv" % (D, arm)))]
ys, down = [], None
for r in S:
    y = int(r["buttons"], 16) & 0x8000
    if y and down is None: down = (ts(r["send_wall"]), ts(r["rec_wall"]))
    if not y and down is not None: ys.append((down[0], ts(r["send_wall"]), down[1])); down = None
def act_at(t):
    a = None
    for tt, x, f in A:
        if tt > t: break
        a = (x, f)
    return a
out = []
for d, u, rw in ys:
    held = (u - d).total_seconds()
    if held < 0.8: continue
    seq = [((t - d).total_seconds(), a, f) for t, a, f in A if 0 <= (t - d).total_seconds() < held + 0.5]
    heavy = next((s for s, a, f in seq if a == "0x624d"), None)
    ma = next((s for s, a, f in seq if a == "0x629d"), None)
    late = None if heavy is None or ma is None else ma - heavy
    pre = act_at(d)
    attack = any(a in ("0x624d", "0x6310", "0x629d") for s, a, f in seq)
    fs = [int(f) for s, a, f in seq if int(f) > 0] or [int(pre[1]) if pre else -1]
    out.append((rw.strftime("%H:%M:%S"), round(held, 2), pre[0] if pre else "?", heavy and round(heavy, 3),
                ma and round(ma, 3), late and round(late, 3), "CHARGED" if ma is not None else "FAILED",
                "LATE" if late and late > 0.3 else "", "sync" if attack else "NOSYNC", "fps %d-%d" % (min(fs), max(fs)),
                [(round(s, 3), a) for s, a, f in seq[:5]]))
for o in out: print(*o)
n = len(out); ch = sum(o[6] == "CHARGED" for o in out); lt = sum(o[7] == "LATE" for o in out); sy = sum(o[8] == "sync" for o in out)
lates = sorted(o[5] for o in out if o[5] is not None)
fa = [int(f) for t, a, f in A if int(f) > 0]
print("ARM %s holds %d charged %d failed %d late %d (>0.3 s) sync %d/%d late_ms p50 %s max %s fps %s-%s" % (
    arm, n, ch, n - ch, lt, sy, n, lates and round(lates[len(lates)//2]*1000), lates and round(lates[-1]*1000), min(fa or [-1]), max(fa or [-1])))
