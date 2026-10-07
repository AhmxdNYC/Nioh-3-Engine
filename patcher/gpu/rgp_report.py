"""rgp_report.py - headless per-pass / per-event GPU cost tables from an RDF .rgp capture.

    py rgp_report.py <trace.rgp> [--out DIR] [--no-waves] [--ses 0,1] [--top 40]

Outputs (next to the trace or in --out):
    <name>.summary.txt   frame overview + top segments + per-pass rollup
    <name>.segments.csv  barrier-delimited GPU segments (the reliable timing unit)
    <name>.events.csv    every draw/dispatch marker with pass path, PSO hash, approx wave cost
    <name>.passes.csv    rollup by user-marker path (PIX/AGS names) - or by PSO when no markers

Method (see notes/gpu-profiling.md):
  * The driver writes RGP instrumentation markers (CB start/end, per-draw/dispatch EVENT,
    BARRIER start/end, PIX/AGS USER_EVENT push/pop, BIND_PIPELINE) into the SQTT stream.
    Their timestamps are CP (command processor) times.
  * A BARRIER_END marker is written after the barrier's wait/flush completes, i.e. when the
    preceding work has drained. So the interval between consecutive drain points is the true
    GPU execution time of the events between them (serialised by the barrier) -> "segment".
  * Segment time is split across its events/passes in proportion to approximate wave-time
    (each wave attributed to the most recent EVENT marker before it started), else equally.
  * Async-compute queues are timed separately (their own segments); their time overlaps gfx.
"""
from __future__ import annotations
import argparse, csv, os, statistics, sys
from collections import defaultdict
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rgp_markers import load, fit_clock

DRAIN_KINDS = {"BARRIER_END"}

# DX12 EVENT api_type codes. 0/2/9/34 verified against RGP's own event statistics on the
# RDTS sample; the rest follow ID3D12GraphicsCommandList method order (unverified -> "?").
DX12_API = {0: "DrawInstanced", 1: "DrawIndexedInstanced", 2: "Dispatch", 3: "CopyBufferRegion?",
            4: "CopyTextureRegion?", 5: "CopyResource?", 6: "CopyTiles?", 7: "ResolveSubresource?",
            8: "ClearDepthStencilView?", 9: "ClearRenderTargetView", 10: "ClearUAVUint?",
            11: "ClearUAVFloat?", 34: "DevDriverOverlay(internal)"}


def api_name(api):
    return DX12_API.get(api, f"api{api}")


def merge_markers(ses):
    """Canonical per-source marker list with median absolute ns across SEs."""
    clocks = {}
    for s in ses:
        fc = fit_clock(s.ts_pairs)
        if fc:
            clocks[s.se] = fc
    if not clocks:
        raise SystemExit("no usable absolute timestamps in any SE stream")
    srcs = set()
    for s in ses:
        srcs |= set(s.sources)
    merged = {}
    for src in srcs:
        cands = [(s, s.sources.get(src, [])) for s in ses if s.se in clocks]
        base_se, base = max(cands, key=lambda x: len(x[1]))
        times = [[clocks[base_se.se][1](m.t)] for m in base]
        for s, lst in cands:
            if s is base_se:
                continue
            j = 0
            f = clocks[s.se][1]
            for i, m in enumerate(base):
                for k in range(j, min(j + 6, len(lst))):
                    if lst[k].dw == m.dw:
                        times[i].append(f(lst[k].t)); j = k + 1
                        break
        for m, ts in zip(base, times):
            m.ns = statistics.median(ts)
            m.spread = (max(ts) - min(ts)) if len(ts) > 1 else 0.0
        merged[src] = base
    return merged, clocks


def classify_sources(merged):
    """Name sources: the one with most EVENTs = GFX; others with events = COMPUTE/other."""
    names = {}
    ranked = sorted(merged, key=lambda s: -sum(1 for m in merged[s] if m.kind == "EVENT"))
    for i, s in enumerate(ranked):
        names[s] = "GFX" if i == 0 else f"Q{i}(src{s:#x})"
    return names


