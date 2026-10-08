r"""ophist.py A.ll B.ll - DXIL opcode histogram difference (dynamic-free, static count)."""
import re, sys, collections
def h(p):
    c = collections.Counter()
    for l in open(p, encoding="utf-8-sig"):
        m = re.match(r"^\s+%\S+ = (?:call \S+ @dx\.op\.([A-Za-z0-9]+)\.\S+\(i32 (\d+)|(\w+) )", l)
        if m:
            c[(m.group(1) + ":" + m.group(2)) if m.group(1) else m.group(3)] += 1
    return c
a, b = h(sys.argv[1]), h(sys.argv[2])
for k in sorted(set(a) | set(b), key=lambda k: -abs(a[k] - b[k])):
    if a[k] != b[k]:
        print("%-28s %5d %5d %+d" % (k, a[k], b[k], b[k] - a[k]))
print("total", sum(a.values()), sum(b.values()))
