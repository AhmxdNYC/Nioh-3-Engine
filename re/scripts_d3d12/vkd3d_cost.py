r"""vkd3d_cost.py -- replay a gpuopt trace through a model of vkd3d-proton (+ RADV) and count what the Ally pays for.

    py -I vkd3d_cost.py <Nioh3EngineFix.gpuopt-trace.csv> [--frames N] [--syms re\symbols\symbols.tsv]
    py -I vkd3d_cost.py --selftest

Input: the CSV written by [GpuOpt] TraceRecords = N (format: gpuopt_trace.py docstring). Record it on the Ally with
Barriers = 2 (count only, nothing deferred) so every barrier row is one ResourceBarrier call as vkd3d sees it.
With batching on, the EMIT rows are the calls and the 'deferred' rows are their structs; both are handled.

Model (vkd3d-proton 2230755, libs/vkd3d/command.c; notes/vkd3d-audit.md):
  * every ResourceBarrier call = one vkCmdPipelineBarrier2 and ends the dynamic render pass (ResourceBarrier()
    calls d3d12_command_list_end_current_render_pass() unconditionally);
  * StateBefore COMMON -> src stage ALL_COMMANDS + MEMORY_READ|WRITE (vk_access_and_stage_flags_from_d3d12_resource_state)
    = on RADV a full wait-for-idle (CS+VS+PS partial flush) plus CB/DB flush;
  * read -> read transitions are NOT elided (barrier_batch_add_layout_transition keeps them; no layout change with
    GENERIC_READ states because both map to common_layout), RADV still waits for the src stages;
  * OMSetRenderTargets always ends the pass; Dispatch / Copy* / partial clears / ClearUAV / ResolveQueryData end it;
    full-rect ClearRenderTargetView is deferred into the next pass's loadOp;
  * after a shader-path ClearUAV vkd3d adds an implicit barrier before the next draw/dispatch (clear_uav_pending) unless
    a UAV barrier / UA-> transition comes first.
Counts are per frame (frames = RT->PRESENT transitions, or --frames). Nothing here is a timing; the note turns counts
into time with the RGP per-barrier costs.
"""
import sys, csv, bisect, collections

READ = 0x1 | 0x2 | 0x40 | 0x80 | 0x200 | 0x800          # GENERIC_READ bits (VCB, IB, NPSR, PSR, IA, COPY_SOURCE)
R1_MASK = 0x1 | 0x2 | 0x40 | 0x80                        # R1 candidates: no IA (vkd3d DGC hoisting), no COPY_SOURCE (copy tracking)
WRITE = 0x4 | 0x8 | 0x10 | 0x100 | 0x400 | 0x1000        # RT, UA, DEPTH_WRITE, SO, COPY_DEST, RESOLVE_DEST
STATES = {0x1: "VCB", 0x2: "IB", 0x4: "RT", 0x8: "UA", 0x10: "DW", 0x20: "DR", 0x40: "NPSR", 0x80: "PSR",
          0x100: "SO", 0x200: "IA", 0x400: "CD", 0x800: "CS", 0x1000: "RD", 0x2000: "RS", 0x1000000: "SRS"}
DRAWS = {"DrawInstanced", "DrawIndexedInstanced", "DispatchMesh"}
COPIES = {"CopyBufferRegion", "CopyTextureRegion", "CopyResource", "CopyTiles", "ResolveSubresource",
          "ResolveSubresourceRegion", "AtomicCopyBufferUINT", "AtomicCopyBufferUINT64", "WriteBufferImmediate"}
CLEAR_UAV = {"ClearUnorderedAccessViewUint", "ClearUnorderedAccessViewFloat"}
COMPUTE_EI = (0x58BC74, 0x58BD90)  # SynTok81 (compute ExecuteIndirect) call site range; other EIs are draws


def sname(v):
    return "COMMON" if v == 0 else ("|".join(n for b, n in STATES.items() if v & b) or hex(v))


def load_syms(path):
    rows = []
    try:
        for ln in open(path, encoding="utf-8"):
            p = ln.rstrip("\n").split("\t")
            if len(p) >= 3 and p[1] == "function" and p[0].startswith("0x"):
                rows.append((int(p[0], 16), p[2]))
    except OSError:
        pass
    rows.sort()
    return rows


def sym(rows, rva):
    if not rows or not rva:
        return ""
    i = bisect.bisect_right([r for r, _ in rows], rva) - 1
    return f"{rows[i][1]}+{rva - rows[i][0]:X}" if i >= 0 else ""


def classify(before, after, prev):
    """barrier class of one transition (vkd3d + RADV view). prev = last known StateAfter of (res, sub) or None."""
    if before == 0:
        if prev is None:
            return "COMMON->X (prev unknown)"
        if prev & WRITE:
            return "COMMON->X (prev write: narrowable)"
        return "COMMON->X (prev read-only: no dependency)"
    if before & WRITE:
        return "write->X (real dependency)"
    if after == 0:
        return "read->COMMON"
    if after & WRITE:
        return "read->write (WAR)"
    if (before & ~READ) == 0 and (after & ~READ) == 0 and after:
        if (before & after) == before and before != after and (before | after) & ~R1_MASK == 0:
            return "read->read superset (R1)"
        return "read->read other"
    return "other (DR/RS/...)"


