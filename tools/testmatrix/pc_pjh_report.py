"""pc_pjh_report.py - PJH boss-route A/B report (performance-pc #96049).

    py -I tools\\testmatrix\\pc_pjh_report.py <arm dir> [<arm dir> ...]

Per arm dir (tools\\testmatrix\\pc_pjh_arm.ps1): window = trace 'fight:' .. 'retry mode done' (wall clock); PresentMon rows are
placed on the wall clock from the pm.err creation time (PresentMon session start; CPUStartTime / TimeInSeconds are relative to it).
Window starts at the "-> PASS" (fight ready, after the load). Frames in the window: count, > 33 / 50 / 100 ms, worst, p99, p99.9 (all frames, loads included: the 2 retry reloads are in every arm).
Engine log: SPR runtime gfx PSO creates in the window (+N, >= 1 ms), max create us, prewarm + PJ status lines.
"""
import csv, datetime as dt, glob, os, re, statistics, sys


def tod(s):
    h, m, x = s.split(":"); return int(h) * 3600 + int(m) * 60 + float(x)


def arm(d):
    tr = open(glob.glob(os.path.join(d, "trace.txt"))[0], encoding="utf-8", errors="replace").read().splitlines()
    t0 = t1 = None
    for l in tr:
        m = re.match(r"\[(\d\d:\d\d:\d\d)\] (.*)", l)
        if not m: continue
        if "  -> PASS" in m.group(2) and t0 is None: t0 = tod(m.group(1))
        if "retry mode done" in m.group(2): t1 = tod(m.group(1))
    res = {"arm": os.path.basename(d), "window_s": None}
    if t0 is None or t1 is None: res["err"] = f"window not found t0={t0} t1={t1}"; return res
    res["window_s"] = round(t1 - t0)
    pmst = dt.datetime.fromtimestamp(os.path.getctime(os.path.join(d, "pm.err")))
    pm0 = pmst.hour * 3600 + pmst.minute * 60 + pmst.second + pmst.microsecond / 1e6
    ft = []
    with open(os.path.join(d, "presentmon.csv"), newline="") as f:
        for r in csv.DictReader(f):
            if r.get("CPUStartTimeInMs"): t = float(r["CPUStartTimeInMs"]) / 1000.0
            elif r.get("TimeInMs"): t = float(r["TimeInMs"]) / 1000.0
            else: continue
            ms = float(r.get("MsBetweenPresents") or 0)
            if r.get("FrameType", "Application") not in ("Application", "NA", ""): continue
            if t0 <= pm0 + t <= t1: ft.append(ms)
    if ft:
        s = sorted(ft); n = len(s)
        res.update(frames=n, gt33=sum(x > 33 for x in s), gt50=sum(x > 50 for x in s), gt100=sum(x > 100 for x in s),
                   worst=round(s[-1], 1), p99=round(s[int(n * 0.99)], 2), p999=round(s[min(n - 1, int(n * 0.999))], 1),
                   fps=round(1000 * n / sum(s), 1))
    logs = sorted(glob.glob(os.path.join(d, "engine_launch*.log")))
    cre = ge1 = 0; mx = 0.0; pj = []
    for lf in logs:
        for l in open(lf, encoding="utf-8", errors="replace"):
            m = re.match(r"\[(\d\d:\d\d:\d\d)\.?\d*\] (.*)", l)
            if not m: continue
            t, x = tod(m.group(1)), m.group(2)
            if "prewarm" in x or "PJ:" in x[:12]: pj.append(x.strip()[:160])
            mm = re.search(r"stats spr: runtime gfx PSO creates \d+ \(\+(\d+) in 30 s, avg [\d.]+ us, max ([\d.]+) us, >=1 ms: (\d+)\)", x)
            if mm and t0 < t <= t1 + 30:
                cre += int(mm.group(1)); mx = max(mx, float(mm.group(2))); ge1 = max(ge1, int(mm.group(3)))
    res.update(spr_creates_win=cre, spr_max_us=round(mx), spr_ge1ms_cum=ge1, pj=pj[-4:])
    return res


if __name__ == "__main__":
    for d in sys.argv[1:]:
        r = arm(d); pj = r.pop("pj", [])
        print(" ".join(f"{k}={v}" for k, v in r.items()))
        for l in pj: print("   ", l)
