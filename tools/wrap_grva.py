r"""wrap_grva.py - one-shot rewrite: wrap every fixed RVA that tools/check_fixed_rvas.py reports for a mod in Grva(...).

    py -I tools/wrap_grva.py Nioh3EngineFix [--skip replayprof_names.h] [--dry]

base + 0xRVA -> base + Grva(0xRVA); base + kName -> base + Grva(kName); base + tbl[i].f -> base + Grva(tbl[i].f);
for (v : tbl) ... base + v.f -> base + Grva(v.f); helpers taking an RVA parameter get base + Grva(rva) in their body.
Adds #include "gamerva.h" to every touched file. Grva is the identity on the known build, so behaviour there is unchanged.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import check_fixed_rvas as C  # noqa: E402


def operand_end(s, i):
    """end of the postfix expression starting at s[i] (identifier / literal, then [..], .x, ->x)"""
    m = re.match(r"\w+", s[i:])
    if not m:
        return i
    j = i + m.end()
    while j < len(s):
        if s[j] == "[":
            d, k = 0, j
            while k < len(s):
                d += {"[": 1, "]": -1}.get(s[k], 0)
                k += 1
                if d == 0:
                    break
            j = k
        elif s.startswith("->", j) and re.match(r"->\w", s[j:]):
            j += 2 + re.match(r"\w+", s[j + 2:]).end()
        elif s[j] == "." and re.match(r"\.[A-Za-z_]", s[j:]):
            j += 1 + re.match(r"\w+", s[j + 1:]).end()
        else:
            break
    return j


def code_part(line):
    c = line.find("//")
    return line if c < 0 else line[:c]


def wrap_line(line, rx_operand):
    """wrap the operand after `base +` wherever rx_operand (a compiled regex ending right before the operand) matches"""
    code = code_part(line)
    out, pos, n = [], 0, 0
    for m in rx_operand.finditer(code):
        st = m.end()
        if code.startswith("Grva(", st):
            continue
        en = operand_end(code, st)
        if en == st:
            continue
        out.append(code[pos:st] + "Grva(" + code[st:en] + ")")
        pos = en
        n += 1
    return "".join(out) + code[pos:] + line[len(code):], n


def main():
    a = sys.argv[1:]
    mod = a[0]
    skip = [a[i + 1] for i, x in enumerate(a) if x == "--skip"]
    uses, helpers = C.scan()
    edits = {}  # file -> {line: [operand regex]}

    base_any = C.BASE

    def want(rel, ln, opnd):
        edits.setdefault(rel, {}).setdefault(ln, set()).add(opnd)

    for u in uses:
        if u["kind"] == "bound" or u.get("wrapped") or u.get("allowed"):
            continue
        where = u.get("used_at", "%s:%d" % (u["file"], u["line"]))
        rel, ln = where.rsplit(":", 1)
        if not rel.startswith("mod/%s/" % mod) or any(s in u["file"] for s in skip):
            continue
        ln = int(ln)
        if u["kind"] in ("literal", "const"):
            want(rel, ln, re.escape(u["expr"].split("+")[-1].strip()))
        elif u["kind"] == "table" and "(for " in u["expr"]:
            var = re.search(r"\(for (\w+) :", u["expr"]).group(1)
            # the loop body: any base + var within 20 lines
            for k in range(ln, ln + 21):
                want(rel, k, re.escape(var))
        elif u["kind"] == "table":
            want(rel, ln, re.escape(u["name"]))
        elif u["kind"] == "helper":
            pass  # handled below (helper bodies)
    # helper bodies: base + <rva param>
    hl = {u["name"] for u in uses if u["kind"] == "helper" and u["file"].startswith("mod/%s/" % mod) and not u.get("wrapped")}
    for f in C.files(False):
        rel = f.relative_to(ROOT).as_posix()
        if not rel.startswith("mod/%s/" % mod):
            continue
        t = C.strip(f.read_text(encoding="utf-8", errors="replace"))
        for m in C.RX_FUNC.finditer(t):
            if m.group(1) not in hl:
                continue
            params = [x.strip() for x in m.group(2).split(",")]
            pm = C.RX_PARAM.search(params[helpers[m.group(1)]]) if helpers.get(m.group(1), 99) < len(params) else None
            if not pm:
                continue
            l0 = t.count("\n", 0, m.end()) + 1
            body = t[m.end():m.end() + 1500]
            hb = base_any + (r"|(?<![\w.>])B(?!\w)" if mod == "Nioh3Training" else "")
            for g in re.finditer("(?:" + hb + r")\s*\+\s*" + pm.group(1) + r"\b", body):
                want(rel, l0 + body.count("\n", 0, g.start()), re.escape(pm.group(1)))
    total = 0
    for rel, lines in sorted(edits.items()):
        f = ROOT / rel
        src = f.read_text(encoding="utf-8")
        nl = "\r\n" if "\r\n" in src else "\n"
        L = src.split(nl)
        t = C.strip(src).split(nl if nl == "\n" else "\n")
        aliases = sorted({m.group(1) for m in C.RX_ALIAS.finditer(C.strip(src))} - {"base"})
        if mod == "Nioh3Training":
            aliases = sorted(set(aliases) | {"B"})  # Training's image base variable
        base = "(?:%s)" % "|".join([base_any] + [r"(?<![\w.>])%s(?!\w)" % re.escape(x) for x in aliases])
        n_file = 0
        for ln, ops in lines.items():
            if ln - 1 >= len(L):
                continue
            rx = re.compile(base + r"\s*\+\s*(?=(?:%s)(?!\w))" % "|".join(sorted(ops)))
            L[ln - 1], n = wrap_line(L[ln - 1], rx)
            n_file += n
        if n_file and not re.search(r'#include\s+"gamerva\.h"', src):
            idx = next((i for i, l in enumerate(L) if l.startswith("#include")), 0)
            L.insert(idx + 1, '#include "gamerva.h"')
        total += n_file
        print("%-45s %3d wrapped" % (rel, n_file))
        if "--dry" not in a and n_file:
            f.write_text(nl.join(L), encoding="utf-8", newline="")
    print("total %d" % total)


if __name__ == "__main__":
    main()
