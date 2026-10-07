"""build_ghdm.py - build the replacement shader_dx12\\0x0104840c.file (group "GHDM") with variant 0
swapped for GHDM_fast.hlsl.  Reads the ORIGINAL from backup\\ (created on first run from the game dir),
never writes to the game folder.

    py build_ghdm.py            -> replacement\\0x0104840c.file (+ build\\ artifacts, verification printout)
"""
import os, shutil, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import idrk

HERE = os.path.dirname(os.path.abspath(__file__))
GAME_FILE = r"C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12\0x0104840c.file"
DXC = r"C:\Users\Ahmad Hamza\nioh3-engine\tools\rdts\RadeonDeveloperToolSuite-2026-05-28-1806\utils\dx12\dxc\dxc.exe"
ORIG_DIGEST = bytes.fromhex("b6429c529ecf08a2cb36bd882a841c19")   # container digest of GHDM variant 0
VARIANT = 0

os.makedirs(os.path.join(HERE, "backup"), exist_ok=True)
os.makedirs(os.path.join(HERE, "build"), exist_ok=True)
os.makedirs(os.path.join(HERE, "replacement"), exist_ok=True)
bak = os.path.join(HERE, "backup", "0x0104840c.file")
if not os.path.exists(bak):
    shutil.copy2(GAME_FILE, bak)
orig = open(bak, "rb").read()
payload = idrk.unpack(orig)
assert payload[:8] == b"_S2G7000"

# --- compile (validator 1.5 = same as the shipped shader; signed by the bundled Microsoft dxil.dll)
out = os.path.join(HERE, "build", "GHDM_fast_v0.dxil")
cmd = [DXC, "-T", "cs_6_0", "-E", "main", "-O3", "-validator-version", "1.5",
       "-Fo", out, os.path.join(HERE, "GHDM_fast.hlsl")]
r = subprocess.run(cmd, capture_output=True, text=True)
if r.returncode != 0:
    print(r.stdout, r.stderr); sys.exit(1)
new = open(out, "rb").read()
assert new[:4] == b"DXBC" and len(new) % 4 == 0
exact = len(new)            # table stores the exact DXBC size; padding only moves later records
if len(new) % 8:
    new += b"\0" * (8 - len(new) % 8)  # keep 8-byte record alignment (DXBC size field still exact)

# --- splice into the S2G group
total, = struct.unpack_from("<I", payload, 0x08)
tab, = struct.unpack_from("<I", payload, 0x98)
base, blobs = struct.unpack_from("<II", payload, 0xB0)
assert total == len(payload) and base + blobs == len(payload)
n = 0
ents = []
while True:
    o, s = struct.unpack_from("<II", payload, tab + 8 * n)
    if base + o + 4 > len(payload) or payload[base + o:base + o + 4] != b"DXBC":
        break
    ents.append((o, s)); n += 1
    if o + s >= blobs:
        break
assert len(ents) == 4, ents
o0, s0 = ents[VARIANT]
assert payload[base + o0 + 4:base + o0 + 20] == ORIG_DIGEST, "variant 0 digest mismatch - game updated?"
delta = len(new) - s0
p = bytearray(payload[:base + o0]) + new + payload[base + o0 + s0:]
struct.pack_into("<I", p, 0x08, len(p))
struct.pack_into("<I", p, 0xB4, blobs + delta)
for i, (o, s) in enumerate(ents):
    if i == VARIANT:
        struct.pack_into("<II", p, tab + 8 * i, o, exact)
    elif o > o0:
        struct.pack_into("<II", p, tab + 8 * i, o + delta, s)
p = bytes(p)

# --- verify: every table entry points at a DXBC of the stated size; other variants byte-identical
for i in range(4):
    o, s = struct.unpack_from("<II", p, tab + 8 * i)
    size, dig, parts = idrk.dxbc_parts(p, base + o)
    assert p[base + o:base + o + 4] == b"DXBC" and size == s, (i, o, s, size)
    if i != VARIANT:
        oo, ss = ents[i]
        assert p[base + o:base + o + s] == payload[base + oo:base + oo + ss]
    print(f"  variant {i}: off {o:6d} size {s:5d} digest {dig.hex()}")

repl = idrk.pack(p, orig)
assert idrk.unpack(repl) == p
dst = os.path.join(HERE, "replacement", "0x0104840c.file")
open(dst, "wb").write(repl)
open(os.path.join(HERE, "build", "0x0104840c.payload.bin"), "wb").write(p)
print(f"original {len(orig)} B (payload {len(payload)}), replacement {len(repl)} B (payload {len(p)}), "
      f"variant0 {s0} -> {exact} B")
print("wrote", dst)
