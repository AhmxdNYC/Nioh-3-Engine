"""build_exports.py - regenerate the tool exports from symbols.tsv (run after editing symbols.tsv).

    py re\\symbols\\build_exports.py            check symbols.tsv + write the exports next to it
    py re\\symbols\\build_exports.py --check    only validate (format, kinds, duplicate names/addresses)

Writes (all derived, never edit by hand):
    symbols.csv            plain CSV, same columns as symbols.tsv
    symbols_x64dbg.txt     x64dbg script: lbl/cmt by module RVA (Nioh3.exe:$rva); load with the `scriptload` command
                           or paste into the Script tab, then run. struct_field rows are skipped.
    symbols_ida.py         IDA (7.x-9.x) script: names/comments by RVA relative to idaapi.get_imagebase(),
                           creates missing functions for kind=function, and declares the /N3 structs.
"""
import csv, os, re, sys
from collections import defaultdict, OrderedDict

HERE = os.path.dirname(os.path.abspath(__file__))
TSV = os.path.join(HERE, "symbols.tsv")
COLS = ["rva", "kind", "name", "signature", "description", "confidence", "source"]
KINDS = {"function", "label", "global", "data", "struct_field", "vtable", "string_table"}
MODULE = "Nioh3.exe"
IMAGE_SIZE = 0x52FB000  # SizeOfImage of the v2.02 dump (mod/Nioh3EngineFix/patches.json)
NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(::[A-Za-z_][A-Za-z0-9_]*)*$")  # C++ RTTI-derived names (Class::vfunc_N) allowed
FIELD_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*$")


def load(path=TSV):
    rows = []
    with open(path, encoding="utf-8") as f:
        for ln, line in enumerate(f, 1):
            line = line.rstrip("\r\n")
            if not line or line.startswith("#") or line.startswith("rva\t"):
                continue
            c = (line.split("\t") + [""] * 7)[:7]
            r = OrderedDict(zip(COLS, (x.strip() for x in c)))
            r["_line"] = ln
            rows.append(r)
    return rows


def check(rows):
    errs = []
    names, addrs, fields = {}, {}, {}
    for r in rows:
        where = "line %d (%s)" % (r["_line"], r["name"])
        if r["kind"] not in KINDS:
            errs.append(where + ": bad kind " + r["kind"])
            continue
        try:
            v = int(r["rva"], 16)
        except ValueError:
            errs.append(where + ": bad rva " + r["rva"])
            continue
        if r["confidence"] not in ("high", "medium", "low"):
            errs.append(where + ": confidence must be high/medium/low")
        if r["kind"] == "struct_field":
            if not FIELD_RE.match(r["name"]):
                errs.append(where + ": struct_field name must be Struct::Field")
            k = (r["name"].split("::")[0], v)
            if k in fields:
                errs.append(where + ": duplicate field offset with line %d" % fields[k])
            fields[k] = r["_line"]
            if r["name"] in names:
                errs.append(where + ": duplicate name with line %d" % names[r["name"]])
            names[r["name"]] = r["_line"]
            continue
        if not NAME_RE.match(r["name"]):
            errs.append(where + ": name must be an identifier")
        if v >= IMAGE_SIZE:
            errs.append(where + ": rva outside image")
        if r["name"] in names:
            errs.append(where + ": duplicate name with line %d" % names[r["name"]])
        names[r["name"]] = r["_line"]
        if v in addrs:
            errs.append(where + ": duplicate address with line %d" % addrs[v])
        addrs[v] = r["_line"]
    return errs


def xstr(s, n=240):
    # x64dbg strings: no embedded quotes; {} is x64dbg string-formatting syntax
    s = s.replace('"', "'").replace("{", "(").replace("}", ")").replace("\\", "/")
    return s if len(s) <= n else s[: n - 3] + "..."


def cmt_text(r):
    t = "%s [%s] %s" % (r["name"], r["confidence"], r["description"])
    if r["signature"]:
        t += " | sig: " + r["signature"]
    return t


CTYPES = {
    "u8": ("unsigned __int8", 1), "uint8": ("unsigned __int8", 1), "uint8_t": ("unsigned __int8", 1), "byte": ("unsigned __int8", 1),
    "bool": ("bool", 1), "i8": ("__int8", 1), "char": ("char", 1),
    "u16": ("unsigned __int16", 2), "uint16_t": ("unsigned __int16", 2), "i16": ("__int16", 2), "short": ("__int16", 2), "word": ("unsigned __int16", 2),
    "u32": ("unsigned int", 4), "uint32": ("unsigned int", 4), "uint32_t": ("unsigned int", 4), "uint": ("unsigned int", 4), "dword": ("unsigned int", 4),
    "i32": ("int", 4), "int32": ("int", 4), "int32_t": ("int", 4), "int": ("int", 4),
    "u64": ("unsigned __int64", 8), "uint64": ("unsigned __int64", 8), "uint64_t": ("unsigned __int64", 8), "qword": ("unsigned __int64", 8), "size_t": ("unsigned __int64", 8),
    "i64": ("__int64", 8), "int64_t": ("__int64", 8), "f32": ("float", 4), "float": ("float", 4), "f64": ("double", 8), "double": ("double", 8),
    "ptr": ("void *", 8), "pointer": ("void *", 8), "handle": ("void *", 8), "hwnd": ("void *", 8), "hinstance": ("void *", 8),
    "hmodule": ("void *", 8), "hcursor": ("void *", 8), "lpcstr": ("void *", 8), "lpcwstr": ("void *", 8), "fnptr": ("void *", 8),
    "wchar_t": ("wchar_t", 2),
}


