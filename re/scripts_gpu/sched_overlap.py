r"""sched_overlap.py -- async-compute / submission-timing study on one RGP capture (notes/gpu-scheduling.md).

    py -I re\scripts_gpu\sched_overlap.py <trace.rgp> [--out DIR] [--bin-us 5] [--cap-pct 99.5]

Builds on patcher/gpu/rgp_report.build() (segments, events, waves). Adds:
  * shader-group names for EVERY PSO in the frame (DXBC digest -> gpu-shaders/dxil_index.csv), not only the top ones;
  * a fine wave-occupancy timeline (waves in flight, all SEs, all queues) and the occupancy of every segment;
  * "wave-slot capacity" C = the cap-pct percentile of the fine bins (the GPU's practical wave-slot ceiling);
  * per GFX command buffer: busy time, average occupancy, free slot-time (C - in_flight integrated over time);
  * every compute (dispatch-only) segment on GFX and on the async queues;
  * an overlap model for moving a compute segment X onto a compute queue so that it runs under a GFX window W:
        packed = max(dur_W, (occ_W*dur_W + occ_X*dur_X) / C)          (perfect slot packing)
        saving_upper = dur_W + dur_X - packed ;  saving_real = saving_upper * eff  (eff 0.5: cache/BW contention)
Outputs: <out>/<trace>.sched_segments.tsv, <trace>.sched_cbs.tsv, <trace>.sched_summary.txt (printed too).
Waves carry no queue id in this parser, so occupancy is GPU-wide (async waves included in the window they ran in).
"""
from __future__ import annotations
import argparse, bisect, os, sys
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "patcher", "gpu"))
from rgp_report import build, api_name, pso_table, api_shader_hashes, DXIL_INDEX  # noqa: E402


