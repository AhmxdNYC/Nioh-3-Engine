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
  * experimental sites: `jmp` shapes, `vptr` slots (value = image base + vtarget, the only aligned pointer to it),
    `callee` E8 targets, `iat_name` FF 15 slots (ordinal imports as DLL!#n), and every `deps` range (exact bytes or
    FNV-1a) against the dump
  * src/patches_gen.h is in sync with patches.json
  * mod menu: the XInputGetState IAT slot and the engine stop-flag code it relies on (pause hint)
  * game 'menu open' getters 0x5A37D4 / 0xD63C0 (pause hint, FG pause reason)
  * frame generation (menu FG item, overlay FG status): FrameGenSettingWrite / IsFrameGenTypeAvailable bytes, the
    SetOptionValue dirty store, the slots/flags they reference, config descriptors 50/54, the config.xml path string
  * (info) any Lyall Nioh3Fix 0.0.6 pattern that lands within 32 bytes of one of our sites
"""
import json, re
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
            elif oft:
                res[ft + 8 * i] = dll + "!#%d" % (ent & 0xFFFF)
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


def lo_sim(img, by_id, iat, check):
    """Runs 1A3146A..1A31486 (stock, RWb rewrite, LO cave word 0/1) through a tiny interpreter; compares ebx + calls."""
    src = (ROOT / "src" / "loadonly.cpp").read_text(encoding="utf-8")
    m = re.search(r"kRwbCave\[0x4C\] = \{(.*?)\};", src, re.S)
    if not check(m is not None, "LO: kRwbCave not found in src/loadonly.cpp"):
        return False
    body = re.sub(r"/\*.*?\*/|//[^\n]*", "", m.group(1), flags=re.S)
    cave = bytes(int(t, 0) for t in body.replace(",", " ").split())
    if not check(len(cave) == 0x4C, "LO: kRwbCave is %d bytes, expected 0x4C" % len(cave)):
        return False
    SITE, BACK, SKIP, CAVE = 0x1A3146A, 0x1A31470, 0x1A31486, 0x7000000
    cave = bytearray(cave)
    for off, val in ((0x15, BACK), (0x31, BACK), (0x3F, SKIP)):  # what InstallRwb fills in (base 0 here)
        cave[off:off + 8] = struct.pack("<Q", val)
    sleep_slot = SITE + 22 + 6 + struct.unpack_from("<i", img, SITE + 22 + 2)[0]
    ok = check(iat.get(sleep_slot) == "KERNEL32.DLL!Sleep", "LO: 1A31480 does not call Sleep")

    def run(mem_patch, word, ebx):
        def rd(a, n):
            if CAVE <= a < CAVE + len(cave):
                b = bytes(cave[a - CAVE:a - CAVE + n])
                if a - CAVE == 0x48 and n == 4:
                    return struct.pack("<I", word)
                return b
            out = bytearray(img[a:a + n])
            for pa, pb in mem_patch.items():
                for k in range(len(pb)):
                    if a <= pa + k < a + n:
                        out[pa + k - a] = pb[k]
            return bytes(out)
        pc, zf, edx, rcx, ev = SITE, None, None, "?", []
        for _ in range(64):
            if pc == SKIP:
                return ebx & 0xFFFFFFFF, tuple(ev)
            b = rd(pc, 8)
            if b[:2] == b"\x83\x3D":
                d = struct.unpack_from("<i", b, 2)[0]
                zf = struct.unpack("<I", rd(pc + 7 + d, 4))[0] == b[6]
                pc += 7
            elif b[0] == 0x75:
                pc += 2 + (0 if zf else struct.unpack_from("<b", b, 1)[0])
            elif b[:2] == b"\x8B\x95":
                edx = ("[rbp+%X]" % struct.unpack_from("<i", b, 2)[0]); pc += 6
            elif b[:3] == b"\x48\x8B\xCD":
                rcx = "rbp"; pc += 3
            elif b[0] == 0xE8:
                t = pc + 5 + struct.unpack_from("<i", b, 1)[0]
                ev.append(("call %X" % t, rcx, edx)); pc += 5; rcx = edx = "clobbered"
            elif b[:2] == b"\xFF\xC3":
                ebx += 1; zf = (ebx & 0xFFFFFFFF) == 0; pc += 2
            elif b[:2] == b"\xFF\xCB":
                ebx -= 1; zf = (ebx & 0xFFFFFFFF) == 0; pc += 2
            elif b[:2] == b"\x84\xDB":
                zf = (ebx & 0xFF) == 0; pc += 2
            elif b[:2] == b"\x33\xC9":
                rcx = 0; pc += 2
            elif b[:2] == b"\xFF\x15":
                slot = pc + 6 + struct.unpack_from("<i", b, 2)[0]
                ev.append(("call [%s]" % iat.get(slot), rcx)); pc += 6
            elif b[:2] == b"\xFF\x25":
                pc = struct.unpack("<Q", rd(pc + 6 + struct.unpack_from("<i", b, 2)[0], 8))[0]
            elif b[0] == 0xE9:
                pc += 5 + struct.unpack_from("<i", b, 1)[0]
            elif b[0] == 0x90:
                pc += 1
            else:
                return ("undecoded %X: %s" % (pc, b.hex()), ())
        return ("no exit", ())

    rwb = by_id["RWb"]
    rwb_patch = {SITE: bytes(int(x, 16) for x in rwb["new"].split())}
    rel = CAVE - (SITE + 5)
    lo_patch = {SITE: b"\xE9" + struct.pack("<i", rel) + b"\x90"}
    for hi in (0x0, 0x1200):
        for bl in range(256):
            e0 = hi + bl
            st, rb = run({}, 0, e0), run(rwb_patch, 0, e0)
            c0, c1 = run(lo_patch, 0, e0), run(lo_patch, 1, e0)
            ok &= check(c0 == st, "LO: cave word 0 differs from stock at ebx %X: %s vs %s" % (e0, c0, st))
            ok &= check(c1 == rb, "LO: cave word 1 differs from RWb at ebx %X: %s vs %s" % (e0, c1, rb))
            ok &= check(st[1] and st[1][0][0] == "call 48B9C" and st[1][0][1] == "rbp", "LO: stock path does not wake via 48B9C")
            if not ok:
                return False
    # sanity: RWb skips the wake unless bl wraps, stock always wakes
    ok &= check(len(run(rwb_patch, 0, 5)[1]) == 0 and len(run(rwb_patch, 0, 0xFF)[1]) == 2, "LO: RWb reference semantics unexpected")
    return ok


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
    ib0 = struct.unpack_from("<Q", img, pe + 24 + 24)[0]
    for s in doc["sites"]:
        sid, rva, ln = s["id"], int(s["rva"], 16), s["len"]
        if s["kind"] == "vptr":
            # aligned pointer in .rdata (no code pattern): value = image base + vtarget, and it is the only such pointer
            want = ib0 + int(s["vtarget"], 16)
            ok = check(ln == 8 and rva % 8 == 0, "%s: vptr must be an aligned 8-byte slot" % sid)
            ok &= check(struct.unpack_from("<Q", img, rva)[0] == want, "%s: slot 0x%X != base+%s" % (sid, rva, s["vtarget"]))
            needle, n, pos = struct.pack("<Q", want), 0, 0
            while True:
                pos = img.find(needle, pos)
                if pos < 0:
                    break
                n += pos % 8 == 0
                pos += 1
            ok &= check(n == 1, "%s: %d aligned pointers to %s in the image (need exactly 1, the slot)" % (sid, n, s["vtarget"]))
            ok &= check(s.get("target") in gen_header.TARGETS[1:], "%s: unknown target" % sid)
            ranges.append((rva, rva + ln, sid))
            print("%-4s %-8s %9X %4d %6s  %s" % (sid, s["group"], rva, ln, "-", "OK" if ok else "FAIL"))
            continue
        off = s["offset"]
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
        elif k == "jmp":
            ok &= check(ln - 5 in (0, 1, 2, 3, 5), "%s: jmp does not fit / bad nop pad" % sid)
            ok &= check(s.get("target") in gen_header.TARGETS[1:], "%s: unknown target" % sid)
        elif k == "check":
            pass  # verified like every site (pattern, RVA, bytes), never written (GP marker code, 1.7.0)
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
        if "callee" in s:
            ok &= check(orig[0] == 0xE8 and rva + 5 + struct.unpack_from("<i", orig, 1)[0] == int(s["callee"], 16),
                        "%s: E8 target != %s" % (sid, s["callee"]))
        if "iat_name" in s:
            at = int(s["iat_disp_at"])
            slot = rva + at + 4 + struct.unpack_from("<i", orig, at)[0]
            ok &= check(orig[at - 2:at] == b"\xFF\x15" and iat.get(slot, "").upper() == s["iat_name"].upper(),
                        "%s: call [rip+disp] -> 0x%X = %s, expected %s" % (sid, slot, iat.get(slot), s["iat_name"]))
        if sid.startswith("P8"):
            p8_targets.add(rva + 5 + struct.unpack_from("<i", orig, 1)[0])
        ranges.append((rva, rva + ln, sid))
        print("%-4s %-8s %9X %4d %6d  %s" % (sid, s["group"], rva, ln, len(hits), "OK" if ok else "FAIL"))

    # coexistence: TSb sits 5 bytes after R1j; its pattern must still match once R1/R1j are written (R1 first)
    by_id = {s["id"]: s for s in doc["sites"]}
    if "TSb" in by_id and "R1j" in by_id:
        r1img = bytearray(img)
        for sid in ("R1j",):
            s = by_id[sid]
            a = int(s["rva"], 16)
            r1img[a:a + s["len"]] = bytes(int(x, 16) for x in s["new"].split())
        hits_r1 = find_all(bytes(r1img), secs, by_id["TSb"]["pattern"])
        check(hits_r1 == [int(by_id["TSb"]["rva"], 16) - by_id["TSb"]["offset"]],
              "TSb pattern does not survive R1j being patched (%d hits)" % len(hits_r1))
        print("coexistence: TSb pattern still unique with R1j written: %s" % ("OK" if len(hits_r1) == 1 else "FAIL"))

    # dependencies of the caves: original code they reproduce or rely on beyond their own site
    deps = doc.get("deps", [])
    groups = {s["group"] for s in doc["sites"]}

    def fnv1a(b):
        h = 0x811C9DC5
        for x in b:
            h = ((h ^ x) * 0x01000193) & 0xFFFFFFFF
        return h
    dep_ok = 0
    for d in deps:
        a = int(d["rva"], 16)
        good = d["group"] in groups
        if "bytes" in d:
            want = bytes(int(x, 16) for x in d["bytes"].split())
            good = good and img[a:a + len(want)] == want
        else:
            good = good and fnv1a(img[a:a + int(d["len"])]) == int(d["fnv1a"], 16)
        dep_ok += good
        check(good, "dep %s @0x%X (%s) differs from the dump" % (d["group"], a, d.get("what", "")))
    print("deps: %d/%d dependency ranges match the dump" % (dep_ok, len(deps)))

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

    # "a game menu is open" getters (menu.cpp pause hint + FG pause reason, 1.7.13): called at run time, not patched
    gm_bytes = {
        0x5A37D4: "40 53 48 83 EC 20 48 8B 05 C7 4C 02 04 48 8B 98 70 01 00 00 48 85 DB 74 1D 48 8B 03 48 8B CB FF 90 00 01 00 00 "
                  "84 C0 74 0D 48 8B 03 48 8B CB FF 50 48 84 C0 75 08 32 C0 48 83 C4 20 5B C3 B0 01 EB F6",
        0xD63C0: "48 8B 0D D9 27 68 04 48 85 C9 74 07 48 8B 01 48 FF 60 08 32 C0 C3",
    }
    gm_ok = all(img[a:a + len(h.split())] == bytes(int(x, 16) for x in h.split()) for a, h in gm_bytes.items())
    check(gm_ok, "menu: game 'menu open' getters 0x5A37D4 / 0xD63C0 differ")
    gm_ok &= check(0x5A37DA + 7 + struct.unpack_from("<i", img, 0x5A37DA + 3)[0] == 0x45C84A8, "menu: 0x5A37D4 reads [0x45C84A8]")
    gm_ok &= check(0xD63C0 + 7 + struct.unpack_from("<i", img, 0xD63C0 + 3)[0] == 0x4758BA0, "menu: 0xD63C0 reads [0x4758BA0]")
    print("menu: game 'menu open' getters 5A37D4 / D63C0 %s" % ("OK" if gm_ok else "DIFFER"))

    # frame generation (menu.cpp FG item / osd.cpp FG status) - called / read at run time, not patched
    fg_bytes = {
        0x1FDDFCC: "40 53 48 83 EC 20 8B D9 E8 CB 5F 00 00 33 C9 84 C0 74 3E 89 1D 23 D0 5D 02 83 EB 01 74 20 83 FB 02 74 09 "
                   "48 89 0D 1A D0 5D 02 EB 2B C7 05 12 D0 5D 02 01 00 00 00 89 0D 08 D0 5D 02 EB 19 48 C7 05 FB CF 5D 02 01 "
                   "00 00 00 89 0D 21 CF 5D 02 EB 06 89 0D E5 CF 5D 02 48 83 C4 20 5B C3",  # FrameGenSettingWrite
        0x1FE3FA4: "48 83 EC 28 83 E9 01 74 16 83 E9 01 74 0D 83 F9 01 75 08 8A 05 ED FE 5D 02 EB 0F B0 01 EB 0B E8 C0 4E "
                   "6D FE 8A 80 F0 00 00 00 48 83 C4 28 C3",  # IsFrameGenTypeAvailable
        0x12A46AD: "C6 05 CC 28 32 03 01",  # SetOptionValue: g_ConfigDirty = 1
    }
    fg_ok = all(img[a:a + len(hx.split())] == bytes(int(x, 16) for x in hx.split()) for a, hx in fg_bytes.items())
    check(fg_ok, "fg: FrameGenSettingWrite / IsFrameGenTypeAvailable / dirty-store bytes differ")
    rip = lambda at, ln, disp_at: at + ln + struct.unpack_from("<i", img, at + disp_at)[0]
    fg_refs = [
        (rip(0x1FDDFDF, 6, 2), 0x45BB008, "FrameGenSettingWrite stores FRAME_GENERATION"),
        (rip(0x1FE3FB7, 6, 2), 0x45C3EAA, "IsFrameGenTypeAvailable reads g_XessFgAvailable"),
        (rip(0x1FE3FC3, 5, 1), 0x6B8E88, "IsFrameGenTypeAvailable calls SlGetFeatureSupport"),
        (rip(0x6B8EAC, 7, 3), 0x46D7690, "SlGetFeatureSupport returns the caps static"),
        (rip(0x12A46AD, 7, 2), 0x45C6F80, "dirty store -> g_ConfigDirty"),
        (rip(0x12A24C4, 5, 1), 0x1FDDFCC, "SetOptionValue(0x126) calls FrameGenSettingWrite"),
        (rip(0x135030D, 6, 2), 0x45BB008, "display watcher compares FRAME_GENERATION"),
        (rip(0x2838930, 7, 2), 0x45C6F80, "SaveConfigIfDirty tests g_ConfigDirty"),
        (rip(0x283894E, 5, 1), 0x1FE56A8, "SaveConfigIfDirty calls SaveConfig"),
    ]
    for got, want, what in fg_refs:
        fg_ok &= check(got == want, "fg: %s: 0x%X != 0x%X" % (what, got, want))
    ib = struct.unpack_from("<Q", img, pe + 24 + 24)[0]

    def cstr(va):
        a = va - ib
        return img[a:img.index(b"\0", a)].decode("latin1")

    def desc(idx):
        d = 0x45C8570 + idx * 0x30
        name, vals = struct.unpack_from("<QQ", img, d)
        cnt = struct.unpack_from("<i", img, d + 0x24)[0]
        return cstr(name), [cstr(struct.unpack_from("<Q", img, vals - ib + 8 * i)[0]) for i in range(cnt)]

    fg_ok &= check(desc(54) == ("FRAME_GENERATION", ["OFF", "DLSS", "FSR", "XeSS"]), "fg: descriptor 54 = %s" % (desc(54),))
    fg_ok &= check(desc(50) == ("RESOLUTION_SCALING", ["OFF", "DLSS", "FSR", "XESS"]), "fg: descriptor 50 = %s" % (desc(50),))
    cfg_path = img[0x4B736F0:0x4B736F0 + 520].decode("utf-16-le", "replace").split("\0")[0]
    fg_ok &= check(cfg_path.lower().endswith("\\koeitecmo\\nioh3\\config.xml"), "fg: config path string = %r" % cfg_path)
    print("fg: setter/gate/dirty bytes, %d references, descriptors 50/54, config path %s" % (len(fg_refs), "OK" if fg_ok else "DIFFER"))

    # GP (1.7.0): the engine's marker token handlers (KtglTokenHandlerTable 3C02220, 8-byte pointers) - the mod checks
    # the same entries at run time before it switches the markers on
    gp_ok = True
    for tok, rva, nm in ((0xA5, 0x114024, "SynTokBeginEvent"), (0xA6, 0x113FC8, "SynTokEndEvent"),
                         (0xA0, 0xA96D70, "SynTokSetMarkerReplay"), (0x8E, 0xA96D90, "SynTokSetMarkerIface")):
        v = struct.unpack_from("<Q", img, 0x3C02220 + tok * 8)[0]
        gp_ok &= check(v == ib + rva, "gp: token table[%02X] = 0x%X, expected base+0x%X (%s)" % (tok, v, rva, nm))
    gp_ok &= check(img[0x11409A:0x11409F] == bytes.fromhex("E8C5FDFFFF"), "gp: SynTokBeginEvent does not call SynBeginEvent 113E64")
    print("gp: marker token handlers A5/A6/A0/8E and SynTokBeginEvent -> SynBeginEvent %s" % ("OK" if gp_ok else "DIFFER"))

    near = []
    for p in NIOH3FIX_PATTERNS:
        for h in find_all(img, secs, p, limit=50):
            plen = len(p.split())
            for (r0, r1, sid) in ranges:
                if h < r1 + 32 and r0 < h + plen + 32:
                    near.append("%s @0x%X near %s" % (p[:23], h, sid))
    print("Nioh3Fix 0.0.6 patterns within 32 bytes of our sites: %s" % ("; ".join(near) if near else "none"))

    # 1.7.3: every Target enum entry needs a written trampoline (1.7.2 SPR hung because kLastTarget stopped at caveSPF)
    hdr = (ROOT / "src" / "patches_gen.h").read_text(encoding="utf-8")
    m = re.search(r"enum class Target : uint8_t \{([^}]*)\}", hdr)
    ents = [e.strip() for e in m.group(1).split(",") if e.strip()] if m else []
    m2 = re.search(r"kLastTarget = \(int\)gen::Target::(\w+)", (ROOT / "src" / "dllmain.cpp").read_text(encoding="utf-8"))
    check(bool(ents) and bool(m2) and m2.group(1) == ents[-1], "kLastTarget (%s) is not the last Target enum entry (%s)" % (m2 and m2.group(1), ents and ents[-1]))
    # 1.7.3: no branch may land inside the bytes a call-kind site overwrites (SPR: SPg, SPu); scanned over the +-0x400 window
    for st in doc["sites"]:
        if st.get("kind") != "call" or st.get("group") != "SPR":
            continue
        rva = int(st["rva"], 16); ln = st["len"]; lo, hi = rva + 1, rva + ln
        base = rva - 0x400
        # rva -> file offset via exec sections is not needed: patterns are resolved in the dump image by find_all
        offs = find_all(img, secs, st["pattern"].replace(" ", " "))
        hit = [o for o in offs if True]
        check(len(hit) == 1, "%s: pattern not unique for branch scan" % st["id"])
        if len(hit) != 1:
            continue
        a = hit[0][1] if isinstance(hit[0], tuple) else hit[0]
        va_of = lambda fo: fo  # offsets below are relative to the match
        # find_all returns (rva, fileoff) style results in this file; derive file offset of RVA via the match
        site_fo = a + st["offset"]
        bad = []
        for d in range(-0x400, 0x400):
            fo = site_fo + d
            if fo < 0 or fo + 6 >= len(img):
                continue
            b0 = img[fo]; tgt = None
            if b0 in (0xE8, 0xE9):
                tgt = d + 5 + struct.unpack_from("<i", img, fo + 1)[0]
            elif b0 == 0x0F and 0x80 <= img[fo + 1] <= 0x8F:
                tgt = d + 6 + struct.unpack_from("<i", img, fo + 2)[0]
            elif 0x70 <= b0 <= 0x7F or b0 == 0xEB:
                tgt = d + 2 + struct.unpack_from("<b", img, fo + 1)[0]
            if tgt is not None and 1 <= tgt < ln and d != 0:
                bad.append("%+d->%+d" % (d, tgt))
        check(not bad, "%s: branch(es) land inside the overwritten range: %s" % (st["id"], bad))
    print("SPR branch-into-range scan and trampoline coverage checked")

    # 1.7.14: GL1 / LC / LW. Site starts are RVAs and the dump is laid out raw == RVA, so the dump is scanned directly.
    def branch_targets_into(rva, ln, win=0x400):
        """near jumps/calls/jcc (rel8/rel32 forms) in [rva-win, rva+win) that land strictly inside (rva, rva+ln)"""
        bad = []
        for fo in range(rva - win, rva + win):
            b0 = img[fo]
            tgt = None
            if b0 in (0xE8, 0xE9):
                tgt = fo + 5 + struct.unpack_from("<i", img, fo + 1)[0]
            elif b0 == 0x0F and 0x80 <= img[fo + 1] <= 0x8F:
                tgt = fo + 6 + struct.unpack_from("<i", img, fo + 2)[0]
            elif 0x70 <= b0 <= 0x7F or b0 == 0xEB:
                tgt = fo + 2 + struct.unpack_from("<b", img, fo + 1)[0]
            if tgt is not None and rva < tgt < rva + ln and fo != rva:
                bad.append("%X->%X" % (fo, tgt))
        return bad
    new_sites = [st for st in doc["sites"] if st["group"] in ("GL1", "LC", "LW")]
    check(len(new_sites) == 8, "1.7.14: expected 8 GL1/LC/LW sites, found %d" % len(new_sites))
    # raw byte scan (no disassembler in stdlib): these two hits are mid-instruction byte patterns, not branches - a linear
    # capstone sweep of E266B0 / E26680 / 1FEE28 / 1FF320 / 1343D00 finds no branch into any of the 8 sites
    not_branches = {"E2666B->E26691", "1FEECE->1FEE6C"}
    for st in new_sites:
        rva, ln = int(st["rva"], 16), st["len"]
        bad = [x for x in branch_targets_into(rva, ln) if x not in not_branches]
        check(not bad, "%s: branch(es) land inside the overwritten range: %s" % (st["id"], bad))
    # 1.7.32 DQ / P1E7 (drawprep.cpp): the byte arrays and RVAs the module checks at run time must equal the dump, the
    # patches.json sites and deps; no branch may land inside either site; the DQ dependency must leave DS's site alone.
    dsrc = (ROOT / "src" / "drawprep.cpp").read_text(encoding="utf-8")
    darr = {m.group(1): bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(2)))
            for m in re.finditer(r"const uint8_t (k\w+)\[\] = \{([^}]*)\};", dsrc)}
    dval = {k: int(v, 16) for k, v in re.findall(r"\b(k\w+) = 0x([0-9A-Fa-f]+)", dsrc)}
    dchk = [("kDqOrig", "kDqSite"), ("kDqXmm6", "kDqXmm6Rva"), ("kDqHead", "kDqLoop"), ("kDqBody", "kDqBodyRva"),
            ("kDqTail", "kDqExit"), ("kPfOrig", "kPfSite"), ("kPfLoop", "kPfLoopRva")]
    for arr, rv in dchk:
        ok = check(arr in darr and rv in dval, "drawprep.cpp: %s / %s not found" % (arr, rv))
        if ok:
            a = dval[rv]
            check(img[a:a + len(darr[arr])] == darr[arr], "drawprep.cpp: %s @0x%X differs from the dump" % (arr, a))
    sdq, spf = by_id.get("DQ"), by_id.get("P1E7")
    check(bool(sdq) and bool(spf), "1.7.32: DQ / P1E7 sites missing in patches.json")
    if sdq and spf:
        check(int(sdq["rva"], 16) == dval.get("kDqSite") and int(spf["rva"], 16) == dval.get("kPfSite"),
              "drawprep.cpp site RVAs != patches.json")
        check(dval.get("kDqLoop", 0) + len(darr.get("kDqHead", b"")) == int(by_id["DS"]["rva"], 16),
              "DQ head check must stop at DS's site 1A74BDA")
        check(dval.get("kDqBodyRva", 0) == int(by_id["DS"]["rva"], 16) + by_id["DS"]["len"]
              and dval["kDqBodyRva"] + len(darr.get("kDqBody", b"")) == dval["kDqSite"], "DQ body check must span 1A74BE0..site")
        check(dval.get("kDqExit") == dval["kDqSite"] + 5 and dval.get("kPfResume") == dval["kPfSite"] + 11,
              "DQ exit / P1E7 resume must be the instruction after the site")
        check(img[dval["kDqExit"]] == 0xE9 and dval["kDqExit"] + 5 + struct.unpack_from("<i", img, dval["kDqExit"] + 1)[0] == 0x1A744F1,
              "DQ exit is not jmp 1A744F1")
        check(img[dval["kDqSite"] + 3] == 0x77 and dval["kDqSite"] + 5 + struct.unpack_from("<b", img, dval["kDqSite"] + 4)[0] == dval["kDqLoop"],
              "DQ site's ja does not go to the loop head")
        for st in (sdq, spf):
            bad = branch_targets_into(int(st["rva"], 16), st["len"])
            check(not bad, "%s: branch(es) land inside the overwritten range: %s" % (st["id"], bad))
    print("1.7.32 DQ / P1E7: drawprep.cpp byte checks, site RVAs, DS coexistence, branch scan checked")

    byid = {s["id"]: s for s in doc["sites"]}
    hb = bytes.fromhex
    # GL1: the cave re-implements `cmp rbx,[rbp|rdi]` + `je`; both loop exits and the register roles
    ga, gb = int(byid["GL1a"]["rva"], 16), int(byid["GL1b"]["rva"], 16)
    check(img[ga:ga + 6] == hb("483B5D000F84") and ga + 10 + struct.unpack_from("<i", img, ga + 6)[0] == 0xE2686F,
          "GL1a: not `cmp rbx,[rbp]; je E2686F`")
    check(img[gb:gb + 4] == hb("483B1F74") and gb + 5 + struct.unpack_from("<b", img, gb + 4)[0] == 0xE266A4,
          "GL1b: not `cmp rbx,[rdi]; je E266A4`")
    check(img[ga + 10:ga + 14] == hb("488B7B08") and img[gb + 5:gb + 9] == hb("488B4B08"),
          "GL1: resume points are not `mov rdi|rcx,[rbx+8]` (entry = {?, task*}, 16 bytes)")
    check(0xE26699 + 5 + struct.unpack_from("<i", img, 0xE2669A)[0] == 0x2866F0, "GL1b: loop does not call 2866F0")
    # LC: only the two immediates change (and r15,-9 -> -15; add r15,0Ah -> 10h; mov eax,0Ah -> 10h)
    oa, na = hb(byid["LCa"]["orig"].replace(" ", "")), hb(byid["LCa"]["new"].replace(" ", ""))
    ob, nb = hb(byid["LCb"]["orig"].replace(" ", "")), hb(byid["LCb"]["new"].replace(" ", ""))
    check([i for i in range(len(oa)) if oa[i] != na[i]] == [3, 11] and (oa[3], na[3], oa[11], na[11]) == (0xF7, 0xF1, 0x0A, 0x10),
          "LCa: must change exactly the and-mask imm8 (F7 -> F1) and the add imm8 (0A -> 10)")
    check([i for i in range(len(ob)) if ob[i] != nb[i]] == [1] and nb[1] == 0x10, "LCb: must change exactly mov eax,0Ah -> 10h")
    # cap arithmetic: ((-(f != 0)) & mask) + base -> f = 0: base, f != 0: must stay 1 (the boot-flag cap)
    check(((-1) & -15) + 16 == 1 and (0 & -15) + 16 == 16 and ((-1) & -9) + 10 == 1, "LC: cap arithmetic")
    # 1.7.30 LC = N (dllmain BuildRepl): LCa imm8 at +3 = (1 - N) & FF, imm8 at +11 = N; LCb imm32 low byte = N. Both imm8 are
    # sign-extended (83 /4 and 83 /0), so N must stay <= 127 and 1 - N >= -128; the boot flag must still give 1, flag 0 N.
    for n in range(11, 65):
        ra = bytearray(na); ra[3] = (1 - n) & 0xFF; ra[11] = n
        rb = bytearray(nb); rb[1] = n
        m = ra[3] - 256 if ra[3] >= 128 else ra[3]
        add = ra[11] - 256 if ra[11] >= 128 else ra[11]
        check(((-1) & m) + add == 1 and (0 & m) + add == n, "LC=%d: cap arithmetic" % n)
        check([i for i in range(len(oa)) if oa[i] != ra[i]] == [3, 11] and [i for i in range(len(ob)) if ob[i] != rb[i]] == [1]
              and struct.unpack_from("<I", bytes(rb), 1)[0] == n, "LC=%d: only the immediates may change" % n)
    na16 = bytearray(na); na16[3] = (1 - 16) & 0xFF; na16[11] = 16
    check(bytes(na16) == bytes(na), "LC: BuildRepl's formula for N = 16 must equal patches.json")
    # LW: the three Sleep sites run with ecx = 1; the signal site sits between the done-bit store and `cmp [rax+54h],0`
    for sid, pre in (("LWa", hb("8D4A01")), ("LWb", hb("418BCD")), ("LWc", hb("B901000000"))):
        a = int(byid[sid]["rva"], 16)
        check(img[a - len(pre):a] == pre, "%s: ecx = 1 setup before the Sleep call differs" % sid)
    ws = int(byid["LWs"]["rva"], 16)
    check(img[ws:ws + 8] == hb("834B2401488B4338"), "LWs: displaced instructions differ")
    check(img[ws - 8:ws] == hb("B801000000874334"), "LWs: not preceded by `mov eax,1 / xchg [rbx+34h],eax`")
    check(img[0x1343D00:0x1343D09] == hb("40534883EC20488BD9"),
          "LW: ResourceStepJob_Run prologue (push rbx / sub rsp,20h => rsp 16-aligned at the site) differs")
    print("1.7.14 GL1/LC/LW: %d sites, branch scan, loop exits, cap arithmetic, Sleep ecx = 1, job prologue checked" % len(new_sites))

    # 1.7.23 LO: the RWb switchable cave (src/loadonly.cpp kRwbCave) behaves exactly like the stock order with its word = 0
    # and exactly like the RWb rewrite with word = 1, for every value of bl (small x86 interpreter over the real bytes)
    lo_ok = lo_sim(img, by_id, iat, check)
    print("LO: RWb switchable cave == stock (word 0) and == RWb (word 1) for all 256 bl values: %s" % ("OK" if lo_ok else "FAIL"))

    # 1.7.39 MA: main-thread atomic-load idiom `xor eax,eax; lock cmpxchg [m],rZ` (rZ == 0) -> `mov eax,[m]` + NOP.
    # Mechanical check of every site: orig is lock cmpxchg r/m32 with the expected zero register in modrm.reg, new is
    # mov eax,<the same memory operand> (REX.R dropped, reg field 0, same SIB/disp) followed by a standard NOP of the
    # remaining length. That the register IS zero on every path to the site is proven by re/scripts_cpu/ma_zero.fsx
    # (Iced reaching-definition over all chained pieces of 4AA34 / 4B104 / 419A8 / 1E80B0); the deps pin those bodies.
    nops = {1: b"\x90", 2: b"\x66\x90", 3: b"\x0F\x1F\x00", 4: b"\x0F\x1F\x40\x00"}
    zero_reg = {"MA1": 10, "MA2": 5, "MA3": 2}  # r10d, ebp, edx
    ma = [s for s in doc["sites"] if s["group"] == "MA"]
    ma_ok = check(len(ma) == 25, "MA: expected 25 sites, found %d" % len(ma))
    for s in ma:
        o = bytes(int(x, 16) for x in s["orig"].split())
        n = bytes(int(x, 16) for x in s["new"].split())
        i, rex = 1, 0
        good = o[0] == 0xF0
        if 0x40 <= o[1] <= 0x4F:
            rex, i = o[1], 2
        good &= o[i:i + 2] == b"\x0F\xB1" and not (rex & 8)  # 32-bit operand size
        modrm = o[i + 2]
        good &= (modrm >> 6) != 3  # memory operand
        reg = ((modrm >> 3) & 7) | (8 if rex & 4 else 0)
        good &= reg == zero_reg[s["id"][:3]]
        mem = o[i + 3:]  # SIB + disp, unchanged
        nrex = rex & ~4
        want = (bytes([nrex]) if nrex not in (0, 0x40) else b"") + b"\x8B" + bytes([modrm & 0xC7]) + mem
        pad = len(o) - len(want)
        good &= pad in nops and n == want + nops[pad]
        ma_ok &= check(good, "MA %s: replacement is not mov eax,<same mem> + NOP of the lock cmpxchg with the zero register" % s["id"])
    print("1.7.39 MA: %d sites = lock cmpxchg [m],<zero reg> -> mov eax,[m] + NOP (zero-source proof: re/scripts_cpu/ma_zero.fsx): %s"
          % (len(ma), "OK" if ma_ok else "FAIL"))

    # 1.7.41: the boss rematch / BossLog / RematchLog / InvLog sites moved to the add-on mod\Nioh3BossRematch (its own
    # patch-resilient site table and tests\test_sites.py); no BL / RM group is left in patches.json.
    check(not any(st["group"] in ("BL", "RM") for st in doc["sites"]), "BL / RM sites belong to Nioh3BossRematch now")
    check(not (ROOT / "src" / "rematch.cpp").exists() and not (ROOT / "src" / "bosslog.cpp").exists(), "boss code is in the add-on")
    # 1.7.40 JT (job-timing probe, measurement only; src/jt.asm). JTc replaces two instructions (`mov rcx,rbx` | `call rdx`):
    # no branch may land on 40E51, and the call must return to `mov rsi,rax` (the job's result). JTw / JTx sit inside the STL
    # condition_variable win7 wait / wait_for, JTn1 / JTn2 are its notify_one / notify_all tail jumps (`48 FF 25` jmp [IAT]);
    # all four functions are slots 0-3 of the vtable at 3942338.
    jt = [s for s in doc["sites"] if s["group"] == "JT"]
    jt_ok = check(len(jt) == 5, "JT: expected 5 sites, found %d" % len(jt))
    for s in jt:
        rva, ln = int(s["rva"], 16), s["len"]
        o = bytes(int(x, 16) for x in s["orig"].split())
        if "iat_jmp_name" in s:
            at = int(s["iat_jmp_at"])
            slot = rva + at + 4 + struct.unpack_from("<i", o, at)[0]
            jt_ok &= check(o[at - 3:at] == hb("48FF25") and iat.get(slot, "").upper() == s["iat_jmp_name"].upper(),
                           "%s: jmp [rip+disp] -> 0x%X = %s, expected %s" % (s["id"], slot, iat.get(slot), s["iat_jmp_name"]))
        bad = branch_targets_into(rva, ln)
        jt_ok &= check(not bad, "%s: branch(es) land inside the overwritten range: %s" % (s["id"], bad))
    jt_ok &= check(img[0x40E4E:0x40E56] == hb("488BCBFFD2488BF0"), "JTc: not `mov rcx,rbx; call rdx; mov rsi,rax`")
    vt = 0x3942338
    for k, (fn, site) in enumerate(((0xB616A4, "JTw"), (0xB616E8, "JTx"), (0xB61670, "JTn1"), (0xB61658, "JTn2"))):
        st = by_id.get(site)
        jt_ok &= check(struct.unpack_from("<Q", img, vt + 8 * k)[0] == ib0 + fn and st is not None
                       and fn <= int(st["rva"], 16) < fn + 0x20, "JT: vtable 3942338 slot %d is not %X containing %s" % (k, fn, site))
    jt_ok &= check(img[0xB61670:0xB61674] == hb("4883C108") and img[0xB61658:0xB6165C] == hb("4883C108"),
                   "JT: notify_one / notify_all do not start with add rcx,8 (the CV the wait cave records)")
    print("1.7.40 JT: 5 sites, IAT jmp names, branch scan, JTc return to mov rsi,rax, STL cv vtable slots 0-3: %s" % ("OK" if jt_ok else "FAIL"))

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
