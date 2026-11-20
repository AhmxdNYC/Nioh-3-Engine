"""cpuprof_diff.py - process-wide top functions from patcher/cpuprof.fsx .tsv files, diffed against a baseline (TASK #124164).
cost of a row = pct/100 * busy/100 (cores actually burning CPU at that function), summed over all threads.
usage: py -I cpuprof_diff.py --base A.tsv [B.tsv ...] --new X.tsv [Y.tsv ...] [--top 15]
"""
import argparse, collections, csv, re


WAIT = re.compile(r"Wait|Delay|SleepConditionVariable|RemoveIoCompletion|NtYield|ZwYield|SleepEx|KERNELBASE.dll!Sleep", re.I)
ROOTS = {"KERNEL32.DLL!BaseThreadInitThunk", "ntdll.dll!RtlUserThreadStart", "Nioh3+BDB740", "Nioh3+1A6BB84", "Nioh3+5B6670"}


def load(paths):
    agg = {"self": collections.Counter(), "incl": collections.Counter()}
    threads = collections.defaultdict(set)
    for p in paths:
        rows = list(csv.DictReader(open(p, encoding="utf-8", errors="replace"), delimiter="	"))
        tb = {}   # thread busy (incl rows have no busy column): mean of its self rows
        for r in rows:
            if r.get("view") == "self" and r.get("busy"):
                tb.setdefault(r["tid"], []).append(float(r["busy"]))
        tb = {k: sum(v) / len(v) for k, v in tb.items()}
        for r in rows:
            v = r.get("view")
            if v not in agg:
                continue
            try:
                pct = float(r["pct"]); busy = float(r["busy"]) if r.get("busy") else tb.get(r["tid"], 0.0)
            except (ValueError, TypeError):
                continue
            fn = r["function"].replace("(leaf?)", "")
            if WAIT.search(fn) or fn in ROOTS:
                continue   # blocked time / thread roots: pct*busy of ~100 idle threads is not CPU work
            agg[v][fn] += pct / 100 * busy / 100 / len(paths)
            threads[fn].add(re.sub(r"\d+$", "", r["thread"]) or "?")
    return agg, threads


def flags(txts):
    out = {}
    for t in txts:
        for line in open(t, encoding="utf-8", errors="replace"):
            m = re.match(r"\s+[\d.]+%\s+\d+%\s+(Nioh3\+[0-9A-F]+)\S* \[\d+B ?([^\]]*)\]", line)
            if m:
                f = m.group(2).strip().replace(" ", "+")
                if re.search(r"loop \w+\.\.\w+ \((\d+) B\)", line) and int(re.search(r"\((\d+) B\)", line).group(1)) <= 40:
                    f = (f + "+tightloop").strip("+")
                if f:
                    out.setdefault(m.group(1), f)
    return out


ap = argparse.ArgumentParser()
ap.add_argument("--base", nargs="+", required=True)
ap.add_argument("--new", nargs="+", required=True)
ap.add_argument("--top", type=int, default=15)
a = ap.parse_args()
B, _ = load(a.base)
N, thr = load(a.new)
fl = flags([p[:-4] for p in a.new if p.endswith(".tsv")])
for v in ("self", "incl"):
    print(f"## top {a.top} {v} (cores, process-wide; base -> new){' - upper bound, includes blocked children' if v == 'incl' else ''}")
    for fn, c in N[v].most_common(a.top):
        b = B[v].get(fn, 0.0)
        tag = "NEW" if b < 0.005 else ("down" if c < b * 0.7 else ("up" if c > b * 1.3 else "same"))
        print(f"{fn:42s} {b:6.3f} -> {c:6.3f}  {tag:4s} {fl.get(fn, ''):14s} {','.join(sorted(thr[fn]))[:60]}")
    print()
