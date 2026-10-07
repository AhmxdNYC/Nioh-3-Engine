"""rgp_frame.py - frame-level analysis on top of rgp_report.build():
command-buffer timeline per queue, GPU idle gaps (bubbles), async-compute overlap,
wave-occupancy coverage per time bin, and details of the top PSOs.

    py rgp_frame.py <trace.rgp> [--top 25] [--bin-us 250]
"""
from __future__ import annotations
import argparse, os, sys, pickle
from collections import defaultdict
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rgp_report import build, api_name


def union(iv):
    iv = sorted(iv)
    out = []
    for a, b in iv:
        if out and a <= out[-1][1]:
            out[-1][1] = max(out[-1][1], b)
        else:
            out.append([a, b])
    return out


def total(iv):
    return sum(b - a for a, b in iv)


def intersect(a, b):
    i = j = 0; out = []
    while i < len(a) and j < len(b):
        lo, hi = max(a[i][0], b[j][0]), min(a[i][1], b[j][1])
        if lo < hi:
            out.append([lo, hi])
        if a[i][1] < b[j][1]:
            i += 1
        else:
            j += 1
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace"); ap.add_argument("--top", type=int, default=25)
    ap.add_argument("--bin-us", type=float, default=250.0)
    a = ap.parse_args()
    res = build(a.trace, True, None)
    merged, qname = res["merged"], res["qname"]
    allm = sorted([m for ms in merged.values() for m in ms], key=lambda m: m.ns)
    t0 = allm[0].ns; t1 = allm[-1].ns
    us = lambda ns: (ns - t0) / 1000
    print(f"frame window (first..last marker): {us(t1):.1f} us")

    # --- command buffers per queue
    cbs = defaultdict(list)
    for src, ms in merged.items():
        cur = None
        for m in ms:
            if m.kind == "CB_START":
                cur = [m.ns, None, (m.dw[0] >> 7) & 0xFFFFF, 0]
            elif m.kind == "EVENT" and cur:
                cur[3] += 1
            elif m.kind == "CB_END" and cur:
                cur[1] = m.ns; cbs[qname[src]].append(cur); cur = None
        if cur:
            cur[1] = ms[-1].ns; cbs[qname[src]].append(cur)
    print("\n== command buffers ==")
    for q, lst in cbs.items():
        busy = union([[s, e] for s, e, *_ in lst])
        print(f"{q}: {len(lst)} CBs, busy(union CB spans) {total(busy)/1000:.1f} us")
        for s, e, cb, n in lst:
            print(f"   cb {cb:#07x}  {us(s):8.1f} -> {us(e):8.1f} us  ({(e-s)/1000:7.1f} us)  events={n}")

    # --- gfx segments busy, gaps
    segs = res["segments"]
    gfx = union([[s["start"], s["end"]] for s in segs if s["queue"] == "GFX"])
    gfx_cb = union([[s, e] for s, e, *_ in cbs.get("GFX", [])])
    print("\n== GFX idle gaps (no GFX command buffer executing) > 20 us ==")
    prev = None
    for s, e in gfx_cb:
        if prev is not None and s - prev > 20000:
            print(f"   gap {us(prev):8.1f} -> {us(s):8.1f} us  = {(s-prev)/1000:.1f} us")
        prev = e
    print(f"GFX CB union {total(gfx_cb)/1000:.1f} us of {us(t1):.1f} us window; GFX segment sum {total(gfx)/1000:.1f} us")

    # --- async overlap
    print("\n== async compute ==")
    for q in cbs:
        if q == "GFX":
            continue
        qs = union([[s["start"], s["end"]] for s in segs if s["queue"] == q])
        ov = intersect(qs, gfx_cb)
        print(f"{q}: segment time {total(qs)/1000:.1f} us, overlapping GFX CBs {total(ov)/1000:.1f} us, "
              f"in GFX gaps {(total(qs)-total(ov))/1000:.1f} us")
        for s in sorted([s for s in segs if s["queue"] == q], key=lambda s: -s["dur"])[:8]:
            e0 = s["events"][0]
            print(f"   seg {s['id']}: {us(s['start']):8.1f} us dur {s['dur']/1000:7.1f} us  ev={len(s['events'])} "
                  f"{api_name(e0['api'])} tg={e0['threads']} pso={e0['pso']}")

    # --- wave coverage per bin (all SEs) -> where is the GPU shader-busy
    from rgp_markers import fit_clock
    nb = int(us(t1) // a.bin_us) + 1
    wave_ns = [0.0] * nb; wcount = [0] * nb
    for s in res["ses"]:
        f = res["clocks"][s.se][1]
        for c0, c1 in s.waves:
            x0 = f(c0); b = int(us(x0) // a.bin_us)
            if 0 <= b < nb:
                wave_ns[b] += f(c1) - x0; wcount[b] += 1
    print(f"\n== wave activity per {a.bin_us:.0f} us bin (sum of wave lifetimes / bin length = avg waves in flight) ==")
    for b in range(nb):
        print(f"   {b*a.bin_us:7.0f} us  waves={wcount[b]:7d}  avg_in_flight={wave_ns[b]/(a.bin_us*1000):8.1f}")

    # --- top segments with event detail
    print(f"\n== top {a.top} segments: event detail ==")
    for s in sorted(segs, key=lambda s: -s["dur"])[:a.top]:
        kinds = defaultdict(int)
        for e in s["events"]:
            kinds[api_name(e["api"])] += 1
        tgs = sorted({e["threads"] for e in s["events"] if e["threads"]})[:3]
        print(f"seg {s['id']:4} {s['queue']:10} {us(s['start']):8.1f} us  {s['dur']/1000:7.1f} us  {dict(kinds)}  tg={tgs}  "
              f"psos={len(s['psos'])} first={s['psos'][:2]}")
    with open(os.path.splitext(a.trace)[0] + ".frame.pkl", "wb") as fh:
        pickle.dump(dict(segments=[{k: v for k, v in s.items()} for s in segs], cbs=dict(cbs), t0=t0, t1=t1), fh)


if __name__ == "__main__":
    main()
