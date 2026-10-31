r"""check_fixed_rvas.py - list every hard-coded game address (image base + fixed RVA) in the mods that is not found by pattern.

    py -I tools/check_fixed_rvas.py              # report, exit 1 if any use is not in tools/fixed_rvas_allow.txt
    py -I tools/check_fixed_rvas.py --json F     # also write the uses as JSON (input of tools/aob_audit.py)
    py -I tools/check_fixed_rvas.py --tests      # include mod/*/tests

A game patch moves code and data, so every `base + 0xRVA` (or `base + kConst` with a hex constant) points at the wrong bytes
after an update. Addresses resolved through Site("name") (BossRematch sites.json -> sites_gen.h) are found by byte pattern at
startup and are not reported. Kinds:
  literal   base + 0x1234567
  const     base + kName              (kName is a hex constant defined somewhere under mod/)
  table     base + name[i] / for (v : name) ... base + v: the RVAs sit in an initializer table (each hex entry is listed)
  helper    Helper(0xRVA): a function whose RVA parameter it adds to the base (BytesAt, CallGameBool, Ptr, ...)
  local aliases (uintptr_t b = g_base;) count as the base too
  bound     base + 0x... used only as a range limit (< / > compare, round value) - reported, never counted
Grva(...) (EngineFix src/gamerva.h) marks a use as pattern-resolved: the wrapper is looked through, and the RVA must be in
the mod's rvas.json (made by mod/<mod>/tools/gen_rvas.py), else it is reported UNLST.
The allowlist (tools/fixed_rvas_allow.txt) holds `path:0xRVA` lines (line-number free, so edits do not break it) or `path:*`.
Warn-only from the build scripts until the audit (notes/aob-audit.md) is worked off.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
ALLOW = ROOT / "tools" / "fixed_rvas_allow.txt"
SIZE_OF_IMAGE = 0x52FB000  # Nioh3.exe 6AA7C106 (mod/Nioh3BossRematch/sites.json "build")
MIN_RVA = 0x1001  # 0x1000 = start of .text (a range bound, same on every build)

BASE = r"(?<![\w.>])(?:[A-Za-z_]\w*(?:\.|->))?(?:g_|s_|m_)?(?:img\.|g_img\.)?(?:mod|image|img|exe|game)?_?[Bb]ase(?!\w)"
RX_LIT = re.compile(BASE + r"\s*\+\s*(0x[0-9A-Fa-f]+)\b")
RX_CONST = re.compile(BASE + r"\s*\+\s*(k[A-Z]\w*)\b(?!\s*\[)")
RX_TABLE = re.compile(BASE + r"\s*\+\s*([A-Za-z_]\w*)\s*\[")
RX_DEF = re.compile(r"\b(k[A-Z]\w*)\s*(?:=|\{)\s*(0x[0-9A-Fa-f]+)\b")
RX_DEFINE = re.compile(r"#\s*define\s+(k[A-Z]\w*)\s+\(?(0x[0-9A-Fa-f]+)")
RX_ARR = re.compile(r"\b([A-Za-z_]\w*)\s*\[[^\]]*\]\s*=\s*\{", re.S)
RX_FOR = re.compile(r"\bfor\s*\([^:;()]*?\b(\w+)\s*:\s*([A-Za-z_]\w*)\s*\)")
RX_ALIAS = re.compile(r"\b(?:auto|uintptr_t|uint64_t|uint8_t\s*\*|const\s+uintptr_t|const\s+uint64_t|const\s+auto)\s+(\w+)\s*=\s*(?:\([^()]*\)\s*)?" + BASE + r"\s*;")
RX_FUNC = re.compile(r"^[ \t]*(?:static\s+|inline\s+)*[\w:<>*& ]+?\b([A-Z]\w*)\s*\(([^()]*)\)\s*(?:const\s*)?\{", re.M)
RX_PARAM = re.compile(r"(?:uint32_t|uint64_t|uintptr_t|size_t|DWORD)\s+(rva|tgt|target|site|at|addr|off)\s*$")
RX_HEX = re.compile(r"\b0x([0-9A-Fa-f]{4,8})\b")


def strip(src):
    """drop comments and string literals, keep newlines (line numbers stay valid)"""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * src.count("\n", i, j))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and src[j] != c and src[j] != "\n":
                j += 2 if src[j] == "\\" else 1
            out.append(c + c)
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def files(tests):
    pats = ["mod/*/src/**/*.cpp", "mod/*/src/**/*.h", "mod/Nioh3Training/src/**/*.c", "mod/*.h", "mod/shared/**/*.h", "mod/shared/**/*.cpp"]
    if tests:
        pats += ["mod/*/tests/**/*.cpp", "mod/*/tests/**/*.h"]
    seen = set()
    for p in pats:
        for f in sorted(ROOT.glob(p)):
            if f not in seen and f.name != "sites_gen.h":
                seen.add(f)
                yield f


def table_entries(text, name, lo=MIN_RVA):
    """hex RVAs inside the initializer of array name[] = { ... } (first definition in text)"""
    for m in RX_ARR.finditer(text):
        if m.group(1) != name:
            continue
        depth, j = 0, m.end() - 1
        while j < len(text):
            depth += {"{": 1, "}": -1}.get(text[j], 0)
            if depth == 0:
                break
            j += 1
        body = text[m.end():j]
        line0 = text.count("\n", 0, m.end()) + 1
        return [(line0 + body.count("\n", 0, h.start()), int(h.group(1), 16)) for h in RX_HEX.finditer(body)
                if lo <= int(h.group(1), 16) < SIZE_OF_IMAGE]
    return []


def call_args(text, pos):
    """top-level comma-separated arguments of the call whose '(' is at text[pos]"""
    depth, cur, out, j = 0, [], [], pos
    while j < len(text):
        c = text[j]
        if c in "([{":
            depth += 1
            if depth == 1:
                j += 1
                continue
        elif c in ")]}":
            depth -= 1
            if depth == 0:
                out.append("".join(cur).strip())
                return out
        elif c == "," and depth == 1:
            out.append("".join(cur).strip())
            cur = []
            j += 1
            continue
        cur.append(c)
        j += 1
    return out


def unwrap(text):
    """drop every Grva( ... ) wrapper (keep its argument) so the raw forms below still match; returns (text, lines with Grva)"""
    marked = {text.count("\n", 0, m.start()) + 1 for m in re.finditer(r"\bGrva\s*\(", text)}
    while True:
        m = re.search(r"\bGrva\s*\(", text)
        if not m:
            return text, marked
        depth, j = 1, m.end()
        while j < len(text) and depth:
            depth += {"(": 1, ")": -1}.get(text[j], 0)
            j += 1
        text = text[:m.start()] + text[m.end():j - 1] + text[j:]


def mod_table(rel):
    """RVAs of the mod's rvas.json (pattern table behind Grva), None if the mod has none"""
    f = ROOT / "mod" / rel.split("/")[1] / "rvas.json"
    if not f.exists():
        return None
    return {int(e["rva"], 16) for e in json.loads(f.read_text())["rvas"]}


