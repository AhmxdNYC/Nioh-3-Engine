"""pj_hitch.py - PSO-compile hitch metric for the PJ (PSO journal + prewarm) A/B over a fixed tour route (notes/stutter.md 6.1).

    py -I tools\\pj_hitch.py [--out report.txt] LABEL=<tour capture dir> [LABEL=<dir> ...]
    py -I tools\\pj_hitch.py --selftest

A capture dir is a tools\\tour\\run_tour.ps1 capture (presentmon.csv + logs\\run*.log). Labels: J (journal-record launch, not scored),
N / N2 (PJ=0, before / after), W (PJ=1 with prewarm). N and N2 bracket W so route drift shows.

Metric (per launch, over the route window = "Tour: ARMED" .. "Tour: DONE", level-load intervals excluded):
  hitch frame   = a rendered PresentMon frame (FG-generated / repeated frames dropped) with FrameTime > 33 ms.
  SPR window    = one 'stats spr' line (30 s); it covers (t-30 s, t]. Its PSO-hot if the cumulative '>=1 ms' creates grew in it.
  attributed    = hitch frames inside a PSO-hot SPR window (30 s resolution: the log has no per-call stamps); the headline numbers are
                  attributed COUNT and attributed SUM of frame ms (the first-use compile stalls), next to ALL route hitches and
                  the SPR create totals (runtime gfx PSO creates, >= 1 ms creates, time spent in creates = sum(+N x avg)).
Verdict W vs the N launches (mean): KEEP when attributed count AND >=1 ms creates both drop >= 30 %, prewarm refused 0, N/N2 within
30 % on creates (route reproducible); DROP when neither drops; else MAYBE (repeat). One W/N pair is a single repeat: stutter.md
asks for two before shipping PJ = 1.
"""
import argparse
import bisect
import csv
import datetime as dt
import glob
import os
import re
import statistics
import sys

TS_RE = re.compile(r"^\[(\d\d):(\d\d):(\d\d)\.(\d\d\d)\] (.*)$")
HITCH_MS = 33.0


def sod(h, m, s, ms=0):
    return h * 3600 + m * 60 + s + ms / 1000.0


def read_log(d):
    """-> list of (second-of-day, text) over logs\\run*.log in name order (midnight unwrapped)."""
    out, off, prev = [], 0.0, None
    for f in sorted(glob.glob(os.path.join(d, "logs", "*.log"))):
        for raw in open(f, encoding="utf-8", errors="replace"):
            m = TS_RE.match(raw.rstrip("\r\n"))
            if not m:
                continue
            t = sod(int(m[1]), int(m[2]), int(m[3]), int(m[4]))
            if prev is not None and t + off < prev - 3600:
                off += 86400.0
            prev = t + off
            out.append((t + off, m[5]))
    return out


def read_pm(path):
    """-> (times second-of-day (unwrapped), frame ms) of rendered Nioh3 frames."""
    ts, ft = [], []
    off, prev = 0.0, None
    with open(path, newline="", encoding="utf-8-sig", errors="replace") as f:
        for row in csv.DictReader(f):
            if (row.get("Application") or "").lower() not in ("nioh3.exe", ""):
                continue
            fty = (row.get("FrameType") or "").strip()
            if fty not in ("", "NA", "Application", "NotSet", "Unspecified"):
                continue
            try:
                v = float(row.get("FrameTime") or row.get("MsBetweenPresents"))
            except (TypeError, ValueError):
                continue
            m = re.search(r" (\d+):(\d+):(\d+)(?:\.(\d+))?", row.get("CPUStartDateTime") or "")
            if not m or v <= 0:
                continue
            t = sod(int(m[1]), int(m[2]), int(m[3])) + int((m[4] or "0")[:6].ljust(6, "0")) / 1e6
            if prev is not None and t + off < prev - 3600:
                off += 86400.0
            prev = t + off
            ts.append(t + off)
            ft.append(v)
    return ts, ft