def event_info(m):
    d0 = m.dw[0]
    api = (d0 >> 7) & 0xFFFFFF
    dims = (d0 >> 31) & 1
    cmd_id = m.dw[2] if len(m.dw) > 2 else -1
    tg = tuple(m.dw[3:6]) if dims and len(m.dw) >= 6 else None
    return api, dims, cmd_id, tg


def build(path, want_waves=True, ses_sel=None):
    rdf, ses = load(path, ses_sel, want_waves)
    merged, clocks = merge_markers(ses)
    qname = classify_sources(merged)

    # ---- per-source walk: pass stack, PSO, segments --------------------------------------
    events, segments = [], []
    for src, ms in merged.items():
        stack, pso = [], ""
        seg = None
        seg_start = None

        def close(end_ns, why):
            nonlocal seg
            if seg is not None:
                seg["end"] = end_ns
                seg["dur"] = max(0.0, end_ns - seg["start"])
                fd = seg.get("first_drain")
                # time spent in the 2nd..nth back-to-back barrier (cache flush/invalidate, EOP waits)
                seg["drain_tail"] = (end_ns - fd) if fd is not None else 0.0
                seg["closed_by"] = why
                segments.append(seg)
            seg = None

        # A segment = [drain before its first event, LAST drain before the next event]. Barrier
        # chains with no work in between are folded into the preceding segment: the work is not
        # finished (RB caches flushed, EOP reached) until the last of those barriers completes.
        for m in ms:
            if m.kind == "EVENT" and seg is not None and seg.get("last_drain") is not None:
                close(seg["last_drain"], "BARRIER_END")
                seg_start = m.ns if seg_start is None else seg_start
            if m.kind == "CB_START":
                if seg is not None:
                    close(seg.get("last_drain") or m.ns, "CB_START")
                seg_start = m.ns
            elif m.kind == "USER_EVENT":
                if m.utype == 2:
                    stack.append(m.text or "?")
                elif m.utype == 1 and stack:
                    stack.pop()
            elif m.kind == "BIND_PIPELINE" and len(m.dw) >= 3:
                pso = f"{m.dw[2]:08x}{m.dw[1]:08x}"
            elif m.kind == "EVENT":
                api, dims, cmd_id, tg = event_info(m)
                if seg is None:
                    seg = dict(queue=qname[src], start=seg_start if seg_start is not None else m.ns,
                               events=[], passes=defaultdict(int))
                ev = dict(queue=qname[src], ns=m.ns, api=api, dispatch=dims, cmd_id=cmd_id,
                          threads=tg, pass_path="/".join(stack), pso=pso, waves=0, wave_ns=0.0,
                          seg=None)
                seg["events"].append(ev)
                events.append(ev)
            elif m.kind in DRAIN_KINDS:
                if seg is not None:
                    seg.setdefault("first_drain", m.ns)
                    seg["last_drain"] = m.ns
                seg_start = m.ns
            elif m.kind == "CB_END":
                if seg is not None:
                    close(seg.get("last_drain") or m.ns, "CB_END")
                seg_start = None
        if seg is not None:   # trace ended inside a segment: lower bound = last marker on this queue
            close(ms[-1].ns, "TRACE_END(open,lower-bound)")

    # ---- approximate wave attribution -----------------------------------------------------
    # A wave is given to the most recent EVENT marker (<= wave start) among events whose
    # segment is still open at that time (segments drain at their BARRIER_END). Exact when a
    # segment holds one event and no other queue overlaps; approximate otherwise.
    total_waves = 0
    if want_waves and events:
        import bisect
        evs = sorted(events, key=lambda e: e["ns"])
        ens = [e["ns"] for e in evs]
        seg_end = {}
        for sg in segments:
            for e in sg["events"]:
                seg_end[id(e)] = sg["end"]
        for s in ses:
            if s.se not in clocks:
                continue
            f = clocks[s.se][1]
            for (c0, c1) in s.waves:
                a, b = f(c0), f(c1)
                total_waves += 1
                j = bisect.bisect_right(ens, a) - 1
                k = 0
                while j >= 0 and k < 64:
                    e = evs[j]
                    if seg_end.get(id(e), 0) >= a:
                        e["waves"] += 1
                        e["wave_ns"] += (b - a)
                        break
                    j -= 1; k += 1

    # ---- segment -> event/pass split ----------------------------------------------------
    for i, sg in enumerate(segments):
        sg["id"] = i
        wsum = sum(e["wave_ns"] for e in sg["events"])
        n = len(sg["events"])
        for e in sg["events"]:
            e["seg"] = i
            share = (e["wave_ns"] / wsum) if wsum > 0 else (1.0 / n)
            e["est_ns"] = sg["dur"] * share
        paths = defaultdict(float)
        for e in sg["events"]:
            paths[e["pass_path"] or "(no marker)"] += e["est_ns"]
        sg["paths"] = dict(paths)
        sg["psos"] = sorted({e["pso"] for e in sg["events"]})
    return dict(rdf=rdf, ses=ses, merged=merged, clocks=clocks, qname=qname,
                events=events, segments=segments, total_waves=total_waves)


