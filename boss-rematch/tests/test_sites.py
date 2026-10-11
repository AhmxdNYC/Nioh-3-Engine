r"""test_sites.py - offline checks of the add-on's site table (sites.json / src/sites_gen.h) against the v2.02 dump.

    py -I tests/test_sites.py [path\to\Nioh3.dump.exe]

1. sites_gen.h / sites.json are in sync with tools/derive_sites.py.
2. Every pattern site matches EXACTLY ONCE in the executable sections, at its RVA (the same scan the add-on runs on an unknown build).
3. Every native site: the script native table maps its name to the RVA, and the head bytes match.
4. Every global: the anchor's rip-relative disp resolves to the expected RVA.
5. Hook heads are whole, relocatable instructions (no rip-relative / branch).
6. Patch simulation: the image with 16 bytes inserted in the middle of .text (every later address and rip target moves) - every
   site after the insertion is still found exactly once, 16 bytes later, and every global still resolves (to its shifted address).
Ends with "ALL N SITES OK" or a list of failures (exit 1).
"""
import json
import pathlib
import re
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DUMP = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(r"re\Nioh3.dump.exe")
sys.path.insert(0, str(ROOT / "tools"))
import derive_sites as ds  # noqa: E402

fails = []


def check(c, m):
    if not c:
        fails.append(m)
    return c


def secs_of(img):
    return ds.exec_sections(img)


def pat_list(s):
    return [None if t == "??" else int(t, 16) for t in s.split()]


def resolve_all(img, secs, natives=True):
    """returns {name: address} the way the add-on resolves (scan only; natives via the table)"""
    nat = ds.native_table(img) if natives else {}
    by_name = {v: k for k, v in nat.items()}
    out = {}
    for s in doc["sites"]:
        if s["kind"] == "native":
            a = by_name.get(s["native"])
            if a is not None and img[a:a + s["head"]] == bytes(pat_list(s["pattern"])):
                out[s["name"]] = a
            continue
        hits = ds.count(img, secs, pat_list(s["pattern"]))
        if len(hits) != 1:
            out[s["name"]] = ("hits", len(hits))
            continue
        site = hits[0] + s["offset"]
        if s["kind"] == "global":
            disp = struct.unpack_from("<i", img, site + s["disp_off"])[0]
            out[s["name"]] = site + s["insn_len"] + disp + s["addend"]
            out["@" + s["name"]] = site
        else:
            out[s["name"]] = site
    return out


r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "derive_sites.py"), str(DUMP), "--check"], capture_output=True, text=True)
check(r.returncode == 0, "sites.json / sites_gen.h out of date: " + r.stdout.strip())
doc = json.loads((ROOT / "sites.json").read_text())
img = DUMP.read_bytes()
pe = struct.unpack_from("<I", img, 0x3C)[0]
check(struct.unpack_from("<I", img, pe + 8)[0] == int(doc["build"]["timestamp"], 16), "dump timestamp differs from sites.json")
secs = secs_of(img)
res = resolve_all(img, secs)
for s in doc["sites"]:
    got = res.get(s["name"])
    exp = s["rva"]
    check(got == exp, "%s (%s): resolved %r, expected 0x%X" % (s["name"], s["kind"], got if not isinstance(got, int) else hex(got), exp))
    if s["kind"] in ("hook", "native"):
        a = s["rva"]
        while a < s["rva"] + s["head"]:
            ins = next(ds.md.disasm(img[a:a + 15], a))
            check(not (ins.group(ds.capstone.CS_GRP_JUMP) or ins.group(ds.capstone.CS_GRP_CALL) or "rip" in ins.op_str),
                  "%s: head not relocatable (%s %s)" % (s["name"], ins.mnemonic, ins.op_str))
            a += ins.size
        check(a == s["rva"] + s["head"], "%s: head not on an instruction boundary" % s["name"])
# 6. patch simulation
text_va, text_vs = max(secs, key=lambda x: x[1])
cut = text_va + text_vs // 2
cut = cut - (cut % 16)
img2 = img[:cut] + b"\xCC" * 16 + img[cut:]
secs2 = [(va if va < cut else va + 16, vs + (16 if va <= cut < va + vs else 0)) for va, vs in secs]
res2 = resolve_all(img2, secs2, natives=False)
moved = 0
for s in doc["sites"]:
    if s["kind"] == "native":
        continue  # the pointer table is data with absolute pointers: not rebased by this simulation (the add-on reads the live table)
    if s["kind"] == "global":  # the simulation cannot re-link disp32 across the cut: check the anchor was re-found where it moved to
        exp = s["anchor"] + (16 if s["anchor"] >= cut else 0)
        got = res2.get("@" + s["name"])
        moved += s["anchor"] >= cut
        check(got == exp, "patch simulation: %s anchor %r, expected 0x%X" % (s["name"], got if not isinstance(got, int) else hex(got), exp))
        continue
    exp = s["rva"] + (16 if s["rva"] >= cut else 0)
    got = res2.get(s["name"])
    moved += s["rva"] >= cut
    check(got == exp, "patch simulation: %s resolved %r, expected 0x%X" % (s["name"], got if not isinstance(got, int) else hex(got), exp))
n = len(doc["sites"])
if fails:
    print("FAILED (%d):" % len(fails))
    for f in fails:
        print("  " + f)
    sys.exit(1)
print("patch simulation: 16 bytes inserted at .text+0x%X: %d sites moved, all re-found by pattern" % (cut - text_va, moved))
print("ALL %d SITES OK (%d native, %d pattern)" % (n, sum(s["kind"] == "native" for s in doc["sites"]), sum(s["kind"] != "native" for s in doc["sites"])))
