r"""aob_audit.py - propose a unique byte pattern for every fixed RVA that tools/check_fixed_rvas.py reports.

    py -I tools/aob_audit.py DUMP [--out notes/aob-audit.md] [--json F] [-j N]

DUMP must be a DECRYPTED image of the running game (file offset == RVA, e.g. a /proc/<pid>/mem or ReadProcessMemory dump):
the on-disk Nioh3.exe keeps .text encrypted, so a static scan of it matches nothing (#57885). The dump's TimeDateStamp /
SizeOfImage must equal the build the RVAs were taken from (sites.json "build").
Patterns come from mod/Nioh3BossRematch/tools/derive_sites.py (same rules as the shipped Site() table): rip-relative
displacements and rel32 branch targets are wildcarded, the window grows until the pattern matches EXACTLY ONCE in the
executable sections. Code RVAs get a direct pattern (+offset); data RVAs (.data/.rdata globals) get a rip-relative anchor
instruction (kind global: target = anchor + insn_len + disp32), like sites.json.
"""
import importlib.util
import json
import multiprocessing as mp
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import check_fixed_rvas  # noqa: E402

spec = importlib.util.spec_from_file_location("derive_sites", ROOT / "mod/Nioh3BossRematch/tools/derive_sites.py")
ds = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ds)

IMG = None
SECS = None


def sections(img):
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    nsec = struct.unpack_from("<H", img, pe + 6)[0]
    sh = pe + 24 + struct.unpack_from("<H", img, pe + 20)[0]
    out = []
    for i in range(nsec):
        o = sh + 40 * i
        name = img[o:o + 8].rstrip(b"\0").decode("latin1")
        vs, va = struct.unpack_from("<II", img, o + 8)
        ch = struct.unpack_from("<I", img, o + 36)[0]
        out.append((name, va, vs, bool(ch & 0x20000000)))
    return out


