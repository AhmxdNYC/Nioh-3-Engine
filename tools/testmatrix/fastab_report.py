r"""fastab_report.py - analysis of a one-launch A/B session (tools\testmatrix\fastab.ps1, notes\fast-testing.md).

    py -I tools\testmatrix\fastab_report.py <session dir>     -> report.md (with a [RESULT]-ready table), results.json
    py -I tools\testmatrix\fastab_report.py selftest

Inputs in the session dir: manifest.jsonl (fastab.ps1: one 'window' row per measuring window with item / pair / arm /
t0_utc / t1_utc, plus 'event' rows for the wall-time budget) and presentmon*.csv (PresentMon v2 --date_time:
CPUStartDateTime, FrameTime, GPUBusy). PresentMon's clock offset is detected as in cost_vs_looks.py (pm_time.py).

Method: windows come in pairs (A then B, or B then A: the order alternates ABBA so a linear drift cancels); each pair gives
one paired delta d_i = mean(B window) - mean(A window) per metric. Reported: mean delta, its standard error sd(d)/sqrt(n),
the 95 % t interval, the delta in % of A, and the drift (last A pair mean - first A pair mean). Verdict per metric:
  B better / B worse  = the 95 % interval excludes 0 (lower is better for frame time and GPU busy, higher for fps);
  no difference       = it includes 0 (the half-width says how small an effect the session could still have missed).
"""
import json
import math
import os
import sys
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cost_vs_looks as cvl  # noqa: E402  (read_pm / align_pm / utc_epoch / read_jsonl)

T95 = [12.706, 4.303, 3.182, 2.776, 2.571, 2.447, 2.365, 2.306, 2.262, 2.228, 2.201, 2.179, 2.160, 2.145, 2.131, 2.120,
       2.110, 2.101, 2.093, 2.086, 2.080, 2.074, 2.069, 2.064, 2.060, 2.056, 2.052, 2.048, 2.045, 2.042]
METRICS = [("gpu_ms", "GPU busy ms", -1), ("frame_ms", "frame time ms", -1), ("fps", "fps", +1)]


def t95(df):
    return T95[df - 1] if 1 <= df <= len(T95) else 1.96


def win_stats(pm, t0, t1):
    import bisect
    ts = [r[0] for r in pm] if not hasattr(win_stats, "ts") or win_stats.src is not pm else win_stats.ts
    win_stats.ts, win_stats.src = ts, pm
    a, b = bisect.bisect_left(ts, t0), bisect.bisect_right(ts, t1)
    seg = pm[a:b]
    if len(seg) < 30:
        return None
    g = sum(r[1] for r in seg) / len(seg)
    f = sum(r[2] for r in seg) / len(seg)
    return dict(gpu_ms=g, frame_ms=f, fps=1000.0 / f if f > 0 else float("nan"), frames=len(seg))


def paired(rows, key):
    """rows of one item -> list of (pair, A value, B value)."""
    by = {}
    for r in rows:
        if r.get("stats") and r["stats"].get(key) is not None:
            by.setdefault(r["pair"], {})[r["arm"]] = r["stats"][key]
    return [(p, v["A"], v["B"]) for p, v in sorted(by.items()) if "A" in v and "B" in v]


def summarize(pairs, sign):
    n = len(pairs)
    if n == 0:
        return None
    d = [b - a for _, a, b in pairs]
    m = sum(d) / n
    a_mean = sum(a for _, a, _ in pairs) / n
    b_mean = sum(b for _, _, b in pairs) / n
    sd = math.sqrt(sum((x - m) ** 2 for x in d) / (n - 1)) if n > 1 else float("nan")
    se = sd / math.sqrt(n) if n > 1 else float("nan")
    hw = t95(n - 1) * se if n > 1 else float("nan")
    drift = pairs[-1][1] - pairs[0][1] if n > 1 else 0.0
    if n < 2 or math.isnan(hw):
        verdict = "too few pairs"
    elif m - hw > 0 or m + hw < 0:
        verdict = "B better" if (m > 0) == (sign > 0) else "B worse"
    else:
        verdict = "no difference"
    return dict(n=n, a=a_mean, b=b_mean, delta=m, se=se, ci=hw, pct=100.0 * m / a_mean if a_mean else float("nan"),
                sd=sd, drift=drift, verdict=verdict)


def fmt(v, nd=3):
    return "-" if v is None or (isinstance(v, float) and math.isnan(v)) else ("%+.*f" % (nd, v) if nd >= 0 else str(v))


