r"""barriers.py -- barrier-level analysis of an RGP capture (uses patcher/gpu/rgp_markers.py).

    py -I barriers.py <trace.rgp> [outdir]

Decodes BARRIER_START / BARRIER_END / LAYOUT_TRANSITION markers (Mesa ac_sqtt.h layouts) on every
queue, groups back-to-back barriers (no draw/dispatch between them) into "chains" (= what one
batched ResourceBarrier call could have been), and reports per chain: barrier count, the GPU time
from the last event before the chain to the first event after it ("gap"), the sync/flush flags
and layout transitions. Also writes <outdir>/<name>.barriers.csv (one row per barrier) and
<name>.chains.csv. GPU-side only: RGP markers carry no resource identity.
"""
import sys, os, csv, collections
sys.path.insert(0, r"C:\Users\Ahmad Hamza\nioh3-engine\patcher\gpu")
from rgp_markers import load            # noqa: E402
from rgp_report import merge_markers, classify_sources, event_info, api_name   # noqa: E402

END_FLAGS1 = [(27, "eop"), (28, "vs_pf"), (29, "ps_pf"), (30, "cs_pf"), (31, "pfp_me")]
END_FLAGS2 = [(0, "cpdma"), (1, "inv_tcp"), (2, "inv_sqI"), (3, "inv_sqK"), (4, "fl_tcc"), (5, "inv_tcc"),
              (6, "fl_cb"), (7, "inv_cb"), (8, "fl_db"), (9, "inv_db"), (26, "inv_gl1"), (27, "wait_ts"),
              (28, "eop_bop"), (29, "eos_ps"), (30, "eos_cs")]
LT_FLAGS = [(7, "ds_expand"), (8, "hiz_expand"), (9, "ds_resum"), (10, "dcc_decomp"), (11, "fmask_decomp"),
            (12, "fce"), (13, "fmask_expand"), (14, "init_mask")]


def flags(dw):
    f = [n for b, n in END_FLAGS1 if (dw[0] >> b) & 1]
    if len(dw) > 1:
        f += [n for b, n in END_FLAGS2 if (dw[1] >> b) & 1]
        nlt = (dw[1] >> 10) & 0xFFFF
        if nlt:
            f.append(f"lt{nlt}")
    return f