def fmt_us(ns):
    return f"{ns / 1000:.1f}"


def report(res, out_base, top=40):
    ev, sg = res["events"], res["segments"]
    gfx = [s for s in sg if s["queue"] == "GFX"]
    all_ns = [m.ns for ms in res["merged"].values() for m in ms]
    t0 = min(all_ns) if all_ns else 0
    span = (max(all_ns) - t0) if all_ns else 0
    have_names = any(e["pass_path"] for e in ev)

    # passes rollup (full path, and depth-1/2 prefixes)
    # est = wave-weighted share of every segment; exact = only segments whose events all
    # belong to this pass (barrier-isolated -> true GPU time of the pass)
    roll = defaultdict(lambda: [0.0, 0, 0, 0.0])
    def pkey(e):
        return (e["queue"], e["pass_path"] or (f"PSO {e['pso']}" if not have_names else "(no marker)"))
    for e in ev:
        r = roll[pkey(e)]; r[0] += e.get("est_ns", 0.0); r[1] += 1; r[2] += e["waves"]
    for s_ in sg:
        keys = {pkey(e) for e in s_["events"]}
        if len(keys) == 1:
            roll[keys.pop()][3] += s_["dur"]
    depth = defaultdict(float)
    for e in ev:
        if e["queue"] != "GFX":
            continue
        parts = (e["pass_path"] or "(no marker)").split("/")
        for d in (1, 2):
            depth[(d, "/".join(parts[:d]))] += e.get("est_ns", 0.0)

    with open(out_base + ".segments.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["seg", "queue", "start_us", "dur_us", "drain_tail_us", "closed_by", "n_events", "dispatches", "draws",
                    "waves", "dominant_pass", "psos"])
        for s in sg:
            dom = max(s["paths"].items(), key=lambda kv: kv[1])[0] if s["paths"] else ""
            nd = sum(e["dispatch"] for e in s["events"])
            w.writerow([s["id"], s["queue"], fmt_us(s["start"] - t0), fmt_us(s["dur"]), fmt_us(s.get("drain_tail", 0)), s["closed_by"],
                        len(s["events"]), nd, len(s["events"]) - nd, sum(e["waves"] for e in s["events"]),
                        dom, " ".join(s["psos"][:8])])
    with open(out_base + ".events.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["queue", "t_us", "seg", "kind", "api_type", "cmd_id", "threadgroup", "pass_path",
                    "pso", "approx_waves", "approx_wave_us", "est_us"])
        for e in sorted(ev, key=lambda e: e["ns"]):
            w.writerow([e["queue"], fmt_us(e["ns"] - t0), e["seg"], api_name(e["api"]),
                        e["api"], e["cmd_id"], "x".join(map(str, e["threads"])) if e["threads"] else "",
                        e["pass_path"], e["pso"], e["waves"], fmt_us(e["wave_ns"]), fmt_us(e.get("est_ns", 0))])
    with open(out_base + ".passes.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["queue", "pass", "est_us", "exact_us", "pct_of_frame", "events", "approx_waves"])
        for (q, p), (ns, n, wv, ex) in sorted(roll.items(), key=lambda kv: -kv[1][0]):
            w.writerow([q, p, fmt_us(ns), fmt_us(ex), f"{100 * ns / span:.1f}" if span else "", n, wv])

    L = []
    L.append(f"trace: {res['rdf'].buf and out_base}")
    L.append(f"SEs decoded: {[s.se for s in res['ses']]}  clock ratios (shader cycles per 10ns tick): "
             + ", ".join(f"SE{k}={v[0]:.1f}" for k, v in res["clocks"].items()))
    L.append(f"queues: {res['qname']}")
    L.append(f"frame span (first..last marker): {span / 1e6:.3f} ms; markers={len(all_ns)} events={len(ev)} "
             f"segments={len(sg)} waves={res['total_waves']}  bad-marker-dwords={sum(s.bad for s in res['ses'])}")
    gsum = sum(s["dur"] for s in gfx)
    L.append(f"GFX segment time sum: {gsum / 1e6:.3f} ms (= GFX-queue busy+barrier time)")
    L.append(f"user-marker names present: {have_names}")
    L.append("")
    L.append(f"== top {top} segments by duration ==")
    L.append(f"{'seg':>5} {'queue':8} {'start_us':>9} {'dur_us':>8} {'ev':>4} {'waves':>7}  dominant pass / PSOs")
    for s in sorted(sg, key=lambda s: -s["dur"])[:top]:
        dom = max(s["paths"].items(), key=lambda kv: kv[1])[0] if s["paths"] else ""
        L.append(f"{s['id']:>5} {s['queue']:8} {fmt_us(s['start'] - t0):>9} {fmt_us(s['dur']):>8} "
                 f"{len(s['events']):>4} {sum(e['waves'] for e in s['events']):>7}  {dom}  [{' '.join(s['psos'][:3])}]")
    L.append("")
    L.append("== passes (est. GPU time) ==")
    L.append("queue      est_us  exact_us  %frame  events  pass  (exact = barrier-isolated segments only)")
    for (q, p), (ns, n, wv, ex) in sorted(roll.items(), key=lambda kv: -kv[1][0])[:top]:
        L.append(f"{q:8} {fmt_us(ns):>8} {fmt_us(ex):>9} {100 * ns / span if span else 0:6.1f}%  {n:>6}  {p}")
    if have_names:
        L.append("")
        L.append("== GFX rollup by marker depth ==")
        for (d, p), ns in sorted(depth.items(), key=lambda kv: (kv[0][0], -kv[1])):
            L.append(f"d{d} {fmt_us(ns):>9} us  {p}")
    txt = "\n".join(L)
    with open(out_base + ".summary.txt", "w", encoding="utf-8") as f:
        f.write(txt + "\n")
    return txt


def pso_table(rdf):
    """PsoCorrelation: api_pso_hash(u64) -> internal pipeline hash(16B) [+ api object name];
    CodeObject chunks are ELF code objects keyed by that pipeline hash (header bytes 8..24)."""
    import struct
    m = {}
    for c in rdf.find("PsoCorrelation"):
        d = rdf.data(c)
        for off in range(0, len(d) - 95, 96):
            api, = struct.unpack_from("<Q", d, off + 8)
            ph = bytes(d[off + 16:off + 32]).hex()
            name = bytes(d[off + 32:off + 96]).split(b"\0")[0].decode("utf-8", "replace")
            m[f"{api:016x}"] = (ph, name)
    cos = {}
    for c in rdf.find("CodeObject"):
        h = rdf.header(c)
        if len(h) >= 24:
            cos[bytes(h[8:24]).hex()] = c
    return m, cos


def api_shader_hashes(elf: bytes):
    """PAL ELF metadata: '.api_shader_hash' = [u64, u64] per hardware stage. The pair is the D3D12
    DXBC container digest: second u64 = LE bytes 0..7, first u64 = LE bytes 8..15."""
    import re, struct
    out = []
    for m in re.finditer(rb"\.api_shader_hash\x92\xcf(.{8})\xcf(.{8})", elf, re.S):
        a = struct.unpack(">Q", m.group(1))[0]
        b = struct.unpack(">Q", m.group(2))[0]
        dig = (struct.pack("<Q", b) + struct.pack("<Q", a)).hex()
        st = re.findall(rb"[\xa0-\xbf]\.(\w{2,8})\x83", elf[max(0, m.start() - 24):m.start()])
        out.append((st[-1].decode() if st else "?", dig))
    return out


def _load_dxil_index():
    """container digest -> (file, group, index) from gpu-shaders/dxil_index.csv (idrk.py index)."""
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "gpu-shaders", "dxil_index.csv")
    idx = {}
    if os.path.exists(p):
        per = defaultdict(int)
        with open(p, encoding="utf-8") as f:
            for r in csv.DictReader(f):
                idx[r["container_digest"]] = (r["file"], r["group"], per[r["file"]])
                per[r["file"]] += 1
    return idx