def report(d):
    man = cvl.read_jsonl(os.path.join(d, "manifest.jsonl"))
    wins = [r for r in man if r.get("kind") == "window"]
    pm = []
    for f in sorted(os.listdir(d)):
        if f.lower().startswith("presentmon") and f.lower().endswith(".csv"):
            pm += cvl.read_pm(os.path.join(d, f))
    pm.sort()
    if not wins:
        raise SystemExit("no window rows in %s" % d)
    if not pm:
        raise SystemExit("no PresentMon rows in %s" % d)
    cvl.align_pm(pm, [(cvl.utc_epoch(r["t0_utc"]), cvl.utc_epoch(r["t1_utc"])) for r in wins], os.path.basename(os.path.normpath(d)))
    for r in wins:
        r["stats"] = win_stats(pm, cvl.utc_epoch(r["t0_utc"]), cvl.utc_epoch(r["t1_utc"]))
    items, res = [], {}
    for r in wins:
        if r["item"] not in items:
            items.append(r["item"])
    for it in items:
        rows = [r for r in wins if r["item"] == it and r.get("ack_ok", True)]
        res[it] = {k: summarize(paired(rows, k), s) for k, _, s in METRICS}
        res[it]["windows"] = len(rows)
        res[it]["dropped"] = len([r for r in wins if r["item"] == it]) - len(rows)
        res[it]["a_label"] = rows[0].get("a_value", "0") if rows else "0"
        res[it]["b_label"] = rows[0].get("b_value", "1") if rows else "1"
    # wall-time budget
    ev = {r["event"]: cvl.utc_epoch(r["t_utc"]) for r in man if r.get("kind") == "event"}
    budget = {}
    if "launch" in ev and "field" in ev:
        budget["launch_to_field_s"] = round(ev["field"] - ev["launch"], 1)
    if "first_window" in ev and "launch" in ev:
        budget["launch_to_first_window_s"] = round(ev["first_window"] - ev["launch"], 1)
    if "last_window" in ev and "first_window" in ev:
        budget["measuring_span_s"] = round(ev["last_window"] - ev["first_window"], 1)
    if "end" in ev and "launch" in ev:
        budget["session_wall_s"] = round(ev["end"] - ev["launch"], 1)
    per = {}
    for it in items:
        t = [cvl.utc_epoch(r["t0_utc"]) for r in wins if r["item"] == it]
        t1 = [cvl.utc_epoch(r["t1_utc"]) for r in wins if r["item"] == it]
        w = [r for r in wins if r["item"] == it]
        per[it] = round(max(t1) - min(t) + (w[0].get("warm_s", 0) if w else 0), 1)
    budget["per_item_s"] = per
    meta = next((r for r in man if r.get("kind") == "meta"), {})
    L = ["# One-launch A/B: %s" % meta.get("tag", os.path.basename(os.path.normpath(d))), ""]
    L.append("Session `%s`. ASI %s, stage %s, %s x %s s windows per item (+ %s s warm-up after each toggle), config: %s." % (
        os.path.basename(os.path.normpath(d)), meta.get("asi", "?"), meta.get("stage", "?"), meta.get("windows_per_item", "?"),
        meta.get("window_s", "?"), meta.get("warm_s", "?"), meta.get("config", "?")))
    L.append("Paired deltas B - A (A = off/baseline value, B = on/test value), ABBA order; SE = sd(d)/sqrt(n); 95 % CI = t(n-1) x SE.")
    L.append("")
    L.append("| item | metric | A | B | delta B-A | SE | 95 % CI +- | delta % | pairs | drift (A) | verdict |")
    L.append("|---|---|---|---|---|---|---|---|---|---|---|")
    for it in items:
        for k, lab, _ in METRICS:
            s = res[it][k]
            if not s:
                L.append("| %s | %s | - | - | - | - | - | - | 0 | - | no data |" % (it, lab))
                continue
            L.append("| %s (%s/%s) | %s | %.3f | %.3f | %s | %.3f | %.3f | %s | %d | %s | %s |" % (
                it, res[it]["a_label"], res[it]["b_label"], lab, s["a"], s["b"], fmt(s["delta"]), s["se"], s["ci"],
                fmt(s["pct"], 2), s["n"], fmt(s["drift"]), s["verdict"]))
    L.append("")
    L.append("Wall time: " + ", ".join("%s %s" % (k, v) for k, v in budget.items() if k != "per_item_s") +
             "; per item (windows + toggles): " + ", ".join("%s %s s" % (k, v) for k, v in per.items()))
    dropped = sum(res[it]["dropped"] for it in items)
    if dropped:
        L.append("Dropped windows (toggle not acknowledged by the mod): %d." % dropped)
    L.append("")
    L.append("```")
    for it in items:
        g, f = res[it]["gpu_ms"], res[it]["fps"]
        if g and f:
            L.append("[RESULT] %s %s: %s %s->%s: GPU busy %s ms +- %.3f SE (%s %%), fps %s %% +- %.2f; %d pairs, one launch -> %s" % (
                meta.get("device", "pc"), meta.get("tag", "fastab"), it, res[it]["a_label"], res[it]["b_label"], fmt(g["delta"]),
                g["se"], fmt(g["pct"], 2), fmt(f["pct"], 2), 100.0 * f["se"] / f["a"] if f["a"] else float("nan"), g["n"], g["verdict"]))
    L.append("```")
    open(os.path.join(d, "report.md"), "w", encoding="utf-8").write("\n".join(L) + "\n")
    json.dump(dict(items=res, budget=budget, meta=meta), open(os.path.join(d, "results.json"), "w"), indent=1, default=str)
    print("\n".join(L))
    return res, budget