def group_names(res):
    m, cos = pso_table(res["rdf"])
    cache = {}

    def name(pso):
        if pso in cache:
            return cache[pso]
        ph, _ = m.get(pso, ("", ""))
        c = cos.get(ph)
        out = []
        if c is not None:
            for stage, dig in api_shader_hashes(res["rdf"].data(c)):
                g = DXIL_INDEX.get(dig)
                if g:
                    out.append(f"{stage[:2]}:{g[1].split('-')[0]}")
        # keep the most specific stage names, deduplicated
        seen, uniq = set(), []
        for s in out:
            k = s.split(":", 1)[1]
            if k not in seen:
                seen.add(k); uniq.append(k)
        cache[pso] = "+".join(uniq) if uniq else ("ext" if c is not None else "?")
        return cache[pso]
    return name


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace")
    ap.add_argument("--out", default=r"D:\nioh3-re\gpusched")
    ap.add_argument("--bin-us", type=float, default=5.0)
    ap.add_argument("--cap-pct", type=float, default=99.5)
    ap.add_argument("--eff", type=float, default=0.5)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    base = os.path.join(a.out, os.path.splitext(os.path.basename(a.trace))[0])
    res = build(a.trace, True, None)
    merged, qname, segs = res["merged"], res["qname"], res["segments"]
    allm = sorted([mk for ms in merged.values() for mk in ms], key=lambda mk: mk.ns)
    t0, t1 = allm[0].ns, allm[-1].ns
    us = lambda ns: (ns - t0) / 1000.0  # noqa: E731
    gname = group_names(res)

    # ---- command buffers per queue
    cbs = []
    for src, ms in merged.items():
        cur = None
        for mk in ms:
            if mk.kind == "CB_START":
                cur = [qname[src], mk.ns, None, (mk.dw[0] >> 7) & 0xFFFFF]
            elif mk.kind == "CB_END" and cur:
                cur[2] = mk.ns; cbs.append(cur); cur = None
    cbs.sort(key=lambda c: c[1])

    # ---- occupancy timeline (waves in flight, GPU wide), difference array over fine bins
    B = a.bin_us * 1000.0
    nb = int((t1 - t0) // B) + 2
    occ = [0.0] * nb
    for s in res["ses"]:
        f = res["clocks"][s.se][1]
        for c0, c1 in s.waves:
            x0, x1 = f(c0) - t0, f(c1) - t0
            if x1 <= 0 or x0 >= nb * B:
                continue
            x0 = max(x0, 0.0); x1 = min(x1, nb * B - 1)
            b0, b1 = int(x0 // B), int(x1 // B)
            if b0 == b1:
                occ[b0] += (x1 - x0) / B
            else:
                occ[b0] += ((b0 + 1) * B - x0) / B
                for b in range(b0 + 1, b1):
                    occ[b] += 1.0
                occ[b1] += (x1 - b1 * B) / B
    srt = sorted(occ)
    C = srt[min(len(srt) - 1, int(len(srt) * a.cap_pct / 100.0))]

    def window(ns0, ns1):
        """average in-flight waves and free slot-time (C - occ, us*slots) in [ns0, ns1)"""
        x0, x1 = ns0 - t0, ns1 - t0
        if x1 <= x0:
            return 0.0, 0.0
        b0, b1 = int(x0 // B), int(x1 // B)
        tot = free = 0.0
        for b in range(max(b0, 0), min(b1, nb - 1) + 1):
            lo, hi = max(x0, b * B), min(x1, (b + 1) * B)
            if hi <= lo:
                continue
            w = (hi - lo) / 1000.0
            tot += occ[b] * w; free += max(0.0, C - occ[b]) * w
        d = (x1 - x0) / 1000.0
        return tot / d, free

    # ---- segment table
    def cb_of(seg):
        for q, s0, s1, cb in cbs:
            if q == seg["queue"] and s0 <= seg["start"] + 1 and seg["start"] <= s1 + 1:
                return cb
        return 0
    rows = []
    for sg in segs:
        kinds = defaultdict(int)
        for e in sg["events"]:
            kinds[api_name(e["api"])] += 1
        groups = []
        for p in sg["psos"]:
            n = gname(p)
            if n not in groups:
                groups.append(n)
        disp_only = all(api_name(e["api"]) in ("Dispatch", "api15") for e in sg["events"])
        o, fr = window(sg["start"], sg["end"])
        tg = [e["threads"] for e in sg["events"] if e["threads"]]
        rows.append(dict(id=sg["id"], q=sg["queue"], cb=cb_of(sg), start=us(sg["start"]), dur=sg["dur"] / 1000.0,
                         n=len(sg["events"]), kind="CS" if disp_only else ("GFX" if not any(
                             api_name(e["api"]) == "Dispatch" for e in sg["events"]) else "MIX"),
                         occ=o, free=fr, groups=",".join(groups)[:120], api=" ".join(f"{k}:{v}" for k, v in kinds.items()),
                         tg=str(tg[0]) if tg else "", psos=" ".join(sg["psos"][:4])))
    rows.sort(key=lambda r: (r["q"] != "GFX", r["start"]))
    with open(base + ".sched_segments.tsv", "w", encoding="utf-8") as f:
        f.write("seg\tqueue\tcb\tstart_us\tdur_us\tkind\tevents\tavg_waves\tocc_pct\tfree_slot_us\tgroups\tapi\tfirst_tg\tpsos\n")
        for r in rows:
            f.write(f"{r['id']}\t{r['q']}\t{r['cb']:#x}\t{r['start']:.1f}\t{r['dur']:.1f}\t{r['kind']}\t{r['n']}\t"
                    f"{r['occ']:.0f}\t{100*r['occ']/C:.0f}\t{r['free']:.0f}\t{r['groups']}\t{r['api']}\t{r['tg']}\t{r['psos']}\n")

    out = []
    P = out.append
    P(f"trace {os.path.basename(a.trace)}  window {us(t1):.1f} us  bins {a.bin_us} us  wave-slot capacity C = {C:.0f} "
      f"(p{a.cap_pct} of bins; max bin {srt[-1]:.0f})")
    gfx_busy = sum(r["dur"] for r in rows if r["q"] == "GFX")
    gfx_free = sum(r["free"] for r in rows if r["q"] == "GFX")
    P(f"GFX segment time {gfx_busy:.0f} us; free wave-slot time inside GFX segments {gfx_free:.0f} slot-us "
      f"= {gfx_free / C:.0f} us of a fully idle GPU")
    tot_o, tot_f = window(t0, t1)
    P(f"whole window: avg waves in flight {tot_o:.0f} ({100*tot_o/C:.0f} % of C), free {tot_f/C:.0f} us-equivalent")

    P("\n== command buffers (avg occupancy over the CB span) ==")
    with open(base + ".sched_cbs.tsv", "w", encoding="utf-8") as f:
        f.write("queue\tcb\tstart_us\tend_us\tdur_us\tavg_waves\tocc_pct\tfree_slot_us\tcs_us\tcs_segs\ttop_groups\n")
        for q, s0, s1, cb in cbs:
            o, fr = window(s0, s1)
            mine = [r for r in rows if r["cb"] == cb and r["q"] == q]
            cs = [r for r in mine if r["kind"] == "CS"]
            top = sorted(mine, key=lambda r: -r["dur"])[:4]
            tops = "; ".join(f"{r['groups'][:28]} {r['dur']:.0f}" for r in top)
            f.write(f"{q}\t{cb:#x}\t{us(s0):.1f}\t{us(s1):.1f}\t{(s1-s0)/1000:.1f}\t{o:.0f}\t{100*o/C:.0f}\t{fr:.0f}\t"
                    f"{sum(r['dur'] for r in cs):.0f}\t{len(cs)}\t{tops}\n")
            P(f"{q:10} cb {cb:#07x} {us(s0):8.1f}-{us(s1):8.1f} ({(s1-s0)/1000:7.1f} us) occ {100*o/C:3.0f}%  "
              f"CS {sum(r['dur'] for r in cs):6.0f} us/{len(cs):3d}  | {tops}")

    P("\n== compute (dispatch-only) segments >= 15 us ==")
    for r in sorted([r for r in rows if r["kind"] == "CS" and r["dur"] >= 15], key=lambda r: r["start"]):
        P(f"{r['q']:10} seg {r['id']:4} cb {r['cb']:#07x} {r['start']:8.1f} {r['dur']:7.1f} us occ {100*r['occ']/C:3.0f}% "
          f"tg={r['tg']:14} {r['groups']}")
    cs_gfx = sum(r["dur"] for r in rows if r["kind"] == "CS" and r["q"] == "GFX")
    cs_async = sum(r["dur"] for r in rows if r["kind"] == "CS" and r["q"] != "GFX")
    P(f"compute on GFX: {cs_gfx:.0f} us in {sum(1 for r in rows if r['kind']=='CS' and r['q']=='GFX')} segments; "
      f"on async queues: {cs_async:.0f} us")

    P("\n== GFX windows by occupancy class (segments >= 20 us) ==")
    cls = defaultdict(lambda: [0.0, 0.0, 0])
    for r in rows:
        if r["q"] != "GFX" or r["dur"] < 20:
            continue
        k = "low<35%" if r["occ"] < 0.35 * C else ("mid35-65%" if r["occ"] < 0.65 * C else "high>65%")
        cls[k][0] += r["dur"]; cls[k][1] += r["free"]; cls[k][2] += 1
    for k, (d, fr, n) in sorted(cls.items()):
        P(f"  {k:10} {n:4d} segs {d:8.0f} us  free {fr/C:7.0f} us-equivalent")

    with open(base + ".sched_summary.txt", "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    print("\n".join(out))
    print(f"\nwrote {base}.sched_segments.tsv / .sched_cbs.tsv / .sched_summary.txt")


if __name__ == "__main__":
    main()