DXIL_INDEX = _load_dxil_index()


def dump_shaders(res, out_dir, top=20):
    """Write the ELF code objects of the PSOs used by the most expensive segments."""
    m, cos = pso_table(res["rdf"])
    os.makedirs(out_dir, exist_ok=True)
    rows = []
    for s in sorted(res["segments"], key=lambda s: -s["dur"])[:top]:
        for pso in s["psos"]:
            ph, name = m.get(pso, ("", ""))
            c = cos.get(ph)
            fn = ""
            if c is not None:
                fn = os.path.join(out_dir, f"pso_{pso}_{ph[:16]}.elf")
                if not os.path.exists(fn):
                    with open(fn, "wb") as f:
                        f.write(res["rdf"].data(c))
            shaders = []
            if c is not None:
                for stage, dig in api_shader_hashes(res["rdf"].data(c)):
                    g = DXIL_INDEX.get(dig)
                    shaders.append(f"{stage}:{dig}" + (f"={g[0]}/{g[1]}#{g[2]}" if g else ""))
            rows.append((s["id"], fmt_us(s["dur"]), pso, ph, name, " ".join(shaders), fn))
    with open(os.path.join(out_dir, "pso_map.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["seg", "seg_dur_us", "api_pso_hash", "pipeline_hash", "api_name",
                    "shaders(stage:dxbc_digest=file/group#index)", "elf"])
        w.writerows(rows)
    return len(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace")
    ap.add_argument("--out")
    ap.add_argument("--no-waves", action="store_true", help="skip wave decoding (faster, equal split per event)")
    ap.add_argument("--ses", help="comma list of SqttData chunk indices to decode (default all)")
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--dump-shaders", action="store_true",
                    help="extract ELF code objects of PSOs used by the top segments (for rga/Ghidra)")
    a = ap.parse_args()
    ses = [int(x) for x in a.ses.split(",")] if a.ses else None
    res = build(a.trace, not a.no_waves, ses)
    od = a.out or os.path.dirname(os.path.abspath(a.trace))
    os.makedirs(od, exist_ok=True)
    base = os.path.join(od, os.path.splitext(os.path.basename(a.trace))[0])
    print(report(res, base, a.top))
    if a.dump_shaders:
        n = dump_shaders(res, base + "_shaders", a.top)
        print(f"shader code objects for top segments -> {base}_shaders (pso_map.csv, {n} rows)")
    print(f"\nwrote {base}.summary.txt / .segments.csv / .events.csv / .passes.csv")


if __name__ == "__main__":
    main()
