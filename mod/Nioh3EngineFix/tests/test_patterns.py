"""test_patterns.py - verify patches.json against the decrypted Nioh 3 v2.02 image (stdlib only).

    py -I tests/test_patterns.py [path/to/Nioh3.dump.exe]

The dump is a memory-layout snapshot of the decrypted image (raw offset == RVA). Checks:
  * build identity (PE TimeDateStamp / SizeOfImage) matches patches.json
  * every pattern matches EXACTLY ONCE in the executable sections (same scan scope as the mod) and
    resolves (match + offset) to the expected RVA
  * the dump's bytes at each site equal `orig`; sites are instruction-aligned in the pattern; no two sites overlap
  * replacement shapes: bytes kind same length; call kind fits (E8 rel32 + nops); ripload is 6 bytes
  * values the mod reads from original bytes resolve to what the scripts hard-coded:
      W3 disp32 -> IAT slot of USER32!PeekMessageA, L1 rel32 -> 0x916EBC, P7 rel32 -> GetDelta60 0x46484,
      P8a-d all call the same function (GetFps 0x46594)
  * src/patches_gen.h is in sync with patches.json
  * mod menu: the XInputGetState IAT slot and the engine stop-flag code it relies on (pause hint)
  * (info) any Lyall Nioh3Fix 0.0.6 pattern that lands within 32 bytes of one of our sites
"""
import json
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_header  # noqa: E402

DUMP = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(r"C:\Users\Ahmad Hamza\nioh3-engine\re\Nioh3.dump.exe")

NIOH3FIX_PATTERNS = [  # notes/nioh3fix-signatures.md (informational overlap check only)
    "41 ?? ?? ?? 04 00 00 00 EB ?? 41 ?? ?? ?? 03 00 00 00 EB ?? 8A ?? ?? 41 88 ?? ??",
    "4C 8D ?? ?? ?? ?? ?? 48 ?? ?? 41 ?? ?? ?? 75 ?? 8B ?? ?? ?? ?? ??",
    "48 89 ?? ?? 75 ?? 8B ?? ?? ?? ?? ?? 4C 8D ?? ?? 89 ?? ?? ?? 45 33 ??",
    "0F 28 ?? 48 83 ?? ?? C3 48 83 ?? ?? F3 0F ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ??",
    "89 ?? ?? ?? ?? ?? 8B ?? 48 69 ?? ?? ?? ?? ?? 80 ?? ?? ?? ?? ?? ?? 00 75 ??",
    "89 ?? ?? 49 8B ?? ?? 48 8B ?? FF 90 ?? ?? ?? ?? 8B ?? 33 ??",
    "4C 8B ?? 45 84 ?? 0F 85 ?? ?? ?? ?? 80 ?? ?? ?? 00 41 ?? 01 00 00 00",
    "E8 ?? ?? ?? ?? F3 0F ?? ?? ?? ?? ?? ?? F7 ?? 66 0F ?? ?? 0F ?? ??",
    "E8 ?? ?? ?? ?? 0F ?? ?? 0F ?? ?? F3 0F ?? ?? F3 0F ?? ?? 0F ?? ?? 76 ??",
    "8B ?? 48 8D ?? 78 E8 ?? ?? ?? ?? 0F 2F ?? ??",
    "BA 08 00 00 00 83 ?? 01 74 ?? 83 ?? 01 75 ??",
    "75 ?? E8 ?? ?? ?? ?? 41 ?? ?? 84 ?? 74 ?? 40 ?? ?? 49 8B ?? ?? ?? ?? ??",
    "44 39 ?? ?? ?? ?? ?? 75 ?? 45 89 ?? ?? E9 ?? ?? ?? ?? 45 38 ?? ?? 0F 85 ?? ?? ?? ??",
    "45 89 ?? ?? 48 8B ?? ?? ?? ?? ?? 48 85 ?? 74 ?? 89 ?? ?? ?? ?? ?? 48 83 ?? ?? C3",
    "48 8B ?? ?? 0F 28 ?? 48 8B ?? 48 ?? ?? 74 ?? 48 8B ?? ??",
]


def parse_pattern(p):
    toks = p.split()
    return bytes(0 if t == "??" else int(t, 16) for t in toks), [t == "??" for t in toks]


def exec_sections(img):
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    nsec = struct.unpack_from("<H", img, pe + 6)[0]
    opt = pe + 24
    sh = opt + struct.unpack_from("<H", img, pe + 20)[0]
    secs = []
    for i in range(nsec):
        vs, va = struct.unpack_from("<II", img, sh + 40 * i + 8)
        ch = struct.unpack_from("<I", img, sh + 40 * i + 36)[0]
        if ch & 0x20000000:
            secs.append((va, vs))
    return secs


