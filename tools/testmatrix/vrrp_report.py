"""vrrp_report.py - [Latency] VrrPresent launch A/B verdict from one PresentMon 2.x CSV (notes/vrr-present.md).

    py -I tools/testmatrix/vrrp_report.py <presentmon.csv> --arms A,B,B,A [--skip 90] [--sec 600]
    py -I tools/testmatrix/vrrp_report.py --selftest

Each game launch has its own ProcessID; launches are taken in order of their first frame and mapped to --arms
(A = VrrPresent 0, B = VrrPresent 1). Per launch the first --skip seconds (boot, title, load) are dropped and at most
--sec seconds are used. Per launch: presented fps, displayed fps (DisplayedTime > 0), PresentMode / SyncInterval /
AllowsTearing shares, DisplayLatency p50/p95, AllInputToPhotonLatency p50 (n). Verdict on the arm means:
KEEP when DisplayLatency p50 drops by >= 3 ms AND displayed fps rises; tearing inside the VRR range is judged by eye.
stdlib only.
"""
import csv
import statistics
import sys


def num(v):
    try:
        x = float(v)
    except (TypeError, ValueError):
        return None
    return x


def pct(xs, p):
    if not xs:
        return None
    xs = sorted(xs)
    k = min(len(xs) - 1, max(0, int(round(p / 100.0 * (len(xs) - 1)))))
    return xs[k]


def launches(rows):
    """rows of one PID in file order -> list of (pid, rows), ordered by first appearance"""
    order, by = [], {}
    for r in rows:
        pid = r.get("ProcessID")
        if pid not in by:
            by[pid] = []
            order.append(pid)
        by[pid].append(r)
    return [(p, by[p]) for p in order]


def stats(rows, skip, sec):
    t, kept = 0.0, []
    for r in rows:
        ft = num(r.get("FrameTime")) or num(r.get("MsBetweenPresents")) or 0.0
        t += ft / 1000.0
        if t < skip:
            continue
        if sec and t > skip + sec:
            break
        kept.append((ft, r))
    if not kept:
        return None
    span = sum(ft for ft, _ in kept) / 1000.0
    n = len(kept)
    shown = [r for _, r in kept if (num(r.get("DisplayedTime")) or 0) > 0]
    modes, sis = {}, {}
    for _, r in kept:
        modes[r.get("PresentMode", "?")] = modes.get(r.get("PresentMode", "?"), 0) + 1
        sis[r.get("SyncInterval", "?")] = sis.get(r.get("SyncInterval", "?"), 0) + 1
    dl = [x for x in (num(r.get("DisplayLatency")) for _, r in kept) if x is not None]
    ip = [x for x in (num(r.get("AllInputToPhotonLatency")) for _, r in kept) if x is not None]
    tear = sum(1 for _, r in kept if r.get("AllowsTearing") == "1")
    return {
        "frames": n, "sec": span, "presented_fps": n / span if span else 0, "displayed_fps": len(shown) / span if span else 0,
        "displayed_pct": 100.0 * len(shown) / n, "modes": {k: 100.0 * v / n for k, v in modes.items()},
        "si": {k: 100.0 * v / n for k, v in sis.items()}, "tearing_pct": 100.0 * tear / n,
        "dl_p50": pct(dl, 50), "dl_p95": pct(dl, 95), "ip_p50": pct(ip, 50), "ip_n": len(ip),
    }