def ctype(sig):
    s = sig.strip().lower()
    if s.startswith("const "):
        s = s[6:].strip()
    m = re.match(r"^([a-z0-9_ ]+?\**)\s*\[(\d+|0x[0-9a-f]+)\](?!\s*\[)", s)
    cnt = 1
    if m:
        s, cnt = m.group(1).strip(), int(m.group(2), 0)
    else:
        m = re.match(r"^([a-z_][a-z0-9_ ]*?\**)(\s|$|[(/,;])", s)
        if not m:
            return ("unsigned __int8", 1, 1)
        s = m.group(1).strip()
    if s.endswith("*"):
        return ("void *", 8, cnt)
    t = CTYPES.get(s)
    if not t:
        return ("unsigned __int8", 1, 1)
    return (t[0], t[1], cnt)


def struct_decls(rows):
    by = defaultdict(list)
    for r in rows:
        if r["kind"] == "struct_field":
            s, f = r["name"].split("::")
            by[s].append((int(r["rva"], 16), f, r))
    out = []
    for s in sorted(by):
        fl = sorted(by[s])
        body, pos, pad = [], 0, 0
        for off, f, r in fl:
            t, sz, cnt = ctype(r["signature"])
            if off < pos:
                body.append("  // skipped %s @+0x%x (overlaps previous field)" % (f, off))
                continue
            if off > pos:
                body.append("  unsigned __int8 _pad%d[0x%x];" % (pad, off - pos))
                pad += 1
            body.append("  %s %s%s; // +0x%x %s" % (t, f, "[%d]" % cnt if cnt > 1 else "", off, xstr(r["description"], 100).replace("*/", "")))
            pos = off + sz * cnt
        out.append("struct %s {\n%s\n};" % (s, "\n".join(body)))
    return "\n".join(out)


def write_exports(rows):
    sym = [r for r in rows if r["kind"] != "struct_field"]
    with open(os.path.join(HERE, "symbols.csv"), "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(COLS)
        for r in rows:
            w.writerow([r[c] for c in COLS])
    with open(os.path.join(HERE, "symbols_x64dbg.txt"), "w", encoding="utf-8", newline="\r\n") as f:
        f.write("// Nioh 3 v2.02 symbols - generated from re/symbols/symbols.tsv by build_exports.py. Do not edit.\n")
        f.write("// Load with: scriptload \"<path>\\symbols_x64dbg.txt\" then scriptrun (or Script tab > Load/Run). Needs %s loaded.\n" % MODULE)
        for r in sym:
            rva = int(r["rva"], 16)
            f.write('lbl %s:$%X, "%s"\n' % (MODULE, rva, r["name"]))
            f.write('cmt %s:$%X, "%s"\n' % (MODULE, rva, xstr(cmt_text(r))))
        f.write("ret\n")
    with open(os.path.join(HERE, "symbols_ida.py"), "w", encoding="utf-8") as f:
        f.write('"""Nioh 3 v2.02 symbols for IDA - generated from re/symbols/symbols.tsv by build_exports.py. Do not edit.\n')
        f.write("File > Script file... (Alt+F7). Addresses are RVAs relative to idaapi.get_imagebase(), so any rebase works.\n")
        f.write('Idempotent: names/comments are overwritten, nothing is deleted."""\n')
        f.write("import idaapi, idc, ida_funcs, ida_name, ida_bytes\n\n")
        f.write("SYMS = [  # (rva, kind, name, comment)\n")
        for r in sym:
            f.write("    (0x%x, %r, %r, %r),\n" % (int(r["rva"], 16), r["kind"], r["name"], cmt_text(r) + " | src: " + r["source"]))
        f.write("]\n\nSTRUCTS = %r\n\n" % struct_decls(rows))
        f.write(
            "def main():\n"
            "    base = idaapi.get_imagebase()\n"
            "    ok = made = bad = 0\n"
            "    for rva, kind, name, cmt in SYMS:\n"
            "        ea = base + rva\n"
            "        if kind == 'function':\n"
            "            f = ida_funcs.get_func(ea)\n"
            "            if f is None or f.start_ea != ea:\n"
            "                if f is None and ida_funcs.add_func(ea):\n"
            "                    made += 1\n"
            "            f = ida_funcs.get_func(ea)\n"
            "            if f is not None and f.start_ea == ea:\n"
            "                idc.set_func_cmt(ea, cmt, 0)\n"
            "            else:\n"
            "                idc.set_cmt(ea, cmt, 0)\n"
            "        else:\n"
            "            idc.set_cmt(ea, cmt, 0)\n"
            "        if idc.set_name(ea, name, idc.SN_NOWARN | idc.SN_NOCHECK):\n"
            "            ok += 1\n"
            "        else:\n"
            "            bad += 1\n"
            "            print('N3: could not name 0x%x %s' % (rva, name))\n"
            "    if STRUCTS:\n"
            "        n = idc.parse_decls(STRUCTS, idc.PT_SILENT)\n"
            "        print('N3: struct declarations parsed, errors=%s' % n)\n"
            "    print('N3: named %d, functions created %d, failed %d' % (ok, made, bad))\n\n"
            "main()\n")


def main():
    rows = load()
    errs = check(rows)
    for e in errs:
        print("ERROR", e)
    kinds = defaultdict(int)
    for r in rows:
        kinds[r["kind"]] += 1
    print("%d rows: %s" % (len(rows), ", ".join("%s=%d" % kv for kv in sorted(kinds.items()))))
    if errs:
        sys.exit(1)
    if "--check" not in sys.argv:
        write_exports(rows)
        print("wrote symbols.csv, symbols_x64dbg.txt, symbols_ida.py")


if __name__ == "__main__":
    main()