def bias_of(pm_last, log_last):
    """PresentMon --date_time may be UTC: whole quarter-hour offset between the two last timestamps."""
    d = (pm_last - log_last + 43200.0) % 86400.0 - 43200.0   # into [-12 h, 12 h)
    return round(d / 900.0) * 900.0


def num(pat, s, cast=float):
    m = re.search(pat, s)
    return cast(m.group(1)) if m else None


def analyse(d):
    log = read_log(d)
    pm_path = os.path.join(d, "presentmon.csv")
    if not log or not os.path.exists(pm_path):
        return {"error": "no logs\\*.log or presentmon.csv in %s" % d}
    ts, ft = read_pm(pm_path)
    if not ts:
        return {"error": "presentmon.csv has no rendered Nioh3 frames"}
    bias = bias_of(ts[-1], log[-1][0])
    day = round((log[-1][0] - (ts[-1] - bias)) / 86400.0) * 86400.0   # PresentMon clock can sit on the other side of midnight
    ts = [t - bias + day for t in ts]
    t_arm = next((t for t, x in log if "Tour: ARMED" in x), None)
    t_done = next((t for t, x in log if "Tour: DONE" in x or "tour already finished" in x), None)
    if t_arm is None:
        return {"error": "tour never armed in %s" % d}
    t_end = t_done if t_done is not None else log[-1][0]
    # level-load intervals: 'loading mission' -> next 'arrived' (+1 s) or the next 'loading mission' / the end
    loads, cur = [], None
    for t, x in log:
        if t < t_arm or t > t_end:
            continue
        if re.search(r"Tour: stage \d+/\d+ pass \d+ - loading mission", x):
            if cur is not None:
                loads.append((cur, t))
            cur = t
        elif cur is not None and re.search(r"Tour: stage \d+ arrived", x):
            loads.append((cur, t + 1.0))
            cur = None
    if cur is not None:
        loads.append((cur, t_end))
    ls = [a for a, _ in loads]

    def in_load(t):
        i = bisect.bisect_right(ls, t) - 1
        return i >= 0 and t <= loads[i][1]

    spr = []   # (t, new, slow_cum, avg_us, max_us)
    for t, x in log:
        if x.startswith("stats spr:") and t_arm <= t <= t_end + 31.0:
            spr.append((t, num(r"PSO creates \d+ \(\+(\d+)", x, int) or 0, num(r"PSO creates .*?>=1 ms: (\d+)", x, int) or 0,
                        num(r"PSO creates .*?avg ([\d.]+) us", x) or 0.0, num(r"PSO creates .*?max ([\d.]+) us", x) or 0.0))
    r = {"route_s": t_end - t_arm, "armed": t_arm, "done": t_done is not None, "bias_s": bias, "load_s": sum(b - a for a, b in loads),
         "spr_lines": len(spr)}
    # SPR windows: hot = cumulative >=1 ms grew. First line of the route has no earlier cumulative -> use its own value as delta.
    hot, prev_slow = [], None
    creates = slow_delta = 0
    create_ms = 0.0
    maxus = 0.0
    for i, (t, new, slow, avg, mx) in enumerate(spr):
        ds = slow - prev_slow if prev_slow is not None else slow
        prev_slow = slow
        hot.append(ds > 0)
        creates += new
        slow_delta += max(ds, 0)
        create_ms += new * avg / 1000.0
        maxus = max(maxus, mx)
    r.update(creates=creates, creates_ge1ms=slow_delta, create_ms=create_ms, max_create_ms=maxus / 1000.0)
    st = [t for t, *_ in spr]
    n_all = s_all = n_att = s_att = 0
    worst = 0.0
    for t, v in zip(ts, ft):
        if t < t_arm or t > t_end or v <= HITCH_MS or in_load(t):
            continue
        n_all += 1
        s_all += v
        worst = max(worst, v)
        i = bisect.bisect_left(st, t)   # first SPR line at or after the frame = the window that covers it
        if i < len(st) and hot[i] and t > st[i] - 30.0:
            n_att += 1
            s_att += v
    r.update(hitches=n_all, hitch_ms=s_all, att=n_att, att_ms=s_att, worst_ms=worst)
    r["frames"] = sum(1 for t in ts if t_arm <= t <= t_end and not in_load(t))
    pj = [x for _, x in log if x.startswith("PJ:") or "stats/s PJ:" in x]
    pd = next((x for x in pj if "prewarm done" in x), "")
    r["prewarm"] = pd
    r["refused"] = num(r"(\d+) refused", pd, int) if pd else None
    r["pj_last"] = pj[-1] if pj else ""
    return r


