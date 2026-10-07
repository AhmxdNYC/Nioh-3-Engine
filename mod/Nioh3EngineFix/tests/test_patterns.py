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
