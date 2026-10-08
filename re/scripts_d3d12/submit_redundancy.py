r"""submit_redundancy.py -- per-frame D3D12 command-list call volume and EXACT redundancy (notes\pc-submit-overhead.md).

    py -I submit_redundancy.py <Nioh3EngineFix.gpuopt-trace.csv> [--frames N] [--us-per-call 0.3]
    py -I submit_redundancy.py --selftest

Input: the gpuopt trace ([GpuOpt] Barriers = 2, TraceRecords = 2000000, TraceDelaySec = 60). Every hooked list call is
one 'call' row (a1..a3 = rdx, r8, r9); barrier structs are slot-26 rows. Frames = back-buffer RT -> PRESENT barriers.

Per list (state is per command list; reset on Reset / ClearState / Close / ExecuteBundle) it counts calls that are
no-ops by the D3D12 spec and could be skipped by an ASI filter (SF1..SF4 in the note):
  PSO       SetPipelineState(p) with p already bound
  RootSig   Set*RootSignature(r) with r already bound (ResourceBinding.md: a redundant set keeps the bindings)
  Table     Set*RootDescriptorTable(i, h) with h already bound at i (since the last root-sig CHANGE / heap set)
  RootVA    Set*Root{Constant,Shader,Unordered}BufferView(i, va) with va already bound at i
  Topology  IASetPrimitiveTopology(t) with t already set
Content-addressed calls (OMSetRenderTargets, RSSet*, IASet*Buffer*, Root32BitConstants) pass pointers; the trace only
has the pointer, so they are counted, and 'same pointer args, no barrier/draw between' is printed as a weak hint until
the trace carries a content hash (note, section 6).
"""
import sys, csv, collections

DRAWS = {"DrawInstanced", "DrawIndexedInstanced", "Dispatch", "ExecuteIndirect", "DispatchMesh"}
RESET = {"Reset", "ClearState", "Close", "ExecuteBundle"}
ROOTVA = {"SetGraphicsRootConstantBufferView", "SetGraphicsRootShaderResourceView", "SetGraphicsRootUnorderedAccessView",
          "SetComputeRootConstantBufferView", "SetComputeRootShaderResourceView", "SetComputeRootUnorderedAccessView"}
WEAK = {"OMSetRenderTargets", "RSSetViewports", "RSSetScissorRects", "IASetVertexBuffers", "IASetIndexBuffer"}


class L:
    def __init__(self):
        self.clear()

    def clear(self):
        self.pso = self.topo = None
        self.rs = {"G": None, "C": None}
        self.tab = {"G": {}, "C": {}}
        self.va = {"G": {}, "C": {}}
        self.weak = {}           # method -> (a1, a2, a3) since the last barrier / draw
        self.psoSinceDraw = False


def h(x):
    return int(x, 16) if x.startswith("0x") or x.startswith("0X") else int(x)


