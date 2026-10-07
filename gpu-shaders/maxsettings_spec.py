r"""maxsettings_spec.py - offline spec + validator for the MAX-settings GPU work (notes\gpu-maxsettings.md).

    py -I gpu-shaders\maxsettings_spec.py            -> prints the tables, checks, cost model; writes maxsettings_patches.json

Reads only re\Nioh3.dump.exe (Steam v2.02, raw offset == RVA) and the r5 RGP segment table. Touches nothing in the game.

What it does
1. Verifies the exact v2.02 bytes of every site the patch designs touch (refuses on mismatch).
2. Re-derives the quality tables FROM THE CODE BYTES (RIP-relative operands are decoded from the instructions, not
   hard-coded): GiScaleLookup (0x187A50), ApplyShadowQuality table (0x3978210), Landscape tables, motion-blur / SSR /
   AO / volumetric immediates.
3. Emulates GiScaleLookup exactly (the stack table, the {0,2,1,0} row map, the GI == -1 fallback) and checks that
   the predicted RefProbe dispatch grids at GI HIGH match the r5 trace (144x81, 120x68, 84x48, 144x81).
4. Builds the replacement lookup table for the proposed caveGiScale (ASI design, notes §5.1) and proves it returns the
   original value for every (which, GI) cell that is not overridden, for every preset.
5. Prints the cost model (r5 PC us per dispatch scaled by pixels; Ally = x6..x8) and writes the JSON patch spec for the
   ASI agent (gpu-shaders\maxsettings_patches.json).
"""
import csv, json, os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
EXE = os.path.join(ROOT, "re", "Nioh3.dump.exe")
SEG = os.path.join(ROOT, "captures", "rgp", "Nioh3_2026-10-06_19-39-58.segments.csv")
b = open(EXE, "rb").read()
fail = 0


def check(cond, msg):
    global fail
    print(("  ok    " if cond else "  FAIL  ") + msg)
    if not cond:
        fail += 1


def rip(ins_rva, ins_len, disp_off):
    """target of a RIP-relative operand: disp32 at ins_rva+disp_off, next ip = ins_rva+ins_len"""
    d, = struct.unpack_from("<i", b, ins_rva + disp_off)
    return ins_rva + ins_len + d


def f32(a, n=1):
    return list(struct.unpack_from("<%df" % n, b, a))


def i32(a, n=1):
    return list(struct.unpack_from("<%di" % n, b, a))


# ------------------------------------------------------------------ 1. site bytes (v2.02)
SITES = {
    # GiScaleLookup: whole function (leaf, /GS cookie, returns xmm0). Callers: 4x in GiRenderScalesApply 0x1872C4.
    "GiScaleLookup": (0x187A50, "4883ec78488b05d50433044833c448894424600f2805e6cba4030f280dcfcba4034863c10f294424100f2805b0cba403"
                                "0f29442430f30f10052bc7a4030f294c24200f280d87cba403f30f11442450660f6f0569cba403f30f7f042448630c84"
                                "0f294c2440f30f100d23c7a403f30f114c2454488d0449488d4c2410488d0cc1486305b134430483f8ff7417f30f1004"
                                "81488b4c24604833cce8f297a0004883c478c3f30f104108ebe7"),
    # ApplyShadowQuality: clamp to [1,5], lea table, shl 6
    "ApplyShadowQuality_table": (0x18A890, "b8050000008bfa488bf13bd07f0cb9010000008bc23bd10f4cc14898488d0d5dd97e0348c1e006"),
    "ApplyShadowQuality_flag7598": (0x18A98A, "488b0df7fb4204"  "4885c9740e807c246c000f94c0888198750000"),
    # Motion blur samples per MOTION_BLUR index (HIGHEST=10, HIGH=8, STANDARD=6, LIGHT=4, LIGHTEST=2)
    "MotionBlur_HIGHEST": (0x39B4CD, "4489818c180000c781981800000a000000"),
    "MotionBlur_HIGH": (0x39B4E0, "c7818c18000001000000c7819818000008000000"),
    # SSR (ApplySetting23 = ApplySsrQuality): HIGHEST x2, HIGH x1.5 of S+0x107C; S+0xB8 of res 0xEF3D849D = 0 for HIGH+
    "Ssr_HIGHEST_x2": (0x39B146, "8b837c10000003c089837c100000"),
    "Ssr_HIGH_x15": (0x39B156, "6b8b7c1000000fb867666666f7e9c1fa02"),
    # AO: HIGHEST -> 14 directions if render height >= 721 (0x2D1) else 7 (+ flag)
    "Ao_HIGHEST": (0x39B20C, "4053"),
    # Volumetric: HIGH = x1.5 capped 160/64
    "Volumetric_HIGH": (0x1995C84, "83f9010f855154a0feb90200000041bb67666666"),
    # Landscape: LOD scale / far distance tables indexed by LANDSCAPE_QUALITY in ApplyGraphicsSettings
    "Landscape_tables": (0x39A74A, "4863055b082204"),
}
print("== 1. v2.02 site bytes")
for k, (rva, hx) in SITES.items():
    exp = bytes.fromhex(hx)
    check(b[rva:rva + len(exp)] == exp, "%-28s @0x%07X  %d bytes" % (k, rva, len(exp)))

