r"""build_wdo28.py - STAGE (not install) a replacement shader_dx12\0x1fd07cc0.file ("WDO-PtSrbVsm") whose
record #28 (sun-shadow-mask depth-aware vertical blur, RGP PSO 3e7955a89ee71e82) is swapped for
WDO28_fast.hlsl. Reads the game file read-only; writes only to re\scripts_gpu\out\wdo28\.

Graphics-group record layout (verified on #26..#30): "CXDK2000" | u32 dxbcSize | u32 tailSize | DXBC | tail,
table entry (u32 offset, u32 recordSize) with recordSize = 16 + dxbcSize + tailSize rounded up to 4.

    py -I build_wdo28.py      -> out\wdo28\0x1fd07cc0.file (+ verification printout)
Install/test (NOT done here, game must be closed): copy over <Nioh3>\shader_dx12\0x1fd07cc0.file after backing
the original up; revert = restore the backup or Steam "verify integrity".
"""
import os, struct, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "gpu-shaders"))
import idrk

GAME_FILE = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12\0x1fd07cc0.file"
DXC = os.path.join(HERE, "..", "..", "tools", "rdts", "RadeonDeveloperToolSuite-2026-05-28-1806", "utils", "dx12", "dxc", "dxc.exe")
ORIG_DIGEST = bytes.fromhex("32e8deff99183945b94bc3be679bfa95")   # container digest of record #28 (pso_map)
REC = 28
OUT = os.path.join(HERE, "out", "wdo28"); os.makedirs(OUT, exist_ok=True)

orig = open(GAME_FILE, "rb").read()
payload = idrk.unpack(orig)
assert payload[:8] == b"_S2G7000" and idrk.group_name(payload) == "WDO-PtSrbVsm"

dxil = os.path.join(OUT, "WDO28_fast.dxil")
r = subprocess.run([DXC, "-T", "ps_6_5", "-E", "main", "-O3", "-validator-version", "1.5", "-Fo", dxil,
                    os.path.join(HERE, "WDO28_fast.hlsl")], capture_output=True, text=True)
if r.returncode:
    print(r.stdout, r.stderr); sys.exit(1)
new = open(dxil, "rb").read()
assert new[:4] == b"DXBC"

total, = struct.unpack_from("<I", payload, 0x08)
tab, = struct.unpack_from("<I", payload, 0x98)
base, blobs = struct.unpack_from("<II", payload, 0xB0)
assert total == len(payload) and base + blobs == len(payload)
ents = []
n = 0
while tab + 8 * n + 8 <= base:
    o, s = struct.unpack_from("<II", payload, tab + 8 * n)
    if base + o + 8 > len(payload) or payload[base + o:base + o + 8] != b"CXDK2000":
        break
    ents.append((o, s)); n += 1
print("records:", len(ents))
o0, s0 = ents[REC]
dsz, tsz = struct.unpack_from("<II", payload, base + o0 + 8)
dx0 = base + o0 + 16
assert payload[dx0:dx0 + 4] == b"DXBC" and payload[dx0 + 4:dx0 + 20] == ORIG_DIGEST, "record #28 mismatch - game updated?"
tail = payload[dx0 + dsz:dx0 + dsz + tsz]
rec = bytearray(b"CXDK2000" + struct.pack("<II", len(new), tsz) + new + tail)
while len(rec) % 4:
    rec += b"\0"
delta = len(rec) - s0
p = bytearray(payload[:base + o0]) + rec + payload[base + o0 + s0:]
struct.pack_into("<I", p, 0x08, len(p))
struct.pack_into("<I", p, 0xB4, blobs + delta)
for i, (o, s) in enumerate(ents):
    if i == REC:
        struct.pack_into("<II", p, tab + 8 * i, o, len(rec))
    elif o > o0:
        struct.pack_into("<II", p, tab + 8 * i, o + delta, s)
p = bytes(p)
# verify every record; all but #28 byte-identical
for i, (oo, ss) in enumerate(ents):
    o, s = struct.unpack_from("<II", p, tab + 8 * i)
    assert p[base + o:base + o + 8] == b"CXDK2000"
    if i != REC:
        assert p[base + o:base + o + s] == payload[base + oo:base + oo + ss], i
repl = idrk.pack(p, orig)
assert idrk.unpack(repl) == p
dst = os.path.join(OUT, "0x1fd07cc0.file")
open(dst, "wb").write(repl)
print("record #28: %d -> %d bytes (dxbc %d -> %d, tail %d kept); file %d -> %d bytes" % (s0, len(rec), dsz, len(new), tsz, len(orig), len(repl)))
print("new container digest:", new[4:20].hex())
print("STAGED (not installed):", dst)