def analyse(rows, frames=None, us=0.3, out=print):
    lists = collections.defaultdict(L)
    calls, red, weak = collections.Counter(), collections.Counter(), collections.Counter()
    dead_pso = 0
    nf = 0
    for r in rows:
        m, act = r["method"], r["action"]
        st = lists[r["list"]]
        if m == "ResourceBarrier":
            if h(r["a2"]) == (4 << 32):      # RT -> PRESENT
                nf += 1
            if act in ("deferred", "pass"):
                calls["ResourceBarrier (structs)"] += 1
            st.weak.clear()
            continue
        if act != "call" or m == "EMIT":
            continue
        calls[m] += 1
        a1, a2, a3 = h(r["a1"]), h(r["a2"]), h(r["a3"])
        if m in RESET:
            st.clear()
        elif m == "SetPipelineState":
            if a1 == st.pso:
                red["PSO"] += 1
            elif st.psoSinceDraw:
                dead_pso += 1
            st.pso, st.psoSinceDraw = a1, True
        elif m in ("SetPipelineState1", "SetProgram"):
            st.pso = None
        elif m in ("SetGraphicsRootSignature", "SetComputeRootSignature"):
            k = "G" if "Graphics" in m else "C"
            if a1 == st.rs[k]:
                red["RootSig"] += 1
            else:
                st.rs[k] = a1
                st.tab[k].clear()
                st.va[k].clear()
        elif m in ("SetGraphicsRootDescriptorTable", "SetComputeRootDescriptorTable"):
            k = "G" if "Graphics" in m else "C"
            if st.tab[k].get(a1) == a2:
                red["Table"] += 1
            st.tab[k][a1] = a2
        elif m in ROOTVA:
            k = "G" if "Graphics" in m else "C"
            if st.va[k].get((m, a1)) == a2:
                red["RootVA"] += 1
            st.va[k][(m, a1)] = a2
        elif m == "SetDescriptorHeaps":   # array content unknown: tables bound before are no longer comparable
            st.tab["G"].clear()
            st.tab["C"].clear()
        elif m == "IASetPrimitiveTopology":
            if a1 == st.topo:
                red["Topology"] += 1
            st.topo = a1
        elif m in WEAK:
            if st.weak.get(m) == (a1, a2, a3):
                weak[m] += 1
            st.weak[m] = (a1, a2, a3)
        if m in DRAWS:
            st.psoSinceDraw = False
            st.weak.clear()
    nf = frames or nf or 1
    tot = sum(calls.values())
    out(f"frames {nf}; hooked list calls {tot / nf:.0f}/frame")
    out("\n== calls per frame by method (top 24)")
    for k, v in calls.most_common(24):
        out(f"  {v / nf:9.1f}  {k}")
    out("\n== exact redundant calls per frame (skippable by the SF filters)")
    rs = sum(red.values())
    for k in ("PSO", "RootSig", "Table", "RootVA", "Topology"):
        base = {"PSO": "SetPipelineState", "Topology": "IASetPrimitiveTopology"}.get(k)
        share = f" ({100 * red[k] / max(1, calls[base]):.0f} % of {base})" if base else ""
        out(f"  {red[k] / nf:9.1f}  {k}{share}")
    out(f"  {rs / nf:9.1f}  total  -> ~{rs / nf * us / 1000:.3f} ms/frame dispatcher time at {us} us/call")
    out(f"  {dead_pso / nf:9.1f}  (info) SetPipelineState overwritten before any draw/dispatch (not filtered: needs lookahead)")
    out("\n== weak hint: same POINTER args, no barrier/draw between (content not in the trace)")
    for k, v in weak.most_common():
        out(f"  {v / nf:9.1f}  {k}")
    return calls, red, weak, nf


def selftest():
    rows, seq = [], [0]

    def add(m, a1=0, a2=0, a3=0, lst="L1", act="call"):
        rows.append(dict(seq=seq[0], method=m, action=act, list=lst, a1=hex(a1), a2=hex(a2), a3=hex(a3)))
        seq[0] += 1
    for _ in range(2):
        add("Reset")
        add("SetGraphicsRootSignature", 0xA)
        add("SetGraphicsRootSignature", 0xA)            # RootSig
        add("SetPipelineState", 0x10)
        add("SetGraphicsRootDescriptorTable", 1, 0x100)
        add("DrawIndexedInstanced", 3)
        add("SetPipelineState", 0x10)                    # PSO
        add("SetGraphicsRootDescriptorTable", 1, 0x100)  # Table
        add("SetGraphicsRootDescriptorTable", 2, 0x200)
        add("IASetPrimitiveTopology", 4)
        add("IASetPrimitiveTopology", 4)                 # Topology
        add("DrawIndexedInstanced", 3)
        add("SetGraphicsRootSignature", 0xB)             # change: tables stale
        add("SetGraphicsRootDescriptorTable", 1, 0x100)  # not redundant
        add("OMSetRenderTargets", 1, 0x5000, 0)
        add("OMSetRenderTargets", 1, 0x5000, 0)          # weak
        add("SetPipelineState", 0x11)
        add("SetPipelineState", 0x12)                    # dead 0x11
        add("Dispatch", 1)
        add("ResourceBarrier", 0x77, 4 << 32, 0, act="pass")  # frame marker
    calls, red, weak, nf = analyse(rows, out=lambda *a: None)
    exp = {"PSO": 2, "RootSig": 2, "Table": 2, "Topology": 2}
    ok = nf == 2 and all(red[k] == v for k, v in exp.items()) and weak["OMSetRenderTargets"] == 2 and red["RootVA"] == 0
    print("selftest", "OK" if ok else f"FAIL red={dict(red)} weak={dict(weak)} nf={nf}")
    return ok


if __name__ == "__main__":
    a = sys.argv[1:]
    if not a or a[0] in ("-h", "--help"):
        print(__doc__)
        sys.exit(0)
    if a[0] == "--selftest":
        sys.exit(0 if selftest() else 1)
    frames = int(a[a.index("--frames") + 1]) if "--frames" in a else None
    us = float(a[a.index("--us-per-call") + 1]) if "--us-per-call" in a else 0.3
    with open(a[0], newline="") as fh:
        analyse(list(csv.DictReader(fh)), frames, us)