def pdata_starts(img, secs):
    for name, va, vs, _ in secs:
        if name == ".pdata":
            return {struct.unpack_from("<I", img, va + 12 * i)[0] for i in range(vs // 12)}
    return set()


def iat_names(img):
    """IAT slot rva -> 'dll!func' (resolve these by import name, not by pattern)"""
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    imp = struct.unpack_from("<I", img, pe + 24 + 112 + 8)[0]
    out = {}
    while imp:
        ilt, _, _, name, iat = struct.unpack_from("<IIIII", img, imp)
        if not name:
            break
        dll = img[name:img.index(b"\0", name)].decode("latin1")
        src = ilt or iat
        k = 0
        while True:
            e = struct.unpack_from("<Q", img, src + 8 * k)[0]
            if not e:
                break
            fn = "#%d" % (e & 0xFFFF) if e >> 63 else img[(e & 0x7FFFFFFF) + 2:img.index(b"\0", (e & 0x7FFFFFFF) + 2)].decode("latin1")
            out[iat + 8 * k] = "%s!%s" % (dll, fn)
            k += 1
        imp += 20
    return out


def init(img):
    global IMG, SECS
    IMG = img
    SECS = ds.exec_sections(img)


def work(job):
    rva, is_exec = job
    try:
        if is_exec:
            pat, back = ds.derive(IMG, SECS, rva, 12)
            return {"rva": rva, "kind": "code", "pattern": ds.to_str(pat), "offset": back, "unique": True}
        refs = ds.find_refs(IMG, SECS, rva)
        cands = [i for i in (ds.insn_at_disp(IMG, p) for p in refs) if i]
        for ins in cands[:40]:
            try:
                pat, back = ds.derive(IMG, SECS, ins.address, 12)
            except SystemExit:
                continue
            return {"rva": rva, "kind": "global", "pattern": ds.to_str(pat), "offset": back, "anchor": ins.address,
                    "disp_off": ins.disp_offset, "insn_len": ins.size, "refs": len(refs),
                    "what": "%s %s" % (ins.mnemonic, ins.op_str), "unique": True}
        # no direct rip-relative reference: try a nearby referenced base (struct/array inside a global), addend = rva - base
        for d in range(8, 0x400, 8):
            refs = ds.find_refs(IMG, SECS, rva - d)
            cands = [i for i in (ds.insn_at_disp(IMG, p) for p in refs) if i]
            for ins in cands[:10]:
                try:
                    pat, back = ds.derive(IMG, SECS, ins.address, 12)
                except SystemExit:
                    continue
                return {"rva": rva, "kind": "global", "pattern": ds.to_str(pat), "offset": back, "anchor": ins.address,
                        "disp_off": ins.disp_offset, "insn_len": ins.size, "addend": d, "refs": len(refs),
                        "what": "%s %s (+0x%X)" % (ins.mnemonic, ins.op_str, d), "unique": True}
        return {"rva": rva, "kind": "global", "unique": False, "why": "no rip-relative reference within 0x400 below"}
    except SystemExit as e:
        return {"rva": rva, "kind": "code" if is_exec else "global", "unique": False, "why": str(e)}


def source_note(u, cache={}):
    f = ROOT / u["file"]
    if f not in cache:
        cache[f] = f.read_text(encoding="utf-8", errors="replace").split("\n")
    line = cache[f][u["line"] - 1]
    c = line.find("//")
    note = line[c + 2:].strip() if c >= 0 else ""
    return (u.get("name", "") + (": " if u.get("name") and note else "") + note)[:110].replace("|", "/")


def main():
    a = sys.argv[1:]
    dump = pathlib.Path(a[0])
    out = pathlib.Path(a[a.index("--out") + 1]) if "--out" in a else ROOT / "notes" / "aob-audit.md"
    jobs_n = int(a[a.index("-j") + 1]) if "-j" in a else max(1, mp.cpu_count() - 2)
    img = dump.read_bytes()
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    ts, soi = struct.unpack_from("<I", img, pe + 8)[0], struct.unpack_from("<I", img, pe + 24 + 56)[0]
    sites = json.loads((ROOT / "mod/Nioh3BossRematch/sites.json").read_text())
    want = sites["build"]
    if (ts, soi) != (int(want["timestamp"], 16), int(want["size_of_image"], 16)):
        raise SystemExit("dump build %X/%X != sites.json build %s/%s" % (ts, soi, want["timestamp"], want["size_of_image"]))
    secs = sections(img)
    starts = pdata_starts(img, secs)
    natives = ds.native_table(img)
    iat = iat_names(img)
    by_site = {}
    for s in sites["sites"]:
        by_site.setdefault(s["rva"], s["name"])
    uses, _ = check_fixed_rvas.scan()
    uses = [u for u in uses if u["kind"] != "bound"]
    rvas = sorted({u["rva"] for u in uses})

    def sec_of(r):
        for name, va, vs, ex in secs:
            if va <= r < va + max(vs, 1):
                return name, ex
        return "?", False

    jobs = [(r, sec_of(r)[1]) for r in rvas if r not in by_site]
    with mp.Pool(jobs_n, initializer=init, initargs=(img,)) as pool:
        res = {r["rva"]: r for r in pool.imap_unordered(work, jobs, chunksize=4)}
    for r in rvas:
        if r in by_site:
            res[r] = {"rva": r, "kind": "site", "pattern": "Site(\"%s\")" % by_site[r], "unique": True}

    n_uni = sum(1 for r in rvas if res[r]["unique"])
    lines = [
        "# Fixed-RVA audit (byte-pattern plan)",
        "",
        "Generated by `tools/aob_audit.py` from `tools/check_fixed_rvas.py` against a decrypted memory dump of build "
        "TimeDateStamp %X / SizeOfImage %X (same build as sites.json). Uniqueness = the pattern matches exactly once in the "
        "executable sections of the dump (the on-disk exe is encrypted, so it cannot be used). Regenerate after code changes:" % (ts, soi),
        "",
        "    py -I tools/check_fixed_rvas.py            # the raw list (warn-only in build.bat)",
        "    py -I tools/aob_audit.py <decrypted dump>  # this file",
        "",
        "**%d raw uses, %d unique RVAs, %d already in sites.json (just call Site()), %d new patterns proposed, %d verified unique, "
        "%d without a unique pattern.**" % (len(uses), len(rvas), sum(1 for r in rvas if r in by_site),
                                           sum(1 for r in rvas if r not in by_site and res[r].get("pattern")), n_uni,
                                           len(rvas) - n_uni),
        "",
        "Kinds: `code` pattern at RVA - offset (offset = bytes the pattern starts BEFORE the RVA); `global` = rip-relative "
        "anchor instruction, target = anchor + insn_len + disp32 + addend (sites.json kind global); `site` = already a Site(). "
        "Use kinds: literal `base+0x..`, const `base+kName`, table (entry of an RVA table), helper (`Helper(0x..)`).",
        "",
        "Per mod (uses / unique RVAs): " + ", ".join("%s %d/%d" % (m, sum(1 for u in uses if u["file"].split("/")[1] == m),
                                                                len({u["rva"] for u in uses if u["file"].split("/")[1] == m}))
                                                   for m in sorted({u["file"].split("/")[1] for u in uses})) + ". "
        "Per use kind: " + ", ".join("%s %d" % (k, sum(1 for u in uses if u["kind"] == k)) for k in ("literal", "const", "table", "helper")) +
        ". IAT slots (resolve by import name instead): %d. Diagnostic-only label tables (replayprof_names.h) are listed but are "
        "allowlist candidates." % sum(1 for r in rvas if r in iat),
        "",
        "No unique pattern: " + (", ".join("%X (%s)" % (r, res[r].get("why", "")) for r in rvas if not res[r]["unique"]) or "none") + ".",
        "",
        "| file:line | RVA | sec | use | what it is (source) | proposed pattern | off | unique |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for u in sorted(uses, key=lambda u: (u["file"], u["line"], u["rva"])):
        r = res[u["rva"]]
        sec, _ = sec_of(u["rva"])
        what = source_note(u)
        tag = []
        if u["rva"] in iat:
            tag.append("IAT slot %s (better: resolve by import name)" % iat[u["rva"]])
        if u["rva"] in natives:
            tag.append("native " + natives[u["rva"]])
        elif u["rva"] in starts:
            tag.append("fn start")
        if r["kind"] == "global" and r.get("what"):
            tag.append("anchor %X `%s`" % (r["anchor"], r["what"]))
        if what or tag:
            what = "; ".join([w for w in [what] + tag if w])
        pat = r.get("pattern") or r.get("why", "")
        off = r.get("offset", "")
        lines.append("| %s:%d | %X | %s | %s | %s | `%s` | %s | %s |" % (
            u["file"], u["line"], u["rva"], sec, u["kind"], what, pat, off, "yes" if r["unique"] else "**NO**"))
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    if "--json" in a:
        pathlib.Path(a[a.index("--json") + 1]).write_text(json.dumps({"uses": uses, "patterns": list(res.values())}, indent=1))
    print("IAT slots %d" % sum(1 for r in rvas if r in iat))
    print("raw uses %d, unique RVAs %d, in sites.json %d, proposed %d, unique-verified %d, no pattern %d -> %s" % (
        len(uses), len(rvas), sum(1 for r in rvas if r in by_site),
        sum(1 for r in rvas if r not in by_site and res[r].get("pattern")), n_uni, len(rvas) - n_uni, out))


if __name__ == "__main__":
    main()