class ListState:
    __slots__ = ("rp", "drew", "cause", "uavpend", "omset")

    def __init__(self):
        self.rp = False; self.drew = False; self.cause = "list start"; self.uavpend = False; self.omset = None


def analyse(rows, frames=None, syms=None, out=print):
    lists = collections.defaultdict(ListState)
    c = collections.Counter()           # misc counters
    bcls = collections.Counter()        # barrier struct classes
    breaks = collections.Counter()      # render-pass ends by cause (only ends of an active pass)
    begins = collections.Counter()      # render-pass begins by the cause of the preceding end
    sites = collections.defaultdict(collections.Counter)  # class -> caller rva
    trans = collections.Counter()       # (before, after) of R1 / COMMON rows
    last = {}                           # (res, sub) -> last StateAfter seen anywhere (trace order)
    batching = any(r["method"] == "EMIT" for r in rows)

    def end(st, cause):
        if st.rp:
            breaks[cause] += 1
            st.rp = False
            st.cause = cause + (" (mid-pass)" if st.drew else "")
        elif st.cause == "list start" or not st.drew:
            st.cause = cause

    def work(st, gfx):
        if st.uavpend:
            c["implicit ClearUAV barriers"] += 1
            st.uavpend = False
        if gfx:
            if not st.rp:
                begins[st.cause] += 1
                st.rp = True
            st.drew = True
            st.omset = None
        else:
            end(st, "Dispatch")

    for r in rows:
        m, act, lst = r["method"], r["action"], r["list"]
        st = lists[lst]
        a1, a2, a3 = (int(r[k], 16) for k in ("a1", "a2", "a3"))
        rva = int(r["caller_rva"], 16)
        if m == "ResourceBarrier":
            typ = a3 >> 56
            c["barrier structs"] += 1
            if not batching or act == "pass":
                c["vkd3d ResourceBarrier calls"] += 1
                end(st, "ResourceBarrier")
            if typ == 0:
                before, after, sub = a2 >> 32, a2 & 0xFFFFFFFF, a3 & 0xFFFFFFFF
                if before == 4 and after == 0:
                    c["RT->PRESENT"] += 1
                key = (a1, sub)
                prev = last.get(key, last.get((a1, 0xFFFFFFFF)))
                k = classify(before, after, prev)
                bcls[k] += 1
                sites[k][rva] += 1
                if k.startswith(("read->read superset", "COMMON")):
                    trans[(k[:6], before, after)] += 1
                if sub == 0xFFFFFFFF:
                    for kk in [x for x in last if x[0] == a1]:
                        del last[kk]
                last[key] = after
                if before == 0x8:
                    st.uavpend = False
            elif typ == 2:
                bcls["UAV barrier"] += 1
                sites["UAV barrier"][rva] += 1
                st.uavpend = False
            else:
                bcls["aliasing (full)"] += 1
        elif m == "EMIT":
            c["vkd3d ResourceBarrier calls"] += 1
            end(st, "ResourceBarrier")
        elif m in DRAWS:
            c["draws"] += 1
            work(st, True)
        elif m == "ExecuteIndirect":
            gfx = not (COMPUTE_EI[0] <= rva < COMPUTE_EI[1])
            c["ExecuteIndirect " + ("draw" if gfx else "dispatch")] += 1
            work(st, gfx)
        elif m == "Dispatch":
            c["dispatches"] += 1
            work(st, False)
        elif m == "OMSetRenderTargets":
            c["OMSetRenderTargets"] += 1
            if st.omset == (a1, a2, a3):
                c["OMSetRenderTargets same args, no draw between"] += 1
            st.omset = (a1, a2, a3)
            end(st, "OMSetRenderTargets")
        elif m in COPIES:
            c[m] += 1
            end(st, "Copy/Resolve")
            for kk in [x for x in last if x[0] == a1]:  # copy destination (a1 = pDst for buffer/resource copies)
                del last[kk]
        elif m == "ClearRenderTargetView":
            full = a3 == 0
            c["ClearRTV " + ("full (deferred loadOp)" if full else "partial (own pass)")] += 1
            if not full:
                end(st, "partial clear")
        elif m == "ClearDepthStencilView":
            c["ClearDSV"] += 1
        elif m in CLEAR_UAV:
            c[m] += 1
            end(st, "ClearUAV")
            st.uavpend = True
        elif m == "ResolveQueryData":
            c["ResolveQueryData"] += 1
            end(st, "ResolveQueryData")
        elif m in ("BeginQuery", "EndQuery"):
            c[m] += 1
        elif m in ("Close", "Reset"):
            if m == "Close":
                c["lists closed"] += 1
            lists[lst] = ListState()

    nf = frames or c["RT->PRESENT"] or 1
    src = "--frames" if frames else ("RT->PRESENT transitions" if c["RT->PRESENT"] else "no frame marker: totals")
    out(f"frames: {nf} ({src}); rows {len(rows)}; batching {'on (EMIT rows)' if batching else 'off'}")
    pf = lambda v: f"{v / nf:9.1f}"
    out("\n== per frame")
    for k in sorted(c):
        out(f"{pf(c[k])}  {k}")
    out("\n== barrier structs by class (per frame)  [RADV: COMMON = full idle + CB/DB flush; R1 = src-stage partial flush]")
    for k, v in bcls.most_common():
        out(f"{pf(v)}  {k}")
    out("\n== render-pass ends (active pass) by cause, per frame")
    for k, v in breaks.most_common():
        out(f"{pf(v)}  {k}")
    out(f"{pf(sum(begins.values()))}  render-pass begins; by the cause of the preceding end:")
    for k, v in begins.most_common(10):
        out(f"{pf(v)}    {k}")
    for k in ("read->read superset (R1)", "COMMON->X (prev read-only: no dependency)",
              "COMMON->X (prev write: narrowable)", "COMMON->X (prev unknown)"):
        if sites.get(k):
            out(f"\n== {k}: top call sites (return RVA in Nioh3.exe)")
            for rva, v in sites[k].most_common(8):
                out(f"{pf(v)}  0x{rva:X} {sym(syms, rva)}")
    if trans:
        out("\n== R1 / COMMON transitions by states (per frame, top 12)")
        for (k, b, a), v in trans.most_common(12):
            out(f"{pf(v)}  {k:6s} {sname(b)} -> {sname(a)}")
    return c, bcls, breaks