# ------------------------------------------------------------------ 2. tables derived from code
print("\n== 2. tables (derived from the instruction operands)")
F = 0x187A50
c_row0a = rip(F + 0x13, 7, 3)    # movaps xmm0,[rip] -> [rsp+10h]
c_row0b = rip(F + 0x1A, 7, 3)    # movaps xmm1,[rip] -> [rsp+20h]
c_row1b = rip(F + 0x29, 7, 3)    # movaps xmm0,[rip] -> [rsp+30h]
c_r2s4 = rip(F + 0x35, 8, 4)     # movss  xmm0,[rip] -> [rsp+50h]
c_row2a = rip(F + 0x42, 7, 3)    # movaps xmm1,[rip] -> [rsp+40h]
c_map = rip(F + 0x4F, 8, 4)      # movdqa xmm0,[rip] -> [rsp] (row map)
c_r2s5 = rip(F + 0x65, 8, 4)     # movss  xmm1,[rip] -> [rsp+54h]
g_gi = rip(F + 0x80, 7, 3)       # movsxd rax,[rip] -> g_CfgGi
check(g_gi == 0x45BAF88, "GiScaleLookup reads g_CfgGi 0x45BAF88 (got 0x%X)" % g_gi)
stack = f32(c_row0a, 4) + f32(c_row0b, 4) + f32(c_row1b, 4) + f32(c_row2a, 4) + f32(c_r2s4) + f32(c_r2s5)
rows = [stack[0:6], stack[6:12], stack[12:18]]
rowmap = i32(c_map, 4)
print("  constants: row0a 0x%X row0b 0x%X row1b 0x%X row2a 0x%X s4 0x%X s5 0x%X map 0x%X" %
      (c_row0a, c_row0b, c_row1b, c_row2a, c_r2s4, c_r2s5, c_map))
GI_NAMES = ["LIGHTEST", "LIGHT", "STANDARD", "HIGH", "VERY_HIGH", "HIGHEST"]
WHICH = ["IndirectRenderScale", "ReflectionRenderScale", "IndirectRenderScaleForOcn", "ReflectionRenderScaleForOcn"]


def gi_lookup(which, gi):
    """exact model of 0x187A50: row = map[which]; GI == -1 -> column 2; else column GI (no clamp)"""
    row = rows[rowmap[which]]
    return row[2] if gi == -1 else row[gi]


check(rowmap == [0, 2, 1, 0], "row map {0,2,1,0} (got %s)" % rowmap)
print("  %-28s %s" % ("which \\ GI", " ".join("%9s" % n for n in GI_NAMES)))
GI_TABLE = []
for w in range(4):
    GI_TABLE.append([gi_lookup(w, g) for g in range(6)])
    print("  %-28s %s" % (WHICH[w], " ".join("%9.4g" % v for v in GI_TABLE[-1])))