def find_all(img, secs, pattern, limit=1000):
    pat, wild = parse_pattern(pattern)
    # anchor = longest exact run (same strategy as the mod's scanner)
    best = blen = cur = cs = 0
    for i, w in enumerate(wild):
        if w:
            cur = 0
            continue
        if cur == 0:
            cs = i
        cur += 1
        if cur > blen:
            best, blen = cs, cur
    anchor = pat[best:best + blen]
    hits = []
    for va, vs in secs:
        end = va + vs
        pos = va
        while len(hits) < limit:
            j = img.find(anchor, pos, end)
            if j < 0:
                break
            st = j - best
            if st >= va and st + len(pat) <= end and all(w or img[st + k] == pat[k] for k, w in enumerate(wild)):
                hits.append(st)
            pos = j + 1
    return hits


def iat_names(img):
    """IAT slot rva -> 'DLL!name' from the import directory of the dump."""
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    dd = pe + 24 + 112
    imp = struct.unpack_from("<I", img, dd + 8)[0]
    res = {}
    d = imp
    while True:
        oft, _, _, name_rva, ft = struct.unpack_from("<IIIII", img, d)
        if name_rva == 0:
            break
        dll = img[name_rva:img.index(b"\0", name_rva)].decode("ascii").upper()
        th = oft or ft
        i = 0
        while True:
            ent = struct.unpack_from("<Q", img, th + 8 * i)[0]
            if ent == 0:
                break
            if not ent >> 63:
                nm = img[ent + 2:img.index(b"\0", ent + 2)].decode("ascii")
                res[ft + 8 * i] = dll + "!" + nm
            i += 1
        d += 20
    return res


def xinput_getstate_slots(img):
    """IAT slot rvas of XInputGetState (ordinal 2 of xinput1_x, or by name) - the mod menu hooks that slot."""
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    imp = struct.unpack_from("<I", img, pe + 24 + 112 + 8)[0]
    out = []
    d = imp
    while True:
        oft, _, _, name_rva, ft = struct.unpack_from("<IIIII", img, d)
        if name_rva == 0:
            break
        dll = img[name_rva:img.index(b"\0", name_rva)].decode("ascii").upper()
        if dll.startswith("XINPUT") and oft:
            i = 0
            while True:
                ent = struct.unpack_from("<Q", img, oft + 8 * i)[0]
                if ent == 0:
                    break
                if ent >> 63:
                    if dll.startswith("XINPUT1_") and ent & 0xFFFF == 2:
                        out.append(ft + 8 * i)
                elif img[ent + 2:img.index(b"\0", ent + 2)] == b"XInputGetState":
                    out.append(ft + 8 * i)
                i += 1
        d += 20
    return out


