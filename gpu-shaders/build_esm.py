r"""build_esm.py - STAGE (not install) a replacement shader_dx12\0x1ca8bb39.file (group "ESM") whose ten separable
Gaussian blur pixel shaders (records #2..#6 vertical R=1..5, #7..#11 horizontal R=1..5) are rebuilt from
ESM_fast.hlsl (R+1 gathers instead of 2R+1 samples). Records #0 (VS) and #1 (ESM generation) stay byte-identical.

Reads the game file read-only; on first run copies it to backup\0x1ca8bb39.file. Writes only inside gpu-shaders\.

    py -I build_esm.py        -> replacement\0x1ca8bb39.file  (+ build\esm\*.dxil, verification printout)

Record layout (graphics group, same as WDO): "CXDK2000" | u32 dxbcSize | u32 tailSize | DXBC | tail, padded to 4;
table entry (u32 offset, u32 recordSize). The 96-byte tail is identical for all ten PS records (interface metadata:
signature names + sampler name), so it is kept unchanged.
"""
import os, shutil, struct, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import idrk

GAME_FILE = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12\0x1ca8bb39.file"
DXC = os.path.join(HERE, "..", "tools", "rdts", "RadeonDeveloperToolSuite-2026-05-28-1806", "utils", "dx12", "dxc", "dxc.exe")
NAME = "0x1ca8bb39.file"
# record -> (R, VERT, original container digest)
RECS = {
    2: (1, 1, "13aeacb0b8d05df6ae3c28e5d09a4c5b"), 3: (2, 1, "ecb66f6b57d61e8948848ad9a9b75d95"),
    4: (3, 1, "ce1d238555b08f0b680df8598fe31c2c"), 5: (4, 1, "2bcaa2facf21865ff2d0865616cf0d1d"),
    6: (5, 1, "416075dc65d519d365fb634b9c0010b1"), 7: (1, 0, "f9934b1c6743e745d30c07674def119e"),
    8: (2, 0, "cb5b8d6fb506dc4133ddb75693d62338"), 9: (3, 0, "787b3b902f5bb7ea1ebfb76fbb03ed50"),
    10: (4, 0, "5085227104a5878b7616d6307004b38c"), 11: (5, 0, "f73672ad5962318f27db88d500e127d6"),
}
BUILD = os.path.join(HERE, "build", "esm"); os.makedirs(BUILD, exist_ok=True)
os.makedirs(os.path.join(HERE, "backup"), exist_ok=True)
os.makedirs(os.path.join(HERE, "replacement"), exist_ok=True)

bak = os.path.join(HERE, "backup", NAME)
if not os.path.exists(bak):
    shutil.copy2(GAME_FILE, bak)
orig = open(bak, "rb").read()
payload = idrk.unpack(orig)
assert payload[:8] == b"_S2G7000" and idrk.group_name(payload) == "ESM"

total, = struct.unpack_from("<I", payload, 0x08)
tab, = struct.unpack_from("<I", payload, 0x98)
base, blobs = struct.unpack_from("<II", payload, 0xB0)
assert total == len(payload) and base + blobs == len(payload)
ents = []
while tab + 8 * len(ents) + 8 <= base:
    o, s = struct.unpack_from("<II", payload, tab + 8 * len(ents))
    if base + o + 8 > len(payload) or payload[base + o:base + o + 8] != b"CXDK2000":
        break
    ents.append((o, s))
assert len(ents) == 12, len(ents)


def rec_parts(buf, o):
    dsz, tsz = struct.unpack_from("<II", buf, base + o + 8)
    dx = base + o + 16
    return dx, dsz, tsz


def dumpbin_iface(path):
    """signature + resource-binding header lines of `dxc -dumpbin` (the shader hash line excluded)."""
    t = subprocess.run([DXC, "-dumpbin", path], capture_output=True, text=True).stdout.splitlines()
    out = []
    for l in t:
        if l.startswith("target datalayout"):
            break
        if l.startswith(";") and "shader hash" not in l and "additional functionality" not in l:
            out.append(l.rstrip())
    return out