sh = 0x3978210
print("  ApplyShadowQuality table 0x%X (64 B rows, index = clamp(SHADOW_QUAL,1,5)):" % sh)
SH = ["OFF", "LIGHTEST", "LIGHT", "STANDARD", "HIGH", "HIGHEST"]
for q in range(1, 6):
    r = sh + q * 0x40
    v = i32(r, 16)
    print("   %-9s +0:%-3d +4:%-5d +8:%-5d +C:%-5d +10:%-5d +14:%-2d +18:%d  +34:%-5d +38:%-6g  +3C bytes %s" %
          (SH[q], v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[13], f32(r + 0x38)[0], b[r + 0x3C:r + 0x40].hex()))
hi, hx = sh + 4 * 0x40, sh + 5 * 0x40
diff = [o for o in range(0, 0x40, 4) if b[hi + o:hi + o + 4] != b[hx + o:hx + o + 4]]
print("   HIGH vs HIGHEST differ at row offsets: %s" % ", ".join("+0x%X" % o for o in diff))
check(diff == [0x38, 0x3C], "HIGH->HIGHEST shadow row differs only in +0x38 (f32 2000/3000/7000) and +0x3C (flag byte)")
print("  LandscapeLodScaleTable 0x3977A58 %s   LandscapeLodParamTable 0x3978390 %s" %
      (["%.3g" % x for x in f32(0x3977A58, 5)], ["%g" % x for x in f32(0x3978390, 5)]))
print("  LandscapeUltraFlagTable 0x3976CA4 %s (read by 0x42B368: ULTRA clears terrain obj +0x4A4)" % list(b[0x3976CA4:0x3976CA9]))
mb = {}
for n, off in (("HIGHEST", 0x39B4D4), ("HIGH", 0x39B4EA), ("STANDARD", 0x39B500), ("LIGHT", 0x39B516), ("LIGHTEST", 0x39B52C)):
    mb[n] = struct.unpack_from("<i", b, off + 6)[0]
print("  MotionBlur samples (S+0x1898): %s" % mb)
check(mb == {"HIGHEST": 10, "HIGH": 8, "STANDARD": 6, "LIGHT": 4, "LIGHTEST": 2}, "motion-blur sample immediates")

# ------------------------------------------------------------------ 3. trace cross-check
print("\n== 3. RefProbe dispatch grid vs the r5 trace (render 1920x1080, GI HIGH)")
W, H = 1920, 1080