def main():
    fails = []

    def check(cond, msg):
        if not cond:
            fails.append(msg)
        return cond

    doc = json.loads((ROOT / "patches.json").read_text(encoding="utf-8"))
    img = DUMP.read_bytes()
    secs = exec_sections(img)
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    ts = struct.unpack_from("<I", img, pe + 8)[0]
    soi = struct.unpack_from("<I", img, pe + 24 + 56)[0]
    check(ts == int(doc["build"]["timestamp"], 16), "build timestamp mismatch: dump 0x%08X" % ts)
    check(soi == int(doc["build"]["size_of_image"], 16), "SizeOfImage mismatch: dump 0x%X" % soi)
    print("dump %s  TimeDateStamp 0x%08X  SizeOfImage 0x%X  exec sections %s"
          % (DUMP.name, ts, soi, ", ".join("%X+%X" % s for s in secs)))

    iat = iat_names(img)
    ranges = []
    p8_targets = set()
    print("%-4s %-8s %9s %4s %6s  %s" % ("id", "group", "rva", "len", "hits", "result"))
    for s in doc["sites"]:
        sid, rva, ln, off = s["id"], int(s["rva"], 16), s["len"], s["offset"]
        orig = bytes(int(x, 16) for x in s["orig"].split())
        pat, wild = parse_pattern(s["pattern"])
        ok = True
        ok &= check(len(orig) == ln, "%s: orig length %d != len %d" % (sid, len(orig), ln))
        ok &= check(off + ln <= len(pat), "%s: site extends past pattern" % sid)
        hits = find_all(img, secs, s["pattern"])
        ok &= check(len(hits) == 1, "%s: pattern matched %d times (need exactly 1)" % (sid, len(hits)))
        if hits:
            ok &= check(hits[0] + off == rva, "%s: resolves to 0x%X, expected 0x%X" % (sid, hits[0] + off, rva))
        ok &= check(img[rva:rva + ln] == orig, "%s: dump bytes at 0x%X != orig" % (sid, rva))
        # exact (non-wildcard) pattern bytes inside the site must equal orig
        ok &= check(all(wild[off + k] or pat[off + k] == orig[k] for k in range(ln)), "%s: pattern/orig disagree" % sid)
        k = s["kind"]
        if k == "bytes":
            ok &= check(len(s["new"].split()) == ln, "%s: replacement length" % sid)
        elif k == "call":
            ca = int(s.get("call_at", 0))
            ok &= check(ca + 5 <= ln and ln - ca - 5 in (0, 1, 2, 3, 5), "%s: call does not fit / bad nop pad" % sid)
            ok &= check(s.get("target") in gen_header.TARGETS[1:], "%s: unknown target" % sid)
        elif k == "ripload":
            ok &= check(ln == 6 and len(s["opcode"].split()) == 2, "%s: ripload must be 6 bytes" % sid)
            ok &= check(s.get("data") in gen_header.DATAS[1:], "%s: unknown data slot" % sid)
        else:
            ok &= check(False, "%s: unknown kind %s" % (sid, k))
        reads = s.get("reads")
        if reads == "iat_disp32@2":
            slot = rva + 6 + struct.unpack_from("<i", orig, 2)[0]
            ok &= check(iat.get(slot) == "USER32.DLL!PeekMessageA", "%s: disp32 -> 0x%X = %s" % (sid, slot, iat.get(slot)))
        elif reads == "callee_rel32@6":
            ok &= check(rva + 10 + struct.unpack_from("<i", orig, 6)[0] == 0x916EBC, "%s: callee != 0x916EBC" % sid)
        elif reads == "callee_rel32@1":
            ok &= check(rva + 5 + struct.unpack_from("<i", orig, 1)[0] == 0x46484, "%s: callee != GetDelta60 0x46484" % sid)
        if sid.startswith("P8"):
            p8_targets.add(rva + 5 + struct.unpack_from("<i", orig, 1)[0])
        ranges.append((rva, rva + ln, sid))
        print("%-4s %-8s %9X %4d %6d  %s" % (sid, s["group"], rva, ln, len(hits), "OK" if ok else "FAIL"))

    check(p8_targets == {0x46594}, "P8 call targets %s != {GetFps 0x46594}" % sorted(hex(t) for t in p8_targets))
    ranges.sort()
    for (a0, a1, an), (b0, b1, bn) in zip(ranges, ranges[1:]):
        check(a1 <= b0, "sites %s and %s overlap" % (an, bn))
    ids = [s["id"] for s in doc["sites"]]
    check(len(ids) == len(set(ids)), "duplicate site ids")

    hdr = (ROOT / "src" / "patches_gen.h").read_text(encoding="utf-8")
    check(hdr == gen_header.render(doc), "src/patches_gen.h out of sync with patches.json (run py tools/gen_header.py)")

    # mod menu (src/menu.cpp) assumptions about v2.02 - not patch sites, but read/hooked at run time
    xin = xinput_getstate_slots(img)
    check(xin == [0x38E2C50], "menu: XINPUT1_4 ordinal-2 (XInputGetState) IAT slots %s != [0x38E2C50]" % [hex(x) for x in xin])
    x1 = 0x435BB7  # the active-pad scan calls XInputGetState through that slot (notes/input-thread.md X1)
    check(img[x1:x1 + 2] == b"\xFF\x15" and x1 + 6 + struct.unpack_from("<i", img, x1 + 2)[0] == 0x38E2C50,
          "menu: call at 0x435BB7 does not go through the XInputGetState IAT slot")
    menu_bytes = {
        0x5AD6B2: "80 BF 8C 05 00 00 00 74 0A 83 A7 24 06 00 00 00 0F 57 D2 80 BF 8B 05 00 00 00",  # FrameDtSetup: 58C/58B stop time
        0x89B6EC: "48 8D 51 08 41 B8 06 00 00 00",  # stop block latch: 6 flags from the counters at +8
        0x123A930: "80 79 04 00 74 06 B8 04",       # GetStopState(G+588)
    }
    for a, hx in menu_bytes.items():
        check(img[a:a + len(hx.split())] == bytes(int(x, 16) for x in hx.split()), "menu: stop-flag code at 0x%X differs" % a)
    print("menu: XInputGetState IAT slot %s, stop-flag code %s" % (
        ", ".join("0x%X" % x for x in xin) or "MISSING",
        "OK" if all(img[a:a + len(h.split())] == bytes(int(x, 16) for x in h.split()) for a, h in menu_bytes.items()) else "DIFFERS"))

    near = []
    for p in NIOH3FIX_PATTERNS:
        for h in find_all(img, secs, p, limit=50):
            plen = len(p.split())
            for (r0, r1, sid) in ranges:
                if h < r1 + 32 and r0 < h + plen + 32:
                    near.append("%s @0x%X near %s" % (p[:23], h, sid))
    print("Nioh3Fix 0.0.6 patterns within 32 bytes of our sites: %s" % ("; ".join(near) if near else "none"))

    print()
    if fails:
        print("FAILED (%d):" % len(fails))
        for f in fails:
            print("  - " + f)
        return 1
    print("ALL %d SITES OK: unique pattern, expected RVA, original bytes, shapes, derived targets, header in sync" % len(doc["sites"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