def fmt(label, r):
    if "error" in r:
        return "  %-3s ERROR %s" % (label, r["error"])
    return ("  %-3s route %4.0f s (%s, loads %3.0f s excluded) | SPR lines %2d: creates %4d, >=1 ms %3d, in-create %6.0f ms, max %6.1f ms | "
            "hitches >33 ms: %3d (%6.0f ms, worst %5.0f) | PSO-attributed: %3d (%6.0f ms)" %
            (label, r["route_s"], "DONE" if r["done"] else "no DONE", r["load_s"], r["spr_lines"], r["creates"], r["creates_ge1ms"],
             r["create_ms"], r["max_create_ms"], r["hitches"], r["hitch_ms"], r["worst_ms"], r["att"], r["att_ms"]))


def verdict(R):
    ns = [R[k] for k in ("N", "N2") if k in R and "error" not in R[k]]
    w = R.get("W")
    if not ns or not w or "error" in w:
        return "PJ: INVALID - need an N launch and a W launch with data"
    notes = []
    if any(x["spr_lines"] == 0 or x["creates"] == 0 for x in ns + [w]):
        return "PJ: INVALID - an SPR-less launch (no 'stats spr' lines / zero runtime creates: SPR = 1 missing or route shorter than 30 s)"
    if len(ns) == 2:
        a, b = ns[0]["creates"], ns[1]["creates"]
        if abs(a - b) > 0.3 * max(a, b):
            return "PJ: INVALID - N and N2 differ by > 30 %% on runtime creates (%d vs %d): the route is not reproducible" % (a, b)
    mean = lambda k: statistics.mean(x[k] for x in ns)
    def drop(k):
        base = mean(k)
        return (1.0 - w[k] / base) if base > 0 else 0.0
    d_att, d_slow, d_cr = drop("att"), drop("creates_ge1ms"), drop("creates")
    summ = ("W vs N: attributed hitches %+.0f %% (%.1f -> %d; ms %.0f -> %.0f), >=1 ms creates %+.0f %% (%.1f -> %d), runtime creates %+.0f %% (%.0f -> %d), "
            "all route hitches %.1f -> %d" %
            (-100 * d_att, mean("att"), w["att"], mean("att_ms"), w["att_ms"], -100 * d_slow, mean("creates_ge1ms"), w["creates_ge1ms"],
             -100 * d_cr, mean("creates"), w["creates"], mean("hitches"), w["hitches"]))
    ref = w.get("refused")
    if ref:
        return "PJ: DROP - prewarm refused %d PSOs; %s" % (ref, summ)
    if d_att >= 0.3 and d_slow >= 0.3:
        return "PJ: KEEP (single pair - repeat once before shipping PJ = 1) - %s" % summ
    if d_att <= 0.05 and d_slow <= 0.05:
        return "PJ: DROP - no gain; %s" % summ
    return "PJ: MAYBE (repeat) - %s" % summ


def build_report(R):
    lines = ["pj_hitch report (metric: tools\\pj_hitch.py docstring)"]
    for k in ("J", "N", "W", "N2"):
        if k in R:
            lines.append(fmt(k, R[k]))
            if "error" not in R[k]:
                if R[k]["prewarm"]:
                    lines.append("      " + R[k]["prewarm"])
                if R[k]["pj_last"] and not R[k]["prewarm"]:
                    lines.append("      " + R[k]["pj_last"])
    lines.append(verdict(R))
    return "\n".join(lines)