def main():
    path = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(path)
    name = os.path.splitext(os.path.basename(path))[0]
    rdf, ses = load(path, None, False)
    merged, clocks = merge_markers(ses)
    qn = classify_sources(merged)
    t0 = min(m.ns for ms in merged.values() for m in ms if m.kind == "EVENT")
    rows, chains = [], []
    for src, ms in merged.items():
        q = qn[src]
        last_ev = None          # (ns, kind, pso)
        pso = ""
        chain = None
        for m in ms:
            if m.kind == "BIND_PIPELINE" and len(m.dw) >= 3:
                pso = f"{m.dw[2]:08x}{m.dw[1]:08x}"
            elif m.kind == "BARRIER_START":
                reason = m.dw[1] & 0x7FFFFFFF if len(m.dw) > 1 else -1
                internal = (m.dw[1] >> 31) & 1 if len(m.dw) > 1 else -1
                if chain is None:
                    chain = dict(queue=q, start=m.ns, n=0, internal=0, reasons=collections.Counter(),
                                 flags=collections.Counter(), lt=collections.Counter(),
                                 prev=last_ev, end=m.ns)
                chain["n"] += 1
                chain["internal"] += internal == 1
                chain["reasons"][hex(reason)] += 1
                rows.append(dict(queue=q, t_us=(m.ns - t0) / 1e3, kind="start", reason=hex(reason), internal=internal,
                                 flags=""))
            elif m.kind == "BARRIER_END":
                f = flags(m.dw)
                if chain is not None:
                    chain["end"] = m.ns
                    chain.setdefault("first_end", m.ns)
                    for x in f:
                        chain["flags"][x] += 1
                rows.append(dict(queue=q, t_us=(m.ns - t0) / 1e3, kind="end", reason="", internal="", flags=" ".join(f)))
            elif m.kind == "LAYOUT_TRANSITION":
                f = [n for b, n in LT_FLAGS if (m.dw[0] >> b) & 1]
                if chain is not None:
                    for x in f:
                        chain["lt"][x] += 1
                rows.append(dict(queue=q, t_us=(m.ns - t0) / 1e3, kind="layout", reason="", internal="", flags=" ".join(f)))
            elif m.kind == "EVENT":
                api, dims, cmd, tg = event_info(m)
                ev = (m.ns, api_name(api), pso, tg)
                if chain is not None:
                    chain["next"] = ev
                    chains.append(chain)
                    chain = None
                last_ev = ev
            elif m.kind in ("CB_END", "CB_START") and chain is not None:
                chain["next"] = (m.ns, m.kind, "", None)
                chains.append(chain)
                chain = None
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, name + ".barriers.csv"), "w", newline="") as fh:
        w = csv.DictWriter(fh, ["queue", "t_us", "kind", "reason", "internal", "flags"])
        w.writeheader()
        w.writerows(sorted(rows, key=lambda r: (r["queue"], r["t_us"])))
    with open(os.path.join(out, name + ".chains.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["queue", "t_us", "n_barriers", "internal", "chain_us", "gap_us", "prev_kind", "prev_pso", "prev_tg",
                    "next_kind", "next_pso", "next_tg", "flags", "layout", "reasons", "tail_us", "after_us"])
        for c in chains:
            p = c["prev"] or (c["start"], "-", "", None)
            n = c.get("next") or (c["end"], "-", "", None)
            w.writerow([c["queue"], f"{(c['start'] - t0) / 1e3:.1f}", c["n"], c["internal"],
                        f"{(c['end'] - c['start']) / 1e3:.2f}", f"{(n[0] - p[0]) / 1e3:.2f}",
                        p[1], p[2], p[3], n[1], n[2], n[3],
                        " ".join(f"{k}:{v}" for k, v in sorted(c["flags"].items())),
                        " ".join(f"{k}:{v}" for k, v in sorted(c["lt"].items())),
                        " ".join(f"{k}:{v}" for k, v in c["reasons"].most_common(6)),
                        f"{(c['end'] - c.get('first_end', c['end'])) / 1e3:.2f}", f"{(n[0] - c['end']) / 1e3:.2f}"])
    # ---- summary ----
    for q in sorted(set(c["queue"] for c in chains)):
        cs = [c for c in chains if c["queue"] == q]
        nb = sum(c["n"] for c in cs)
        ni = sum(c["internal"] for c in cs)
        hist = collections.Counter(min(c["n"], 9) for c in cs)
        print(f"== {q}: {nb} barriers ({ni} driver-internal) in {len(cs)} chains; chain length histogram "
              + " ".join(f"{k}{'+' if k == 9 else ''}:{v}" for k, v in sorted(hist.items())))
        tot = collections.Counter()
        for c in cs:
            tot.update(c["flags"])
        print("   END flags over all barriers:", " ".join(f"{k}:{v}" for k, v in tot.most_common()))
        lt = collections.Counter()
        for c in cs:
            lt.update(c["lt"])
        print("   layout transitions:", " ".join(f"{k}:{v}" for k, v in lt.most_common()) or "none")
        rs = collections.Counter()
        for c in cs:
            rs.update(c["reasons"])
        print("   driver_reason codes:", " ".join(f"{k}:{v}" for k, v in rs.most_common(12)))
        multi = [c for c in cs if c["n"] > 1]
        print(f"   chains with >1 barrier: {len(multi)} holding {sum(c['n'] for c in multi)} barriers; "
              f"sum chain time {sum(c['end'] - c['start'] for c in multi) / 1e3:.1f} us "
              f"(all chains {sum(c['end'] - c['start'] for c in cs) / 1e3:.1f} us)")
        tails = [(c["end"] - c.get("first_end", c["end"])) / 1e3 for c in cs]
        print(f"   tail (first BARRIER_END -> last BARRIER_END inside a chain): sum {sum(tails):.1f} us over {sum(1 for t in tails if t > 0)} chains")
        aft = [((c.get("next") or (c["end"],))[0] - c["end"]) / 1e3 for c in cs]
        print(f"   last BARRIER_END -> next event: sum {sum(aft):.1f} us")
        gaps = [((c.get('next') or (c['end'],))[0] - (c['prev'] or (c['start'],))[0]) / 1e3 for c in cs]
        print(f"   event->event gap across chains: sum {sum(gaps):.1f} us, median {sorted(gaps)[len(gaps) // 2]:.2f} us")


if __name__ == "__main__":
    main()

