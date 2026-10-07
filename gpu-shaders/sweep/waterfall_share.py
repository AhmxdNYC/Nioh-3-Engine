r"""waterfall_share.py - how much of a captured frame's wave time is spent in pixel/compute shaders whose ISA (AMD PAL
compiler, gfx1201) contains descriptor waterfall loops (v_readfirstlane + exec loop) that wait for their own fetch.

    py -I waterfall_share.py <events.csv with approx_wave_us> <isa dir> [<isa dir> ...] [--names pso_map.csv]
"""
import collections, csv, os, re, sys
args = sys.argv[1:]
names = {}
if "--names" in args:
    i = args.index("--names"); nm = args[i + 1]; del args[i:i + 2]
    for r in csv.DictReader(open(nm, encoding="utf-8")):
        s = r.get("shaders(stage:dxbc_digest=file/group#index)", "")
        best = None
        for t in s.split():
            st, _, rest = t.partition(":")
            if st in ("pixel", "compute") or best is None:
                best = st[:2] + " " + (rest.split("=")[1] if "=" in rest else "exe:" + rest[:8])
        names[r["api_pso_hash"]] = best
ev, dirs = args[0], args[1:]
FETCH = re.compile(r"^\s*(image_sample|image_load|image_gather|buffer_load|tbuffer_load)")
WAIT0 = re.compile(r"s_wait_(loadcnt|samplecnt)\s+0x0\b|vmcnt\(0\)")
LABEL = re.compile(r"^\s*(_L\d+|label_\w+):")
BR = re.compile(r"s_(cbranch_\w+|branch)\s+(\w+)")


def wf_loops(pso):
    for d in dirs:
        p = os.path.join(d, pso + ".txt")
        if os.path.exists(p):
            L = open(p, encoding="utf-8", errors="replace").read().splitlines()
            lab = {}
            for i, l in enumerate(L):
                m = LABEL.match(l)
                if m:
                    lab[m.group(1)] = i
            n = 0
            for i, l in enumerate(L):
                m = BR.search(l)
                if m and m.group(2) in lab and lab[m.group(2)] < i:
                    body = L[lab[m.group(2)]:i + 1]
                    if any("readfirstlane" in x for x in body) and any(FETCH.search(x) for x in body) and any(WAIT0.search(x) for x in body):
                        n += 1
            return n
    return None


W = collections.defaultdict(float)
for r in csv.DictReader(open(ev)):
    if r["pso"] != "ffffffffffffffff":
        W[r["pso"]] += float(r["approx_wave_us"] or 0)
tot = sum(W.values())
known = wf = 0.0
rows = []
for p, w in W.items():
    n = wf_loops(p)
    if n is None:
        continue
    known += w
    if n:
        wf += w
        rows.append((w, n, p))
print("frame wave-time %.0f wave-us; with ISA %.0f (%.0f%%); in shaders with waiting waterfall loops %.0f (%.1f%% of the frame)"
      % (tot, known, 100 * known / tot, wf, 100 * wf / tot))
for w, n, p in sorted(rows, reverse=True)[:25]:
    print("  %8.0f %5.1f%%  %3d waiting waterfall loops  %s %s" % (w, 100 * w / tot, n, p[:8], names.get(p, "")[:60]))
