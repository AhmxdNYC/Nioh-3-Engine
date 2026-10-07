"""isa_loops.py - find latency-bound loops (GHDM pattern) in rga ISA dumps.
For every backward branch (a loop), count memory fetches and full waits (s_wait_*cnt 0 / s_waitcnt vmcnt(0)) inside
the loop body. A loop where (almost) every fetch is followed by a full wait is serialised on memory latency.
    py isa_loops.py <shaders_dir> [pso ...]
"""
import os, re, sys

d = sys.argv[1]
want = set(sys.argv[2:])
isa_dir = os.path.join(d, "isa")
FETCH = re.compile(r"\b(image_load|image_sample|buffer_load|global_load|s_buffer_load|tbuffer_load|image_gather)")
WAIT0 = re.compile(r"s_wait_(loadcnt|samplecnt|vmcnt)\s+0x0+\b|s_wait_(loadcnt|samplecnt)\s+0\b|s_waitcnt\s+vmcnt\(0\)")
LABEL = re.compile(r"^\s*(_L\d+|label_\w+|\w+):")
BR = re.compile(r"s_(cbranch_\w+|branch)\s+(\w+)")
for fn in sorted(os.listdir(isa_dir)):
    pso = fn.split(".")[0]
    if want and not any(pso.startswith(w) or w in fn for w in want):
        continue
    lines = open(os.path.join(isa_dir, fn), encoding="utf-8", errors="replace").read().splitlines()
    labels = {}
    for i, l in enumerate(lines):
        m = LABEL.match(l)
        if m:
            labels[m.group(1)] = i
    loops = []
    for i, l in enumerate(lines):
        m = BR.search(l)
        if m and m.group(2) in labels and labels[m.group(2)] < i:
            body = lines[labels[m.group(2)]:i + 1]
            f = sum(1 for x in body if FETCH.search(x))
            w = sum(1 for x in body if WAIT0.search(x))
            loops.append((len(body), f, w, labels[m.group(2)]))
    total_f = sum(1 for x in lines if FETCH.search(x))
    flag = [lp for lp in loops if lp[1] > 0 and lp[2] >= lp[1]]
    print("%-24s inst %5d fetches %4d loops %3d  serial-fetch loops %d %s" % (
        fn[:24], len(lines), total_f, len(loops), len(flag),
        " ".join("[%dl f%d w%d]" % (n, f, w) for n, f, w, _ in flag[:4])))