def scan(tests=False):
    srcs, marks = {}, {}
    for f in files(tests):
        srcs[f], marks[f] = unwrap(strip(f.read_text(encoding="utf-8", errors="replace")))
    consts = {}
    for f, t in srcs.items():
        for rx in (RX_DEF, RX_DEFINE):
            for m in rx.finditer(t):
                consts.setdefault(m.group(1), {})[f] = int(m.group(2), 16)
    # helpers taking an RVA parameter that they add to the image base: Name -> argument index
    helpers, wrapped_helpers = {}, set()
    for f, t in srcs.items():
        for m in RX_FUNC.finditer(t):
            params = [x.strip() for x in m.group(2).split(",")]
            body = t[m.end():m.end() + 1500]
            bl = t.count("\n", 0, m.end()) + 1
            for i, prm in enumerate(params):
                pm = RX_PARAM.search(prm)
                hb = BASE + r"|(?<![\w.>])B(?!\w)" if "/Nioh3Training/" in str(f).replace("\\", "/") else BASE
                g = re.search("(?:" + hb + r")\s*\+\s*" + pm.group(1) + r"\b", body) if pm else None
                if g:
                    helpers[m.group(1)] = i
                    if bl + body.count("\n", 0, g.start()) in marks[f]:
                        wrapped_helpers.add(m.group(1))
    uses = []

    def add(rel, ln, kind, v, expr, **kw):
        if MIN_RVA <= v < SIZE_OF_IMAGE:
            uses.append(dict(file=rel, line=ln, kind=kind, rva=v, expr=expr, **kw))

    def table(f, name, rel, ln, expr, lo):
        for tf in [f] + [g for g in srcs if g != f]:
            ent = table_entries(srcs[tf], name, lo if not name.startswith("k") else MIN_RVA)
            if ent:
                trel = tf.relative_to(ROOT).as_posix()
                for eln, v in ent:
                    add(trel, eln, "table", v, expr, name=name, used_at="%s:%d" % (rel, ln))
                return

    for f, t in srcs.items():
        rel = f.relative_to(ROOT).as_posix()
        aliases = sorted({m.group(1) for m in RX_ALIAS.finditer(t)} - {"base"})
        if rel.startswith("mod/Nioh3Training/"):
            aliases = sorted(set(aliases) | {"B"})  # Training's image base is `static uintptr_t B` (training.c)
        base = "(?:%s)" % "|".join([BASE] + [r"(?<![\w.>])%s(?!\w)" % re.escape(x) for x in aliases])
        rx_lit = re.compile(base + r"\s*\+\s*(0x[0-9A-Fa-f]+|\d{5,9}(?![\w.]))\b" if rel.startswith("mod/Nioh3Training/") else base + r"\s*\+\s*(0x[0-9A-Fa-f]+)\b")
        rx_const = re.compile(base + r"\s*\+\s*(k[A-Z]\w*)\b(?!\s*\[)")
        rx_table = re.compile(base + r"\s*\+\s*([A-Za-z_]\w*)\s*\[")
        lines = t.split("\n")
        for ln, line in enumerate(lines, 1):
            for m in rx_lit.finditer(line):
                v = int(m.group(1), 0)
                if v < MIN_RVA:
                    continue
                before = line[:m.start()].rstrip()
                kind = "literal"
                if v >= SIZE_OF_IMAGE or ((v & 0xFFFF) == 0 and re.search(r"(<=?|>=?)\s*\(?\s*$", before)):
                    kind = "bound"
                uses.append({"file": rel, "line": ln, "kind": kind, "rva": v, "expr": m.group(0)})
            if rel.startswith("mod/Nioh3Training/"):
                for m in re.finditer(base + r"\s*\+\s*\(\s*[^?()]+\?\s*(0x[0-9A-Fa-f]+)\s*:\s*(0x[0-9A-Fa-f]+)\s*\)", line):
                    for g in (m.group(1), m.group(2)):
                        uses.append({"file": rel, "line": ln, "kind": "literal", "rva": int(g, 16), "expr": "B + (? %s : ...)" % g})
                for m in re.finditer(r"(?<![\w.>])(\w+)\s*-\s*B\)?\s*(?:==|!=|<=?|>=?)\s*(0x[0-9A-Fa-f]{5,8})\b", line):
                    v = int(m.group(2), 16)
                    if MIN_RVA <= v < SIZE_OF_IMAGE:
                        uses.append({"file": rel, "line": ln, "kind": "literal", "rva": v, "expr": m.group(0)})
            for m in rx_const.finditer(line):
                d = consts.get(m.group(1))
                if d:
                    add(rel, ln, "const", d.get(f, next(iter(d.values()))), m.group(0), name=m.group(1))
            for m in rx_table.finditer(line):
                table(f, m.group(1), rel, ln, m.group(0), 0x10000)
        # range-for over a table whose element (or element.field) is added to the base within 20 lines
        for m in RX_FOR.finditer(t):
            var, arr = m.group(1), m.group(2)
            ln = t.count("\n", 0, m.start()) + 1
            body = "\n".join(lines[ln - 1:ln + 20])
            if re.search(base + r"\s*\+\s*" + re.escape(var) + r"\b", body):
                table(f, arr, rel, ln, "%s + %s (for %s : %s)" % ("base", var, var, arr), 0x10000)
        # calls to RVA helpers with a literal RVA argument
        if helpers:
            for m in re.finditer(r"\b(%s)\s*\(" % "|".join(map(re.escape, helpers)), t):
                if re.match(r"[\w:<>*& ]+\s$", t[max(0, m.start() - 40):m.start()].split("\n")[-1] or "x") and \
                        re.search(r"(bool|void|int|uint\w*|uintptr_t)\s+$", t[max(0, m.start() - 40):m.start()]):
                    continue  # the definition itself
                args = call_args(t, m.end() - 1)
                i = helpers[m.group(1)]
                if i < len(args):
                    h = re.fullmatch(r"\(?\s*(?:\(\s*\w+\s*\))?\s*0x([0-9A-Fa-f]+)\s*\)?", args[i])
                    if h:
                        add(rel, t.count("\n", 0, m.start()) + 1, "helper", int(h.group(1), 16),
                            "%s(%s)" % (m.group(1), args[i]), name=m.group(1))
                    elif re.fullmatch(r"k[A-Z]\w*", args[i]) and args[i] in consts:
                        d = consts[args[i]]
                        add(rel, t.count("\n", 0, m.start()) + 1, "helper", d.get(f, next(iter(d.values()))),
                            "%s(%s)" % (m.group(1), args[i]), name=m.group(1))
    # pattern-resolved: the use sits on a Grva( line (table: the use line; helper: the helper's own base + rva line)
    tables = {}
    for u in uses:
        rel = u.get("used_at", "%s:%d" % (u["file"], u["line"]))
        uf, ul = rel.rsplit(":", 1)
        span = range(int(ul), int(ul) + 21) if "(for " in u["expr"] else (int(ul),)  # range-for: the loop body
        wrapped = (u["kind"] == "helper" and u["name"] in wrapped_helpers) or any(k in marks.get(ROOT / uf, ()) for k in span)
        if uf not in tables:
            tables[uf] = mod_table(uf)
        u["wrapped"] = wrapped
        u["listed"] = tables[uf] is not None and u["rva"] in tables[uf]
    # one row per (file, line, rva)
    seen, out = set(), []
    for u in uses:
        k = (u["file"], u["line"], u["rva"])
        if k not in seen:
            seen.add(k)
            out.append(u)
    out.sort(key=lambda u: (u["file"], u["line"], u["rva"]))
    return out, helpers


