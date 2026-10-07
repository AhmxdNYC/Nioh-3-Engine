r"""rank_passes.py - rank every PSO in an RGP events.csv by GPU time and attach the shader file/group#index
plus gfx1201 ISA statistics (instructions, fetches, full memory waits, max VGPR, waterfall vs data loops).

    py -I rank_passes.py <events.csv> <shaders_dir_with_pso_map_and_isa> [top]

Columns: est_us (RGP est, per PSO summed), ev (events), stage name, file/group#idx, inst, fetch, wait0
(full s_wait_*cnt 0), fw = wait0/fetch, vgpr (highest v register referenced + 1), dloops (backward branches whose
body has a fetch and a full wait and is NOT a descriptor waterfall [readfirstlane]), wfl (waterfall loops)."""
import csv, os, re, sys, collections
ev, d = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 80
FETCH = re.compile(r"^\s*(image_load|image_sample|image_gather|buffer_load|global_load|tbuffer_load)")
WAIT0 = re.compile(r"s_wait_(loadcnt|samplecnt)\s+0x0\b")
LABEL = re.compile(r"^\s*(_L\d+|label_\w+|\w+):")
BR = re.compile(r"s_(cbranch_\w+|branch)\s+(\w+)")
VREG = re.compile(r"\bv\[?(\d+)(?::(\d+))?\]?")

def isa(pso):
    p = os.path.join(d, "isa", pso + ".txt")
    if not os.path.exists(p):
        return None
    L = open(p, encoding="utf-8", errors="replace").read().splitlines()
    out = collections.OrderedDict()
    # split per stage
    stages = {}; cur = None
    for l in L:
        m = re.match(r"^(_amdgpu_\w+?_main)\w*:", l)
        if m: cur = m.group(1); stages.setdefault(cur, [])
        elif cur: stages[cur].append(l)
    res = {}
    for st, lines in stages.items():
        ins = [l for l in lines if l.startswith("\t") and not l.strip().startswith("//")]
        f = sum(1 for x in ins if FETCH.search(x)); w = sum(1 for x in ins if WAIT0.search(x))
        mv = 0
        for x in ins:
            code = x.split("//")[0]
            for a, b in VREG.findall(code):
                mv = max(mv, int(b or a))
        labels = {}
        for i, l in enumerate(lines):
            m = LABEL.match(l)
            if m: labels[m.group(1)] = i
        dl = wf = 0
        for i, l in enumerate(lines):
            m = BR.search(l)
            if m and m.group(2) in labels and labels[m.group(2)] < i:
                body = lines[labels[m.group(2)]:i + 1]
                bf = sum(1 for x in body if FETCH.search(x)); bw = sum(1 for x in body if WAIT0.search(x))
                if any("readfirstlane" in x for x in body) and len(body) < 90:
                    wf += 1
                elif bf:
                    dl += 1
        res[st] = (len(ins), f, w, mv + 1, dl, wf)
    return res

names = {}
for r in csv.DictReader(open(os.path.join(d, "pso_map.csv"), newline="", encoding="utf-8")):
    names[r["api_pso_hash"]] = r["shaders(stage:dxbc_digest=file/group#index)"]
tot = collections.defaultdict(float); cnt = collections.Counter(); qs = collections.defaultdict(set)
for r in csv.DictReader(open(ev, newline="")):
    if r["pso"] == "ffffffffffffffff": continue
    tot[r["pso"]] += float(r["est_us"] or 0); cnt[r["pso"]] += 1; qs[r["pso"]].add(r["queue"])
print("%7s %4s %-5s %-62s %6s %5s %5s %5s %4s %3s %3s" % ("us", "ev", "q", "shader", "inst", "fetch", "wait0", "fw", "vgpr", "dl", "wf"))
for pso, us in sorted(tot.items(), key=lambda kv: -kv[1])[:top]:
    s = names.get(pso, "?")
    toks = s.split()
    main = None
    for t in toks:
        st, _, rest = t.partition(":")
        if st in ("pixel", "compute"): main = (st, rest)
    if main is None and toks: st, _, rest = toks[0].partition(":"); main = (st, rest)
    st, rest = main if main else ("?", "")
    nm = rest.split("=", 1)[1] if "=" in rest else "exe:" + rest[:16]
    I = isa(pso) or {}
    key = "_amdgpu_ps_main" if st == "pixel" else "_amdgpu_cs_main"
    if key not in I and I: key = list(I)[-1]
    n, f, w, v, dl, wf = I.get(key, (0, 0, 0, 0, 0, 0))
    print("%7.1f %4d %-5s %-62s %6d %5d %5d %5.2f %4d %3d %3d" % (us, cnt[pso], "/".join(sorted(qs[pso]))[:5], (st[:2] + " " + nm)[:62], n, f, w, (w / f if f else 0), v, dl, wf))