def grid(s, w=W, h=H):
    return (-(-int(round(w * s)) // 8), -(-int(round(h * s)) // 8))


r5 = {0: (144, 81), 1: (120, 68), 2: (84, 48), 3: (144, 81)}   # #121 main, #414 main, #121 water, #414 water
for w in range(4):
    g = grid(GI_TABLE[w][3])
    check(g == r5[w], "%-28s HIGH scale %.2f -> grid %dx%d (r5 %dx%d)" % (WHICH[w], GI_TABLE[w][3], g[0], g[1], *r5[w]))
segs = {}
if os.path.exists(SEG):
    for r in csv.DictReader(open(SEG, newline="")):
        segs[int(r["seg"])] = float(r["dur_us"])
# r5 RefProbe dispatch times (segments 228/229 #121 main, 230/231 #414 main, 283+304 #121 water, 284 #414 water)
T = {0: segs.get(228, 35.5) + segs.get(229, 302.1), 1: segs.get(230, 57.3) + segs.get(231, 259.8),
     2: segs.get(283, 23.9) + segs.get(304, 5.6), 3: segs.get(284, 23.1)}
print("  r5 us at HIGH: " + ", ".join("%s %.1f" % (WHICH[w], T[w]) for w in range(4)))

# ------------------------------------------------------------------ 4. cost model + presets
print("\n== 4. RefProbe cost by GI level / preset (PC us = r5 x pixel ratio; Ally ms = PC x 6..8)")


def cost(scales):
    return sum(T[w] * (scales[w] / GI_TABLE[w][3]) ** 2 for w in range(4))


for g in range(2, 6):
    c = cost([GI_TABLE[w][g] for w in range(4)])
    print("  GI %-9s %7.0f us PC   Ally %5.1f-%5.1f ms" % (GI_NAMES[g], c, c * 6e-3, c * 8e-3))
PRESETS = {
    "stock": {},
    "P1_water_views_at_HIGH": {2: GI_TABLE[2][3], 3: GI_TABLE[3][3]},
    "P2_diffuse_0.75": {0: 0.75},
    "P3_reflection_0.75": {1: 0.75},
    "P1+P2": {0: 0.75, 2: GI_TABLE[2][3], 3: GI_TABLE[3][3]},
    "P1+P2+P3": {0: 0.75, 1: 0.75, 2: GI_TABLE[2][3], 3: GI_TABLE[3][3]},
}
base = cost([GI_TABLE[w][5] for w in range(4)])
out_presets = {}
for name, ov in PRESETS.items():
    tab = [row[:] for row in GI_TABLE]
    for w, v in ov.items():
        tab[w][5] = v
    # proof: every non-overridden cell equals the original lookup, for GI in -1..5
    ok = all((tab[w][g] if g >= 0 else tab[w][2]) == gi_lookup(w, g)
             for w in range(4) for g in range(-1, 6) if not (g == 5 and w in ov))
    c = cost([tab[w][5] for w in range(4)])
    check(ok, "%-24s HIGHEST %6.0f us PC (saves %5.0f us; Ally %4.1f-%4.1f ms)  table identical elsewhere" %
          (name, c, base - c, (base - c) * 6e-3, (base - c) * 8e-3))
    out_presets[name] = {"highest_column": [tab[w][5] for w in range(4)], "pc_us": round(c, 1),
                         "saves_pc_us": round(base - c, 1)}

# ------------------------------------------------------------------ 5. JSON spec for the ASI agent
import re
AOB_GI = "48 83 EC 78 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 60 0F 28 05 ?? ?? ?? ?? 0F 28 0D ?? ?? ?? ?? 48 63 C1"
rx = b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in AOB_GI.split())
hits = [m.start() for m in re.finditer(rx, b, re.S)]
check(hits == [0x187A50], "GI1 AOB is unique in the image (hits %s)" % [hex(h) for h in hits])
spec = {
    "game": "Nioh 3 Steam v2.02 (PE timestamp 0x6AA7C106)",
    "doc": "notes/gpu-maxsettings.md §5",
    "patches": [
        {"id": "GI1", "name": "caveGiScale (GI HIGHEST render-scale override)",
         "site_rva": "0x187A50", "kind": "replace function entry with jmp to cave (5 B over 'sub rsp,78h' + 1 B)",
         "orig_bytes": SITES["GiScaleLookup"][1][:32],
         "aob": "48 83 EC 78 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 44 24 60 0F 28 05 ?? ?? ?? ?? 0F 28 0D ?? ?? ?? ?? 48 63 C1",
         "callers": ["0x18745A", "0x187464", "0x187471", "0x18747F (GiRenderScalesApply 0x1872C4, every frame)"],
         "contract": "in ecx = which (0..3); out xmm0 = scale; may clobber rax rcx rdx r8-r11 xmm1-xmm5 (leaf, Win64 volatile)",
         "default_table_rows_by_which": GI_TABLE, "rowmap": rowmap, "gi_slot": "0x45BAF88",
         "presets": out_presets, "risk": "visual only (render scale of the GI gather); live, takes effect next frame"},
        {"id": "MB1", "name": "MOTION_BLUR HIGHEST samples 10 -> 8 (== HIGH)", "site_rva": "0x39B4DA",
         "orig_bytes": "0a", "new_bytes": "08", "note": "pointless as a patch: choose HIGH in the menu; listed for completeness"},
    ],
}
dst = os.path.join(HERE, "maxsettings_patches.json")
json.dump(spec, open(dst, "w"), indent=1)
print("\nwrote", dst)
print("RESULT:", "ALL PASSED" if not fail else "%d FAILED" % fail)
sys.exit(1 if fail else 0)
