"""hsumm.py RUNDIR... : per arm launch, per 60 s window (from the cpu json UTC bounds) -> process cores, est fps, 1%/0.1% low
from EF HITCH lines (HitchLogMs 20) + the frame budget, hitch counts, mean game-thread CPU ms per hitch. (#103509; PresentMon
sees no Nioh3 presents on Main PC since 11-13, TECH_DEBT.)"""
import datetime as dt, glob, json, os, re, statistics, sys
H = re.compile(r"^\[(\d\d):(\d\d):(\d\d)\.(\d{3})\] HITCH #\d+ gap ([\d.]+) ms \| gt cpu ([\d.]+) ms")
TH = re.compile(r"HitchLogMs = (\d+)")
def sod(h, m, s, ms=0): return h * 3600 + m * 60 + s + ms / 1000
def arm(d):
    log = os.path.join(d, "Nioh3EngineFix.log")
    if not os.path.exists(log): return []
    txt = open(log, encoding="utf-8", errors="replace").read().splitlines()
    th = next((int(m[1]) for l in txt for m in [TH.search(l)] if m), 100)
    hs = [(sod(*map(int, m.groups()[:4])), float(m[5]), float(m[6])) for l in txt for m in [H.match(l)] if m]
    out = []
    for j in sorted(glob.glob(os.path.join(d, "?-[0-9]-cpu.json"))):
        c = next(x for x in map(json.loads, open(j)) if x.get("kind") == "summary")
        t0 = dt.datetime.fromisoformat(c["start_utc"]).astimezone(); t1 = dt.datetime.fromisoformat(c["end_utc"]).astimezone()
        a, b = sod(t0.hour, t0.minute, t0.second, t0.microsecond / 1000), sod(t1.hour, t1.minute, t1.second, t1.microsecond / 1000)
        w = [g for (t, g, _) in hs if a <= t < b]; gc = [c2 for (t, _, c2) in hs if a <= t < b]
        span = (b - a) * 1000; n = len(w) + max(0.0, span - sum(w)) / 16.667
        def low(q):
            k = max(1, round(n * q)); top = sorted(w, reverse=True)[:k]; top += [float(th)] * (k - len(top))
            return round(1000 / statistics.mean(top), 2), len(w) >= k
        l1, e1 = low(.01); l01, e01 = low(.001)
        out.append({"win": os.path.basename(j)[:3], "cores": round(c["cpu_cores"], 3), "fps": round(n / (span / 1000), 2),
                    "low1": l1, "low1_exact": e1, "low01": l01, "low01_exact": e01, "h20": len(w),
                    "h33": sum(g >= 33 for g in w), "h50": sum(g >= 50 for g in w), "gtcpu_mean": round(statistics.mean(gc), 1) if gc else 0,
                    "thresh": th})
    return out
for run in sys.argv[1:]:
    for d in sorted(glob.glob(os.path.join(run, "*-[AB]"))):
        for r in arm(d): print(os.path.basename(d), json.dumps(r))
