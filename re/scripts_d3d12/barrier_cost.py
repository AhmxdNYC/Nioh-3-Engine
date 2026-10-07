r"""barrier_cost.py -- what the GFX queue's barriers cost, measured with wave occupancy (RGP / SQTT).

    py -I barrier_cost.py <trace.rgp> [outdir] [--window t0 t1]

Builds on barriers.py (same markers), but adds the shader-wave timeline so every barrier's time can be split into
  * drain  : the barrier waits while earlier work is still running on the GPU (waves in flight) - real work, not waste
  * idle   : no wave of ANY queue in flight while the barrier/chain is still open - pure sync/flush overhead
and every multi-barrier chain (no draw/dispatch/clear/copy between, i.e. what ONE ResourceBarrier(n) call would be)
into: first barrier (keeps the drain) + the time the 2nd..kth barrier add after the previous one ended ("extra").

Merge estimate per chain (what batching the API barriers into one call saves):
  upper = sum of "extra" increments of API barriers 2..k (driver-internal ones stay separate)
  cons  = upper - the largest increment whose flush flags were not already done earlier in the chain
          (a merged barrier still has to do the union of the flushes once)
Only chains whose extra time is idle GPU time count (if waves are still running, the extra barrier hides under work).

Also reports GPU idle gaps over the frame and attributes each idle interval to what the GFX CP was doing:
inside a barrier chain, at a command-buffer boundary (CB_END -> next CB_START: submission/queue wait), or other.

Outputs <name>.bcost_chains.csv, <name>.bcost_idle.csv and a summary on stdout.
"""
import sys, os, csv, bisect, collections
sys.path.insert(0, r"C:\Users\Ahmad Hamza\nioh3-engine\patcher\gpu")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rgp_markers import load            # noqa: E402
from rgp_report import merge_markers, classify_sources, event_info, api_name   # noqa: E402
from barriers import flags as end_flags  # noqa: E402

DRAIN_FLAGS = {"cs_pf", "ps_pf", "vs_pf", "eop", "eos_ps", "eos_cs"}


def busy_union(ses, clocks):
    """Union of all wave intervals (ns), all SEs, all queues."""
    iv = []
    for s in ses:
        if s.se not in clocks:
            continue
        f = clocks[s.se][1]
        for (a, b) in s.waves:
            iv.append((f(a), f(b)))
    iv.sort()
    out = []
    for a, b in iv:
        if out and a <= out[-1][1]:
            if b > out[-1][1]:
                out[-1][1] = b
        else:
            out.append([a, b])
    return out


class Busy:
    def __init__(self, u):
        self.u = u
        self.starts = [a for a, _ in u]
        # prefix sums of busy time
        self.pre = [0.0]
        for a, b in u:
            self.pre.append(self.pre[-1] + (b - a))

    def busy(self, t0, t1):
        """busy ns inside [t0, t1]"""
        if t1 <= t0:
            return 0.0
        i = bisect.bisect_right(self.starts, t0) - 1
        tot = 0.0
        i = max(i, 0)
        while i < len(self.u) and self.u[i][0] < t1:
            a, b = self.u[i]
            lo, hi = max(a, t0), min(b, t1)
            if hi > lo:
                tot += hi - lo
            i += 1
        return tot

    def idle(self, t0, t1):
        return max(0.0, (t1 - t0) - self.busy(t0, t1))

    def last_busy_before(self, t):
        """end of the last busy interval that started before t (clamped to t)"""
        i = bisect.bisect_left(self.starts, t) - 1
        if i < 0:
            return None
        return min(self.u[i][1], t)