def selftest():
    H = ["seq", "us", "tid", "list", "set", "slot", "method", "action", "caller_rva", "a1", "a2", "a3"]
    rows, seq = [], 0

    def add(method, action="call", a1=0, a2=0, a3=0, rva=0, lst="L1"):
        nonlocal seq
        rows.append(dict(zip(H, [str(seq), "0", "1", lst, "0", "0", method, action, hex(rva), hex(a1), hex(a2), hex(a3)])))
        seq += 1

    tr = lambda res, b, a, rva=0x115E97, sub=0xFFFFFFFF: add("ResourceBarrier", "pass", res, (b << 32) | a, sub, rva)
    for _ in range(2):                       # two frames
        add("OMSetRenderTargets", a1=1, a2=0x10)
        tr(0xA, 0, 0x80)                     # COMMON -> PSR, prev unknown (1st frame) / prev read-only (2nd)
        add("DrawIndexedInstanced")          # begin pass
        tr(0xB, 0x40, 0xC0)                  # NPSR -> NPSR|PSR: R1, mid-pass split
        add("DrawIndexedInstanced")
        tr(0xC, 0x4, 0x80)                   # RT -> PSR real
        add("DrawIndexedInstanced")
        add("OMSetRenderTargets", a1=1, a2=0x10)
        add("OMSetRenderTargets", a1=1, a2=0x10)   # same args, no draw between
        add("ClearRenderTargetView", a1=1, a2=2, a3=0)
        add("DrawInstanced")
        add("ClearUnorderedAccessViewUint")
        add("Dispatch")                      # implicit barrier
        tr(0xD, 0x80, 0x40)                  # PSR -> NPSR: read->read other
        tr(0xE, 0x4, 0x0, rva=0x3D6400)      # back buffer RT -> PRESENT (frame marker)
        add("Close")
    c, bcls, breaks = analyse(rows, out=lambda *_: None)
    exp = [(c["RT->PRESENT"], 2), (c["draws"], 8), (c["vkd3d ResourceBarrier calls"], 10),
           (bcls["read->read superset (R1)"], 2), (bcls["COMMON->X (prev unknown)"], 1),
           (bcls["COMMON->X (prev read-only: no dependency)"], 1), (bcls["read->read other"], 2),
           (c["OMSetRenderTargets same args, no draw between"], 2), (c["implicit ClearUAV barriers"], 2),
           (breaks["ResourceBarrier"], 4), (breaks["OMSetRenderTargets"], 2), (c["ClearRTV full (deferred loadOp)"], 2)]
    bad = [(i, got, want) for i, (got, want) in enumerate(exp) if got != want]
    print("SELFTEST", "OK" if not bad else f"FAILED {bad}")
    return not bad


def main():
    a = sys.argv[1:]
    if not a or a[0] in ("-h", "--help"):
        print(__doc__)
        return
    if a[0] == "--selftest":
        sys.exit(0 if selftest() else 1)
    frames = int(a[a.index("--frames") + 1]) if "--frames" in a else None
    syms = load_syms(a[a.index("--syms") + 1] if "--syms" in a else
                     __file__.replace("scripts_d3d12", "symbols").rsplit("\\", 1)[0].rsplit("/", 1)[0] + "/symbols.tsv")
    with open(a[0], newline="", encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    analyse(rows, frames, syms)


if __name__ == "__main__":
    main()
