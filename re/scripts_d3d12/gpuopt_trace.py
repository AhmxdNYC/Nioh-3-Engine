r"""gpuopt_trace.py -- analyse Nioh3EngineFix.gpuopt-trace.csv ([GpuOpt] TraceRecords = N, see notes/gpu-barriers.md).

    py -I gpuopt_trace.py <Nioh3EngineFix.gpuopt-trace.csv> [symbols.tsv]

The trace has one row per hooked ID3D12GraphicsCommandList call (all lists, all threads) plus one row per barrier
struct (slot 26: action deferred / pass / drop_*) and one EMIT row per batch gpuopt sent to the driver
(a1 = batch size, a2 = vtable slot that triggered it). Barrier rows: a1 = resource, a2 = (before << 32) | after,
a3 = (type << 56) | (flags << 48) | subresource; caller_rva = first return address inside Nioh3.exe.

Reports:
  * barrier calls per engine call site (caller RVA -> nearest symbol)
  * emitted batch-size histogram and which list method triggered the emits
  * undo pairs (A: S1->S2 then A: S2->S1 with no work in between)
  * UAV barriers sandwiched between two dispatches of the same PSO (candidate "independent dispatch" barriers -
    dropping one still needs proof that the dispatches touch disjoint data; this only finds where to look)
  * resources transitioned back and forth across work (split-barrier / state-merge candidates)
"""
import sys, csv, bisect, collections

WORK = {"DrawInstanced", "DrawIndexedInstanced", "Dispatch", "CopyBufferRegion", "CopyTextureRegion", "CopyResource",
        "CopyTiles", "ResolveSubresource", "ClearDepthStencilView", "ClearRenderTargetView",
        "ClearUnorderedAccessViewUint", "ClearUnorderedAccessViewFloat", "DiscardResource", "ExecuteIndirect",
        "ExecuteBundle", "ResolveQueryData", "ResolveSubresourceRegion", "AtomicCopyBufferUINT",
        "AtomicCopyBufferUINT64", "WriteBufferImmediate", "DispatchRays", "DispatchMesh", "BeginRenderPass",
        "EndRenderPass", "Close"}
STATES = {0x1: "VCB", 0x2: "IB", 0x4: "RT", 0x8: "UA", 0x10: "DW", 0x20: "DR", 0x40: "NPSR", 0x80: "PSR",
          0x100: "SO", 0x200: "IA", 0x400: "CD", 0x800: "CS", 0x1000: "RD", 0x2000: "RS", 0x1000000: "SRS"}


def sname(v):
    if v == 0:
        return "COMMON"
    return "|".join(n for b, n in STATES.items() if v & b) or hex(v)


def load_syms(path):
    rows = []
    try:
        with open(path, encoding="utf-8") as fh:
            for ln in fh:
                if ln.startswith("#") or ln.startswith("rva\t"):
                    continue
                p = ln.rstrip("\n").split("\t")
                if len(p) >= 3 and p[1] == "function":
                    try:
                        rows.append((int(p[0], 16), p[2]))
                    except ValueError:
                        pass
    except OSError:
        pass
    rows.sort()
    return rows


def sym(rows, rva):
    if not rows or not rva:
        return ""
    i = bisect.bisect_right([r for r, _ in rows], rva) - 1
    if i < 0:
        return ""
    r, n = rows[i]
    return f"{n}+0x{rva - r:X}" if rva - r < 0x4000 else ""