def selftest():
    import tempfile
    tmp = tempfile.mkdtemp(prefix="pjh_")

    def mk(name, hitch_at, slows, creates=40, utc=4):
        d = os.path.join(tmp, name)
        os.makedirs(os.path.join(d, "logs"))
        t0 = 20 * 3600 + 10 * 60.0   # 20:10:00 local
        def hms(t):
            return "%02d:%02d:%02d.%03d" % (t // 3600 % 24, t // 60 % 60, t % 60, int(t * 1000) % 1000)
        L = [(t0, "Tour: ARMED (mode 1)"), (t0 + 5, "Tour: stage 1/5 pass 1 - loading mission 7356"), (t0 + 20, "Tour: stage 1 arrived")]
        cum = 0
        for i, s in enumerate(slows):
            cum += s
            L.append((t0 + 30 * (i + 1) + 0.1, "stats spr: runtime gfx PSO creates %d (+%d in 30 s, avg 500 us, max 9000 us, >=1 ms: %d) | upload-heap dedicated fallbacks 0 (+0 in 30 s, avg 0 us, max 0 us, >=1 ms: 0)" % (creates * (i + 1), creates, cum)))
        t_end = t0 + 30 * len(slows) + 5
        L.append((t_end, "Tour: DONE - all stages visited"))
        open(os.path.join(d, "logs", "run01.log"), "w").write("\n".join("[%s] %s" % (hms(t), x) for t, x in L) + "\n")
        with open(os.path.join(d, "presentmon.csv"), "w") as f:
            f.write("Application,ProcessID,CPUStartDateTime,FrameTime\n")
            t = t0 - 5
            while t < t_end + 3:
                v = 100.0 if any(abs(t - h) < 0.0085 for h in hitch_at) else 8.0
                u = t + utc * 3600
                f.write("Nioh3.exe,1,2026-10-8 %02d:%02d:%02d.%06d,%.1f\n" % (u // 3600 % 24, u // 60 % 60, u % 60, int((u % 1) * 1e6), v))
                t += 0.01
        return d
    t0 = 20 * 3600 + 10 * 60.0
    # N: hitches at 40.0 s and 70.0 s in hot windows (slows in windows 2 and 3), one in the load (t0+10, excluded), one in a cold window
    N = mk("N", [t0 + 40.0, t0 + 70.0, t0 + 10.0, t0 + 100.0], [0, 3, 2, 0])
    N2 = mk("N2", [t0 + 45.0, t0 + 75.0, t0 + 105.0], [0, 2, 3, 0])
    W = mk("W", [t0 + 105.0], [0, 0, 0, 0])
    Wb = mk("Wb", [t0 + 40.0, t0 + 70.0], [0, 3, 2, 0])
    rn, rn2, rw, rwb = analyse(N), analyse(N2), analyse(W), analyse(Wb)
    for x in (rn, rn2, rw, rwb):
        assert "error" not in x, x
    assert rn["hitches"] == 3 and rn["att"] == 2, rn
    assert rw["hitches"] == 1 and rw["att"] == 0, rw
    assert rn["load_s"] == 16.0, rn
    assert rn["bias_s"] == 4 * 3600.0, rn
    assert verdict({"N": rn, "N2": rn2, "W": rw}).startswith("PJ: KEEP"), verdict({"N": rn, "N2": rn2, "W": rw})
    assert verdict({"N": rn, "N2": rn2, "W": rwb}).startswith("PJ: DROP"), verdict({"N": rn, "N2": rn2, "W": rwb})
    assert verdict({"N": rn, "W": {"error": "x"}}).startswith("PJ: INVALID")
    print(build_report({"N": rn, "N2": rn2, "W": rw}))
    print("pj_hitch selftest OK")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("items", nargs="*", help="LABEL=capture dir")
    ap.add_argument("--out")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    R = {}
    for it in a.items:
        k, _, d = it.partition("=")
        R[k.upper()] = analyse(d)
    rep = build_report(R)
    print(rep)
    if a.out:
        open(a.out, "w", encoding="utf-8").write(rep + "\n")


if __name__ == "__main__":
    main()