new_recs = {}
for i, (R, V, dig) in RECS.items():
    o, s = ents[i]
    dx, dsz, tsz = rec_parts(payload, o)
    assert payload[dx:dx + 4] == b"DXBC" and payload[dx + 4:dx + 20].hex() == dig, f"record #{i} digest mismatch - game updated?"
    out = os.path.join(BUILD, f"ESM_fast_r{R}_{'v' if V else 'h'}.dxil")
    r = subprocess.run([DXC, "-T", "ps_6_5", "-E", "main", "-O3", "-validator-version", "1.5", "-Qstrip_reflect",
                        "-Qstrip_debug", "-D", f"R={R}", "-D", f"VERT={V}", "-Fo", out,
                        os.path.join(HERE, "ESM_fast.hlsl")], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout, r.stderr); sys.exit(1)
    new = open(out, "rb").read()
    assert new[:4] == b"DXBC"
    # ---- interface checks against the shipped record
    od = payload[dx:dx + dsz]
    open(os.path.join(BUILD, f"orig_{i}.dxbc"), "wb").write(od)
    _, _, op = idrk.dxbc_parts(od, 0)
    _, ndig, np_ = idrk.dxbc_parts(new, 0)
    assert sorted(op) == sorted(np_), (i, sorted(op), sorted(np_))       # same parts, no STAT/ILDN/RTS0
    for part in ("SFI0", "ISG1", "OSG1", "PSV0"):
        a = od[op[part][0]:op[part][0] + op[part][1]]
        b = new[np_[part][0]:np_[part][0] + np_[part][1]]
        assert a == b, f"record #{i}: {part} differs"
    assert dumpbin_iface(os.path.join(BUILD, f"orig_{i}.dxbc")) == dumpbin_iface(out), f"record #{i}: interface differs"
    tail = payload[dx + dsz:dx + dsz + tsz]
    rec = bytearray(b"CXDK2000" + struct.pack("<II", len(new), tsz) + new + tail)
    while len(rec) % 4:
        rec += b"\0"
    new_recs[i] = bytes(rec)
    print(f"  #{i:2d} R={R} {'V' if V else 'H'}: {s} -> {len(rec)} B  digest {dig} -> {ndig.hex()}  (SFI0/ISG1/OSG1/PSV0 identical)")

# ---- splice (records are contiguous in table order)
p = bytearray(payload[:base])
offs = []
for i, (o, s) in enumerate(ents):
    offs.append(len(p) - base)
    p += new_recs.get(i, payload[base + o:base + o + s])
tail_from = base + ents[-1][0] + ents[-1][1]
p += payload[tail_from:]
for i in range(len(ents)):
    struct.pack_into("<II", p, tab + 8 * i, offs[i], len(new_recs[i]) if i in new_recs else ents[i][1])
struct.pack_into("<I", p, 0x08, len(p))
struct.pack_into("<I", p, 0xB4, len(p) - base)
p = bytes(p)
# ---- verify
for i, (oo, ss) in enumerate(ents):
    o, s = struct.unpack_from("<II", p, tab + 8 * i)
    assert p[base + o:base + o + 8] == b"CXDK2000"
    if i not in new_recs:
        assert p[base + o:base + o + s] == payload[base + oo:base + oo + ss], i
    else:
        assert p[base + o:base + o + s] == new_recs[i]
repl = idrk.pack(p, orig)
assert idrk.unpack(repl) == p
dst = os.path.join(HERE, "replacement", NAME)
open(dst, "wb").write(repl)
print(f"file {len(orig)} -> {len(repl)} B (payload {len(payload)} -> {len(p)}); records #0,#1 byte-identical")
print("STAGED (not installed):", dst)
