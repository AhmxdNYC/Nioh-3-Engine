#!/usr/bin/env python3
"""summary.py - Ally final-benchmark summary in the SAME table format as the PC tm.py (summary_ally.csv + verdict_ally.md).

  python3 summary.py [runs_dir] [--matrix ../matrix.json] [--noise-floor 0.5]

Reads runs/ally_<RUNID>__<kind>__<n>__<PRESET>__<VARIANT>.csv (+ .meta) written by run_log.sh, builds per-run result dicts in
the PC schema (windows: kind stand/move/combat, fps, low1, p99, gpu_ms, cpu_ms, ok) and calls tm.report() itself, so verdict
rules (noise = max(floor, BASE spread), KEEP/DROP at 2x, p99 at 4x) are identical. Needs BASE / BASE#1.. runs.
Extra outputs:
  summary_ally_hw.csv  per run: temps, power, clocks, ram/swap, frametime p50/p99 (every kind, incl. lat)
  pairs_ally.md        PRE_<P>_MOD vs PRE_<P>_STOCK; SET_<KEY>@<LEVEL> vs SET_<KEY>@REF; LAT_<cfg> frametime
Differences from PC: MangoHud has no GPU/CPU busy ms. gpu_ms = mean(frametime * gpu_load%), cpu_ms = mean(frametime * cpu_load%)
(aggregate CPU load, so only comparable between Ally runs). fps = 1000 / mean frametime; low1 = 1000 / p99 frametime.
"""
import csv, glob, json, os, re, statistics, sys, tempfile, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import tm  # noqa: E402  (PC helper; main guarded)


def read_run(path):
    L = open(path, encoding="utf-8-sig").read().splitlines()
    hi = next(i for i, l in enumerate(L) if l.startswith("fps,"))
    return list(csv.DictReader(L[hi:]))


def num(r, k):
    try:
        return float(r[k])
    except (KeyError, ValueError, TypeError):
        return None


def mean(xs):
    xs = [x for x in xs if x is not None]
    return statistics.mean(xs) if xs else None


def window(rows, kind):
    ft = [v for r in rows if (v := num(r, "frametime")) and v > 0]
    if len(ft) < 50:
        return {"kind": kind, "ok": False}
    p99 = tm.pct(ft, .99)
    gl = [num(r, "frametime") * num(r, "gpu_load") / 100 for r in rows if num(r, "gpu_load") is not None and num(r, "frametime")]
    cl = [num(r, "frametime") * num(r, "cpu_load") / 100 for r in rows if num(r, "cpu_load") is not None and num(r, "frametime")]
    return {"kind": kind, "ok": True, "fps": round(1000 / statistics.mean(ft), 2), "low1": round(1000 / p99, 2), "p99": round(p99, 2),
            "gpu_ms": round(mean(gl), 2) if gl else None, "cpu_ms": round(mean(cl), 2) if cl else None}


def hw(rows):
    g = lambda k: [v for r in rows if (v := num(r, k)) is not None]
    ft = g("frametime")
    f = lambda xs, fn=statistics.mean: round(fn(xs), 1) if xs else None
    return {"frames": len(ft), "ft_p50": f(ft, statistics.median), "ft_p99": round(tm.pct(ft, .99), 2) if ft else None,
            "cpu_temp_max": f(g("cpu_temp"), max), "gpu_temp_max": f(g("gpu_temp"), max), "cpu_temp_avg": f(g("cpu_temp")),
            "gpu_temp_avg": f(g("gpu_temp")), "cpu_w": f(g("cpu_power")), "gpu_w": f(g("gpu_power")), "gpu_mhz": f(g("gpu_core_clock")),
            "cpu_mhz": f(g("cpu_mhz")), "gpu_busy_pct": f(g("gpu_load")), "cpu_busy_pct": f(g("cpu_load")),
            "vram_mb": f(g("gpu_vram_used")), "ram_mb": f(g("ram_used")), "swap_mb": f(g("swap_used"))}


def meta(path):
    d = {}
    if os.path.exists(path):
        for l in open(path, encoding="utf-8", errors="replace"):
            if "=" in l:
                k, v = l.rstrip("\n").split("=", 1); d[k] = v
    return d


