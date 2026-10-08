r"""pin_audit.py - which pins could change the driver's FMA fusion?  ACO (RADV, the Ally) fuses v_mul_f32 feeding
v_add_f32/v_sub_f32 into v_fma_f32 for non-precise ops. A pin max(a*b, -inf) between the product and its add blocks
that fusion, so a pinned PRODUCT that feeds an add/sub (directly, or through a select/phi copy) is reported.

    py -I pin_audit.py SRC.hlsl PINS
"""
import re, sys
src = open(sys.argv[1]).read().split("\n")
pins = [p for p in open(sys.argv[2]).read().strip().split(",") if p]
defs, uses = {}, {}
for l in src:
    m = re.match(r"^\s*(v\d+) = (.*);$", l)
    if not m:
        continue
    d, e = m.groups()
    defs.setdefault(d, []).append(e)
    for o in set(re.findall(r"\bv\d+\b", e)):
        uses.setdefault(o, []).append((d, e))
BIN = re.compile(r"^(\S+) ([-+*/]) (\S+)$")


def feeds_add(v, seen=()):
    out = []
    for d, e in uses.get(v, []):
        m = BIN.match(e)
        if m and m.group(2) in "+-":
            out.append(d)
        elif re.match(r"^\S+ \? \S+ : \S+$", e) or re.match(r"^v\d+$", e):      # select / copy (phi)
            if d not in seen:
                out += feeds_add(d, seen + (d,))
    return out


bad = []
for p in pins:
    es = defs.get(p, [])
    if any((BIN.match(e) or [None, None, None])[2] == "*" for e in es if BIN.match(e)):
        f = feeds_add(p)
        if f:
            bad.append((p, es[0], f[:3]))
print("%d pins, %d are products feeding an add/sub" % (len(pins), len(bad)))
for b in bad:
    print("  %s = %s  -> %s" % b)