def main():
    a = sys.argv[1:]
    win = None
    if "--window" in a:
        k = a.index("--window")
        win = (float(a[k + 1]), float(a[k + 2]))
        del a[k:k + 3]
    path = a[0]
    out = a[1] if len(a) > 1 else os.path.dirname(path)
    name = os.path.splitext(os.path.basename(path))[0]
    rdf, ses = load(path, None, True)
    merged, clocks = merge_markers(ses)
    qn = classify_sources(merged)
    t0 = min(m.ns for ms in merged.values() for m in ms if m.kind == "EVENT")
    B = Busy(busy_union(ses, clocks))
    gfx = next(s for s, n in qn.items() if n == "GFX")
    ms = merged[gfx]
    us = lambda ns: (ns - t0) / 1e3   # noqa: E731

    if win:
        lo, hi = win
        for m in ms:
            if lo <= us(m.ns) <= hi:
                extra = ""
                if m.kind == "EVENT":
                    api, dims, cmd, tg = event_info(m)
                    extra = f"{api_name(api)} tg={tg}"
                elif m.kind == "BARRIER_START":
                    extra = f"reason={m.dw[1] & 0x7FFFFFFF:#x} internal={(m.dw[1] >> 31) & 1}"
                elif m.kind == "BARRIER_END":
                    extra = " ".join(end_flags(m.dw))
                busy = B.busy(m.ns - 500, m.ns + 500) / 1e3
                print(f"{us(m.ns):10.2f}  {m.kind:18s} {extra}   busy(+-0.5us)={busy:.2f}us")
        return

    # ---- walk the GFX stream: barriers (paired start/end), chains, CB boundaries --------------------------------------
    chains, cur, pend = [], None, None
    cb_gaps = []          # (cb_end_ns, next_cb_start_ns)
    last_cb_end = None
    last_ev = None
    pso = ""
    for m in ms:
        if m.kind == "BIND_PIPELINE" and len(m.dw) >= 3:
            pso = f"{m.dw[2]:08x}{m.dw[1]:08x}"
        elif m.kind == "BARRIER_START":
            internal = (m.dw[1] >> 31) & 1 if len(m.dw) > 1 else 0
            if cur is None:
                cur = dict(start=m.ns, bars=[], prev=last_ev, cb_in=None)
            pend = dict(s=m.ns, e=None, internal=internal, flags=set(), reason=(m.dw[1] & 0x7FFFFFFF) if len(m.dw) > 1 else -1)
            cur["bars"].append(pend)
        elif m.kind == "BARRIER_END":
            if pend is not None:
                pend["e"] = m.ns
                pend["flags"] = set(end_flags(m.dw))
                pend = None
        elif m.kind == "EVENT":
            api, dims, cmd, tg = event_info(m)
            ev = (m.ns, api_name(api), pso, tg)
            if cur is not None:
                cur["next"] = ev
                chains.append(cur)
                cur = None
            last_ev = ev
        elif m.kind == "CB_END":
            if cur is not None:
                cur["next"] = (m.ns, "CB_END", "", None)
                chains.append(cur)
                cur = None
            last_cb_end = m.ns
        elif m.kind == "CB_START":
            if cur is not None:
                cur["next"] = (m.ns, "CB_START", "", None)
                chains.append(cur)
                cur = None
            if last_cb_end is not None:
                cb_gaps.append((last_cb_end, m.ns))
            last_cb_end = None

    rows = []
    agg = collections.Counter()
    hist_extra = collections.Counter()
    for c in chains:
        bars = [b for b in c["bars"] if b["e"] is not None]
        if not bars:
            continue
        n = len(bars)
        napi = sum(1 for b in bars if not b["internal"])
        first = bars[0]
        span0, span1 = first["s"], bars[-1]["e"]
        # time each barrier adds after the previous one ended
        incs = []
        prev_e = None
        done = set()
        cons_new = []
        for b in bars:
            base = b["s"] if prev_e is None else max(b["s"], prev_e)
            inc = max(0.0, b["e"] - base) if prev_e is not None else (b["e"] - b["s"])
            idle_inc = B.idle(base, b["e"]) if prev_e is not None else B.idle(b["s"], b["e"])
            newf = b["flags"] - done
            incs.append((b, inc, idle_inc, bool(newf - {"inv_tcp", "inv_sqK", "inv_sqI"})))
            done |= b["flags"]
            prev_e = b["e"] if prev_e is None else max(prev_e, b["e"])
        extra = incs[1:]
        extra_api = [(b, i, ii, nf) for (b, i, ii, nf) in extra if not b["internal"]]
        upper = sum(ii for (_, _, ii, _) in extra_api)
        newflush = [ii for (_, _, ii, nf) in extra_api if nf]
        cons = max(0.0, upper - (max(newflush) if newflush else 0.0))
        first_dur = first["e"] - first["s"]
        first_idle = B.idle(first["s"], first["e"])
        drain = first_dur - first_idle
        nxt = c.get("next") or (span1, "-", "", None)
        prv = c["prev"] or (span0, "-", "", None)
        after_idle = B.idle(span1, nxt[0])
        allf = collections.Counter()
        for b in bars:
            allf.update(b["flags"])
        rows.append(dict(t_us=f"{us(span0):.2f}", n=n, n_api=napi, prev=prv[1], prev_pso=prv[2], next=nxt[1], next_pso=nxt[2],
                         next_tg=nxt[3], span_us=f"{(span1 - span0) / 1e3:.2f}", first_us=f"{first_dur / 1e3:.2f}",
                         first_drain_us=f"{drain / 1e3:.2f}", first_idle_us=f"{first_idle / 1e3:.2f}",
                         extra_us=f"{sum(i for _, i, _, _ in extra) / 1e3:.2f}",
                         extra_idle_us=f"{sum(ii for _, _, ii, _ in extra) / 1e3:.2f}",
                         merge_upper_us=f"{upper / 1e3:.2f}", merge_cons_us=f"{cons / 1e3:.2f}",
                         after_idle_us=f"{after_idle / 1e3:.2f}",
                         flags=" ".join(f"{k}:{v}" for k, v in sorted(allf.items())),
                         reasons=" ".join(sorted({hex(b['reason']) for b in bars}))))
        agg["chains"] += 1
        agg["barriers"] += n
        agg["api"] += napi
        agg["internal"] += n - napi
        agg["first_ns"] += first_dur
        agg["first_idle_ns"] += first_idle
        agg["first_drain_ns"] += drain
        agg["extra_ns"] += sum(i for _, i, _, _ in extra)
        agg["extra_idle_ns"] += sum(ii for _, _, ii, _ in extra)
        agg["upper_ns"] += upper
        agg["cons_ns"] += cons
        agg["after_idle_ns"] += after_idle
        agg["mergeable_api_extra"] += max(0, napi - 1)
        pe = None
        for b in bars:
            if pe is not None and b["s"] > pe:
                g = b["s"] - pe
                agg["gap_ns"] += g
                agg["gap_idle_ns"] += B.idle(pe, b["s"])
                if g > 5000:
                    agg["gap_big_n"] += 1
                    agg["gap_big_ns"] += g
                    if "cpdma" in b["flags"]:
                        agg["gap_big_cpdma_ns"] += g
                    print(f"    inter-barrier CP gap {g / 1e3:7.1f} us at {us(pe):9.1f} (next barrier flags: {' '.join(sorted(b['flags']))})")
            pe = b["e"] if pe is None else max(pe, b["e"])
        for b in bars:
            f = b["flags"]
            if f & {"eop"}:
                agg["n_eop"] += 1
            if "cs_pf" in f:
                agg["n_cs_pf"] += 1
            if "ps_pf" in f:
                agg["n_ps_pf"] += 1
            if "fl_tcc" in f:
                agg["n_l2_flush"] += 1
            if "inv_tcc" in f:
                agg["n_l2_inv"] += 1
            if f & {"fl_cb", "fl_db"}:
                agg["n_rb_flush"] += 1
            if not (f & DRAIN_FLAGS):
                agg["n_nowait"] += 1
            agg["bar_ns"] += b["e"] - b["s"]
            agg["bar_idle_ns"] += B.idle(b["s"], b["e"])
        hist_extra[min(napi, 9)] += 1
        # by next-work kind
        key = nxt[1]
        agg["k_n:" + key] += 1
        agg["k_api:" + key] += napi
        agg["k_up:" + key] += upper
        agg["k_cons:" + key] += cons

    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, name + ".bcost_chains.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)

    # ---- frame idle gaps and their attribution ----------------------------------------------------------------------
    u = B.u
    f0 = min(m.ns for m in ms)
    f1 = max(m.ns for m in ms)
    gaps = []
    prev_end = f0
    for a, b in u:
        if a > prev_end and a <= f1 and prev_end >= f0:
            gaps.append((prev_end, min(a, f1)))
        prev_end = max(prev_end, b)
    chain_iv = sorted((c["bars"][0]["s"], max(b["e"] for b in c["bars"] if b["e"]))
                      for c in chains if any(b["e"] for b in c["bars"]))
    cb_iv = sorted(cb_gaps)

    def overlap(iv, a, b):
        tot = 0.0
        i = bisect.bisect_right([x for x, _ in iv], b)
        for x, y in iv[max(0, i - 64):i]:
            lo, hi = max(a, x), min(b, y)
            if hi > lo:
                tot += hi - lo
        return tot

    idle_rows = []
    tot = collections.Counter()
    for a, b in gaps:
        d = b - a
        inb = overlap(chain_iv, a, b)
        incb = overlap(cb_iv, a, b)
        other = max(0.0, d - inb - incb)
        tot["idle"] += d
        tot["in_barrier"] += inb
        tot["cb_gap"] += incb
        tot["other"] += other
        if d >= 2000:
            idle_rows.append(dict(t_us=f"{us(a):.2f}", dur_us=f"{d / 1e3:.2f}", in_barrier_us=f"{inb / 1e3:.2f}",
                                  cb_gap_us=f"{incb / 1e3:.2f}", other_us=f"{other / 1e3:.2f}"))
    with open(os.path.join(out, name + ".bcost_idle.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, ["t_us", "dur_us", "in_barrier_us", "cb_gap_us", "other_us"])
        w.writeheader()
        w.writerows(idle_rows)

    ms_ = lambda ns: f"{ns / 1e6:.3f} ms"   # noqa: E731
    print(f"frame span {ms_(f1 - f0)}; GPU busy (any wave in flight) {ms_(B.busy(f0, f1))}; idle {ms_(tot['idle'])}")
    print(f"  idle while a GFX barrier chain is open: {ms_(tot['in_barrier'])}; at CB boundaries (CB_END->CB_START): "
          f"{ms_(tot['cb_gap'])}; other (CP busy issuing / waiting w/o barrier): {ms_(tot['other'])}")
    print(f"GFX barriers: {agg['barriers']} ({agg['api']} API, {agg['internal']} driver-internal) in {agg['chains']} chains")
    print(f"  flags: EOP waits {agg['n_eop']}, CS partial flush {agg['n_cs_pf']}, PS partial flush {agg['n_ps_pf']}, "
          f"L2 flush {agg['n_l2_flush']}, L2 inv {agg['n_l2_inv']}, CB/DB flush {agg['n_rb_flush']}, no wait {agg['n_nowait']}")
    print(f"  sum of barrier durations {ms_(agg['bar_ns'])}, of which GPU fully idle {ms_(agg['bar_idle_ns'])}")
    print(f"  first barrier of each chain: {ms_(agg['first_ns'])} = drain under running work {ms_(agg['first_drain_ns'])}"
          f" + idle {ms_(agg['first_idle_ns'])}")
    print(f"  2nd..kth barriers add {ms_(agg['extra_ns'])} (idle {ms_(agg['extra_idle_ns'])}); idle from chain end to next"
          f" event {ms_(agg['after_idle_ns'])}")
    print(f"  merge (batch API barriers per chain): {agg['mergeable_api_extra']} API barriers removed; saving upper "
          f"{ms_(agg['upper_ns'])}, conservative {ms_(agg['cons_ns'])}")
    print(f"  CP gaps between consecutive barriers of a chain (no marker; CP DMA copies / waits): {ms_(agg['gap_ns'])}"
          f" (GPU idle {ms_(agg['gap_idle_ns'])}); >5 us: {agg['gap_big_n']} gaps {ms_(agg['gap_big_ns'])}, followed by a"
          f" CP-DMA wait {ms_(agg['gap_big_cpdma_ns'])}")
    print("  chains by API-barrier count:", " ".join(f"{k}{'+' if k == 9 else ''}:{v}" for k, v in sorted(hist_extra.items())))
    print("  by next work kind (chains / API barriers / merge upper / cons):")
    kinds = sorted({k.split(':', 1)[1] for k in agg if k.startswith('k_n:')}, key=lambda k: -agg['k_up:' + k])
    for k in kinds:
        print(f"    {k:28s} {agg['k_n:' + k]:4d} {agg['k_api:' + k]:5d}  {agg['k_up:' + k] / 1e3:7.1f} us  {agg['k_cons:' + k] / 1e3:7.1f} us")


if __name__ == "__main__":
    main()
