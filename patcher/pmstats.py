"""Summarise a PresentMon 2.x CSV: fps, lows, pacing, GPU/CPU busy, frame-gen split.

usage: py pmstats.py capture.csv [more.csv ...]
Pacing metric: frame-to-frame |delta| of displayed-frame intervals — judder shows up as
large alternating deltas even when the average fps looks fine.
"""
import csv, statistics, sys

def pct(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(len(xs) * p))] if xs else float("nan")

def col(row, *names):
    for n in names:
        v = row.get(n)
        if v not in (None, "", "NA"):
            try:
                return float(v)
            except ValueError:
                pass
    return None

for path in sys.argv[1:]:
    rows = list(csv.DictReader(open(path, newline="", encoding="utf-8-sig")))
    if not rows:
        print(path, "empty"); continue
    ftypes = {}
    for r in rows:
        ftypes[r.get("FrameType", "?")] = ftypes.get(r.get("FrameType", "?"), 0) + 1
    present = [v for r in rows if (v := col(r, "MsBetweenPresents", "FrameTime")) is not None]
    disp = [v for r in rows if (v := col(r, "MsBetweenDisplayChange", "DisplayedTime")) is not None and v > 0]
    app = [r for r in rows if r.get("FrameType", "Application") in ("Application", "NotSet", "")]
    gpu = [v for r in app if (v := col(r, "MsGPUBusy", "GPUBusy")) is not None]
    cpu = [v for r in app if (v := col(r, "MsCPUBusy", "CPUBusy")) is not None]
    lat = [v for r in app if (v := col(r, "MsPCLatency", "MsClickToPhotonLatency", "DisplayLatency", "MsAllInputToPhotonLatency")) is not None]
    dur = sum(present) / 1000 if present else 0
    print(f"== {path}")
    print(f"   columns: {', '.join(list(rows[0].keys())[:40])}")
    print(f"   frames={len(rows)} over {dur:.1f}s  frame types={ftypes}  present mode={rows[len(rows)//2].get('PresentMode')}")
    for name, xs in (("present interval", present), ("displayed interval", disp)):
        if not xs:
            continue
        avg = statistics.mean(xs)
        deltas = [abs(a - b) for a, b in zip(xs, xs[1:])]
        print(f"   {name:18}: avg {avg:6.2f} ms ({1000/avg:6.1f} fps)  p99 {pct(xs,.99):6.2f}  "
              f"max {max(xs):6.2f}  1%low {1000/pct(xs,.99):6.1f} fps  "
              f"pacing |Δ| mean {statistics.mean(deltas):5.2f} p95 {pct(deltas,.95):5.2f} ms  "
              f">2x-avg spikes {sum(1 for x in xs if x > 2*avg)}")
    if gpu:
        print(f"   GPU busy (app frames): avg {statistics.mean(gpu):6.2f} ms  p95 {pct(gpu,.95):6.2f}")
    if cpu:
        print(f"   CPU busy (app frames): avg {statistics.mean(cpu):6.2f} ms  p95 {pct(cpu,.95):6.2f}")
    if lat:
        print(f"   latency: avg {statistics.mean(lat):6.2f} ms")