def main():
    a = sys.argv[1:]
    floor = float(a[a.index("--noise-floor") + 1]) if "--noise-floor" in a else 0.5
    mfile = a[a.index("--matrix") + 1] if "--matrix" in a else os.path.join(HERE, "..", "matrix.json")
    pos = [x for i, x in enumerate(a) if not x.startswith("--") and (i == 0 or not a[i - 1].startswith("--"))]
    runs = os.path.abspath(pos[0]) if pos else os.path.join(HERE, "runs")
    items = {i["id"]: i for i in json.load(open(mfile, encoding="utf-8-sig"))["items"]}
    byid, hwrows = {}, []
    for p in sorted(glob.glob(os.path.join(runs, "ally_*__*__*__*__*.csv"))):
        m = re.match(r"ally_(.+?)__(\w+?)__(\d+)__(.+?)__(.+)\.csv$", os.path.basename(p))
        if not m:
            continue
        rid, kind, n, preset, variant = m.groups()
        rows = read_run(p)
        byid.setdefault(rid, {"windows": [], "preset": preset, "variant": variant}).setdefault("windows", []).append(window(rows, kind))
        h = hw(rows); h.update(id=rid, kind=kind, n=n, preset=preset, variant=variant); hwrows.append(h)
    if not byid:
        print("no runs in", runs); return 1
    tmp = tempfile.mkdtemp()
    for rid, d in byid.items():
        it = items.get(rid.split("#")[0], {})
        pre = rid.split("_")[0] if "_" in rid else ""
        typ = it.get("type") or {"PRE": "preset", "SET": "setting", "LAT": "latency", "DRP": "retest"}.get(pre, "asi")
        j = {"id": rid, "device": "ally", "type": typ, "grp": it.get("grp", 90), "risk": it.get("risk"), "metric": it.get("metric"),
             "shot": it.get("shot"), "preset": d["preset"], "valid": any(w.get("ok") for w in d["windows"]), "windows": d["windows"]}
        # preset/setting/latency ids are compared in pairs_ally.md, not against BASE
        if typ not in ("preset", "setting", "latency"):
            json.dump(j, open(os.path.join(tmp, f"ally_{rid}.json"), "w"))
    rc = tm.report(tmp, "ally", floor)
    for f in ("summary_ally.csv", "verdict_ally.md"):
        if os.path.exists(os.path.join(tmp, f)):
            shutil.copy(os.path.join(tmp, f), os.path.join(runs, "..", f))
    shutil.rmtree(tmp, ignore_errors=True)
    # hardware table
    if hwrows:
        keys = ["id", "kind", "n", "preset", "variant"] + [k for k in hwrows[0] if k not in ("id", "kind", "n", "preset", "variant")]
        with open(os.path.join(runs, "..", "summary_ally_hw.csv"), "w", newline="") as f:
            w = csv.DictWriter(f, keys); w.writeheader(); w.writerows(hwrows)
    # pairs
    def agg(rid):
        s = tm.summarize({"windows": byid[rid]["windows"], "valid": True}) if rid in byid else None
        return s if s and s["fps"] else None
    out = ["# Ally pairs (fps = median of stand windows)", "", "| run | vs | fps | ref fps | d_fps % | p99 ms | ref p99 | d_gpu_ms |", "|---|---|---|---|---|---|---|---|"]
    for rid in sorted(byid):
        ref = None
        if rid.startswith("PRE_") and rid.endswith("_MOD"):
            ref = rid[:-4] + "_STOCK"
        elif rid.startswith("SET_") and "@" in rid and not rid.endswith("@REF"):
            ref = rid.split("@")[0] + "@REF"
        elif rid.startswith("LAT_") and rid != "LAT_STOCK":
            ref = "LAT_STOCK"
        s, r = agg(rid), agg(ref) if ref else None
        if s and r:
            dg = None if s["gpu_ms"] is None or r["gpu_ms"] is None else round(s["gpu_ms"] - r["gpu_ms"], 2)
            out.append(f"| {rid} | {ref} | {s['fps']:.1f} | {r['fps']:.1f} | {(s['fps'] / r['fps'] - 1) * 100:+.1f} | {s['p99']:.1f} | {r['p99']:.1f} | {dg} |")
    open(os.path.join(runs, "..", "pairs_ally.md"), "w", encoding="utf-8").write("\n".join(out) + "\n")
    print(f"hw rows: {len(hwrows)} -> summary_ally_hw.csv; pairs_ally.md written")
    return rc


if __name__ == "__main__":
    sys.exit(main())