def main():
    a = sys.argv[1:]
    uses, helpers = scan("--tests" in a)
    print("RVA helpers: " + ", ".join("%s(arg %d)" % kv for kv in sorted(helpers.items())))
    allow = set()
    if ALLOW.exists():
        allow = {l.split("#")[0].strip().lower() for l in ALLOW.read_text().splitlines() if l.split("#")[0].strip()}
    bad = []
    for u in uses:
        u["allowed"] = ("%s:0x%x" % (u["file"], u["rva"])).lower() in allow or ("%s:*" % u["file"]).lower() in allow
        tag = ("bound" if u["kind"] == "bound" else "allow" if u["allowed"] else
               ("grva" if u["listed"] else "UNLST") if u["wrapped"] else "RAW")
        if tag in ("RAW", "UNLST"):
            bad.append(u)
        print("%s:%d: %-5s %-7s 0x%07X  %s%s" % (u["file"], u["line"], tag, u["kind"], u["rva"], u["expr"],
                                               "  (used at %s)" % u["used_at"] if "used_at" in u else ""))
    counted = [u for u in uses if u["kind"] != "bound"]
    for m in sorted({u["file"].split("/")[1] for u in counted}):
        mu = [u for u in counted if u["file"].split("/")[1] == m]
        print("  %-18s %4d uses: %4d RAW, %4d via Grva + pattern table, %d Grva but not in rvas.json, %d allowlisted" % (
            m, len(mu), sum(1 for u in mu if not u["wrapped"] and not u["allowed"]), sum(1 for u in mu if u["wrapped"] and u["listed"]),
            sum(1 for u in mu if u["wrapped"] and not u["listed"]), sum(1 for u in mu if u["allowed"])))
    print("fixed-RVA uses: %d (%d unique RVAs) in %d files, %d RAW/unlisted, %d range bounds ignored"
          % (len(counted), len({u["rva"] for u in counted}), len({u["file"] for u in counted}), len(bad), len(uses) - len(counted)))
    if "--json" in a:
        pathlib.Path(a[a.index("--json") + 1]).write_text(json.dumps(uses, indent=1))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