def report(rows, arms, skip, sec, out=print):
    ls = launches(rows)
    if len(ls) != len(arms):
        out("launches in the CSV: %d, arms given: %d (%s) - check the run" % (len(ls), len(arms), ",".join(arms)))
        return None
    per = {}
    for (pid, rs), arm in zip(ls, arms):
        s = stats(rs, skip, sec)
        if not s:
            out("launch pid %s (%s): no frames after %d s" % (pid, arm, skip))
            continue
        top = max(s["modes"], key=s["modes"].get)
        out("pid %-6s %s  %5.0f s  presented %6.1f  displayed %6.1f fps (%5.1f %%)  %s %.0f %%  SI0 %.0f %%  tearing %.0f %%  "
            "DisplayLatency p50 %s p95 %s  AllInputToPhoton p50 %s (n %d)" % (
                pid, arm, s["sec"], s["presented_fps"], s["displayed_fps"], s["displayed_pct"], top, s["modes"][top],
                s["si"].get("0", 0.0), s["tearing_pct"], fmt(s["dl_p50"]), fmt(s["dl_p95"]), fmt(s["ip_p50"]), s["ip_n"]))
        per.setdefault(arm, []).append(s)
    if "A" not in per or "B" not in per:
        out("need at least one A and one B launch with frames")
        return None
    # the stuck SyncInterval 1 does not happen on every launch: an A launch that already ran SI 0 is not a baseline
    stuck = [s for s in per["A"] if s["si"].get("0", 0.0) < 95.0]
    if len(stuck) < len(per["A"]):
        out("note: %d A launch(es) already presented SI 0 (stuck-1 state did not occur) - excluded from the A mean"
            % (len(per["A"]) - len(stuck)))
    if not stuck:
        out("VERDICT: INCONCLUSIVE (no A launch had the SI 1 state; VrrPresent can only help when it occurs - rerun A)")
        return None
    per["A"] = stuck

    def mean(arm, k):
        v = [s[k] for s in per[arm] if s[k] is not None]
        return statistics.mean(v) if v else None

    a_dl, b_dl = mean("A", "dl_p50"), mean("B", "dl_p50")
    a_fps, b_fps = mean("A", "displayed_fps"), mean("B", "displayed_fps")
    a_ip, b_ip = mean("A", "ip_p50"), mean("B", "ip_p50")
    d_dl = (a_dl - b_dl) if a_dl is not None and b_dl is not None else None
    keep = d_dl is not None and d_dl >= 3.0 and b_fps > a_fps
    out("A (VrrPresent 0): DisplayLatency p50 %s, displayed %.1f fps, AllInputToPhoton p50 %s" % (fmt(a_dl), a_fps, fmt(a_ip)))
    out("B (VrrPresent 1): DisplayLatency p50 %s, displayed %.1f fps, AllInputToPhoton p50 %s" % (fmt(b_dl), b_fps, fmt(b_ip)))
    out("VERDICT: %s (display latency %s ms lower, displayed fps %+.1f; KEEP needs >= 3 ms lower and more displayed fps, "
        "plus no tearing seen in the VRR range)" % ("KEEP" if keep else "DROP", fmt(d_dl), b_fps - a_fps))
    return keep


def fmt(x):
    return "NA" if x is None else "%.2f" % x


def selftest():
    def mk(pid, n, ft, si, mode, dl, shown_every, tear):
        rows = []
        for i in range(n):
            rows.append({"ProcessID": pid, "FrameTime": str(ft), "SyncInterval": si, "PresentMode": mode,
                         "AllowsTearing": tear, "DisplayLatency": str(dl + (i % 5) * 0.1),
                         "DisplayedTime": "0" if (shown_every and i % shown_every == 0) else str(ft),
                         "AllInputToPhotonLatency": str(dl + 4) if i % 50 == 0 else "NA"})
        return rows
    a = lambda pid: mk(pid, 20000, 6.5, "1", "Composed: Flip", 15.0, 10, "0")
    b = lambda pid: mk(pid, 20000, 6.5, "0", "Hardware Composed: Independent Flip", 7.6, 0, "1")
    lines = []
    keep = report(a("1") + b("2") + b("3") + a("4"), ["A", "B", "B", "A"], 10, 0, lines.append)
    ok = keep is True and any("VERDICT: KEEP" in l for l in lines)
    drop = report(a("1") + a("2") + a("3") + a("4"), ["A", "B", "B", "A"], 10, 0, lambda s: None)
    ok = ok and drop is False
    bad = report(a("1") + b("2"), ["A", "B", "A"], 10, 0, lambda s: None)
    ok = ok and bad is None
    print("\n".join(lines))
    print("SELFTEST OK" if ok else "SELFTEST FAILED")
    return 0 if ok else 1


def main(argv):
    if "--selftest" in argv:
        return selftest()
    if not argv or argv[0].startswith("--"):
        print(__doc__)
        return 2
    path = argv[0]
    arms = ["A", "B", "B", "A"]
    skip, sec = 90.0, 600.0
    i = 1
    while i < len(argv):
        if argv[i] == "--arms":
            arms = [x.strip().upper() for x in argv[i + 1].split(",") if x.strip()]
            i += 2
        elif argv[i] == "--skip":
            skip = float(argv[i + 1])
            i += 2
        elif argv[i] == "--sec":
            sec = float(argv[i + 1])
            i += 2
        else:
            print("unknown argument %s" % argv[i])
            return 2
    with open(path, newline="", encoding="utf-8-sig") as f:
        rows = list(csv.DictReader(f))
    rows = [r for r in rows if r.get("Application", "").lower() == "nioh3.exe"]
    keep = report(rows, arms, skip, sec)
    return 0 if keep is not None else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