def main():
    path = sys.argv[1]
    syms = load_syms(sys.argv[2] if len(sys.argv) > 2 else r"C:\Users\Ahmad Hamza\nioh3-engine\re\symbols\symbols.tsv")
    rows = list(csv.DictReader(open(path, encoding="utf-8")))
    print(f"{len(rows)} records, {len({r['tid'] for r in rows})} threads, {len({r['list'] for r in rows})} lists")
    meth = collections.Counter(r["method"] for r in rows if r["slot"] != "26")
    print("calls by method:", ", ".join(f"{k} {v}" for k, v in meth.most_common(14)))
    bars = [r for r in rows if r["slot"] == "26"]
    act = collections.Counter(r["action"] for r in bars)
    print("barrier structs:", dict(act))
    by_site = collections.Counter((int(r["caller_rva"], 16), r["action"]) for r in bars)
    print("\nbarriers by engine call site (caller RVA = return address in Nioh3.exe):")
    for (rva, a), n in sorted(by_site.items(), key=lambda x: -x[1])[:20]:
        print(f"  0x{rva:07X} {sym(syms, rva):40s} {a:9s} {n}")
    emits = [r for r in rows if r["method"] == "EMIT"]
    if emits:
        h = collections.Counter(min(int(r["a1"], 16), 9) for r in emits)
        why = collections.Counter(r["a2"] for r in emits)
        print("\nemitted batches:", len(emits), "size histogram", dict(sorted(h.items())),
              "| triggered by slot", dict(why.most_common(8)))

    # per list, in order: undo pairs, UAV between same-PSO dispatches, back-and-forth transitions
    per_list = collections.defaultdict(list)
    for r in rows:
        per_list[r["list"]].append(r)
    undo = collections.Counter()
    uav_between = collections.Counter()
    pingpong = collections.Counter()
    for lst, rs in per_list.items():
        window = {}             # resource -> (before, after) of barriers since the last work call
        last_state = {}         # resource -> (state it was moved to, index of the work count)
        work_n = 0
        pso = None
        last_dispatch_pso = None
        pending_uav = []
        for r in rs:
            m = r["method"]
            if r["slot"] == "26":
                a1, a2, a3 = int(r["a1"], 16), int(r["a2"], 16), int(r["a3"], 16)
                typ = a3 >> 56
                if typ == 0:
                    b, a = a2 >> 32, a2 & 0xFFFFFFFF
                    if a1 in window and window[a1] == (a, b):
                        undo[(sname(a), sname(b), int(r["caller_rva"], 16))] += 1
                    window[a1] = (b, a)
                    if a1 in last_state and last_state[a1][0] != b:
                        pass
                    prev = last_state.get(a1)
                    if prev and prev[1] < work_n and prev[2] == a:
                        pingpong[(sname(prev[2]), sname(prev[0]), int(r["caller_rva"], 16))] += 1
                    last_state[a1] = (a, work_n, b)
                elif typ == 2 and last_dispatch_pso is not None:
                    pending_uav.append((a1, int(r["caller_rva"], 16)))
            elif m == "SetPipelineState":
                pso = r["a1"]
            elif m in WORK:
                if m == "Dispatch":
                    if pending_uav and last_dispatch_pso == pso:
                        for res, site in pending_uav:
                            uav_between[(pso, site)] += 1
                    last_dispatch_pso = pso
                else:
                    last_dispatch_pso = None
                pending_uav = []
                window = {}
                work_n += 1
    print("\nundo pairs (A:S1->S2, A:S2->S1, no work between):", sum(undo.values()))
    for (s1, s2, site), n in undo.most_common(10):
        print(f"  {s1} -> {s2} -> {s1}  at 0x{site:X} {sym(syms, site)}  x{n}")
    print("\nUAV barriers between two dispatches of the same PSO:", sum(uav_between.values()))
    for (p, site), n in uav_between.most_common(10):
        print(f"  PSO {p}  barrier at 0x{site:X} {sym(syms, site)}  x{n}")
    print("\nresources moved S1->S2 for one piece of work and straight back (split-barrier candidates):",
          sum(pingpong.values()))
    for (a, b, site), n in pingpong.most_common(10):
        print(f"  {a} -> {b} -> {a}  (return at 0x{site:X} {sym(syms, site)})  x{n}")


if __name__ == "__main__":
    main()