def selftest():
    import random
    import tempfile
    random.seed(5)
    d = tempfile.mkdtemp(prefix="fastab_selftest_")
    t = datetime(2026, 10, 9, 12, 0, 0, tzinfo=timezone.utc).timestamp()
    man = [dict(kind="meta", tag="selftest", asi="x", stage="7292", windows_per_item=8, window_s=20, warm_s=5, config="fake")]
    man.append(dict(kind="event", event="launch", t_utc=iso(t - 150)))
    man.append(dict(kind="event", event="field", t_utc=iso(t - 40)))
    man.append(dict(kind="event", event="first_window", t_utc=iso(t)))
    pmrows = []
    cur = t
    # item X: B saves 0.50 ms GPU busy; item Y: no effect; a slow drift of +0.3 ms over the session
    for it, eff in (("Hot.x", -0.50), ("PP", 0.0)):
        for p in range(4):
            for arm in (("A", "B") if p % 2 == 0 else ("B", "A")):
                cur += 5
                t0, t1 = cur, cur + 20
                for k in range(int(20 / 0.008)):
                    tt = t0 + k * 0.008
                    g = 7.0 + 0.3 * (tt - t) / 400.0 + (eff if arm == "B" else 0) + random.gauss(0, 0.4)
                    pmrows.append("%s,%.3f,%.3f" % (datetime.fromtimestamp(tt, timezone.utc).strftime("%Y-%m-%d %H:%M:%S.%f")[:-3], g + 1.0, g))
                man.append(dict(kind="window", item=it, pair=p, arm=arm, t0_utc=iso(t0), t1_utc=iso(t1), ack_ok=True, warm_s=5,
                                a_value="0", b_value="1"))
                cur = t1
    man.append(dict(kind="event", event="last_window", t_utc=iso(cur)))
    man.append(dict(kind="event", event="end", t_utc=iso(cur + 30)))
    with open(os.path.join(d, "manifest.jsonl"), "w") as f:
        f.write("\n".join(json.dumps(m) for m in man) + "\n")
    with open(os.path.join(d, "presentmon.csv"), "w") as f:
        f.write("CPUStartDateTime,FrameTime,GPUBusy\n" + "\n".join(pmrows) + "\n")
    res, budget = report(d)
    gx, gy = res["Hot.x"]["gpu_ms"], res["PP"]["gpu_ms"]
    assert abs(gx["delta"] + 0.50) < 0.05 and gx["verdict"] == "B better", gx
    assert abs(gy["delta"]) < 0.05 and gy["verdict"] == "no difference", gy
    assert gx["se"] < 0.03 and res["Hot.x"]["fps"]["verdict"] == "B better"
    assert budget["launch_to_first_window_s"] == 150.0 and budget["per_item_s"]["Hot.x"] > 190
    txt = open(os.path.join(d, "report.md"), encoding="utf-8").read()
    assert "[RESULT] pc selftest: Hot.x 0->1: GPU busy -0.5" in txt, txt
    print("fastab_report selftest OK (%s)" % d)


def iso(t):
    return datetime.fromtimestamp(t, timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%f")[:-3] + "Z"


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    selftest() if sys.argv[1] == "selftest" else report(sys.argv[1])
